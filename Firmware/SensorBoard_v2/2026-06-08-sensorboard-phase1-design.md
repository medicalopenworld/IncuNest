# SensorBoard — Fase 1: Comunicación USB CDC

**Fecha:** 2026-06-08
**Estado:** Aprobado
**Hardware:** ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM Octal)

---

## Contexto

El SensorBoard es un ESP32-S3 que actúa como periférico de sensores para la motherboard IncuNest. Se comunica con la motherboard mediante el puerto USB nativo (GPIO19 D−, GPIO20 D+). Esta fase establece la capa de comunicación base sobre la que se construirán todas las fases siguientes.

---

## Decisiones de arquitectura

| Aspecto | Decisión | Razón |
|---|---|---|
| Framework | ESP-IDF v6 puro (`idf.py`) | Soporte nativo TinyUSB, `esp_camera` IDF-native, I2S API v2 |
| Módulo | ESP32-S3-WROOM-1-N16R8 | 16MB Flash, 8MB PSRAM OPI (esencial para frame buffers en Fase 5) |
| USB driver | `esp_tinyusb` CDC-ACM | Driver oficial Espressif, estable en ESP32-S3 |
| Pins USB | IO19 (D−), IO20 (D+) | Hardwired en ESP32-S3, sin configuración de pin adicional |
| Framing | Magic + Type + Len + Payload + CRC16 | Soporta JSON (Fases 1–4) y JPEG binario (Fase 5) sin cambios |
| Logs | `esp_log_set_vprintf()` → JSON frames | Canal único, parseable en motherboard, silenciable en producción |
| Concurrencia | FreeRTOS: `usb_rx_task` + `usb_tx_task` + `tx_queue` | Sensores futuros publican a la cola sin tocar USB |

---

## Estructura de archivos

```
SensorBoard/
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv
├── main/
│   ├── CMakeLists.txt
│   └── main.c                              ← app_main: init + arranca tareas
└── components/
    └── usb_comm/
        ├── CMakeLists.txt
        ├── include/
        │   ├── sensorBoard_comm.h          ← API pública del componente
        │   └── sensorBoard_comm_protocol.h ← constantes del protocolo (replicable en motherBoard)
        ├── sensorBoard_comm.c              ← init CDC, tareas RX/TX, cola, log interceptor
        └── sensorBoard_cmd_handler.c       ← despacho de comandos (status, futuras fases)
```

---

## Protocolo de comunicación

### Capa de framing (binario)

Todos los mensajes, sin excepción, se envuelven en este frame:

```
┌────────────┬──────┬──────────────┬─────────────┬──────────┐
│ Magic 2B   │Type  │  Length 4B   │  Payload NB │ CRC16 2B │
│ 0xAB 0xCD  │ 1B   │  LE uint32   │             │          │
└────────────┴──────┴──────────────┴─────────────┴──────────┘

TYPE:
  0x00 = JSON text  (Fases 1–4)
  0x01 = JPEG raw   (Fase 5)

CRC16: CRC16-CCITT (polinomio 0x1021, valor inicial 0xFFFF)
       calculado sobre Type + Length + Payload
Overhead: 9 bytes por mensaje
```

**Motivación:** el delimitador `\n` (NDJSON puro) no es válido para payloads binarios (JPEG contiene bytes `0x0A`). El framing con longitud explícita resuelve ambos casos sin cambios en Fase 5.

### Capa de payload JSON (TYPE=0x00)

Todos los mensajes JSON son objetos planos o con un solo nivel de anidamiento en `data`. Compatibles con `StaticJsonDocument<256>` de ArduinoJson 6.x en la motherboard.

**Campos estándar:**

| Campo | Tipo | Descripción |
|---|---|---|
| `type` | string | `"cmd"` \| `"resp"` \| `"event"` \| `"log"` |
| `cmd` | string | Nombre del comando |
| `id` | uint32 | Transaction ID (motherboard genera, SensorBoard devuelve en resp) |
| `status` | string | `"ok"` \| `"error"` (solo en `resp`) |
| `ts` | uint32 | Uptime en ms (solo en mensajes de SensorBoard) |
| `data` | object | Payload de datos (fases futuras, un nivel de anidamiento máx.) |
| `msg` | string | Mensaje de error o log |

**Ejemplos por tipo:**

Comando (motherboard → SensorBoard):
```json
{"type":"cmd","cmd":"status","id":1}
```

Respuesta OK (SensorBoard → motherboard):
```json
{"type":"resp","cmd":"status","id":1,"status":"ok","device":"SensorBoard","fw":"1.0.0","uptime":4521}
```

Respuesta de error:
```json
{"type":"resp","cmd":"unknown","id":2,"status":"error","msg":"cmd not found","ts":4522}
```

Evento (fases futuras, SensorBoard → motherboard):
```json
{"type":"event","cmd":"door_open","ts":5100}
{"type":"event","cmd":"sensor_data","data":{"temp":37.5,"hum":60.2,"lux":320},"ts":5200}
```

Log (SensorBoard → motherboard, silenciable):
```json
{"type":"log","level":"I","tag":"BOOT","msg":"SensorBoard ready","ts":100}
```

**Comando `status` — respuesta extendida (fases futuras):**

Sin romper compatibilidad, el campo `sensors` se añadirá en Fase 2+:
```json
{"type":"resp","cmd":"status","id":1,"status":"ok","device":"SensorBoard","fw":"1.0.0",
 "uptime":4521,"sensors":{"sht40":true,"als":true,"mic":true,"door":true,"cam":false}}
```

---

## Arquitectura de tareas FreeRTOS

```
motherBoard
    │ USB CDC (IO19/IO20)
    ▼
┌─────────────────────────────────────────────────┐
│              usb_comm component                 │
│                                                 │
│  usb_rx_task                  usb_tx_task       │
│  ┌────────────┐               ┌─────────────┐   │
│  │ read CDC   │               │ dequeue     │   │
│  │ deframe    │               │ frame msg   │   │
│  │ parse JSON │──cmd_handler─▶│ write CDC   │   │
│  └────────────┘   (directo    └──────▲──────┘   │
│                    en Fase 1)        │           │
│                               tx_queue           │
└───────────────────────────────────┬─────────────┘
                                    │ usb_comm_send_event()
                          ┌─────────┴──────────┐
                          │  sensor_task (F2)  │
                          │  audio_task  (F3)  │
                          │  door_task   (F4)  │
                          │  camera_task (F5)  │
                          └────────────────────┘
```

**Tareas y stacks:**

| Tarea | Stack | Prioridad | Descripción |
|---|---|---|---|
| `usb_rx_task` | 4096B | 5 | Lee CDC, deframes, parsea JSON, llama cmd_handler |
| `usb_tx_task` | 4096B | 5 | Consume `tx_queue`, framea, escribe CDC |

**Cola `tx_queue`:**
- Items de tipo `sb_tx_item_t` con puntero a buffer heap + longitud + tipo
- Fase 1: JSON pequeños, copia por valor en buffer fijo (≤256B)
- Fase 5: JPEG heap-allocated, transferencia por puntero con `free()` post-envío

---

## Configuración ESP-IDF (`sdkconfig.defaults`)

```
# USB TinyUSB CDC
CONFIG_TINYUSB_CDC_ENABLED=y
CONFIG_TINYUSB_CDC_RX_BUFSIZE=4096
CONFIG_TINYUSB_CDC_TX_BUFSIZE=4096
CONFIG_TINYUSB_RHPORT_NUM=0

# Consola: NO a UART, logs via JSON custom handler
CONFIG_ESP_CONSOLE_NONE=y          # verificar key exacto en ESP-IDF v6 menuconfig

# Flash y PSRAM (N16R8)
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y
CONFIG_SPIRAM_RODATA=y

# Logs
CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y

# FreeRTOS
CONFIG_FREERTOS_HZ=1000
```

---

## API pública del componente (`sensorBoard_comm.h`)

```c
// Inicializa USB CDC, intercepta logs, arranca tareas RX/TX
esp_err_t sensorBoard_comm_init(void);

// Encola un mensaje JSON para enviar a la motherboard
// payload: string JSON null-terminated
esp_err_t sensorBoard_comm_send_json(const char *payload);

// Encola un buffer binario (Fase 5: JPEG)
// buf transferido al componente, se libera tras envío
esp_err_t sensorBoard_comm_send_binary(uint8_t type, uint8_t *buf, size_t len);
```

Los sensores de fases futuras solo llaman `sensorBoard_comm_send_json()` con su payload. No conocen nada de USB, framing, ni CRC.

---

## Compatibilidad con motherBoard

La motherboard (Arduino + PlatformIO) necesitará:
1. Un parser de framing binario (~50 líneas): lee magic, type, len, payload, verifica CRC16
2. Para TYPE=0x00: `deserializeJson()` con `StaticJsonDocument<256>`
3. Para TYPE=0x01 (Fase 5): buffer en PSRAM + escritura a display/SD

El archivo `sensorBoard_comm_protocol.h` define las constantes del protocolo y puede copiarse/referenciarse en el proyecto `motherBoard`.

---

## Fases futuras — puntos de extensión

| Fase | Cambio en `usb_comm` | Cambio en código nuevo |
|---|---|---|
| 2 — Sensores ambient | Ninguno | `sensor_task` llama `send_json()` |
| 3 — Micrófono / dBA | Ninguno | `audio_task` llama `send_json()` |
| 4 — Puerta | Ninguno | ISR encola evento, `send_json()` |
| 5 — Cámara | Añadir `TYPE=0x01` en tx_task | `camera_task` llama `send_binary()` |

---

## Criterios de éxito para Fase 1

- [ ] Proyecto compila sin errores con `idf.py build`
- [ ] SensorBoard aparece como dispositivo USB CDC en la motherboard
- [ ] Comando `{"type":"cmd","cmd":"status","id":1}` devuelve respuesta válida con `"status":"ok"`
- [ ] Logs ESP_LOG aparecen en la motherboard como frames JSON `type=log`
- [ ] Comando desconocido devuelve `"status":"error"` con `"msg":"cmd not found"`
- [ ] Frame con CRC incorrecto es descartado silenciosamente
