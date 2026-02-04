# Makita OBI ESP32 Pro 🔋

Herramienta avanzada de diagnóstico para baterías Makita LXT (18V) basada en ESP32.

## ✨ Características de la Versión 2.0 Pro

- **Triple Verificación de Presencia**: Eliminación de falsos positivos (baterías fantasma).
- **WiFi Dual**: Acceso simultáneo vía AP (Directo) o Station (Red de tu taller).
- **Gráficos en Tiempo Real**: Historial de voltajes celda por celda para diagnóstico de fatiga.
- **Asistente de Balanceo**: Indicaciones precisas para equilibrar packs descompensados.
- **Interfaz Web Premium**: Con modo oscuro, bilingüe (ES/EN) y visualización HUD.
- **Compatibilidad**: Diseñado para funcionar en cualquier ESP32 (incluido Mini/SuperMini).

## 📂 Estructura del Proyecto

- `/src`: Código fuente del firmware (C++).
- `/data`: Interfaz web (HTML/JS/CSS).
- `/lib`: Librerías personalizadas para el protocolo OneWire de Makita.
- `/docs`: Documentación técnica, manuales y esquemas eléctricos.

## 🛠️ Requisitos de Hardware

- **ESP32** (Cualquier variante).
- Transistor NPN (BC547 o similar) + Resistencia 1kΩ (para el pin ENABLE).
- Resistencia Pull-up 4.7kΩ (para el pin DATA).
- [Ver Esquema Eléctrico](./docs/esquema_electrico.md)

## 🚀 Instalación rápida

1. Abre el proyecto en **VS Code** con **PlatformIO**.
2. Conecta tu ESP32.
3. Ejecuta **Upload** (Firmware).
4. Ejecuta **Upload Filesystem Image** (Interfaz Web).

---
*Desarrollado con ❤️ para la comunidad de herramientas eléctricas.*
