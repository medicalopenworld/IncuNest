# 🛠️ Arquitectura Técnica y Componentes

Este documento detalla el hardware utilizado y las librerías de software implementadas en el firmware de IncuNest.

## 🏗️ Hardware

### Motherboard (Placa Base)

El controlador principal encargado de la lógica de control, sensórica y comunicaciones.

- **Microcontrolador:** ESP32-S3 (v15) / ESP32 (v14)
- **Frecuencia CPU:** 240MHz
- **Flash:** 16MB (v14) / 8MB (v15)

#### Sensores y Drivers
- **INA3221:** Monitor de corriente y voltaje (I2C) para gestión de energía.
- **SHTC3 / SHT4X:** Sensores de temperatura y humedad de alta precisión para el ambiente.
- **STS3X:** Sensor de temperatura de alta precisión (digital).
- **TCA9555 / PCA9557:** Expansores de IO (I2C) para gestionar periféricos adicionales.
- **BQ25792:** Controlador de carga de baterías (v15).
- **AFE4490:** Front-End Analógico para pulsioximetría (SpO2).

### Display HMI (Interfaz de Usuario)

Pantalla táctil inteligente para la interacción con el usuario (CrowPanel 7.0).

- **Plataforma:** ESP32-S3
- **Pantalla:** TFT LCD 7.0" (800x480) con driver RGB y chip SC7277 para inicialización SPI.
- **Táctil:** GT911 (Capacitivo) vía I2C.
- **Framework Gráfico:** LVGL 8.3.11 operando sobre LovyanGFX.

---

## 📚 Librerías de Firmware

### Motherboard (`Firmware/motherBoard`)

| Librería | Propósito |
| :--- | :--- |
| **Arduino-PID-Library** | Control PID para regulación térmica. |
| **RotaryEncoder** | Gestión de encoders físicos. |
| **Adafruit-GFX** / **TFT_eSPI** | Gestión de pantalla integrada (v14). |
| **ThingsBoard Arduino SDK** | Telemetría IoT (v0.13.0). |
| **Sensirion (SHTC3, STS3x)** | Drivers para sensores SHT/STS. |
| **Protocentral AFE4490** | Driver para el sensor de SpO2. |
| **BQ25792_Driver** | Gestión del chip de carga de batería. |
| **ArduinoJson** | Procesamiento de mensajes (v6.21.5). |

### Display HMI (`Firmware/Display_HMI`)

| Librería | Propósito |
| :--- | :--- |
| **LVGL** | Framework de UI (v8.3.11). |
| **GFX Library for Arduino** | Soporte base para paneles RGB. |
| **LovyanGFX** | Driver gráfico de alto rendimiento (v1.1.12). |
| **TAMC_GT911** | Driver para el panel táctil GT911. |
| **PCA9557-arduino** | Control del expansor de puertos I2C. |
| **ThingsBoard SDK / MQTT** | Sincronización de datos con Motherboard y Nube. |
