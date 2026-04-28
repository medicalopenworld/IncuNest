# Requisitos para la Reimplementación en ESP-IDF Nativo

> Extraídos del análisis exhaustivo del código actual. Cada RF incluye referencia al archivo/función fuente.

## 1. Requisitos Funcionales (RF)

### Comunicación con Motherboard

**RF-001** — Comunicar con la Motherboard via UART a 115200 bps, 8N1, sobre USB-CDC (Serial).
- **Fuente**: `src/CommTask.cpp:466`, `include/CommTask.h:16` (`COMM_SERIAL = Serial`)
- **Prioridad**: Alta

**RF-002** — Parsear y procesar mensajes `CTRL,STATE` con hasta 18 campos usando compatibilidad retroactiva (aceptar 12-18 campos).
- **Fuente**: `src/CommTask.cpp:133-171`
- **Prioridad**: Alta

**RF-003** — Parsear y procesar mensajes `CTRL,TEL` (telemetría en tiempo real de temperatura y humedad).
- **Fuente**: `src/CommTask.cpp:109-126`
- **Prioridad**: Alta

**RF-004** — Parsear y procesar mensajes `CTRL,ALM` con mapeo directo ID→índice en `alarmList[]`.
- **Fuente**: `src/CommTask.cpp:424-462`, `include/CommTask.h:104-110`
- **Prioridad**: Alta

**RF-005** — Enviar mensajes `HMI,...` al Motherboard cuando el usuario cambia parámetros, incluyendo datos de fototerapia y sonda de piel.
- **Fuente**: `src/CommTask.cpp:77-90`
- **Prioridad**: Alta

**RF-006** — Implementar handshake `HMI,UI_READY` enviado una sola vez al completar la inicialización de la UI.
- **Fuente**: `src/CommTask.cpp:54-57`, `Firmware/PROTOCOL.md`
- **Prioridad**: Alta

**RF-007** — Implementar auto-corrección de alarmas fantasma via bitmask (`alarmBitmask`): limpiar visualmente alarmas no presentes en el bitmask recibido en `CTRL,STATE`.
- **Fuente**: `src/CommTask.cpp:362-383`
- **Prioridad**: Alta

**RF-008** — Implementar retry de sincronización de estado (`HMI,REQ,STATE` cada 500ms) hasta recibir confirmación.
- **Fuente**: `src/CommTask.cpp:390-405`
- **Prioridad**: Alta

**RF-009** — Transmitir información de arranque (`HMI,BOOT,rst,count`) incluyendo causa de reset y número de boot.
- **Fuente**: `src/CommTask.cpp:60-66`, `src/main.cpp:88-98`
- **Prioridad**: Media

**RF-010** — Parsear mensajes `CTRL,PPG` (25 Hz) y `CTRL,VIT` (1 Hz) para la pantalla de pulse oximetría.
- **Fuente**: `src/CommTask.cpp:178-189`
- **Prioridad**: Media

### Interfaz Gráfica LVGL

**RF-011** — Inicializar el panel RGB de 800×480 px con bounce buffers de 20 líneas en SRAM interna y framebuffer en PSRAM.
- **Fuente**: `src/UITask.cpp:2811-2880`
- **Prioridad**: Alta

**RF-012** — Inicializar el touch GT911 via I2C (SDA:15, SCL:16) con 3 reintentos y configuración de rotación.
- **Fuente**: `src/UITask.cpp:2885-2905`
- **Prioridad**: Alta

**RF-013** — Implementar las 7 pantallas de la UI: Intro (logo), Lock (seguridad), Main (dashboard), Alarms (alarmas), Charts (historial), PulseOxi (SpO2), Settings (configuración).
- **Fuente**: Carpeta `SquareLineProject/Exports/`, `src/UITask.cpp`
- **Prioridad**: Alta

**RF-014** — Mostrar continuamente en pantalla Main: temperatura aire medida, temperatura aire objetivo, temperatura piel medida, temperatura piel objetivo, humedad medida, humedad objetivo, modo de control activo (AIR/BABY).
- **Fuente**: `src/UITask.cpp:263-391`, constantes en `include/main.h`
- **Prioridad**: Alta

**RF-015** — Actualizar labels de temperatura solo cuando el cambio supera 0.05ºC (filtro anti-jitter para DMA LVGL).
- **Fuente**: `src/UITask.cpp:275`, `include/main.h:137` (`TEMP_LABEL_UPDATE_THRESHOLD = 0.05`)
- **Prioridad**: Alta

**RF-016** — Implementar pantalla de alarmas con lista de hasta 4 alarmas activas simultáneas, cada una con ID, texto corto, descripción larga y estado.
- **Fuente**: `include/main.h:296-307`, `src/UITask.cpp`
- **Prioridad**: Alta

**RF-017** — Implementar botón de silencio de alarma (MUTE) que silencia el audio sin eliminar las indicaciones visuales.
- **Fuente**: `src/CommTask.cpp:453-455`, `Firmware/docs/known_issues.md §Silencio`
- **Prioridad**: Alta

**RF-018** — Implementar bloqueo de pantalla (Screen Lock) con desbloqueo por pulsación larga de 1.5 segundos y arco de progreso animado.
- **Fuente**: `include/main.h:253-258` (`LOCK_PROGRESS_DURATION_MS = 1500`)
- **Prioridad**: Alta

**RF-019** — Implementar gráficas históricas de temperatura aire, temperatura piel y humedad con 4 rangos de tiempo: 5min, 30min, 1h, 2h.
- **Fuente**: `src/UITask.cpp:57-63` (`HISTORY_BUFFER_SIZE = 720`), `include/main.h:198-201`
- **Prioridad**: Media

**RF-020** — Implementar configuración de setpoints con flechas arriba/abajo y confirmación explícita antes de enviar al Motherboard.
- **Fuente**: `src/UITask.cpp`, evento `Button_SAVE` en `src/CommTask.cpp:486`
- **Prioridad**: Alta

**RF-021** — Implementar soporte de idiomas: Español, Inglés, Francés. Todas las cadenas de la UI deben estar localizadas.
- **Fuente**: `src/UITask.cpp:466-599`, `include/main.h:39` (`typedef enum { LANG_ES, LANG_EN, LANG_FR }`)
- **Prioridad**: Media

**RF-022** — Implementar Modo Auto Air: calcular zona de confort térmica óptima según peso del bebé, edad gestacional y edad postnatal.
- **Fuente**: `src/UITask.cpp:119-128`, `src/CommTask.cpp:83`
- **Prioridad**: Media

**RF-023** — Gestionar el temporizador de fototerapia: mostrar cuenta regresiva, sincronizar con Motherboard como fuente de verdad.
- **Fuente**: `src/CommTask.cpp:311-331`, `Firmware/docs/known_issues.md §4`
- **Prioridad**: Media

**RF-024** — Implementar Modo Oscuro (Dark Mode) con colores de fondo/texto invertidos.
- **Fuente**: `src/UITask.cpp`, `include/main.h:348-350`
- **Prioridad**: Baja

### Almacenamiento Persistente

**RF-025** — Persistir en NVS: idioma, temperatura deseada aire, temperatura deseada piel, humedad deseada, número de serie, modo oscuro, pixel clock, modo humedad habilitado, Auto Air (peso, gest, edad).
- **Fuente**: `include/EEPROM_defines.h`
- **Prioridad**: Alta

**RF-026** — Restaurar el estado anterior tras un reinicio no planificado (`g_hmiRestoreState`), omitiendo el delay de estabilidad de arranque.
- **Fuente**: `src/main.cpp:95-119`
- **Prioridad**: Alta

### OTA y Conectividad

**RF-027** — Implementar OTA via WiFi: crear AP "IncuNest_Display", servidor HTTP con formulario de upload de firmware.
- **Fuente**: `src/Wifi_OTA.cpp`, `include/main.h:22` (`WIFI_NAME = "IncuNest_Display"`)
- **Prioridad**: Media

**RF-028** — Transmitir credenciales WiFi ingresadas por el usuario al Motherboard via `HMI,WIFI,ssid,pass`.
- **Fuente**: `src/CommTask.cpp:69-72`
- **Prioridad**: Baja

### Control de Backlight

**RF-029** — Controlar el backlight via I2C al STC8H1K28 en dirección 0x30 (no via GPIO directo).
- **Fuente**: `include/display_config.h:58-61`, `src/UITask.cpp`
- **Prioridad**: Alta

---

## 2. Requisitos No Funcionales (RNF)

**RNF-001 — Rendimiento**: El display debe mantener un frame rate visible de al menos 20 fps en condiciones normales de operación (pantalla Main con labels actualizándose a 1 Hz).

**RNF-002 — Latencia de toque**: La respuesta visual a un evento táctil debe producirse en menos de **200 ms** desde el toque hasta la actualización visible.

**RNF-003 — Tiempo de arranque**: Desde power-on hasta pantalla funcional (ScreenLock o ScreenMain): máximo **5 segundos** en cold boot, máximo **2 segundos** en warm reset (reinicio).

**RNF-004 — Estabilidad**: El sistema debe operar **sin reinicios no planificados** durante un período de prueba de 24 horas con actualización de telemetría a 1 Hz y una alarma activa.

**RNF-005 — Watchdog**: No deben producirse disparos del TWDT (Task Watchdog Timer). El assert handler de LVGL debe llamar a `esp_restart()` con log previo, no a `while(1)`.

**RNF-006 — Separación de responsabilidades**: Las actualizaciones de UI desde CommTask deben realizarse mediante colas de mensajes (IPC queue) o a través de `lv_async_call()`, sin acceso directo a objetos LVGL desde CommTask.

**RNF-007 — Memoria**: El uso de memoria dinámica de LVGL debe utilizar PSRAM (`MALLOC_CAP_SPIRAM`) para objetos grandes, reservando la SRAM interna para bounce buffers y variables críticas de tiempo real.

**RNF-008 — Compatibilidad de protocolo**: El nuevo firmware debe ser **100% compatible con el protocolo ASCII existente** (RF-001 a RF-010) para no requerir cambios simultáneos en el firmware de la Motherboard.

---

## 3. Requisitos de Hardware/Configuración

**RHW-001 — ESP-IDF version**: Mínimo IDF 5.1.0 (para `esp_lcd_panel_rgb` con bounce buffers estables).

**RHW-002 — sdkconfig.defaults clave**:
```
CONFIG_ESP32S3_DEFAULT_CPU_FREQ_MHZ=240
CONFIG_SPIRAM_SUPPORT=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y
CONFIG_SPIRAM_RODATA=y
CONFIG_FREERTOS_HZ=1000
CONFIG_FREERTOS_UNICORE=n
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=4096
CONFIG_LCD_RGB_RESTART_IN_VSYNC=y
CONFIG_PM_ENABLE=n
```

**RHW-003 — Flash**: 16 MB QIO a 80 MHz.

**RHW-004 — Particiones**: Mantener el esquema `IncuNest_display_v1_audio.csv` con OTA dual (3 MB + 3 MB) y SPIFFS de 8 MB.

---

## 4. Requisitos de Interfaz con Motherboard

**RIF-001** — Compatible backward con protocolo ASCII v1.5.0 (definido en `Firmware/PROTOCOL.md`).

**RIF-002** — Enviar `HMI,BOOT,rst,count` al inicio para diagnóstico remoto.

**RIF-003** — Enviar `HMI,UI_READY` cuando la UI esté completamente inicializada (primera pantalla visible).

**RIF-004** — El nuevo firmware **no debe requerir cambios en el firmware de la Motherboard** para funcionar correctamente.

**RIF-005** — Si se migra al protocolo TLV binario en el futuro, debe ser de forma coordinada y documentada con el equipo de Motherboard.
