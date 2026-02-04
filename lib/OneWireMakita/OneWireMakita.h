// lib/OneWireMakita/OneWireMakita.h - CAPA DE ABSTRACCIÓN FÍSICA

#ifndef OneWireMakita_h
#define OneWireMakita_h

#include <Arduino.h>

/**
 * Clase para la implementación del protocolo OneWire (Bus de un solo hilo).
 * Ha sido adaptada específicamente para los tiempos y niveles lógicos de las baterías Makita.
 */
class OneWireMakita
{
  public:
    // --- Constantes de Tiempo (en microsegundos) ---
    // Estas constantes definen la "física" del bus para que el BMS reconozca la señal.
    
    // Tiempos para el ciclo de Reinicio (Reset)
    static constexpr uint16_t TIME_RESET_PULSE = 750; // Pulso bajo para resetear esclavos
    static constexpr uint16_t TIME_RESET_WAIT  = 70;  // Espera antes de leer presencia
    static constexpr uint16_t TIME_RESET_SLOT  = 410; // Tiempo para completar el slot
    
    // Tiempos para la escritura de bits
    static constexpr uint16_t TIME_WRITE1_LOW  = 12;  // Pulso corto para un '1' lógico
    static constexpr uint16_t TIME_WRITE1_HIGH = 120; // Recuperación tras un '1'
    static constexpr uint16_t TIME_WRITE0_LOW  = 100; // Pulso largo para un '0' lógico
    static constexpr uint16_t TIME_WRITE0_HIGH = 30;  // Recuperación tras un '0'
    
    // Tiempos para la lectura de bits
    static constexpr uint16_t TIME_READ_PULSE  = 10;  // Pulso de inicio de lectura
    static constexpr uint16_t TIME_READ_SAMPLE = 10;  // Espera antes de muestrear el bit
    static constexpr uint16_t TIME_READ_SLOT   = 53;  // Tiempo para completar el slot de lectura

    /**
     * Constructor: Inicializa el bus en el pin indicado.
     */
    OneWireMakita(uint8_t pin);
    
    /**
     * Realiza un pulso de reinicio y verifica si hay una batería (Presence Pulse).
     * @return true si se detecta respuesta del BMS.
     */
    bool reset(void);

    /**
     * Envía un byte completo al bus, bit a bit.
     */
    void write(uint8_t v);

    /**
     * Lee un byte completo del bus, bit a bit.
     */
    uint8_t read(void);

  private:
    gpio_num_t _pin; // Pin físico configurado en modo Open-Drain
};

#endif