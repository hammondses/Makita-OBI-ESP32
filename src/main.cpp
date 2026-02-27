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
            } else {
                // Reset auto-detection state so loop re-detects
                autoReadIdentified = false;
                lastPresenceState = false;
                detectionFailCount = 0;
                dynamicFailCount = 0;
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

    unsigned long now = millis();

    // --- Auto-detect battery ---
    if (!autoReadIdentified) {
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
    if (autoReadIdentified && (now - lastDynamicRead > DYNAMIC_READ_INTERVAL)) {
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
                sendPresence(false);
                logToClients("Battery disconnected.", LOG_LEVEL_INFO);
            }
        }
    }
}
