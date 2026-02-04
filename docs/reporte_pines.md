# Reporte de Pines (Pinout) - Makita OBI ESP32 Pro

Este reporte detalla la función de cada pin utilizado en el ESP32 para asegurar el correcto funcionamiento del sistema y evitar conflictos de hardware.

| Pin (GPIO) | Función | Descripción |
| :--- | :--- | :--- |
| **GPIO 4** | **DATA (OneWire)** | Comunicación bidireccional con el BMS de la batería. |
| **GPIO 5** | **ENABLE (BMS Power)** | Controla el transistor que activa/desactiva la alimentación del BMS. |
| **GPIO 1 (TX0)** | **DEBUG (Serial)** | Salida de mensajes de depuración al PC (Monitor Serie). |
| **GPIO 3 (RX0)** | **DEBUG (Serial)** | Entrada de datos del PC (no utilizado actualmente). |
| **GPIO 5** | **Libre** | Recomendado para pantalla OLED (SCL) si se añade. |
| **GPIO 18** | **Libre** | Recomendado para pantalla OLED (SDA) si se añade. |

## Notas de Hardware

- **Conflicto Resuelto**: Se han seleccionado pines inferiores al GPIO 12 para total compatibilidad con placas ESP32 Mini / SuperMini.
- **Conexión**: El cable del transistor debe ir al **Pin 5**.
- **Resistencia Pull-up**: El sistema ahora activa una resistencia interna en el **GPIO 4**, pero para máxima estabilidad se recomienda una resistencia física de 4.7kΩ entre el GPIO 4 y 3.3V.
