# 📚 Inventario de Documentación de IncuNest

Este documento sirve como índice centralizado para navegar por la documentación y los recursos del proyecto IncuNest.

## 💻 Firmware

El código fuente para los microcontroladores y la interfaz de usuario.

### [Display HMI](../Firmware/Display_HMI)
Interfaz Hombre-Máquina (Human-Machine Interface). Gestiona la pantalla táctil y la interacción con el usuario.
- **Ruta:** `Firmware/Display_HMI`
- **Componentes clave:** Interfaz gráfica (LVGL/SquareLine), lógica de visualización, comunicación USB Serial.

### [Motherboard (Placa Base)](../Firmware/motherBoard)
El "cerebro" de la incubadora. Gestiona la sensórica, el control de temperatura/humedad y la comunicación.
- **Ruta:** `Firmware/motherBoard`
- **Plataforma:** ESP32-S3 (v15) / ESP32 (v14)
- **Funciones:**
    - Lectura de sensores (SHTC3, STS3X, SpO2).
    - Control de actuadores (PWN para calefacción, ventiladores).
    - Gateway USB Host para comunicación con el HMI.
    - Conectividad Telemetry (ThingsBoard).

---

## 🛠️ Hardware

### Electrónica

#### [Motherboard (Lógica)](../Hardware/Electronics/Motherboard)
Diseño de la PCB principal que aloja el microcontrolador y los circuitos de potencia.

#### [Sensores y Actuadores](../Hardware/Electronics/Ambient_sensor)
- **Sensores Ambientales:** `Hardware/Electronics/Ambient_sensor`
- **Iluminación LED:** `Hardware/Electronics/FullLEDStrip`

### Mecánica

#### [Estructura y Chasis](../Hardware/Mechanical)
Archivos CAD y modelos 3D para la construcción física. Incluye las piezas para impresión 3D y corte láser.

---

## 📖 Guías Adicionales

- **[Arquitectura Técnica](TECNOLOGIA.md)**: Detalle de componentes y librerías.
- **[Análisis de Comunicación](ANALISIS_COMUNICACION.md)**: Explicación del protocolo entre placas.
- **[Sistema de Alarmas](SISTEMA_ALARMAS.md)**: Catálogo y lógica de seguridad del sistema.
