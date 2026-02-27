# ESP32-C3 Super Mini Adaptation

This documents the changes made to adapt [urtziDV/Makita-OBI-ESP32](https://github.com/urtziDV/Makita-OBI-ESP32) (designed for ESP32-WROOM-32) to run on an **ESP32-C3 Super Mini**.

## Hardware Differences

The original project uses an NPN transistor (BC547) to control the BMS enable line. This adaptation uses **direct drive** with a pull-up resistor instead.

| | Original (ESP32-WROOM-32) | This Adaptation (ESP32-C3 Super Mini) |
|---|---|---|
| Board | esp32dev | lolin_c3_mini |
| OneWire Pin | GPIO4 | GPIO4 |
| Enable Pin | GPIO5 | GPIO3 |
| Enable Circuit | NPN transistor (LOW=on) | Direct drive with 4.7k pull-up (HIGH=on) |
| USB | UART bridge | Native USB CDC |
| CPU Cores | Dual-core | Single-core (RISC-V) |

## Wiring

```
ESP32-C3 Super Mini          Makita Battery Terminal
                    +-----------------------------------------+
                    |     [1]  [2]  [3]  [4]  [5]  [6]  [7]  |
                    |  B+      DATA                EN     B-  |
                    +-----------------------------------------+

  3.3V --+----------------- Powers pull-up resistors only
         |
         +-- 4.7k --- GPIO3 ---------- Pin 6 (EN)
         |
         +-- 4.7k --- GPIO4 ---------- Pin 2 (DATA)

  GND -------------------------------- Pin 5 (B-)

  DO NOT CONNECT: B+ (Pin 1) = 18V -- will destroy the ESP32
```

**Important**: Pull-up resistors MUST be connected to 3.3V, not 5V. Using 5V causes all-0xFF garbage data on reads.

**Note on pin numbering**: Per [appositeit/obi-esp32#4](https://github.com/appositeit/obi-esp32/issues/4), the Makita terminal pin numbering in some documentation is reversed. The mapping above (GPIO3->EN, GPIO4->DATA) has been tested and confirmed working. If reads fail, try swapping the two battery-side wires.

## Changes Made

### platformio.ini
- Board: `esp32dev` -> `lolin_c3_mini`
- Added ESP32-C3 specific settings: `board_build.mcu`, `cdc_on_boot`, flash mode
- Added USB CDC build flags: `-DARDUINO_USB_MODE=1`, `-DARDUINO_USB_CDC_ON_BOOT=1`
- Pinned platform to `espressif32@6.7.0`
- Upload speed set to 115200 (460800 causes baud rate change errors on C3 USB CDC)

### src/MakitaBMS.h
- `cmd_and_read_cc` / `cmd_and_read_33` return type changed from `void` to `bool` (returns reset/presence result)
- Added three new private methods: `powerCycle()`, `resetWithRetry()`, `isResponseGarbage()`

### src/MakitaBMS.cpp
- **Inverted all enable pin logic** throughout the file (7 locations):
  - Original: `digitalWrite(_enable_pin, LOW)` = ON, `HIGH` = OFF (NPN transistor)
  - Adapted: `digitalWrite(_enable_pin, HIGH)` = ON, `LOW` = OFF (direct drive)
- Fixed C++14 temporary array errors (`(const byte[]){...}` -> named local arrays)
- Lock status strings changed from Spanish to English ("LOCKED"/"UNLOCKED")
- All error/status messages translated to English
- **Auto-detection reliability overhaul:**
  - `powerCycle()`: LOW 100ms -> HIGH 300ms, forces BMS wake from dormancy
  - `resetWithRetry()`: retries OneWire reset up to 3x with 100ms gaps (BMS frequently ignores presence pulses)
  - `isResponseGarbage()`: detects all-0xFF or all-0x00 responses (no battery / shorted bus)
  - `readStaticData()` rewritten: tries full read sequence, if garbage does a power cycle and retries once more. Power cycles before each model identification attempt (getModel / getF0513Model)
  - `readDynamicData()` now validates responses: garbage check on STANDARD 29-byte response, all-0xFFFF/0x0000 cell voltage check on F0513, and post-parse sanity check (pack > 25V or cell > 5V = mid-read disconnect)
- Reduced all blocking delays from 400ms to 300ms

### src/main.cpp
- `ONEWIRE_PIN`: 4 -> 4 (unchanged, maps to DATA)
- `ENABLE_PIN`: 5 -> 3 (maps to EN)
- All log/status messages translated from Spanish to English
- Default language changed to English
- **Auto-detection loop rewritten:**
  - Polls `readStaticData()` every 5s; backs off to 15s after 3 consecutive failures (reduces WiFi disruption on single-core C3)
  - Requires 2 consecutive `readDynamicData()` failures before declaring disconnect (tolerates transient glitches)
  - Logs backoff state transitions for debugging
  - Resets all counters on state changes (detected / disconnected)
- **WebSocket state synchronization:**
  - New clients receive cached battery data on connect (no blank page if battery was already detected)
  - Manual `read_static` success/failure syncs auto-detection state (fixes stale UI after battery swap)
  - Manual `read_dynamic` failures count toward disconnect detection
- Hardcoded WiFi credentials removed (defaults to empty, configured via Settings panel)

### data/ (Web Interface)
- **Complete UI redesign**: Mobile-first iOS-style layout replaced with a desktop dashboard
  - Fixed sidebar with controls, status, and settings
  - Multi-column grid layout for data panels
  - Responsive: sidebar collapses to hamburger menu on screens < 900px
  - System font stack instead of Google Fonts (works offline)
- All Spanish strings and comments translated to English
- Default language set to English (ES still available via toggle)
- Removed emoji-heavy text in favor of cleaner typography
- Client-side voltage range validation: rejects dynamic data with pack > 25V or cell > 5V before rendering (prevents UI crash on mid-read disconnect)

### lib/OneWireMakita/ (unchanged)
The OneWire library was not modified. It already uses:
- `OUTPUT_OPEN_DRAIN` pin mode (proper for OneWire)
- `portENTER_CRITICAL` / `portEXIT_CRITICAL` for interrupt protection
- Static `portMUX_TYPE` mutex (no brace-scoping issues)

## Building and Flashing

```bash
# Build
pio run -e esp32c3

# Upload web interface (LittleFS) -- do this first or after any changes to data/
pio run -e esp32c3 -t uploadfs

# Upload firmware
pio run -e esp32c3 -t upload
```

If upload fails with "No serial data received", hold the BOOT button on the ESP32-C3 while plugging in USB, then retry. This is a common issue with the C3's native USB CDC -- the baud rate change during stub upload can fail.

## Accessing the Interface

1. Connect to WiFi network **"Makita_OBI_ESP32"** (open, no password)
2. Open http://192.168.4.1 (captive portal should redirect automatically)
3. Alternatively, configure home WiFi through the Settings panel and access via http://makita.local

## Auto-Detection Behavior

- Battery is detected automatically within 5-10s of insertion (no button press needed)
- If no battery is found after 3 attempts (15s), polling backs off to every 15s to reduce WiFi disruption
- Battery removal is detected after 2 consecutive failed dynamic reads (~20s)
- Opening the web interface while a battery is already connected shows data immediately
- Swapping batteries is handled cleanly: failed reads reset state, new battery is re-detected automatically

## Known Issues

- Pull-up resistors MUST be connected to 3.3V, not 5V (5V causes all-0xFF garbage data)
- The ESP32-C3 is single-core, so blocking BMS communication (~300ms per call) can briefly disrupt WiFi. Detection backs off after failures to mitigate this.
- Flashing often requires BOOT button hold due to USB CDC baud rate negotiation failures
- The `isPresent()` function may return false even when a battery is connected, because the Makita BMS doesn't always respond with a standard OneWire presence pulse. Auto-detection uses `readStaticData()` directly instead.
