# Makita BMS Tool — ESP32-C3 Fork

Diagnostic tool for Makita LXT 18V (BL18xx) and 14.4V (BL14xx) batteries, running on an **ESP32-C3 Super Mini** mounted inside a Makita charger for automatic battery testing.

This fork ports [urtziDV/Makita-OBI-ESP32](https://github.com/urtziDV/Makita-OBI-ESP32) from the ESP32-WROOM-32 to the ESP32-C3, with a rewritten auto-detection system and redesigned web interface.

> Vibe-coded with [Claude Opus 4.6](https://claude.ai) (Anthropic).

## What's Different From Upstream

- **ESP32-C3 Super Mini support** — direct-drive enable pin (no NPN transistor), native USB CDC, single-core RISC-V optimizations
- **Reliable auto-detection** — power cycling, retry logic, and response validation. Detects battery insertion in 5-10s, removal in ~20s
- **Garbage rejection** — validates OneWire responses (all-0xFF / all-0x00 = no battery) and parsed voltages (cell > 5V = mid-read disconnect)
- **Redesigned web UI** — sidebar dashboard layout, responsive, works offline (no Google Fonts), English default with Spanish toggle
- **WebSocket state sync** — new clients get cached data immediately, manual reads sync with auto-detection state
- **Battery history database** — persistent per-battery history stored on LittleFS with long-term voltage/SOH tracking
- **WiFi network scanner** — scan and select networks from the UI instead of typing SSIDs manually

See [ESP32-C3-ADAPTATION.md](./ESP32-C3-ADAPTATION.md) for the full technical changelog.

## Features

- Automatic battery detection and live monitoring (no button press needed)
- Cell-level voltage readings with imbalance detection
- Charge cycle counter, lock status, manufacturing date
- SOH (State of Health) estimation and balancing assistant
- Real-time voltage history chart
- **Battery history** — per-battery long-term history with voltage trends, charge cycles, and SOH tracking
- **Inline history view** — browse past batteries when no battery is connected, view connected battery's history by scrolling down
- **Auto-detect toggle** — pause battery detection from the UI (useful during WiFi configuration)
- **WiFi scanner** — scan for networks and select from a dropdown in Settings
- **History data validation** — rejects out-of-range voltage readings before storing
- LED test and error clearing (STANDARD controller batteries)
- Dark mode, bilingual (EN/ES), OTA firmware updates
- Dual WiFi: AP mode + station mode with mDNS (`http://makita.local`)
- Captive portal with proper connectivity-check handling (no browser login nag)

## Hardware

- **ESP32-C3 Super Mini** (or any ESP32-C3 board)
- 2x 4.7k pull-up resistors to 3.3V (DATA and EN lines)
- No transistor needed — direct GPIO drive

```
ESP32-C3                     Makita Battery Terminal
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

**Important**: Pull-ups MUST go to 3.3V, not 5V. 5V causes all-0xFF garbage reads.

## Building & Flashing

Requires [PlatformIO](https://platformio.org/).

```bash
# Upload web interface (do this first, or after changes to data/)
pio run -e esp32c3 -t uploadfs

# Upload firmware
pio run -e esp32c3 -t upload
```

If upload fails with "No serial data received", unplug USB and replug (or hold BOOT while pressing RESET).

## Usage

1. Connect to WiFi network **Makita_OBI_ESP32** (open, no password)
2. Open http://192.168.4.1 (captive portal should redirect)
3. Insert a battery — data appears automatically within 5-10s
4. Optionally configure home WiFi in Settings for `http://makita.local` access

## Project Structure

- `/src` — Firmware (C++): BMS protocol, auto-detection, WebSocket server
- `/data` — Web interface (HTML/JS/CSS), served from LittleFS
- `/lib/OneWireMakita` — Custom OneWire library for Makita's protocol

## Credits & License

Based on work by:
- **urtziDV** — [Makita-OBI-ESP32](https://github.com/urtziDV/Makita-OBI-ESP32)
- **Belik1982** — [esp32-makita-bms-reader](https://github.com/Belik1982/esp32-makita-bms-reader) (original implementation)

License: [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/) — Attribution, NonCommercial, ShareAlike.
