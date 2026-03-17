# 🛠️ Arquitectura Técnica y Componentes

Este documento detalla el hardware utilizado y las librerías de software implementadas en el firmware de IncuNest.

## 🏗️ Hardware

### Motherboard (Placa Base)
El controlador principal encargado de la lógica de control, sensórica y comunicaciones.

- **Microcontrolador:** ESP32 (FireBeetle 32 en v14, ESP32-S3 en v15)
- **Frecuencia CPU:** 240MHz
- **Flash:** 16MB (v14) / 8MB (v15)

#### Sensores y Drivers
Según análisis del código y librerías:
- **INA3221:** Monitor de corriente y voltaje (I2C).
- **SHTC3 / SHT4X:** Sensores de temperatura y humedad de alta precisión.
- **STS3X:** Sensor de temperatura de alta precisión.
- **TCA9555 / PCA9557:** Expansores de IO (I2C) para gestionar periféricos adicionales.
- **BQ25792:** Controlador de carga de baterías (Gestión de energía).
- **AFE4490:** Front-End Analógico para pulsioximetría (SpO2).

### Display HMI (Interfaz de Usuario)
Pantalla táctil inteligente para la interacción con el usuario.

- **Plataforma:** ESP32-S3
- **Pantalla:** TFT LCD (Driver ILI9341 o compatible)
- **Táctil:** GT911 (Capacitivo)
- **Framework Gráfico:** LVGL (Light and Versatile Graphics Library)

---

## 📚 Librerías de Firmware

A continuación se listan las principales librerías utilizadas en el proyecto, extraídas de la configuración de PlatformIO.

### Motherboard (`Firmware/motherBoard`)

| Librería | Propósito |
| :--- | :--- |
| **Arduino-PID-Library** | Algoritmo de control PID para regulación térmica precisa. |
| **RotaryEncoder** | Gestión de encoders rotatorios para interfaz física. |
| **Adafruit-GFX-Library** | Gráficos base para pantallas. |
| **TFT_eSPI** | Driver optimizado para pantallas TFT. |
| **TinyGSM** | Gestión de módulos GSM/GPRS para conectividad móvil. |
| **ThingsBoard Arduino SDK** | Telemetría y comunicación IoT con servidor ThingsBoard. |
| **ArduinoJson** | Procesamiento de datos en formato JSON. |
| **ArduinoMqttClient** | Cliente MQTT para mensajería ligera. |
| **Sensirion (SHTC3, STS3x)** | Drivers oficiales para sensores de temperatura/humedad. |
| **Protocentral AFE4490** | Driver para el sensor de SpO2. |

### Display HMI (`Firmware/Display_HMI`)

| Librería | Propósito |
| :--- | :--- |
| **LVGL** | Potente librería de gráficos vectoriales para interfaces modernas. |
| **LovyanGFX** | Driver gráfico de alto rendimiento para ESP32. |
| **TAMC_GT911** | Driver para el panel táctil GT911. |
| **PCA9557-arduino** | Control del expansor de puertos I2C. |
| **ThingsBoard / MQTT** | Librerías compartidas para comunicación con la Motherboard/Nube. |

---

> [!NOTE]
> Las versiones específicas de las librerías se encuentran fijadas en los archivos `platformio.ini` para asegurar la estabilidad del sistema.
