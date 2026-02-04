# Guía de Montaje - Fabricación del Dispositivo

Sigue estos pasos para construir tu propia herramienta de diagnóstico Makita OBI ESP32 Pro.

## Fase 1: Preparación del Hardware

### 1. Preparar el Conector de la Batería

- Puedes usar una vieja herramienta Makita rota o imprimir una pieza en 3D.
- Necesitas tres contactos: **V+** (Positivo), **V-** (Negativo) y el **Pin de Datos** (el terminal central pequeño en el puerto amarillo).

### 2. Montaje en Placa (Breadboard o Perfboard)

- Coloca el ESP32 en el centro.
- Conecta el **GND** del ESP32 al carril negativo.
- Conecta el **GPIO 4** al pin de datos de la batería.
- **Pull-up**: Pon una resistencia de 4.7kΩ entre el **GPIO 4** y el pin **3.3V** del ESP32. Esto asegura que la señal sea limpia.

### 3. Circuito de Habilitación (Opcional pero recomendado)

- Muchas baterías Makita necesitan "despertar".
- Conecta el **GPIO 5** a la base de un transistor NPN a través de una resistencia de 1kΩ.
- El transistor debe actuar como un interruptor para cerrar el circuito de alimentación del BMS si es necesario.

## Fase 2: Configuración del Software

### 1. Instalación de Drivers

- Asegúrate de tener instalado el driver **CH340** o **CP2102** segun tu modelo de ESP32 para que el PC reconozca el puerto COM.

### 2. Carga del Firmware

- Abre el proyecto en VS Code con **PlatformIO**.
- Conecta el ESP32 al USB.
- Pulsa el botón **Upload** (flecha derecha en la barra inferior).

### 3. Carga de la Interfaz Web

- En el menú de PlatformIO, busca `esp32-wroom-32` -> `Platform`.
- Ejecuta **Upload Filesystem Image**. ¡Este paso es vital para que la web funcione!

## Fase 3: Pruebas Finales

1. Alimenta el ESP32 (vía USB o convertidor DC-DC desde la propia batería).
2. Busca la red WiFi `Makita_OBI_ESP32`.
3. Entra en `http://192.168.4.1`.
4. Conecta una batería y pulsa "Leer Info".

> [!TIP]
> Si vas a alimentar el ESP32 directamente desde la batería Makita (18V-21V), **debes usar un regulador de voltaje (Buck Converter)** para bajarlo a 5V. ¡No conectes los 18V directamente al ESP32 o se quemará!
