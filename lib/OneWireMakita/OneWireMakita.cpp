// lib/OneWireMakita/OneWireMakita.cpp - IMPLEMENTACIÓN DE BAJO NIVEL

#include "OneWireMakita.h"

// Mutex para proteger los tiempos críticos en sistemas con múltiples tareas (FreeRTOS)
// Impide que una interrupción de red afecte a los micro-tiempos del bus.
static portMUX_TYPE oneWireMux = portMUX_INITIALIZER_UNLOCKED;

/**
 * Constructor de la clase OneWireMakita.
 * El pin se configura en modo OUTPUT_OPEN_DRAIN para permitir la comunicación bidireccional
 * sin riesgo de cortocircuito (la línea sube mediante una resistencia de pull-up).
 */
OneWireMakita::OneWireMakita(uint8_t pin) : _pin((gpio_num_t)pin) {
    pinMode(_pin, INPUT_PULLUP);
    gpio_pullup_en(_pin);             // Asegura pull-up a nivel de hardware ESP32
    pinMode(_pin, OUTPUT_OPEN_DRAIN); 
    digitalWrite(_pin, HIGH); 
}

/**
 * Implementación del reinicio (reset) del bus.
 */
bool OneWireMakita::reset(void) {
    // 1. Verificación de bus en reposo (debe estar ALTO por el pull-up)
    pinMode(_pin, INPUT_PULLUP);
    if (digitalRead(_pin) == LOW) {
        // El bus está en corto a tierra o el pin está flotando sin pull-up efectivo
        return false; 
    }
    
    pinMode(_pin, OUTPUT_OPEN_DRAIN);
    
    portENTER_CRITICAL(&oneWireMux);
    digitalWrite(_pin, LOW); 
    portEXIT_CRITICAL(&oneWireMux);

    delayMicroseconds(TIME_RESET_PULSE); 

    portENTER_CRITICAL(&oneWireMux);
    digitalWrite(_pin, HIGH);           // Soltamos el bus
    delayMicroseconds(TIME_RESET_WAIT);  
    bool presence = !digitalRead(_pin);  // Leemos el pulso de presencia
    portEXIT_CRITICAL(&oneWireMux);

    delayMicroseconds(TIME_RESET_SLOT); 
    
    // 2. Verificación de recuperación: El bus DEBE volver a ALTO.
    // Si sigue en BAJO después del slot, es un falso positivo por pin flotante.
    if (digitalRead(_pin) == LOW) {
        presence = false; 
    }

    return presence;
}

/**
 * Envía un byte completo manejando los tiempos críticos de cada bit.
 */
void OneWireMakita::write(uint8_t v) {
    for (uint8_t bitMask = 0x01; bitMask; bitMask <<= 1) {
        if (bitMask & v) { // Escritura de un '1' lógico
            portENTER_CRITICAL(&oneWireMux);
            digitalWrite(_pin, LOW); 
            delayMicroseconds(TIME_WRITE1_LOW); // Pulso corto
            digitalWrite(_pin, HIGH);
            portEXIT_CRITICAL(&oneWireMux);
            delayMicroseconds(TIME_WRITE1_HIGH);
        } else { // Escritura de un '0' lógico
            portENTER_CRITICAL(&oneWireMux);
            digitalWrite(_pin, LOW); 
            delayMicroseconds(TIME_WRITE0_LOW); // Pulso largo
            digitalWrite(_pin, HIGH);
            portEXIT_CRITICAL(&oneWireMux);
            delayMicroseconds(TIME_WRITE0_HIGH);
        }
    }
}

/**
 * Lee un byte completo del bus mediante muestreo rápido tras el pulso de inicio.
 */
uint8_t OneWireMakita::read() {
    uint8_t result = 0;
    for (uint8_t bitMask = 0x01; bitMask; bitMask <<= 1) {
        portENTER_CRITICAL(&oneWireMux);
        digitalWrite(_pin, LOW); 
        delayMicroseconds(TIME_READ_PULSE); // Generamos el pulso de inicio de lectura
        digitalWrite(_pin, HIGH); 
        delayMicroseconds(TIME_READ_SAMPLE); // Esperamos a que el BMS fije el dato
        if (digitalRead(_pin)) {
            result |= bitMask; // El bus está en alto -> bit es '1'
        }
        portEXIT_CRITICAL(&oneWireMux);
        delayMicroseconds(TIME_READ_SLOT); // Completamos el slot de tiempo del bit
    }
    return result;
}
