# Glosario y Referencias

## 1. Glosario de Términos del Proyecto

| Término | Definición |
|---|---|
| **AIR CONTROLLED** | Modo de control donde el PID regula la temperatura del aire del compartimento (modo por defecto) |
| **BABY CONTROLLED** | Modo de control donde el PID regula la temperatura de la piel del bebé (requiere sonda NTC) |
| **alarmBitmask** | Máscara de bits 32-bit donde el bit N indica si la alarma con ID N está activa. Enviada en `CTRL,STATE` para sincronización robusta |
| **alarmList[]** | Array de estructuras `Alarm` en el HMI, indexado directamente por ID de alarma (alarmList[id]) |
| **Auto Air** | Función que calcula automáticamente el rango de temperatura óptima según peso gestacional, edad gestacional y edad postnatal del neonato |
| **Bounce Buffer** | Buffer intermedio en SRAM interna que desacopla el DMA del bus RGB del acceso a la PSRAM, eliminando parpadeos |
| **CH340C** | Chip USB-Serial bridge que convierte el UART de la Motherboard en un Virtual COM Port accesible via USB |
| **CommTask** | Tarea FreeRTOS del HMI responsable de la comunicación serie con la Motherboard |
| **CONTROL_AIR** | Constante `true` que indica modo AIR CONTROLLED |
| **CONTROL_SKIN** | Constante `false` que indica modo BABY CONTROLLED (piel) |
| **CrowPanel Advance 7"** | Módulo de display táctil de Elecrow con ESP32-S3, panel RGB 800×480, touch GT911 |
| **EEPROM** | Memoria no volátil emulada en flash (263 bytes). En la nueva implementación se reemplaza por NVS |
| **esp_lcd** | Componente ESP-IDF nativo para control de paneles LCD (RGB, SPI, I80) |
| **FB** | Framebuffer — buffer de imagen completo de 800×480×2 bytes (768 KB) alojado en PSRAM |
| **FW** | Firmware |
| **g_hmiRestoreState** | Flag `true` cuando el HMI se reinició inesperadamente (no cold boot), permite saltarse delays de startup |
| **g_lvgl_mutex** | Mutex recursivo FreeRTOS que protege el acceso concurrente a objetos LVGL |
| **g_stateSynced** | Flag que indica si el HMI ha recibido y aplicado exitosamente el primer `CTRL,STATE` |
| **GT911** | Controlador táctil capacitivo Goodix GT911 (I2C), usado en el CrowPanel Advance 7" |
| **HMI** | Human-Machine Interface — módulo Display |
| **hmi_msg** | Estructura `HMI_Message` que contiene los parámetros actuales del HMI listos para enviar a la Motherboard |
| **HSYNC** | Señal de sincronización horizontal del bus RGB |
| **IDF** | ESP-IDF (Espressif IoT Development Framework) |
| **IPC Queue** | Inter-Process Communication Queue — cola FreeRTOS para comunicación entre tareas sin mutex compartido |
| **LVGL** | Light and Versatile Graphics Library — motor gráfico embebido |
| **MB** | Motherboard |
| **NTC** | Negative Temperature Coefficient — tipo de termistor usado como sonda de temperatura de piel |
| **NVS** | Non-Volatile Storage — partición de almacenamiento clave-valor del ESP-IDF (reemplaza EEPROM) |
| **OPI** | Octal Peripheral Interface — modo de acceso a PSRAM con 8 líneas de datos (más rápido que SPI) |
| **OTA** | Over-The-Air — actualización de firmware inalámbrica |
| **PCA9557** | Expansor de I/O de 8 bits via I2C (dirección 0x18). Declarado como dependencia pero no encontrado en HW v1.3/1.4 |
| **PCLK** | Pixel Clock — señal de reloj del bus RGB que define la velocidad de transferencia |
| **PID** | Proportional-Integral-Derivative — algoritmo de control en lazo cerrado (temperatura y humedad) |
| **PSRAM** | Pseudo Static RAM — 8 MB de RAM externa OPI en el módulo ESP32-S3 |
| **RGB565** | Formato de color de 16 bits: 5 bits rojo, 6 bits verde, 5 bits azul |
| **ScreenLock** | Pantalla de bloqueo de seguridad para evitar cambios accidentales (desbloqueo por pulsación larga) |
| **skinPanelEnabled** | Flag que indica si el panel de temperatura de piel está visible en el dashboard |
| **SOUP** | Software of Unknown Provenance (IEC 62304) — librerías de terceros integradas en el software médico |
| **STC8H1K28** | MCU auxiliar de la placa CrowPanel que gestiona backlight y buzzer via I2C @0x30 |
| **TAMC_GT911_Fixed** | Fork local de la librería Arduino para el GT911, con correcciones de bugs |
| **TEMP_LABEL_UPDATE_THRESHOLD** | Umbral de 0.05ºC por debajo del cual no se actualiza el label de temperatura (anti-jitter) |
| **TLV** | Type-Length-Value — formato de encapsulamiento de datos del protocolo binario alternativo |
| **UITask** | Tarea FreeRTOS del HMI responsable del render LVGL, init del display y procesamiento del touch |
| **UART** | Universal Asynchronous Receiver Transmitter — interfaz serie física |
| **VCP** | Virtual COM Port — puerto COM virtual presentado por el CH340C sobre USB |
| **VSYNC** | Señal de sincronización vertical del bus RGB |
| **Warm Reset** | Reinicio del sistema sin corte de alimentación (por watchdog, panic, etc.) |

---

## 2. Referencias al Código Fuente Original

| Componente | Archivo(s) | Descripción |
|---|---|---|
| Configuración del display | `Firmware/Display_HMI/include/display_config.h` | Pines RGB, touch, timing, pixel clock |
| Variables globales y constantes | `Firmware/Display_HMI/include/main.h` | Todas las constantes de la aplicación |
| Definiciones EEPROM | `Firmware/Display_HMI/include/EEPROM_defines.h` | Mapa de memoria NVS/EEPROM |
| Configuración LVGL | `Firmware/Display_HMI/include/lv_conf.h` | lv_conf.h de LVGL 8.3.11 |
| Protocolo de comunicación | `Firmware/Display_HMI/include/display_comms.h` | Definiciones del protocolo TLV (no activo) |
| Protocolo de comunicación (texto) | `Firmware/Display_HMI/include/CommTask.h` | Estructuras de datos y protocolo ASCII |
| Tarea de comunicación | `Firmware/Display_HMI/src/CommTask.cpp` | Parse de mensajes, sync de estado y alarmas |
| Tarea de UI y display | `Firmware/Display_HMI/src/UITask.cpp` | Init display/touch/LVGL, callbacks, screens |
| Punto de entrada | `Firmware/Display_HMI/src/main.cpp` | setup(), creación de tareas |
| Driver touch local | `Firmware/Display_HMI/lib/TAMC_GT911_Fixed/` | Librería Arduino GT911 fork |
| Configuración PlatformIO | `Firmware/Display_HMI/platformio.ini` | Dependencias, board, build flags |
| Tabla de particiones | `Firmware/Display_HMI/IncuNest_display_v1_audio.csv` | Particiones flash 16 MB |
| Protocolo documentado | `Firmware/PROTOCOL.md` | Especificación protocolo ASCII v1.5.0 |
| Arquitectura del sistema | `Firmware/docs/architecture.md` | Descripción de tareas y flujo |
| Alarmas del sistema | `Firmware/docs/alarms.md` | IDs, umbrales, ciclo de vida |
| Problemas conocidos | `Firmware/docs/known_issues.md` | Bugs y mitigaciones implementadas |
| Interfaz HMI | `Firmware/docs/hmi.md` | Diseño visual y navegación |
| Protocolo de comunicación | `Firmware/docs/communication.md` | Especificación detallada del protocolo |
| Diseños UI (SquareLine) | `Firmware/Display_HMI/SquareLineProject/` | Proyectos SquareLine Studio + exports C |

---

## 3. Referencias Externas

### 3.1 Hardware

- CrowPanel Advance 7" Product Page: `https://www.elecrow.com/crowpanel-advance-7-0-hmi-esp32-s3-ai-powered-ips-touch-screen-800x480.html`
- GT911 Datasheet: Goodix Technology GT911 Touchscreen Datasheet
- STC8H1K28: STC Micro STC8H1K28 Datasheet

### 3.2 ESP-IDF

- ESP-IDF Programming Guide: `https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/`
- esp_lcd_panel_rgb API: `https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/lcd/rgb_lcd.html`
- esp_lcd_touch_gt911 Component: `https://components.espressif.com/components/espressif/esp_lcd_touch_gt911`
- LVGL IDF Component: `https://components.espressif.com/components/lvgl/lvgl`
- NVS Flash API: `https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/storage/nvs_flash.html`
- IDF Component Manager: `https://docs.espressif.com/projects/idf-component-manager/en/latest/`

### 3.3 LVGL

- LVGL Documentation: `https://docs.lvgl.io/8.4/`
- LVGL Forum ESP32: `https://forum.lvgl.io/c/boards-and-shields/esp32`
- SquareLine Studio: `https://squareline.io/`

### 3.4 Normativas (Dispositivo Médico Clase IIb)

- IEC 62304:2006/A1:2015 — Software lifecycle for medical devices
- IEC 60601-1-8:2007/A1:2013 — Alarm systems for medical electrical equipment  
- IEC 60601-2-19:2021 — Particular requirements for infant incubators
- IEC 62366-1:2015/A1:2020 — Usability engineering for medical devices
