// src/main.cpp - VERSIÓN FINAL DOCUMENTADA

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include "FS.h"
#include "LittleFS.h"
#include <Update.h>
#include "MakitaBMS.h"

// --- Declaraciones Forward (Prototipos) ---
void saveConfig(const String& lang, const String& theme, const String& ssid = "", const String& pass = "");
void loadConfig(String& lang, String& theme, String& wifi_ssid, String& wifi_pass);
String statusToString(BMSStatus status); 

// --- Configuraciones y objetos globales ---
// Pin GPIO para la comunicación de un solo hilo (OneWire)
#define ONEWIRE_PIN 4
// Pin GPIO para la señal de habilitación del BMS (directo, sin transistor NPN)
#define ENABLE_PIN  3

// SSID del Punto de Acceso WiFi que creará el ESP32
const char* ssid = "Makita_OBI_ESP32";

// Servidor DNS para soportar el portal cautivo (redirección automática)
DNSServer dnsServer;
// Servidor Web asíncrono en el puerto estándar 80
AsyncWebServer server(80);
// Canal WebSocket para comunicación en tiempo real con la interfaz web
AsyncWebSocket ws("/ws");
// SSID del Punto de Acceso WiFi que creará el ESP32
const char* ssid_ap = "Makita_OBI_ESP32";

// Instancia de la clase controladora del BMS de Makita
MakitaBMS bms(ONEWIRE_PIN, ENABLE_PIN);

// Caché global de datos para mantener la información estática al solicitar actualizaciones dinámicas
static BatteryData cached_data;
// Configuración persistente (WiFi Station)
static String current_lang = "en";
static String current_theme = "light";
static String current_wifi_ssid = "";
static String current_wifi_pass = "";

// Auto-detection timing and state
const unsigned long DETECTION_INTERVAL = 5000;      // 5s normal polling
const unsigned long BACKOFF_INTERVAL = 15000;        // 15s after repeated failures
const unsigned long DYNAMIC_READ_INTERVAL = 10000;   // 10s between dynamic reads
const uint8_t MAX_DETECTION_ATTEMPTS = 3;            // failures before backing off
const uint8_t MAX_DYNAMIC_FAILS = 2;                 // consecutive fails before disconnect

unsigned long lastDetectionAttempt = 0;
bool lastPresenceState = false;
bool autoReadIdentified = false;
unsigned long lastDynamicRead = 0;
uint8_t detectionFailCount = 0;          // consecutive static read failures
uint8_t dynamicFailCount = 0;            // consecutive dynamic read failures
SupportedFeatures cached_features;
bool historyRecorded = false;            // one snapshot per insertion
static unsigned long browserEpoch = 0;   // unix epoch from browser
static unsigned long browserSyncMillis = 0; // millis() when synced
volatile bool wifiScanRequested = false;  // set by WS handler, consumed by loop
bool autoDetectEnabled = true;           // toggled from UI

// --- Funciones de Comunicación ---

/**
 * Envía la información de la batería formateada en JSON a todos los clientes conectados.
 * @param type Tipo de mensaje (static_data o dynamic_data)
 * @param data Estructura con los valores leídos de la batería
 * @param features Puntero a las funciones soportadas (opcional)
 */
void sendJsonResponse(const String& type, const BatteryData& data, const SupportedFeatures* features) {
    if (ws.count() == 0) return;
    DynamicJsonDocument doc(2048);
    doc["type"] = type;

    JsonObject dataObj = doc.createNestedObject("data");
    dataObj["model"] = data.model;
    dataObj["charge_cycles"] = data.charge_cycles;
    dataObj["lock_status"] = data.lock_status;
    dataObj["status_code"] = data.status_code;
    dataObj["mfg_date"] = data.mfg_date;
    dataObj["capacity"] = data.capacity;
    dataObj["battery_type"] = data.battery_type;
    dataObj["pack_voltage"] = data.pack_voltage;
    JsonArray cellV = dataObj.createNestedArray("cell_voltages");
    for(int i=0; i<data.cell_count; i++) cellV.add(data.cell_voltages[i]);
    dataObj["cell_diff"] = data.cell_diff;
    dataObj["temp1"] = data.temp1;
    dataObj["temp2"] = data.temp2;
    dataObj["rom_id"] = data.rom_id;

    if (features) {
        JsonObject featuresObj = doc.createNestedObject("features");
        featuresObj["read_dynamic"] = features->read_dynamic;
        featuresObj["led_test"] = features->led_test;
        featuresObj["clear_errors"] = features->clear_errors;
    }

    String output;
    serializeJson(doc, output);
    ws.textAll(output);
}

/**
 * Envía un mensaje de éxito, error o informativo a la interfaz web.
 */
void sendFeedback(const String& type, const String& message) {
    if (ws.count() == 0) return;
    DynamicJsonDocument doc(512);
    doc["type"] = type;
    doc["message"] = message;
    String output;
    serializeJson(doc, output);
    ws.textAll(output);
}

/**
 * Notifica a los clientes si hay una batería físicamente detectada en el bus.
 */
void sendPresence(bool is_present) {
    if (ws.count() == 0) return;
    DynamicJsonDocument doc(64);
    doc["type"] = "presence";
    doc["present"] = is_present;
    String output;
    serializeJson(doc, output);
    ws.textAll(output);
}

/**
 * Envía mensajes de log del sistema a la interfaz web para depuración remota.
 */
void logToClients(const String& message, LogLevel level) {
    Serial.println(message);
    String prefix = (level == LOG_LEVEL_DEBUG) ? "[DBG] " : "";
    sendFeedback("debug", prefix + message);
}

// --- Battery History ---

// History file header (12 bytes)
struct __attribute__((packed)) HistoryHeader {
    uint8_t  magic[2];      // 0xBA 0x7E
    uint8_t  version;       // format version
    uint8_t  cell_count;    // 4 or 5
    char     model[8];      // null-padded model name
};

// History record (24 bytes)
struct __attribute__((packed)) HistoryRecord {
    uint32_t timestamp;     // unix seconds
    uint16_t charge_cycles;
    uint16_t pack_voltage;  // millivolts
    uint16_t cell_voltages[5]; // millivolts, unused=0
    uint16_t cell_diff;     // millivolts×10
    int16_t  temp1;         // °C×100
    int16_t  temp2;         // °C×100
};

// Get best available unix timestamp: NTP > browser sync > uptime
uint32_t getTimestamp() {
    time_t now = time(nullptr);
    if (now > 1700000000) return (uint32_t)now;  // NTP synced
    if (browserEpoch > 0) return (uint32_t)(browserEpoch + (millis() - browserSyncMillis) / 1000);
    return (uint32_t)(millis() / 1000);  // fallback: uptime
}

String romIdToFilename(const String& rom_id) {
    String clean;
    for (unsigned int i = 0; i < rom_id.length(); i++) {
        if (rom_id[i] != ' ') clean += rom_id[i];
    }
    return "/h/" + clean;
}

void appendHistoryRecord(const BatteryData& data) {
    if (data.rom_id.length() == 0) {
        logToClients("History: empty ROM ID, skipping", LOG_LEVEL_INFO);
        return;
    }

    // Sanity gate: reject snapshots with obviously bad readings
    for (int i = 0; i < data.cell_count; i++) {
        if (data.cell_voltages[i] < 2.0f || data.cell_voltages[i] > 4.5f) {
            logToClients("History: cell " + String(i+1) + " out of range (" +
                         String(data.cell_voltages[i], 3) + "V), skipping", LOG_LEVEL_INFO);
            return;
        }
    }
    if (data.pack_voltage < 8.0f || data.pack_voltage > 23.0f) {
        logToClients("History: pack voltage out of range (" +
                     String(data.pack_voltage, 2) + "V), skipping", LOG_LEVEL_INFO);
        return;
    }

    String path = romIdToFilename(data.rom_id);
    logToClients("History: writing to " + path, LOG_LEVEL_INFO);

    File f = LittleFS.open(path, "a");
    if (!f) {
        logToClients("History: failed to open " + path, LOG_LEVEL_INFO);
        return;
    }

    if (f.size() == 0) {
        HistoryHeader hdr = {};
        hdr.magic[0] = 0xBA;
        hdr.magic[1] = 0x7E;
        hdr.version = 1;
        hdr.cell_count = (uint8_t)data.cell_count;
        strncpy(hdr.model, data.model.c_str(), sizeof(hdr.model));
        f.write((uint8_t*)&hdr, sizeof(hdr));
    }

    HistoryRecord rec = {};
    rec.timestamp = getTimestamp();
    rec.charge_cycles = (uint16_t)data.charge_cycles;
    rec.pack_voltage = (uint16_t)(data.pack_voltage * 1000.0f);
    for (int i = 0; i < 5; i++) {
        rec.cell_voltages[i] = (i < data.cell_count)
            ? (uint16_t)(data.cell_voltages[i] * 1000.0f) : 0;
    }
    rec.cell_diff = (uint16_t)(data.cell_diff * 10000.0f);
    rec.temp1 = (int16_t)(data.temp1 * 100.0f);
    rec.temp2 = (int16_t)(data.temp2 * 100.0f);
    size_t written = f.write((uint8_t*)&rec, sizeof(rec));
    size_t fileSize = f.size();
    f.close();
    logToClients("History snapshot saved (" + String(written) + "B, file=" + String(fileSize) + "B)", LOG_LEVEL_INFO);
}

void sendBatteryList(AsyncWebSocketClient* client) {
    DynamicJsonDocument doc(4096);
    doc["type"] = "battery_list";
    JsonArray arr = doc.createNestedArray("data");

    File dir = LittleFS.open("/h");
    if (!dir || !dir.isDirectory()) {
        String out;
        serializeJson(doc, out);
        client->text(out);
        return;
    }

    File entry = dir.openNextFile();
    while (entry) {
        if (!entry.isDirectory() && entry.size() >= (int)sizeof(HistoryHeader)) {
            HistoryHeader hdr;
            entry.read((uint8_t*)&hdr, sizeof(hdr));

            if (hdr.magic[0] == 0xBA && hdr.magic[1] == 0x7E) {
                size_t dataBytes = entry.size() - sizeof(HistoryHeader);
                int count = dataBytes / sizeof(HistoryRecord);

                JsonObject obj = arr.createNestedObject();
                // entry.name() may return full path or just filename
                String fname = String(entry.name());
                int lastSlash = fname.lastIndexOf('/');
                if (lastSlash >= 0) fname = fname.substring(lastSlash + 1);
                obj["rom_id"] = fname;
                char modelBuf[9] = {};
                memcpy(modelBuf, hdr.model, 8);
                obj["model"] = String(modelBuf);
                obj["cell_count"] = hdr.cell_count;
                obj["readings"] = count;

                // Read last record for "last seen" timestamp
                if (count > 0) {
                    HistoryRecord lastRec;
                    entry.seek(entry.size() - sizeof(HistoryRecord));
                    entry.read((uint8_t*)&lastRec, sizeof(lastRec));
                    obj["last_seen"] = lastRec.timestamp;
                    obj["last_voltage"] = lastRec.pack_voltage / 1000.0f;
                    obj["last_cycles"] = lastRec.charge_cycles;
                    obj["last_diff"] = lastRec.cell_diff / 10000.0f;
                }
            }
        }
        entry = dir.openNextFile();
    }

    String out;
    serializeJson(doc, out);
    client->text(out);
}

void sendBatteryHistory(AsyncWebSocketClient* client, const String& rom_id) {
    String path = romIdToFilename(rom_id);
    DynamicJsonDocument doc(24576);
    doc["type"] = "battery_history";
    doc["rom_id"] = rom_id;
    JsonArray arr = doc.createNestedArray("data");

    File f = LittleFS.open(path, "r");
    if (!f || f.size() < (int)sizeof(HistoryHeader)) {
        String out;
        serializeJson(doc, out);
        client->text(out);
        return;
    }

    HistoryHeader hdr;
    f.read((uint8_t*)&hdr, sizeof(hdr));
    char modelBuf[9] = {};
    memcpy(modelBuf, hdr.model, 8);
    doc["model"] = String(modelBuf);
    doc["cell_count"] = hdr.cell_count;

    size_t dataBytes = f.size() - sizeof(HistoryHeader);
    int total = dataBytes / sizeof(HistoryRecord);
    int cap = 100;
    int skip = (total > cap) ? total - cap : 0;
    if (skip > 0) f.seek(sizeof(HistoryHeader) + skip * sizeof(HistoryRecord));

    HistoryRecord rec;
    int count = (total > cap) ? cap : total;
    for (int i = 0; i < count; i++) {
        if (f.read((uint8_t*)&rec, sizeof(rec)) != sizeof(rec)) break;
        JsonObject obj = arr.createNestedObject();
        obj["ts"] = rec.timestamp;
        obj["cycles"] = rec.charge_cycles;
        obj["pack_mv"] = rec.pack_voltage;
        JsonArray cells = obj.createNestedArray("cells");
        for (int c = 0; c < hdr.cell_count; c++) {
            cells.add(rec.cell_voltages[c]);
        }
        obj["diff"] = rec.cell_diff;
        obj["t1"] = rec.temp1;
        obj["t2"] = rec.temp2;
    }
    f.close();

    String out;
    serializeJson(doc, out);
    client->text(out);
}

void deleteHistory(const String& rom_id) {
    String path = romIdToFilename(rom_id);
    if (LittleFS.exists(path)) {
        LittleFS.remove(path);
        Serial.println("History deleted: " + path);
    }
}

void sendWifiStatus(AsyncWebSocketClient* client) {
    DynamicJsonDocument doc(512);
    doc["type"] = "wifi_status";
    bool staConnected = WiFi.isConnected();
    doc["sta_connected"] = staConnected;
    doc["sta_ssid"] = current_wifi_ssid;
    if (staConnected) {
        doc["sta_ip"] = WiFi.localIP().toString();
        doc["sta_rssi"] = WiFi.RSSI();
    }
    doc["ap_ip"] = WiFi.softAPIP().toString();
    doc["ap_clients"] = WiFi.softAPgetStationNum();
    doc["has_time"] = (getTimestamp() > 1700000000);
    String out;
    serializeJson(doc, out);
    client->text(out);
}

/**
 * Manejador principal de eventos WebSocket: procesa comandos desde la interfaz web.
 */
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WS client #%u connected\n", client->id());
        sendPresence(lastPresenceState);
        // If battery already identified, send cached data to new client
        if (autoReadIdentified) {
            sendJsonResponse("static_data", cached_data, &cached_features);
        }
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WS client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        DynamicJsonDocument doc(256);
        if (deserializeJson(doc, (char*)data) != DeserializationError::Ok) return;
        
        String command = doc["command"];

        if (command == "presence") {
            sendPresence(bms.isPresent());
        } else if (command == "read_static") {
            // Lectura única de datos maestros de la batería
            BatteryData fresh_data;
            SupportedFeatures fresh_features;
            BMSStatus status = bms.readStaticData(fresh_data, fresh_features);
            if (status == BMSStatus::OK) {
                cached_data = fresh_data;
                cached_features = fresh_features;
                autoReadIdentified = true;
                lastPresenceState = true;
                detectionFailCount = 0;
                dynamicFailCount = 0;
                lastDynamicRead = millis();
                sendJsonResponse("static_data", cached_data, &cached_features);
                sendPresence(true);
                if (!historyRecorded) {
                    appendHistoryRecord(cached_data);
                    historyRecorded = true;
                }
            } else {
                // Reset auto-detection state so loop re-detects
                autoReadIdentified = false;
                lastPresenceState = false;
                detectionFailCount = 0;
                dynamicFailCount = 0;
                historyRecorded = false;
                sendPresence(false);
                sendFeedback("error", statusToString(status));
            }
        } else if (command == "read_dynamic") {
            // Lectura de voltajes y temperaturas actuales
            BMSStatus status = bms.readDynamicData(cached_data);
            if (status == BMSStatus::OK) {
                dynamicFailCount = 0;
                sendJsonResponse("dynamic_data", cached_data, nullptr);
            } else {
                dynamicFailCount++;
                if (dynamicFailCount >= MAX_DYNAMIC_FAILS && autoReadIdentified) {
                    autoReadIdentified = false;
                    lastPresenceState = false;
                    detectionFailCount = 0;
                    dynamicFailCount = 0;
                    historyRecorded = false;
                    sendPresence(false);
                    logToClients("Battery disconnected.", LOG_LEVEL_INFO);
                } else {
                    sendFeedback("error", statusToString(status));
                }
            }
        } else if (command == "led_on") {
            // Enciende los LEDs de la batería (solo modelos STANDARD)
            BMSStatus status = bms.ledTest(true);
            if (status == BMSStatus::OK) sendFeedback("success", "LED ON sent.");
            else sendFeedback("error", statusToString(status));
        } else if (command == "led_off") {
            BMSStatus status = bms.ledTest(false);
            if (status == BMSStatus::OK) sendFeedback("success", "LED OFF sent.");
            else sendFeedback("error", statusToString(status));
        } else if (command == "clear_errors") {
            // Intenta resetear contadores de error del controlador
            BMSStatus status = bms.clearErrors();
            if (status == BMSStatus::OK) sendFeedback("success", "Clear errors sent.");
            else sendFeedback("error", statusToString(status));
        } else if (command == "set_logging") {
            // Activa o desactiva la depuración detallada
            bool enabled = doc["enabled"];
            bms.setLogLevel(enabled ? LOG_LEVEL_DEBUG : LOG_LEVEL_INFO);
            logToClients(String("Log level: ") + (enabled ? "DEBUG" : "INFO"), LOG_LEVEL_INFO);
        } else if (command == "get_config") {
            DynamicJsonDocument configDoc(256);
            configDoc["type"] = "config";
            configDoc["lang"] = current_lang;
            configDoc["theme"] = current_theme;
            String out;
            serializeJson(configDoc, out);
            client->text(out);
        } else if (command == "save_config") {
            current_lang = doc["lang"].as<String>();
            current_theme = doc["theme"].as<String>();
            saveConfig(current_lang, current_theme, current_wifi_ssid, current_wifi_pass);
            logToClients("Settings saved.", LOG_LEVEL_INFO);
        } else if (command == "set_wifi") {
            current_wifi_ssid = doc["ssid"].as<String>();
            current_wifi_pass = doc["pass"].as<String>();
            saveConfig(current_lang, current_theme, current_wifi_ssid, current_wifi_pass);
            logToClients("WiFi configured. Restarting...", LOG_LEVEL_INFO);
            delay(1000);
            ESP.restart();
        } else if (command == "set_time") {
            // Browser sends unix timestamp so ESP32 has a clock without NTP
            unsigned long epoch = doc["epoch"];
            if (epoch > 1700000000) {
                browserEpoch = epoch;
                browserSyncMillis = millis();
                logToClients("Clock synced from browser", LOG_LEVEL_INFO);
            }
        } else if (command == "get_wifi_status") {
            sendWifiStatus(client);
        } else if (command == "list_batteries") {
            sendBatteryList(client);
        } else if (command == "get_history") {
            String rid = doc["rom_id"].as<String>();
            sendBatteryHistory(client, rid);
        } else if (command == "clear_history") {
            String rid = doc["rom_id"].as<String>();
            deleteHistory(rid);
            sendBatteryList(client);  // refresh list for requester
            logToClients("History cleared for " + rid, LOG_LEVEL_INFO);
        } else if (command == "scan_wifi") {
            wifiScanRequested = true;
        } else if (command == "set_auto_detect") {
            autoDetectEnabled = doc["enabled"];
            logToClients(String("Auto-detect: ") + (autoDetectEnabled ? "ON" : "OFF"), LOG_LEVEL_INFO);
            if (!autoDetectEnabled) {
                // If turning off while battery was identified, send disconnect
                if (autoReadIdentified) {
                    autoReadIdentified = false;
                    lastPresenceState = false;
                    detectionFailCount = 0;
                    dynamicFailCount = 0;
                    historyRecorded = false;
                    sendPresence(false);
                }
            }
        }
    }
}

// Clase para forzar la redirección del Portal Cautivo hacia index.html
class CaptiveRequestHandler : public AsyncWebHandler {
public:
    CaptiveRequestHandler() {}
    virtual ~CaptiveRequestHandler() {}
    bool canHandle(AsyncWebServerRequest *request){ return true; }
    void handleRequest(AsyncWebServerRequest *request) {
        request->send(LittleFS, "/index.html", "text/html");
    }
};

/**
 * Persistencia de configuración en Flash.
 */
void saveConfig(const String& lang, const String& theme, const String& wifi_ssid, const String& wifi_pass) {
    File file = LittleFS.open("/config.json", "w");
    if (!file) return;
    DynamicJsonDocument doc(512);
    doc["lang"] = lang;
    doc["theme"] = theme;
    doc["wifi_ssid"] = wifi_ssid;
    doc["wifi_pass"] = wifi_pass;
    serializeJson(doc, file);
    file.close();
}

void loadConfig(String& lang, String& theme, String& wifi_ssid, String& wifi_pass) {
    if (!LittleFS.exists("/config.json")) return;
    File file = LittleFS.open("/config.json", "r");
    if (!file) return;
    DynamicJsonDocument doc(512);
    deserializeJson(doc, file);
    lang = doc["lang"] | "en";
    theme = doc["theme"] | "light";
    wifi_ssid = doc["wifi_ssid"] | "";
    wifi_pass = doc["wifi_pass"] | "";
    file.close();
}

void setup() {
    Serial.begin(115200);
    Serial.println("\nStarting Makita BMS Tool...");
    
    // Inicialización del sistema de archivos LittleFS
    if(!LittleFS.begin(true)){ 
        Serial.println("LittleFS mount failed");
        return; 
    }
    Serial.println("LittleFS mounted OK.");
    
    // Cargar configuración guardada
    loadConfig(current_lang, current_theme, current_wifi_ssid, current_wifi_pass);
    Serial.printf("Config loaded: Lang=%s, Theme=%s\n", current_lang.c_str(), current_theme.c_str());

    bms.setLogCallback(logToClients);

    // Pin diagnostics
    Serial.printf("ONEWIRE_PIN=%d, ENABLE_PIN=%d\n", ONEWIRE_PIN, ENABLE_PIN);
    pinMode(ENABLE_PIN, OUTPUT);
    digitalWrite(ENABLE_PIN, HIGH);
    delay(500);
    Serial.printf("Enable=HIGH -> OneWire reads: %d\n", digitalRead(ONEWIRE_PIN));
    bool resetOk = bms.isPresent();
    Serial.printf("Presence check: %s\n", resetOk ? "DETECTED" : "EMPTY");
    digitalWrite(ENABLE_PIN, LOW);
    delay(100);
    Serial.printf("Enable=LOW  -> OneWire reads: %d\n", digitalRead(ONEWIRE_PIN));
    digitalWrite(ENABLE_PIN, HIGH);
    delay(500);
    Serial.printf("Enable=HIGH -> OneWire reads: %d\n", digitalRead(ONEWIRE_PIN));
    resetOk = bms.isPresent();
    Serial.printf("Presence check: %s\n", resetOk ? "DETECTED" : "EMPTY");

    // Modo WiFi Dual: SoftAP + Station
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(ssid_ap);
    Serial.print("AP started: ");
    Serial.println(WiFi.softAPIP());

    if (current_wifi_ssid.length() > 0) {
        Serial.printf("Connecting to WiFi: %s\n", current_wifi_ssid.c_str());
        WiFi.begin(current_wifi_ssid.c_str(), current_wifi_pass.c_str());
        // No bloqueamos el setup; la conexión se gestionará de fondo
    }

    configTime(0, 0, "pool.ntp.org");

    // Create history directory and log filesystem usage
    if (!LittleFS.exists("/h")) LittleFS.mkdir("/h");
    Serial.printf("LittleFS used: %u / %u bytes\n", LittleFS.usedBytes(), LittleFS.totalBytes());
    
    ws.onEvent(onWebSocketEvent);
    server.addHandler(&ws);

    // Endpoint OTA
    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
        bool updateFailed = Update.hasError();
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", updateFailed ? "FAIL" : "OK");
        response->addHeader("Connection", "close");
        request->send(response);
        if(!updateFailed) ESP.restart();
    }, [&](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
        if (!index) {
            Serial.printf("Update started: %s\n", filename.c_str());
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                Update.printError(Serial);
            }
        }
        if (!Update.hasError()) {
            if (Update.write(data, len) != len) {
                Update.printError(Serial);
            }
        }
        if (final) {
            if (Update.end(true)) {
                Serial.printf("Update complete: %u bytes\n", index + len);
            } else {
                Update.printError(Serial);
            }
        }
    });

    // Servir archivos estáticos
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    dnsServer.start(53, "*", WiFi.softAPIP());

    // Handle captive portal detection so browsers stop nagging once on the page
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *r){ r->send(204); });
    server.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest *r){ r->send(204); });
    server.on("/canonical.html", HTTP_GET, [](AsyncWebServerRequest *r){
        r->send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    });
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *r){
        r->send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    });
    server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *r){ r->send(200, "text/plain", "Microsoft Connect Test"); });

    server.addHandler(new CaptiveRequestHandler());
    
    if (MDNS.begin("makita")) {
        Serial.println("mDNS iniciado: http://makita.local");
    }

    server.begin();
    Serial.println("HTTP/WS server ready.");
}

void loop() {
    dnsServer.processNextRequest();
    ws.cleanupClients();

    // --- Synchronous WiFi scan (requested from Settings) ---
    if (wifiScanRequested) {
        wifiScanRequested = false;
        logToClients("WiFi scan starting...", LOG_LEVEL_INFO);

        // Disconnect STA temporarily if it's trying to connect
        // (ESP32 rejects scans while STA is in connecting state)
        bool wasConnecting = (WiFi.status() != WL_CONNECTED && current_wifi_ssid.length() > 0);
        if (wasConnecting) {
            WiFi.disconnect(false);  // don't erase saved config
            delay(100);
        }

        // Synchronous, passive scan, 120ms/channel (~1.5s total)
        int16_t n = WiFi.scanNetworks(false, false, true, 120);

        DynamicJsonDocument scanDoc(2048);
        scanDoc["type"] = "wifi_list";
        JsonArray arr = scanDoc.createNestedArray("data");
        if (n > 0) {
            for (int i = 0; i < n; i++) {
                JsonObject net = arr.createNestedObject();
                net["ssid"] = WiFi.SSID(i);
                net["rssi"] = WiFi.RSSI(i);
                net["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
            }
        }
        String out;
        serializeJson(scanDoc, out);
        ws.textAll(out);
        WiFi.scanDelete();
        logToClients("WiFi scan: " + String(n > 0 ? n : 0) + " networks found", LOG_LEVEL_INFO);

        // Reconnect STA if it was previously configured
        if (wasConnecting) {
            WiFi.begin(current_wifi_ssid.c_str(), current_wifi_pass.c_str());
        }
    }

    unsigned long now = millis();

    // --- Auto-detect battery ---
    if (autoDetectEnabled && !autoReadIdentified) {
        unsigned long interval = (detectionFailCount >= MAX_DETECTION_ATTEMPTS)
                                 ? BACKOFF_INTERVAL : DETECTION_INTERVAL;

        if (now - lastDetectionAttempt > interval) {
            lastDetectionAttempt = now;

            BatteryData fresh_data;
            SupportedFeatures fresh_features;
            BMSStatus status = bms.readStaticData(fresh_data, fresh_features);

            if (status == BMSStatus::OK) {
                cached_data = fresh_data;
                cached_features = fresh_features;
                autoReadIdentified = true;
                lastPresenceState = true;
                detectionFailCount = 0;
                dynamicFailCount = 0;
                sendPresence(true);
                sendJsonResponse("static_data", cached_data, &cached_features);
                logToClients("Battery detected: " + cached_data.model, LOG_LEVEL_INFO);

                // Immediately read dynamic data too
                status = bms.readDynamicData(cached_data);
                if (status == BMSStatus::OK) {
                    sendJsonResponse("dynamic_data", cached_data, nullptr);

                    // Record history snapshot once per insertion
                    if (!historyRecorded) {
                        appendHistoryRecord(cached_data);
                        historyRecorded = true;
                    }
                }
                lastDynamicRead = millis();
            } else {
                detectionFailCount++;
                if (detectionFailCount == MAX_DETECTION_ATTEMPTS) {
                    logToClients("No battery after " + String(MAX_DETECTION_ATTEMPTS) +
                                 " attempts, backing off to " + String(BACKOFF_INTERVAL / 1000) + "s",
                                 LOG_LEVEL_INFO);
                }
            }
        }
    }

    // --- Auto-poll dynamic data while battery is identified ---
    if (autoDetectEnabled && autoReadIdentified && (now - lastDynamicRead > DYNAMIC_READ_INTERVAL)) {
        lastDynamicRead = now;
        BMSStatus status = bms.readDynamicData(cached_data);

        if (status == BMSStatus::OK) {
            dynamicFailCount = 0;
            sendJsonResponse("dynamic_data", cached_data, nullptr);
        } else {
            dynamicFailCount++;
            Serial.printf("[DBG] Dynamic read fail %d/%d\n", dynamicFailCount, MAX_DYNAMIC_FAILS);

            if (dynamicFailCount >= MAX_DYNAMIC_FAILS) {
                // Battery truly gone
                autoReadIdentified = false;
                lastPresenceState = false;
                detectionFailCount = 0;
                dynamicFailCount = 0;
                historyRecorded = false;
                sendPresence(false);
                logToClients("Battery disconnected.", LOG_LEVEL_INFO);
            }
        }
    }
}
