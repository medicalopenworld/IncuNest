# SensorBoard — Hardware y pinout (referencia)

**Fecha:** 2026-07-03
**Fuente:** esquema de hardware, transcrito por Pablo Sánchez Bergasa.
**MCU:** ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM Octal)

Referencia de configuración para todas las fases del roadmap (`Firmware/docs/superpowers/specs/2026-07-03-sensorboard-roadmap.md`). Los pines de esta tabla son la fuente de verdad; las notas de la sección "Derivado" son interpretación del firmware y deben confirmarse en el diseño de cada fase.

## Sensores y módulos

| Componente | Referencia | Interfaz |
|---|---|---|
| Temperatura/humedad ×2 | SHT40-AD1B-R3 + SHT40-CD1B-R3 | I2C sensores temp (IO41/IO42) |
| Temperatura/humedad ×1 | SHT40-AD1B-R3 | I2C principal (IO4/IO5, compartido con cámara) |
| Luz ambiental | ALS-PT19-315C/L177/TR8 | Analógica (IO1) |
| Micrófono | ICS-41350 | PDM (IO39/IO40) |
| Sensor de puerta (efecto Hall) | DRV5032FBDBZR | GPIO digital (IO47) |
| Cámara | OV2640 u OV5640 (autodetección por SCCB: 0x30 / 0x3C) | DVP + SCCB (I2C principal) |

## Pinout

| Pin | Función | Módulo |
|---|---|---|
| IO1 | ALS (salida analógica del fototransistor) | Luz ambiental |
| IO4 | I2C_SDA | I2C principal (cámara SCCB + SHT40) |
| IO5 | I2C_SCL | I2C principal (cámara SCCB + SHT40) |
| IO6 | DVP_VSYNC | Cámara |
| IO7 | DVP_HREF | Cámara |
| IO8 | DVP_Y4 | Cámara |
| IO9 | DVP_Y3 | Cámara |
| IO10 | DVP_Y5 | Cámara |
| IO11 | DVP_Y2 | Cámara |
| IO12 | DVP_Y6 | Cámara |
| IO13 | DVP_PCLK | Cámara |
| IO15 | XMCLK | Cámara (reloj maestro) |
| IO16 | DVP_Y9 | Cámara |
| IO17 | DVP_Y8 | Cámara |
| IO18 | DVP_Y7 | Cámara |
| IO19 | USB_N / D_N | USB nativo |
| IO20 | USB_P / D_P | USB nativo |
| IO21 | CAM_PWDN | Cámara (power down) |
| IO39 | MIC_DATA | Micrófono |
| IO40 | MIC_SCK | Micrófono |
| IO41 | I2C_TEMP_SENSORS_SDA | I2C sensores temperatura |
| IO42 | I2C_TEMP_SENSORS_SCL | I2C sensores temperatura |
| IO47 | HALL_SENSOR | Sensor de puerta |

**No conectados (NC):** IO2, IO14, IO35, IO36, IO37, IO38, IO45, IO48, TXD0, RXD0.

## Derivado (interpretación de firmware — confirmar en diseño de cada fase)

- **Dos buses I2C**: principal (IO4/IO5: SCCB de la OV2640, addr 7-bit 0x30, + un SHT40-AD1B) y de sensores de temperatura (IO41/IO42: SHT40-AD1B + SHT40-CD1B).
- **Direcciones SHT4x**: la letra del sufijo codifica la dirección I2C — AD1B = 0x44, CD1B = 0x46. Por eso los dos sensores del bus IO41/IO42 no colisionan. El AD1B del bus principal (0x44) tampoco colisiona con la cámara (0x30).
- **ALS-PT19 es un fototransistor analógico**, no un sensor I2C: se lee por ADC en IO1 (ADC1_CH0). Esto resuelve el "I2C o ADC a confirmar" de la Fase 2 del roadmap. La conversión a lux depende de la resistencia de carga del esquema — calibración pendiente de diseño de fase.
- **El ICS-41350 es un micrófono PDM**, no I2S estándar: IO40 = clock PDM, IO39 = data. En ESP32-S3 se lee con el periférico I2S en modo PDM RX. Afecta al diseño de la Fase 3 (el roadmap asumía "MEMS I2S").
- **DRV5032FB**: salida digital push-pull, versión de muy bajo consumo a 5 Hz de muestreo interno — el debounce de la Fase 4 debe considerar esa latencia propia del sensor.
- **OV2640 sin pin RESET dedicado** en el pinout (solo PWDN en IO21): asumir RESET fijado por hardware; confirmar al diseñar la Fase 5.
- **USB nativo** en IO19/IO20: la Fase 1 (USB CDC) usa el periférico USB-OTG del S3, no un puente UART.
