// src/MakitaBMS.cpp - VERSIÓN OPTIMIZADA Y DOCUMENTADA

#include "MakitaBMS.h"

// --- Definiciones de comandos estáticos (Requerido para el Linker en C++14) ---
constexpr byte MakitaBMS::CMD_READ_STATIC[];
constexpr byte MakitaBMS::CMD_READ_DYNAMIC[];
constexpr byte MakitaBMS::CMD_LED_TEST_INIT[];
constexpr byte MakitaBMS::CMD_LED_ON[];
constexpr byte MakitaBMS::CMD_LED_OFF[];
constexpr byte MakitaBMS::CMD_CLEAR_ERR_INIT[];
constexpr byte MakitaBMS::CMD_CLEAR_ERR_EXEC[];
constexpr byte MakitaBMS::CMD_GET_MODEL[];

/**
 * Convierte el estado interno del BMS a un mensaje de texto comprensible.
 */
String statusToString(BMSStatus status) {
    switch (status) {
        case BMSStatus::OK: return "";
        case BMSStatus::ERROR_NOT_PRESENT: return "Battery not detected.";
        case BMSStatus::ERROR_NOT_IDENTIFIED: return "Battery must be identified first (read static data).";
        case BMSStatus::ERROR_MODEL_NOT_SUPPORTED: return "Battery model not supported.";
        case BMSStatus::ERROR_COMMUNICATION: return "Communication error (data integrity).";
        case BMSStatus::ERROR_NOT_AVAILABLE: return "Function not available for this model.";
        default: return "Unknown error.";
    }
}

/**
 * Constructor de la clase MakitaBMS.
 * Configura los pines de comunicación y de control de energía.
 */
MakitaBMS::MakitaBMS(uint8_t onewire_pin, uint8_t enable_pin) 
    : makita(onewire_pin), _enable_pin(enable_pin) {
    pinMode(_enable_pin, OUTPUT);
    digitalWrite(_enable_pin, LOW); // Initial state: OFF (direct drive, HIGH=on)
}

void MakitaBMS::setLogCallback(LogCallback callback) { _log = callback; }
void MakitaBMS::setLogLevel(LogLevel level) { _logLevel = level; }

/**
 * Envía mensajes de texto a través del callback registrado (Serial/Web).
 */
void MakitaBMS::logger(const String& message, LogLevel level) { 
    if (_log && level <= _logLevel) _log(message, level); 
}

/**
 * Helper para imprimir bloques de datos en formato hexadecimal (depuración profunda).
 */
void MakitaBMS::log_hex(const String& prefix, const byte* data, int len) { 
    if (_logLevel < LOG_LEVEL_DEBUG || !_log) return; 
    String hex_str;
    hex_str.reserve(prefix.length() + len * 3);
    hex_str = prefix; 
    if (data && len > 0) {
        for (int i = 0; i < len; i++) { 
            char buf[4]; 
            sprintf(buf, "%02X ", data[i]); 
            hex_str += buf; 
        } 
    }
    _log(hex_str, LOG_LEVEL_DEBUG); 
}

/**
 * Power cycle the BMS: LOW 100ms -> HIGH 300ms.
 * Forces a fresh wake-up from dormancy.
 */
void MakitaBMS::powerCycle() {
    digitalWrite(_enable_pin, LOW);
    delay(100);
    digitalWrite(_enable_pin, HIGH);
    delay(300);
}

/**
 * Retry OneWire reset() up to max_attempts times with 100ms gaps.
 * BMS frequently ignores presence pulses, so retrying is essential.
 */
bool MakitaBMS::resetWithRetry(uint8_t max_attempts) {
    for (uint8_t i = 0; i < max_attempts; i++) {
        if (makita.reset()) return true;
        delay(100);
    }
    return false;
}

/**
 * Check if a response buffer is garbage (all 0xFF or all 0x00).
 * Pull-up idle with no battery produces 0xFF; shorted bus produces 0x00.
 */
bool MakitaBMS::isResponseGarbage(const byte* data, uint8_t len) {
    if (data == nullptr || len == 0) return true;
    uint8_t check = (len < 3) ? len : 3;
    bool all_ff = true, all_00 = true;
    for (uint8_t i = 0; i < check; i++) {
        if (data[i] != 0xFF) all_ff = false;
        if (data[i] != 0x00) all_00 = false;
    }
    return all_ff || all_00;
}

/**
 * Ejecuta una transacción de comando/respuesta usando el protocolo tipo 'CC' (Común).
 * Returns true if reset() succeeded (presence pulse detected).
 */
bool MakitaBMS::cmd_and_read_cc(const byte* cmd, uint8_t cmd_len, byte* rsp, uint8_t rsp_len) {
    bool present = makita.reset();
    delayMicroseconds(400);
    makita.write(0xcc); // byte de control tipo CC
    log_hex(">> CC (cmd): ", cmd, cmd_len);
    if (cmd != nullptr) for (int i = 0; i < cmd_len; i++) { makita.write(cmd[i]); delayMicroseconds(90); }
    if (rsp != nullptr) for (int i = 0; i < rsp_len; i++) { rsp[i] = makita.read(); delayMicroseconds(90); }
    log_hex("<< CC (rsp): ", rsp, rsp_len);
    return present;
}

/**
 * Ejecuta una transacción compleja usando el protocolo tipo '33'.
 * Este protocolo requiere una lectura inicial de 8 bytes (ROM ID) antes de enviar el comando.
 * Returns true if reset() succeeded (presence pulse detected).
 */
bool MakitaBMS::cmd_and_read_33(const byte* cmd, uint8_t cmd_len, byte* rsp, uint8_t rsp_len) {
    bool present = makita.reset();
    delayMicroseconds(400);
    makita.write(0x33); // byte de control tipo 33
    log_hex(">> 33 (env): ", cmd, cmd_len);
    byte initial_read[8];
    for (int i = 0; i < 8; i++) { initial_read[i] = makita.read(); delayMicroseconds(90); }
    log_hex("<< 33 (8b ROM): ", initial_read, 8);
    for (int i = 0; i < cmd_len; i++) { makita.write(cmd[i]); delayMicroseconds(90); }
    for (int i = 0; i < rsp_len; i++) { rsp[i] = makita.read(); delayMicroseconds(90); }
    log_hex("<< 33 (rsp): ", rsp, rsp_len);
    return present;
}

/**
 * Comprueba si hay una batería físicamente conectada al puerto.
 */
bool MakitaBMS::isPresent() {
    digitalWrite(_enable_pin, HIGH); // Encender alimentación del BMS
    delay(300);

    bool present = makita.reset();   // Intentar resetear el bus y capturar pulso de presencia

    digitalWrite(_enable_pin, LOW);  // Apagar para seguridad
    
    if (present) {
        logger("Presence: Battery detected", LOG_LEVEL_INFO);
    } else {
        logger("Presence: Empty bus", LOG_LEVEL_INFO);
    }
    return present;
}

/**
 * Identifica la batería leyendo la tabla de datos estáticos maestros.
 * Determina el modelo de procesador y las funciones disponibles.
 *
 * Uses power cycling and retry logic for reliable BMS wake-up.
 * Worst-case blocking: ~2.6s (no battery). Typical success: ~1.5s.
 */
BMSStatus MakitaBMS::readStaticData(BatteryData &data, SupportedFeatures &features) {
    logger("--- Reading Static Data (Identification) ---", LOG_LEVEL_INFO);
    _is_identified = false;

    byte response[40];
    bool data_valid = false;

    // Try up to 2 attempts: initial read, then power cycle + retry
    for (int attempt = 0; attempt < 2 && !data_valid; attempt++) {
        if (attempt == 0) {
            // First attempt: simple power on
            digitalWrite(_enable_pin, HIGH);
            delay(300);
        } else {
            // Second attempt: full power cycle to wake dormant BMS
            logger("Retrying with power cycle...", LOG_LEVEL_DEBUG);
            powerCycle();
        }

        // Single clean reset→command→read sequence (matches original timing)
        makita.reset();
        delayMicroseconds(400);
        makita.write(0x33);

        byte initial_read[8];
        for (int i = 0; i < 8; i++) { initial_read[i] = makita.read(); delayMicroseconds(90); }
        for (int i = 0; i < 2; i++) { makita.write(CMD_READ_STATIC[i]); delayMicroseconds(90); }

        byte remaining[32];
        for (int i = 0; i < 32; i++) { remaining[i] = makita.read(); delayMicroseconds(90); }

        memcpy(response, initial_read, 8);
        memcpy(response + 8, remaining, 32);

        log_hex("Static raw: ", response, 40);

        if (!isResponseGarbage(response, 40)) {
            data_valid = true;
        }
    }

    if (!data_valid) {
        logger("Response is garbage (no battery)", LOG_LEVEL_DEBUG);
        digitalWrite(_enable_pin, LOW);
        return BMSStatus::ERROR_NOT_PRESENT;
    }

    // Parse static fields
    data.charge_cycles = ((nibble_swap(response[34]) << 8) | nibble_swap(response[35])) & 0x0FFF;
    data.lock_status = (response[28] & 0x0F) > 0 ? "LOCKED" : "UNLOCKED";

    char buf[12];
    sprintf(buf, "%02X", response[27]); data.status_code = buf;
    sprintf(buf, "%02d/%02d/20%02d", response[2], response[1], response[0]); data.mfg_date = buf;
    data.capacity = String(nibble_swap(response[24]) / 10.0f, 1) + "Ah";
    data.battery_type = String(nibble_swap(response[19]));

    data.rom_id = "";
    data.rom_id.reserve(24);
    for(int i = 0; i < 8; i++) { char r_buf[4]; sprintf(r_buf, "%02X ", response[i]); data.rom_id += r_buf; }

    // Power cycle before model identification — fresh wake-up
    powerCycle();

    // Try standard controller first
    _controller = ControllerType::UNKNOWN;
    String m_str = getModel();
    if (m_str != "") {
        _controller = ControllerType::STANDARD;
        data.model = m_str;
    } else {
        // Power cycle before F0513 attempt
        powerCycle();
        m_str = getF0513Model();
        if (m_str != "") {
            _controller = ControllerType::F0513;
            data.model = m_str;
        }
    }

    // Cell count detection based on model
    if (data.model.startsWith("BL14")) {
        data.cell_count = 4;
    } else {
        data.cell_count = 5;
    }

    digitalWrite(_enable_pin, LOW);

    if (_controller == ControllerType::UNKNOWN) return BMSStatus::ERROR_MODEL_NOT_SUPPORTED;

    _is_identified = true;
    features.read_dynamic = true;
    if (_controller == ControllerType::STANDARD) {
        features.led_test = true;
        features.clear_errors = true;
    }

    logger("Identification complete: " + data.model, LOG_LEVEL_INFO);
    return BMSStatus::OK;
}

/**
 * Lee voltajes y temperaturas actuales.
 * Utiliza algoritmos diferentes según el controlador detectado previamente.
 */
BMSStatus MakitaBMS::readDynamicData(BatteryData &data) {
    if (!_is_identified) return BMSStatus::ERROR_NOT_IDENTIFIED;
    logger("--- Reading Voltages & Temperatures ---", LOG_LEVEL_INFO); 
    
    if (_controller == ControllerType::STANDARD) {
        digitalWrite(_enable_pin, HIGH);
        delay(300);

        byte rsp[29];
        cmd_and_read_cc(CMD_READ_DYNAMIC, 4, rsp, sizeof(rsp));

        // Validate response — garbage means battery not responding
        if (isResponseGarbage(rsp, sizeof(rsp))) {
            logger("Dynamic read: garbage response", LOG_LEVEL_DEBUG);
            digitalWrite(_enable_pin, LOW);
            return BMSStatus::ERROR_COMMUNICATION;
        }

        // Conversión de bytes a voltajes reales
        data.pack_voltage = ((rsp[1] << 8) | rsp[0]) / 1000.0f;
        float min_v = 5.0, max_v = 0.0;
        for(int i=0; i<data.cell_count; i++) {
            float val = ((rsp[i*2+3] << 8) | rsp[i*2+2]) / 1000.0f;
            data.cell_voltages[i] = val;
            if (val > 0.5 && val < min_v) min_v = val;
            if (val > max_v) max_v = val;
        }
        // Limpiamos celdas no usadas si es 4S
        if (data.cell_count < 5) {
            for(int i=data.cell_count; i<5; i++) data.cell_voltages[i] = 0.0;
        }
        data.cell_diff = (max_v > min_v) ? (max_v - min_v) : 0.0;
        data.temp1 = ((rsp[15] << 8) | rsp[14]) / 100.0f;
        data.temp2 = ((rsp[17] << 8) | rsp[16]) / 100.0f;

        digitalWrite(_enable_pin, LOW);

        // Sanity check: catch mid-read disconnects where partial data is 0xFF
        if (data.pack_voltage > 25.0f || max_v > 5.0f) {
            logger("Dynamic read: voltage out of range (mid-read disconnect?)", LOG_LEVEL_DEBUG);
            return BMSStatus::ERROR_COMMUNICATION;
        }

    } else if (_controller == ControllerType::F0513) {
        // El controlador F0513 es modo "Power-Request": necesita alimentación por cada comando
        auto exec = [&](const byte* c, uint8_t cl, byte* r, uint8_t rl) {
            digitalWrite(_enable_pin, HIGH); delay(300);
            cmd_and_read_cc(c, cl, r, rl);
            digitalWrite(_enable_pin, LOW); delay(50);
        };

        const byte clr[] = {0xF0, 0x00};
        exec(clr, 2, nullptr, 0); exec(clr, 2, nullptr, 0);
        
        byte r[2]; float v[5], t_v = 0;
        bool all_garbage = true;
        // Solicita el voltaje de cada celda por separado
        for(int i=0; i<data.cell_count; i++) {
            byte cmd_cell[] = {(byte)(0x31 + i)};
            exec(cmd_cell, 1, r, 2);
            uint16_t raw = (r[1]<<8)|r[0];
            v[i] = raw / 1000.0f;
            if (raw != 0xFFFF && raw != 0x0000) all_garbage = false;
        }

        if (all_garbage) {
            logger("F0513 dynamic read: all cells garbage", LOG_LEVEL_DEBUG);
            return BMSStatus::ERROR_COMMUNICATION;
        }

        byte cmd_temp[] = {0x52};
        exec(cmd_temp, 1, r, 2); data.temp1=((r[1]<<8)|r[0])/100.0f;

        float min_v = 5.0, max_v = 0.0;
        for(int i=0; i<5; i++) { 
            if (i < data.cell_count) {
                data.cell_voltages[i] = v[i]; t_v += v[i]; 
                if(v[i] > 0.5 && v[i] < min_v) min_v = v[i]; 
                if(v[i] > max_v) max_v = v[i]; 
            } else {
                data.cell_voltages[i] = 0.0;
            }
        }
        data.pack_voltage = t_v; 
        data.cell_diff = (max_v > 0.5 && max_v > min_v) ? (max_v - min_v) : 0.0;
        data.temp2 = 0;
    }
    
    logger("Dynamic read complete.", LOG_LEVEL_INFO); 
    return BMSStatus::OK;
}

String MakitaBMS::getModel() {
    byte rsp[16];
    cmd_and_read_cc(CMD_GET_MODEL, 2, rsp, sizeof(rsp));
    if (rsp[0] == 0xFF || rsp[0] == 0x00) return "";
    char m[8]; memcpy(m, rsp, 7); m[7] = '\0';
    return String(m);
}

String MakitaBMS::getF0513Model() {
    byte cmd_99[] = {0x99};
    cmd_and_read_cc(cmd_99, 1, nullptr, 0); delay(100);
    makita.reset(); delayMicroseconds(400); makita.write(0x31);
    byte r[2];
    delayMicroseconds(90); r[0] = makita.read(); delayMicroseconds(90); r[1] = makita.read();
    byte cmd_f0[] = {0xF0, 0x00};
    cmd_and_read_cc(cmd_f0, 2, nullptr, 0);
    if (r[0] == 0xFF && r[1] == 0xFF) return "";
    char b[8]; sprintf(b, "BL%02X%02X", r[1], r[0]);
    return String(b);
}

/**
 * Control directo de los LEDs de la placa de la batería.
 */
BMSStatus MakitaBMS::ledTest(bool on) {
    if (!_is_identified || _controller != ControllerType::STANDARD) return BMSStatus::ERROR_NOT_AVAILABLE;
    digitalWrite(_enable_pin, HIGH); delay(300);
    byte b[9];
    cmd_and_read_33(CMD_LED_TEST_INIT, 3, b, 9);
    cmd_and_read_33(on ? CMD_LED_ON : CMD_LED_OFF, 2, b, 9);
    digitalWrite(_enable_pin, LOW);
    return BMSStatus::OK; 
}

/**
 * Intenta borrar errores persistentes y desbloquear el controlador.
 */
BMSStatus MakitaBMS::clearErrors() {
    if (!_is_identified || _controller != ControllerType::STANDARD) return BMSStatus::ERROR_NOT_AVAILABLE;
    digitalWrite(_enable_pin, HIGH); delay(300);
    byte b[9];
    cmd_and_read_33(CMD_CLEAR_ERR_INIT, 3, b, 9);
    cmd_and_read_33(CMD_CLEAR_ERR_EXEC, 2, b, 9);
    digitalWrite(_enable_pin, LOW);
    return BMSStatus::OK; 
}
