# Guía de Carga al ESP32

Este proyecto utiliza **PlatformIO**. Para que funcione correctamente, debes realizar dos cargas distintas: el **firmware** (código C++) y el **sistema de archivos** (archivos HTML/CSS/JS en la carpeta `/data`).

## 1. Cargar el Firmware (Código)

Esto compila y sube el archivo `main.cpp` y las bibliotecas.

- **En VS Code (Interfaz):**
    1. Haz clic en el icono de **PlatformIO** (la hormiga) en la barra lateral izquierda.
    2. En el menú `Project Tasks`, busca `esp32-wroom-32`.
    3. Selecciona **General** -> **Upload**.
- **Por Línea de Comandos (CLI):**

    ```powershell
    pio run --target upload
    ```

## 2. Cargar el Sistema de Archivos (LittleFS)

**¡IMPORTANTE!** Si no haces este paso, la web no cargará porque el ESP32 no encontrará el `index.html`.

- **En VS Code (Interfaz):**
    1. Ve al icono de **PlatformIO** -> `Project Tasks` -> `esp32-wroom-32`.
    2. Busca la sección **Platform**.
    3. Haz clic en **Upload Filesystem Image**.
- **Por Línea de Comandos (CLI):**

    ```powershell
    pio run --target uploadfs
    ```

## 3. Monitorización

Para ver los mensajes de depuración (como la IP que asigna el ESP32):

- Haz clic en el icono del "enchufe" en la barra inferior de VS Code o:
- **PlatformIO** -> `Project Tasks` -> **Monitor**.

---

### Notas Adicionales

- Asegúrate de tener el ESP32 conectado por USB.
- Si falla la carga, intenta mantener presionado el botón **BOOT** de la placa justo cuando aparezca "Connecting..." en la terminal.
- El puerto y la velocidad están configurados en el archivo [platformio.ini](file:///d:/GITHUB/Makita-OBI-ESP32/platformio.ini).
