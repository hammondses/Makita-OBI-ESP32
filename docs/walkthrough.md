# Resumen de Puesta en Marcha

¡Objetivo conseguido! El ESP32 ya tiene el firmware y la web funcionando.

## Logros

- **Compilación Corregida**: Se solucionaron los errores de `Serial` y de enlazador (`undefined reference`) en la clase `MakitaBMS`.
- **Carga Exitosa**: Se superó el error `PermissionError(13)` del chip CH340 ajustando velocidades y refrescando los drivers de Windows.
- **Web Operativa**: Archivos LittleFS cargados con nuevas funciones.
- **Modo WiFi Station**: El ESP32 ya puede conectarse a un router externo (Configurable desde la web).
- **Gráficos Pro**: Visualización en tiempo real de la evolución de las celdas.

## Cómo usar las nuevas funciones

### 1. Cambio físico: Ahora el cable que controla el encendido de la batería debe ir conectado al **Pin 5** (Compatible con ESP32 Mini)

### 2. Conectar al WiFi del Taller

- Ve al icono de engranaje (⚙️) en la parte superior derecha.
- Introduce el nombre de tu red y la contraseña en la nueva sección WiFi.
- Pulsa "Conectar al WiFi".
- El dispositivo se reiniciará. Seguirá emitiendo la red `Makita_OBI_ESP32` pero también estará conectado a tu router.

### 3. Ver Gráficos de Historial

- Al activar la **Real-time Updates**, verás un gráfico debajo de las celdas que dibuja la tendencia de cada una. Ideal para diagnosticar baterías que "se rinden" bajo carga.

## Cómo usar la herramienta

1. **Acceso**: Conéctate al WiFi `Makita_OBI_ESP32`.
2. **Navegador**: Si no salta el portal automático, entra en `http://192.168.4.1`.
3. **Lectura**: Conecta una batería Makita al pin ONEWIRE (GPIO 4) y alimenta el BMS. La web empezará a mostrar los voltajes de las celdas, ciclos de carga y estado de bloqueo en tiempo real.

## Notas Técnicas Finales

- El puerto de carga configurado es el **COM3** (automático).
- La velocidad de carga se ha dejado en **115200** para mantener la estabilidad con el chip CH340.
- La configuración de idioma y tema se guarda permanentemente en la memoria Flash del ESP32.
