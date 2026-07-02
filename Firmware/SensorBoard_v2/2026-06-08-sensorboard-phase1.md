# SensorBoard Fase 1 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Crear el proyecto SensorBoard en ESP-IDF v6 con comunicación USB CDC nativa, protocolo JSON enmarcado en framing binario (Magic+Type+Len+Payload+CRC16-CCITT), y el comando `status`.

**Architecture:** Componente `usb_comm` con dos tareas FreeRTOS (`usb_rx_task` / `usb_tx_task`) comunicadas por una cola. Los logs ESP_LOG se interceptan via `esp_log_set_vprintf()` y se emiten como frames JSON. Las fases futuras sólo necesitan llamar `sensorBoard_comm_send_json()`.

**Tech Stack:** ESP-IDF v6.0.1, esp_tinyusb CDC-ACM, FreeRTOS, cJSON (built-in IDF), CRC16-CCITT propio.

---

## Mapa de archivos

| Archivo | Responsabilidad |
|---|---|
| `SensorBoard/CMakeLists.txt` | Raíz del proyecto IDF |
| `SensorBoard/sdkconfig.defaults` | Flash 16MB, PSRAM OPI, USB CDC, consola suprimida |
| `SensorBoard/partitions.csv` | Tabla de particiones 16MB |
| `SensorBoard/main/CMakeLists.txt` | Componente main |
| `SensorBoard/main/main.c` | `app_main`: init + log de arranque |
| `SensorBoard/components/usb_comm/CMakeLists.txt` | Componente usb_comm |
| `SensorBoard/components/usb_comm/include/sensorBoard_comm_protocol.h` | Constantes del protocolo (compartible con motherBoard) |
| `SensorBoard/components/usb_comm/include/sensorBoard_comm.h` | API pública |
| `SensorBoard/components/usb_comm/sensorBoard_crc16.h` | Header interno CRC16 |
| `SensorBoard/components/usb_comm/sensorBoard_crc16.c` | CRC16-CCITT byte-a-byte |
| `SensorBoard/components/usb_comm/sensorBoard_frame.h` | Header interno encoder/decoder |
| `SensorBoard/components/usb_comm/sensorBoard_frame.c` | Frame encode + decoder state machine |
| `SensorBoard/components/usb_comm/sensorBoard_comm.c` | Init CDC, tareas RX/TX, cola, log interceptor |
| `SensorBoard/components/usb_comm/sensorBoard_cmd_handler.c` | Dispatch: status + unknown |
| `SensorBoard/test_apps/comm_test/CMakeLists.txt` | App de tests Unity |
| `SensorBoard/test_apps/comm_test/sdkconfig.defaults` | Consola USB CDC activa para ver output Unity |
| `SensorBoard/test_apps/comm_test/main/CMakeLists.txt` | Componente main del test app |
| `SensorBoard/test_apps/comm_test/main/test_main.c` | Tests Unity: CRC16, frame encode, frame decode |

---

## Task 1: Project scaffold

**Files:**
- Create: `SensorBoard/CMakeLists.txt`
- Create: `SensorBoard/sdkconfig.defaults`
- Create: `SensorBoard/partitions.csv`
- Create: `SensorBoard/main/CMakeLists.txt`
- Create: `SensorBoard/main/main.c`
- Create: `SensorBoard/components/usb_comm/CMakeLists.txt`
- Create: `SensorBoard/components/usb_comm/include/sensorBoard_comm.h`
- Create: `SensorBoard/components/usb_comm/sensorBoard_comm.c`

- [ ] **Step 1: Crear `SensorBoard/CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(SensorBoard)
```

- [ ] **Step 2: Crear `SensorBoard/sdkconfig.defaults`**

```
# USB TinyUSB CDC-ACM
CONFIG_TINYUSB_CDC_ENABLED=y
CONFIG_TINYUSB_CDC_RX_BUFSIZE=4096
CONFIG_TINYUSB_CDC_TX_BUFSIZE=4096

# Sin consola UART — los logs van por JSON sobre USB CDC
CONFIG_ESP_CONSOLE_NONE=y

# Flash 16MB + PSRAM OPI 8MB (ESP32-S3-WROOM-1-N16R8)
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y
CONFIG_SPIRAM_RODATA=y

# Logs verbose en desarrollo
CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y

# FreeRTOS tick 1kHz
CONFIG_FREERTOS_HZ=1000
```

- [ ] **Step 3: Crear `SensorBoard/partitions.csv`**

```csv
# Name,   Type, SubType,  Offset,   Size,     Flags
nvs,      data, nvs,      0x9000,   0x6000,
phy_init, data, phy,      0xF000,   0x1000,
factory,  app,  factory,  0x10000,  0x300000,
ota_0,    app,  ota_0,    0x310000, 0x300000,
ota_1,    app,  ota_1,    0x610000, 0x300000,
storage,  data, spiffs,   0x910000, 0x6F0000,
```

- [ ] **Step 4: Crear `SensorBoard/main/CMakeLists.txt`**

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES usb_comm
)
```

- [ ] **Step 5: Crear `SensorBoard/main/main.c` (stub mínimo)**

```c
#include "esp_log.h"
#include "sensorBoard_comm.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "SensorBoard booting...");
    ESP_ERROR_CHECK(sensorBoard_comm_init());
    ESP_LOGI(TAG, "SensorBoard ready");
}
```

- [ ] **Step 6: Crear `SensorBoard/components/usb_comm/CMakeLists.txt`**

```cmake
idf_component_register(
    SRCS
        "sensorBoard_comm.c"
        "sensorBoard_cmd_handler.c"
        "sensorBoard_crc16.c"
        "sensorBoard_frame.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES esp_tinyusb esp_timer cJSON
)
```

- [ ] **Step 7: Crear `SensorBoard/components/usb_comm/include/sensorBoard_comm.h` (stub)**

```c
#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

esp_err_t sensorBoard_comm_init(void);
esp_err_t sensorBoard_comm_send_json(const char *json_str);
esp_err_t sensorBoard_comm_send_binary(uint8_t type, uint8_t *buf, size_t len);
```

- [ ] **Step 8: Crear `SensorBoard/components/usb_comm/sensorBoard_comm.c` (stub compilable)**

```c
#include "sensorBoard_comm.h"
#include "esp_log.h"

static const char *TAG = "USB_COMM";

esp_err_t sensorBoard_comm_init(void)
{
    ESP_LOGI(TAG, "comm init stub");
    return ESP_OK;
}

esp_err_t sensorBoard_comm_send_json(const char *json_str)
{
    (void)json_str;
    return ESP_OK;
}

esp_err_t sensorBoard_comm_send_binary(uint8_t type, uint8_t *buf, size_t len)
{
    (void)type; (void)buf; (void)len;
    return ESP_OK;
}
```

- [ ] **Step 9: Crear stub `sensorBoard_cmd_handler.c`, `sensorBoard_crc16.c`, `sensorBoard_frame.c`**

`sensorBoard_cmd_handler.c`:
```c
#include "sensorBoard_comm.h"
void sensorBoard_cmd_handle(const uint8_t *payload, size_t len) { (void)payload; (void)len; }
```

`sensorBoard_crc16.c`:
```c
#include "sensorBoard_crc16.h"
uint16_t sb_crc16_byte(uint16_t crc, uint8_t byte) { (void)crc; (void)byte; return 0; }
uint16_t sb_crc16(const uint8_t *data, size_t len) { (void)data; (void)len; return 0; }
```

`sensorBoard_frame.c`:
```c
#include "sensorBoard_frame.h"
size_t sb_frame_encode(uint8_t t, const uint8_t *p, size_t plen, uint8_t *out, size_t outsz)
{ (void)t;(void)p;(void)plen;(void)out;(void)outsz; return 0; }
void sb_frame_dec_init(sb_frame_dec_t *d, uint8_t *buf, size_t sz)
{ (void)d;(void)buf;(void)sz; }
void sb_frame_dec_feed(sb_frame_dec_t *d, uint8_t b, sb_frame_cb_t cb, void *ctx)
{ (void)d;(void)b;(void)cb;(void)ctx; }
```

También crear los headers internos vacíos:

`sensorBoard_crc16.h`:
```c
#pragma once
#include <stdint.h>
#include <stddef.h>
uint16_t sb_crc16_byte(uint16_t crc, uint8_t byte);
uint16_t sb_crc16(const uint8_t *data, size_t len);
```

`sensorBoard_frame.h`:
```c
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef enum {
    SB_DEC_MAGIC0=0, SB_DEC_MAGIC1, SB_DEC_TYPE,
    SB_DEC_LEN0, SB_DEC_LEN1, SB_DEC_LEN2, SB_DEC_LEN3,
    SB_DEC_PAYLOAD, SB_DEC_CRC0, SB_DEC_CRC1,
} sb_dec_state_t;

typedef struct {
    sb_dec_state_t state;
    uint8_t        type;
    uint32_t       payload_len;
    uint32_t       payload_pos;
    uint8_t       *payload_buf;
    size_t         payload_buf_size;
    uint8_t        crc_rx[2];
    uint16_t       crc_acc;
} sb_frame_dec_t;

typedef void (*sb_frame_cb_t)(uint8_t type, const uint8_t *payload, size_t len, void *ctx);

size_t sb_frame_encode(uint8_t type, const uint8_t *payload, size_t payload_len,
                       uint8_t *out_buf, size_t out_buf_size);
void sb_frame_dec_init(sb_frame_dec_t *dec, uint8_t *buf, size_t buf_size);
void sb_frame_dec_feed(sb_frame_dec_t *dec, uint8_t byte, sb_frame_cb_t cb, void *ctx);
```

- [ ] **Step 10: Verificar que compila**

```
cd SensorBoard
idf.py build
```

Resultado esperado: `Project build complete.` Sin errores de compilación.

- [ ] **Step 11: Commit**

```
git add Firmware/SensorBoard/
git commit -m "feat(sensorboard): project scaffold — empty IDF project compiles"
```

---

## Task 2: Protocol constants

**Files:**
- Create: `SensorBoard/components/usb_comm/include/sensorBoard_comm_protocol.h`

- [ ] **Step 1: Crear el header completo**

```c
#pragma once
#include <stdint.h>

/* ── Frame framing ─────────────────────────────────────────── */
#define SB_PROTO_MAGIC_0            0xABu
#define SB_PROTO_MAGIC_1            0xCDu

#define SB_PROTO_TYPE_JSON          0x00u
#define SB_PROTO_TYPE_JPEG          0x01u   /* Phase 5 */

/* Header: 2 magic + 1 type + 4 len */
#define SB_PROTO_FRAME_HEADER_SIZE  7u
#define SB_PROTO_FRAME_CRC_SIZE     2u
#define SB_PROTO_FRAME_OVERHEAD     (SB_PROTO_FRAME_HEADER_SIZE + SB_PROTO_FRAME_CRC_SIZE)

#define SB_PROTO_MAX_JSON_PAYLOAD   256u
/* Max frame buffer needed for JSON: */
#define SB_PROTO_MAX_JSON_FRAME     (SB_PROTO_FRAME_OVERHEAD + SB_PROTO_MAX_JSON_PAYLOAD)

/* ── Device identity ───────────────────────────────────────── */
#define SB_PROTO_FW_VERSION         "1.0.0"
#define SB_PROTO_DEVICE_NAME        "SensorBoard"

/* ── JSON field names ──────────────────────────────────────── */
#define SB_JSON_TYPE        "type"
#define SB_JSON_CMD         "cmd"
#define SB_JSON_ID          "id"
#define SB_JSON_STATUS      "status"
#define SB_JSON_TS          "ts"
#define SB_JSON_DATA        "data"
#define SB_JSON_MSG         "msg"
#define SB_JSON_LEVEL       "level"
#define SB_JSON_TAG         "tag"
#define SB_JSON_DEVICE      "device"
#define SB_JSON_FW          "fw"
#define SB_JSON_UPTIME      "uptime"

/* ── JSON type values ──────────────────────────────────────── */
#define SB_JSON_TYPE_CMD    "cmd"
#define SB_JSON_TYPE_RESP   "resp"
#define SB_JSON_TYPE_EVENT  "event"
#define SB_JSON_TYPE_LOG    "log"

/* ── JSON status values ────────────────────────────────────── */
#define SB_JSON_STATUS_OK   "ok"
#define SB_JSON_STATUS_ERR  "error"

/* ── Commands ──────────────────────────────────────────────── */
#define SB_CMD_STATUS       "status"
```

- [ ] **Step 2: Verificar que compila**

```
idf.py build
```

Resultado esperado: `Project build complete.`

- [ ] **Step 3: Commit**

```
git add Firmware/SensorBoard/components/usb_comm/include/sensorBoard_comm_protocol.h
git commit -m "feat(sensorboard): add protocol constants header"
```

---

## Task 3: Test app scaffold + CRC16-CCITT

**Files:**
- Create: `SensorBoard/test_apps/comm_test/CMakeLists.txt`
- Create: `SensorBoard/test_apps/comm_test/sdkconfig.defaults`
- Create: `SensorBoard/test_apps/comm_test/main/CMakeLists.txt`
- Create: `SensorBoard/test_apps/comm_test/main/test_main.c`
- Modify: `SensorBoard/components/usb_comm/sensorBoard_crc16.c`

- [ ] **Step 1: Crear `test_apps/comm_test/CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(comm_test)
```

- [ ] **Step 2: Crear `test_apps/comm_test/sdkconfig.defaults`**

```
# Consola USB CDC activa para ver output de Unity
CONFIG_ESP_CONSOLE_USB_CDC=y
CONFIG_TINYUSB_CDC_ENABLED=y
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
```

Copiar `partitions.csv` del proyecto principal:
```
cp SensorBoard/partitions.csv SensorBoard/test_apps/comm_test/partitions.csv
```

- [ ] **Step 3: Crear `test_apps/comm_test/main/CMakeLists.txt`**

```cmake
idf_component_register(
    SRCS "test_main.c"
    INCLUDE_DIRS "."
    REQUIRES unity usb_comm
)
```

- [ ] **Step 4: Escribir el test CRC16 (test falla — CRC impl es stub)**

`test_apps/comm_test/main/test_main.c`:
```c
#include "unity.h"
#include "sensorBoard_crc16.h"
#include "sensorBoard_frame.h"
#include "sensorBoard_comm_protocol.h"
#include <string.h>

/* ── CRC16-CCITT tests ─────────────────────────────────────── */

TEST_CASE("CRC16 known vector: '123456789' == 0x29B1", "[crc16]")
{
    const uint8_t data[] = "123456789";
    uint16_t crc = sb_crc16(data, 9);
    TEST_ASSERT_EQUAL_HEX16(0x29B1, crc);
}

TEST_CASE("CRC16 empty data returns 0xFFFF", "[crc16]")
{
    uint16_t crc = sb_crc16(NULL, 0);
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, crc);
}

TEST_CASE("CRC16 single byte 0x00", "[crc16]")
{
    const uint8_t data[] = {0x00};
    uint16_t crc = sb_crc16(data, 1);
    /* poly=0x1021, init=0xFFFF: 0xFF00 ^ bits = 0x84C0 */
    TEST_ASSERT_EQUAL_HEX16(0x84C0, crc);
}

TEST_CASE("CRC16 incremental equals batch", "[crc16]")
{
    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    uint16_t batch = sb_crc16(data, 4);

    uint16_t inc = 0xFFFF;
    for (int i = 0; i < 4; i++) {
        inc = sb_crc16_byte(inc, data[i]);
    }
    TEST_ASSERT_EQUAL_HEX16(batch, inc);
}

void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
```

- [ ] **Step 5: Construir test app — confirmar que falla por stub**

```
cd SensorBoard/test_apps/comm_test
idf.py build
```

Resultado esperado: compila. Al flashear: tests fallan (`0x0000 != 0x29B1`).

- [ ] **Step 6: Implementar CRC16-CCITT real**

`SensorBoard/components/usb_comm/sensorBoard_crc16.c`:
```c
#include "sensorBoard_crc16.h"

uint16_t sb_crc16_byte(uint16_t crc, uint8_t byte)
{
    crc ^= ((uint16_t)byte << 8);
    for (int i = 0; i < 8; i++) {
        crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

uint16_t sb_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc = sb_crc16_byte(crc, data[i]);
    }
    return crc;
}
```

- [ ] **Step 7: Flashear test app y verificar que los tests CRC16 pasan**

```
cd SensorBoard/test_apps/comm_test
idf.py -p COMx flash monitor
```

Resultado esperado en consola USB:
```
-----------------------
4 Tests 0 Failures 0 Ignored
OK
```

- [ ] **Step 8: Commit**

```
git add Firmware/SensorBoard/
git commit -m "feat(sensorboard): implement CRC16-CCITT with unit tests"
```

---

## Task 4: Frame encoder

**Files:**
- Modify: `SensorBoard/components/usb_comm/sensorBoard_frame.c`
- Modify: `SensorBoard/test_apps/comm_test/main/test_main.c`

- [ ] **Step 1: Agregar tests de encoder (antes de implementar)**

Añadir al final de `test_main.c`, antes de `app_main`:

```c
/* ── Frame encoder tests ───────────────────────────────────── */

TEST_CASE("frame_encode: output size = payload + overhead", "[frame]")
{
    const uint8_t payload[] = "hello";
    uint8_t out[SB_PROTO_MAX_JSON_FRAME];
    size_t len = sb_frame_encode(SB_PROTO_TYPE_JSON, payload, 5, out, sizeof(out));
    TEST_ASSERT_EQUAL(5 + SB_PROTO_FRAME_OVERHEAD, len);
}

TEST_CASE("frame_encode: magic bytes correct", "[frame]")
{
    uint8_t out[32];
    sb_frame_encode(SB_PROTO_TYPE_JSON, (uint8_t *)"A", 1, out, sizeof(out));
    TEST_ASSERT_EQUAL_HEX8(SB_PROTO_MAGIC_0, out[0]);
    TEST_ASSERT_EQUAL_HEX8(SB_PROTO_MAGIC_1, out[1]);
}

TEST_CASE("frame_encode: type byte at position 2", "[frame]")
{
    uint8_t out[32];
    sb_frame_encode(SB_PROTO_TYPE_JSON, (uint8_t *)"A", 1, out, sizeof(out));
    TEST_ASSERT_EQUAL_HEX8(SB_PROTO_TYPE_JSON, out[2]);
}

TEST_CASE("frame_encode: length LE at bytes 3-6", "[frame]")
{
    uint8_t out[32];
    sb_frame_encode(SB_PROTO_TYPE_JSON, (uint8_t *)"ABCDE", 5, out, sizeof(out));
    TEST_ASSERT_EQUAL_HEX8(5, out[3]);
    TEST_ASSERT_EQUAL_HEX8(0, out[4]);
    TEST_ASSERT_EQUAL_HEX8(0, out[5]);
    TEST_ASSERT_EQUAL_HEX8(0, out[6]);
}

TEST_CASE("frame_encode: payload copied at offset 7", "[frame]")
{
    uint8_t out[32];
    sb_frame_encode(SB_PROTO_TYPE_JSON, (uint8_t *)"HI", 2, out, sizeof(out));
    TEST_ASSERT_EQUAL_HEX8('H', out[7]);
    TEST_ASSERT_EQUAL_HEX8('I', out[8]);
}

TEST_CASE("frame_encode: returns 0 when buffer too small", "[frame]")
{
    uint8_t out[5];
    size_t len = sb_frame_encode(SB_PROTO_TYPE_JSON, (uint8_t *)"hello", 5, out, sizeof(out));
    TEST_ASSERT_EQUAL(0, len);
}
```

- [ ] **Step 2: Build test app — confirmar que encoder tests fallan**

```
cd SensorBoard/test_apps/comm_test && idf.py build flash monitor -p COMx
```

Tests de `[frame]` fallan. Tests de `[crc16]` siguen pasando.

- [ ] **Step 3: Implementar `sb_frame_encode` en `sensorBoard_frame.c`**

```c
#include "sensorBoard_frame.h"
#include "sensorBoard_crc16.h"
#include "sensorBoard_comm_protocol.h"
#include <string.h>

size_t sb_frame_encode(uint8_t type, const uint8_t *payload, size_t payload_len,
                       uint8_t *out_buf, size_t out_buf_size)
{
    size_t total = SB_PROTO_FRAME_OVERHEAD + payload_len;
    if (out_buf_size < total) return 0;

    out_buf[0] = SB_PROTO_MAGIC_0;
    out_buf[1] = SB_PROTO_MAGIC_1;
    out_buf[2] = type;
    out_buf[3] = (uint8_t)( payload_len        & 0xFFu);
    out_buf[4] = (uint8_t)((payload_len >>  8) & 0xFFu);
    out_buf[5] = (uint8_t)((payload_len >> 16) & 0xFFu);
    out_buf[6] = (uint8_t)((payload_len >> 24) & 0xFFu);

    if (payload_len > 0) {
        memcpy(out_buf + SB_PROTO_FRAME_HEADER_SIZE, payload, payload_len);
    }

    /* CRC over: type(1) + len_LE(4) + payload(N) */
    uint16_t crc = sb_crc16_byte(0xFFFFu, type);
    crc = sb_crc16_byte(crc, out_buf[3]);
    crc = sb_crc16_byte(crc, out_buf[4]);
    crc = sb_crc16_byte(crc, out_buf[5]);
    crc = sb_crc16_byte(crc, out_buf[6]);
    for (size_t i = 0; i < payload_len; i++) {
        crc = sb_crc16_byte(crc, payload[i]);
    }

    out_buf[SB_PROTO_FRAME_HEADER_SIZE + payload_len]     = (uint8_t)(crc >> 8);
    out_buf[SB_PROTO_FRAME_HEADER_SIZE + payload_len + 1] = (uint8_t)(crc & 0xFFu);

    return total;
}

/* Decoder stubs — implementados en Task 5 */
void sb_frame_dec_init(sb_frame_dec_t *dec, uint8_t *buf, size_t buf_size)
{
    (void)dec; (void)buf; (void)buf_size;
}

void sb_frame_dec_feed(sb_frame_dec_t *dec, uint8_t byte,
                       sb_frame_cb_t cb, void *ctx)
{
    (void)dec; (void)byte; (void)cb; (void)ctx;
}
```

- [ ] **Step 4: Flashear y verificar que todos los tests pasan**

```
idf.py -p COMx flash monitor
```

Resultado esperado: `10 Tests 0 Failures 0 Ignored  OK`

- [ ] **Step 5: Commit**

```
git add Firmware/SensorBoard/
git commit -m "feat(sensorboard): implement frame encoder with unit tests"
```

---

## Task 5: Frame decoder (state machine)

**Files:**
- Modify: `SensorBoard/components/usb_comm/sensorBoard_frame.c`
- Modify: `SensorBoard/test_apps/comm_test/main/test_main.c`

- [ ] **Step 1: Agregar tests del decoder**

Añadir a `test_main.c` antes de `app_main`:

```c
/* ── Frame decoder tests ───────────────────────────────────── */

static uint8_t s_cb_type;
static uint8_t s_cb_payload[64];
static size_t  s_cb_len;
static int     s_cb_calls;

static void test_frame_cb(uint8_t type, const uint8_t *payload, size_t len, void *ctx)
{
    (void)ctx;
    s_cb_type = type;
    s_cb_len  = len < sizeof(s_cb_payload) ? len : sizeof(s_cb_payload);
    memcpy(s_cb_payload, payload, s_cb_len);
    s_cb_calls++;
}

static void feed_frame(sb_frame_dec_t *dec, const uint8_t *payload, size_t len, uint8_t type)
{
    uint8_t frame[SB_PROTO_MAX_JSON_FRAME];
    size_t flen = sb_frame_encode(type, payload, len, frame, sizeof(frame));
    for (size_t i = 0; i < flen; i++) {
        sb_frame_dec_feed(dec, frame[i], test_frame_cb, NULL);
    }
}

TEST_CASE("decoder: round-trip JSON payload", "[decoder]")
{
    uint8_t dec_buf[SB_PROTO_MAX_JSON_PAYLOAD];
    sb_frame_dec_t dec;
    sb_frame_dec_init(&dec, dec_buf, sizeof(dec_buf));
    s_cb_calls = 0;

    const uint8_t payload[] = "hello world";
    feed_frame(&dec, payload, 11, SB_PROTO_TYPE_JSON);

    TEST_ASSERT_EQUAL(1, s_cb_calls);
    TEST_ASSERT_EQUAL(SB_PROTO_TYPE_JSON, s_cb_type);
    TEST_ASSERT_EQUAL(11, s_cb_len);
    TEST_ASSERT_EQUAL_MEMORY(payload, s_cb_payload, 11);
}

TEST_CASE("decoder: bad CRC discarded silently", "[decoder]")
{
    uint8_t dec_buf[SB_PROTO_MAX_JSON_PAYLOAD];
    sb_frame_dec_t dec;
    sb_frame_dec_init(&dec, dec_buf, sizeof(dec_buf));
    s_cb_calls = 0;

    uint8_t frame[32];
    size_t flen = sb_frame_encode(SB_PROTO_TYPE_JSON, (uint8_t *)"X", 1, frame, sizeof(frame));
    frame[flen - 1] ^= 0xFF; /* corrupt CRC low byte */

    for (size_t i = 0; i < flen; i++) {
        sb_frame_dec_feed(&dec, frame[i], test_frame_cb, NULL);
    }
    TEST_ASSERT_EQUAL(0, s_cb_calls);
}

TEST_CASE("decoder: resync after garbage bytes", "[decoder]")
{
    uint8_t dec_buf[SB_PROTO_MAX_JSON_PAYLOAD];
    sb_frame_dec_t dec;
    sb_frame_dec_init(&dec, dec_buf, sizeof(dec_buf));
    s_cb_calls = 0;

    /* Feed garbage */
    uint8_t garbage[] = {0x00, 0xFF, 0x12, 0x34};
    for (int i = 0; i < 4; i++) {
        sb_frame_dec_feed(&dec, garbage[i], test_frame_cb, NULL);
    }
    /* Then a valid frame */
    feed_frame(&dec, (uint8_t *)"OK", 2, SB_PROTO_TYPE_JSON);

    TEST_ASSERT_EQUAL(1, s_cb_calls);
    TEST_ASSERT_EQUAL(2, s_cb_len);
}

TEST_CASE("decoder: empty payload frame", "[decoder]")
{
    uint8_t dec_buf[SB_PROTO_MAX_JSON_PAYLOAD];
    sb_frame_dec_t dec;
    sb_frame_dec_init(&dec, dec_buf, sizeof(dec_buf));
    s_cb_calls = 0;

    feed_frame(&dec, NULL, 0, SB_PROTO_TYPE_JSON);
    TEST_ASSERT_EQUAL(1, s_cb_calls);
    TEST_ASSERT_EQUAL(0, s_cb_len);
}
```

- [ ] **Step 2: Build + flash — confirmar que decoder tests fallan**

```
idf.py -p COMx build flash monitor
```

Tests `[decoder]` fallan. Tests anteriores siguen pasando.

- [ ] **Step 3: Implementar decoder en `sensorBoard_frame.c`**

Reemplazar los stubs `sb_frame_dec_init` y `sb_frame_dec_feed`:

```c
void sb_frame_dec_init(sb_frame_dec_t *dec, uint8_t *buf, size_t buf_size)
{
    dec->state           = SB_DEC_MAGIC0;
    dec->payload_buf     = buf;
    dec->payload_buf_size = buf_size;
    dec->payload_len     = 0;
    dec->payload_pos     = 0;
    dec->crc_acc         = 0xFFFFu;
}

void sb_frame_dec_feed(sb_frame_dec_t *dec, uint8_t byte,
                       sb_frame_cb_t cb, void *ctx)
{
    switch (dec->state) {

    case SB_DEC_MAGIC0:
        if (byte == SB_PROTO_MAGIC_0) dec->state = SB_DEC_MAGIC1;
        break;

    case SB_DEC_MAGIC1:
        dec->state = (byte == SB_PROTO_MAGIC_1) ? SB_DEC_TYPE : SB_DEC_MAGIC0;
        break;

    case SB_DEC_TYPE:
        dec->type    = byte;
        dec->crc_acc = sb_crc16_byte(0xFFFFu, byte); /* start fresh CRC */
        dec->state   = SB_DEC_LEN0;
        break;

    case SB_DEC_LEN0:
        dec->payload_len  = byte;
        dec->crc_acc      = sb_crc16_byte(dec->crc_acc, byte);
        dec->state        = SB_DEC_LEN1;
        break;

    case SB_DEC_LEN1:
        dec->payload_len |= ((uint32_t)byte << 8);
        dec->crc_acc      = sb_crc16_byte(dec->crc_acc, byte);
        dec->state        = SB_DEC_LEN2;
        break;

    case SB_DEC_LEN2:
        dec->payload_len |= ((uint32_t)byte << 16);
        dec->crc_acc      = sb_crc16_byte(dec->crc_acc, byte);
        dec->state        = SB_DEC_LEN3;
        break;

    case SB_DEC_LEN3:
        dec->payload_len |= ((uint32_t)byte << 24);
        dec->crc_acc      = sb_crc16_byte(dec->crc_acc, byte);
        if (dec->payload_len == 0) {
            dec->state = SB_DEC_CRC0;
        } else if (dec->payload_len > dec->payload_buf_size) {
            dec->state = SB_DEC_MAGIC0; /* payload too large: discard */
        } else {
            dec->payload_pos = 0;
            dec->state       = SB_DEC_PAYLOAD;
        }
        break;

    case SB_DEC_PAYLOAD:
        dec->payload_buf[dec->payload_pos++] = byte;
        dec->crc_acc = sb_crc16_byte(dec->crc_acc, byte);
        if (dec->payload_pos >= dec->payload_len) {
            dec->state = SB_DEC_CRC0;
        }
        break;

    case SB_DEC_CRC0:
        dec->crc_rx[0] = byte;
        dec->state     = SB_DEC_CRC1;
        break;

    case SB_DEC_CRC1:
        dec->crc_rx[1] = byte;
        dec->state     = SB_DEC_MAGIC0;
        {
            uint16_t crc_rx = ((uint16_t)dec->crc_rx[0] << 8) | dec->crc_rx[1];
            if (crc_rx == dec->crc_acc && cb) {
                cb(dec->type, dec->payload_buf, dec->payload_len, ctx);
            }
        }
        break;
    }
}
```

- [ ] **Step 4: Flashear y verificar que todos los tests pasan**

```
idf.py -p COMx flash monitor
```

Resultado esperado: `14 Tests 0 Failures 0 Ignored  OK`

- [ ] **Step 5: Commit**

```
git add Firmware/SensorBoard/
git commit -m "feat(sensorboard): implement frame decoder state machine with unit tests"
```

---

## Task 6: USB CDC init

**Files:**
- Modify: `SensorBoard/components/usb_comm/sensorBoard_comm.c`

- [ ] **Step 1: Implementar init USB CDC**

Reemplazar `sensorBoard_comm.c` completo:

```c
#include "sensorBoard_comm.h"
#include "sensorBoard_comm_protocol.h"
#include "sensorBoard_frame.h"
#include "sensorBoard_crc16.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

static const char *TAG = "USB_COMM";

/* ── TX queue ──────────────────────────────────────────────── */
typedef struct {
    uint8_t frame[SB_PROTO_MAX_JSON_FRAME];
    size_t  len;
} sb_tx_item_t;

#define TX_QUEUE_DEPTH  8

static QueueHandle_t     s_tx_queue  = NULL;
static SemaphoreHandle_t s_rx_sem    = NULL;
static bool              s_cdc_ready = false;

/* Forward declarations */
static void usb_tx_task(void *arg);
static void usb_rx_task(void *arg);
static int  sb_log_vprintf(const char *fmt, va_list args);

/* ── CDC callbacks ─────────────────────────────────────────── */
static void cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    (void)itf; (void)event;
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_rx_sem, &woken);
    portYIELD_FROM_ISR(woken);
}

static void cdc_line_state_callback(int itf, cdcacm_event_t *event)
{
    (void)itf;
    s_cdc_ready = event->line_state_changed_data.dtr;
}

/* ── Public init ───────────────────────────────────────────── */
esp_err_t sensorBoard_comm_init(void)
{
    s_tx_queue = xQueueCreate(TX_QUEUE_DEPTH, sizeof(sb_tx_item_t));
    if (!s_tx_queue) return ESP_ERR_NO_MEM;

    s_rx_sem = xSemaphoreCreateBinary();
    if (!s_rx_sem) return ESP_ERR_NO_MEM;

    /* Install TinyUSB driver */
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor        = NULL,
        .string_descriptor        = NULL,
        .string_descriptor_count  = 0,
        .external_phy             = false,
        .configuration_descriptor = NULL,
    };
    ESP_RETURN_ON_ERROR(tinyusb_driver_install(&tusb_cfg), TAG, "TinyUSB install failed");

    /* Init CDC-ACM */
    const tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev                    = TINYUSB_USBDEV_0,
        .cdc_port                   = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz           = 64,
        .callback_rx                = cdc_rx_callback,
        .callback_rx_wanted_char    = NULL,
        .callback_line_state_changed = cdc_line_state_callback,
        .callback_line_coding_changed = NULL,
    };
    ESP_RETURN_ON_ERROR(tusb_cdc_acm_init(&acm_cfg), TAG, "CDC ACM init failed");

    /* Redirect ESP_LOG to JSON frames */
    esp_log_set_vprintf(sb_log_vprintf);

    /* Start tasks */
    xTaskCreate(usb_tx_task, "usb_tx", 4096, NULL, 5, NULL);
    xTaskCreate(usb_rx_task, "usb_rx", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "USB CDC ready");
    return ESP_OK;
}
```

- [ ] **Step 2: Verificar que el proyecto principal compila**

```
cd SensorBoard && idf.py build
```

Resultado esperado: `Project build complete.`

- [ ] **Step 3: Commit**

```
git add Firmware/SensorBoard/components/usb_comm/sensorBoard_comm.c
git commit -m "feat(sensorboard): USB CDC init with TinyUSB"
```

---

## Task 7: TX task + queue + send_json

**Files:**
- Modify: `SensorBoard/components/usb_comm/sensorBoard_comm.c`

- [ ] **Step 1: Añadir TX task, send_json y send_binary al final de `sensorBoard_comm.c`**

```c
/* ── TX task ───────────────────────────────────────────────── */
static void usb_tx_task(void *arg)
{
    (void)arg;
    sb_tx_item_t item;

    for (;;) {
        if (xQueueReceive(s_tx_queue, &item, portMAX_DELAY) == pdTRUE) {
            if (s_cdc_ready) {
                tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, item.frame, item.len);
                tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, pdMS_TO_TICKS(50));
            }
        }
    }
}

/* ── Public send API ───────────────────────────────────────── */
esp_err_t sensorBoard_comm_send_json(const char *json_str)
{
    if (!json_str || !s_tx_queue) return ESP_ERR_INVALID_STATE;

    sb_tx_item_t item;
    size_t payload_len = strlen(json_str);

    if (payload_len > SB_PROTO_MAX_JSON_PAYLOAD) return ESP_ERR_INVALID_SIZE;

    item.len = sb_frame_encode(SB_PROTO_TYPE_JSON,
                               (const uint8_t *)json_str, payload_len,
                               item.frame, sizeof(item.frame));
    if (item.len == 0) return ESP_ERR_NO_MEM;

    return (xQueueSend(s_tx_queue, &item, pdMS_TO_TICKS(10)) == pdTRUE)
           ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t sensorBoard_comm_send_binary(uint8_t type, uint8_t *buf, size_t len)
{
    /* Phase 5 placeholder — binary frames are large (JPEG), needs heap approach */
    (void)type; (void)buf; (void)len;
    return ESP_ERR_NOT_SUPPORTED;
}
```

- [ ] **Step 2: Verificar que compila**

```
cd SensorBoard && idf.py build
```

- [ ] **Step 3: Commit**

```
git add Firmware/SensorBoard/components/usb_comm/sensorBoard_comm.c
git commit -m "feat(sensorboard): TX task and send_json API"
```

---

## Task 8: Log interceptor

**Files:**
- Modify: `SensorBoard/components/usb_comm/sensorBoard_comm.c`

- [ ] **Step 1: Añadir `sb_log_vprintf` a `sensorBoard_comm.c`**

Añadir esta función antes de `sensorBoard_comm_init`:

```c
/* ── Log interceptor ───────────────────────────────────────── */
static int sb_log_vprintf(const char *fmt, va_list args)
{
    if (!s_tx_queue) {
        /* Queue not ready — drop silently */
        return 0;
    }

    char msg_buf[160];
    int msg_len = vsnprintf(msg_buf, sizeof(msg_buf) - 1, fmt, args);
    if (msg_len < 0) return msg_len;
    msg_buf[sizeof(msg_buf) - 1] = '\0';

    /* Strip trailing \r\n */
    while (msg_len > 0 &&
           (msg_buf[msg_len - 1] == '\n' || msg_buf[msg_len - 1] == '\r')) {
        msg_buf[--msg_len] = '\0';
    }

    uint32_t ts_ms = (uint32_t)(esp_timer_get_time() / 1000);

    /* Build JSON with manual escaping of " and \ */
    char json_buf[SB_PROTO_MAX_JSON_PAYLOAD];
    int  jpos = 0;

    jpos += snprintf(json_buf + jpos, sizeof(json_buf) - (size_t)jpos,
                     "{\"type\":\"log\",\"ts\":%lu,\"msg\":\"", (unsigned long)ts_ms);

    for (int i = 0; i < msg_len && jpos < (int)sizeof(json_buf) - 4; i++) {
        if (msg_buf[i] == '"' || msg_buf[i] == '\\') {
            json_buf[jpos++] = '\\';
        }
        json_buf[jpos++] = msg_buf[i];
    }

    if (jpos < (int)sizeof(json_buf) - 2) {
        json_buf[jpos++] = '"';
        json_buf[jpos++] = '}';
        json_buf[jpos]   = '\0';
        sensorBoard_comm_send_json(json_buf);
    }

    return msg_len;
}
```

- [ ] **Step 2: Verificar que compila**

```
cd SensorBoard && idf.py build
```

- [ ] **Step 3: Commit**

```
git add Firmware/SensorBoard/components/usb_comm/sensorBoard_comm.c
git commit -m "feat(sensorboard): ESP_LOG interceptor emits JSON frames"
```

---

## Task 9: RX task

**Files:**
- Modify: `SensorBoard/components/usb_comm/sensorBoard_comm.c`

- [ ] **Step 1: Añadir callback del frame completo y RX task**

Añadir antes de `sensorBoard_comm_init`:

```c
/* ── RX frame callback (called by decoder when frame is complete) ── */
static void on_frame_received(uint8_t type, const uint8_t *payload, size_t len, void *ctx)
{
    (void)ctx;
    if (type == SB_PROTO_TYPE_JSON) {
        sensorBoard_cmd_handle(payload, len);
    }
}
```

Añadir la RX task (antes del `init`):

```c
/* ── RX task ───────────────────────────────────────────────── */
static void usb_rx_task(void *arg)
{
    (void)arg;

    static uint8_t rx_buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE];
    static uint8_t frame_payload_buf[SB_PROTO_MAX_JSON_PAYLOAD];
    sb_frame_dec_t decoder;
    sb_frame_dec_init(&decoder, frame_payload_buf, sizeof(frame_payload_buf));

    for (;;) {
        xSemaphoreTake(s_rx_sem, portMAX_DELAY);

        size_t   rx_size = 0;
        esp_err_t ret = tinyusb_cdcacm_read(TINYUSB_CDC_ACM_0,
                                             rx_buf, sizeof(rx_buf), &rx_size);
        if (ret == ESP_OK && rx_size > 0) {
            for (size_t i = 0; i < rx_size; i++) {
                sb_frame_dec_feed(&decoder, rx_buf[i], on_frame_received, NULL);
            }
        }
    }
}
```

Actualizar `sensorBoard_cmd_handler.c` para exponer la función:

```c
/* sensorBoard_cmd_handler.c — stub actualizado */
#include "sensorBoard_comm.h"
#include <stdint.h>
#include <stddef.h>

void sensorBoard_cmd_handle(const uint8_t *payload, size_t len)
{
    (void)payload; (void)len;
}
```

Actualizar `sensorBoard_comm.h` para declarar `sensorBoard_cmd_handle`:

```c
#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

esp_err_t sensorBoard_comm_init(void);
esp_err_t sensorBoard_comm_send_json(const char *json_str);
esp_err_t sensorBoard_comm_send_binary(uint8_t type, uint8_t *buf, size_t len);

/* Called internally by RX task; implemented in sensorBoard_cmd_handler.c */
void sensorBoard_cmd_handle(const uint8_t *payload, size_t len);
```

- [ ] **Step 2: Verificar que compila**

```
cd SensorBoard && idf.py build
```

- [ ] **Step 3: Commit**

```
git add Firmware/SensorBoard/
git commit -m "feat(sensorboard): RX task with frame decoder wired to cmd handler"
```

---

## Task 10: Command handler — status

**Files:**
- Modify: `SensorBoard/components/usb_comm/sensorBoard_cmd_handler.c`

- [ ] **Step 1: Implementar `sensorBoard_cmd_handler.c` completo**

```c
#include "sensorBoard_comm.h"
#include "sensorBoard_comm_protocol.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "CMD";

/* ── Helpers ───────────────────────────────────────────────── */
static uint32_t get_id(const cJSON *root)
{
    cJSON *id = cJSON_GetObjectItemCaseSensitive(root, SB_JSON_ID);
    return (id && cJSON_IsNumber(id)) ? (uint32_t)id->valuedouble : 0;
}

static void send_error(const char *cmd_str, uint32_t id, const char *msg)
{
    char buf[SB_PROTO_MAX_JSON_PAYLOAD];
    uint32_t ts = (uint32_t)(esp_timer_get_time() / 1000);
    snprintf(buf, sizeof(buf),
             "{\"type\":\"resp\",\"cmd\":\"%s\",\"id\":%lu,"
             "\"status\":\"error\",\"msg\":\"%s\",\"ts\":%lu}",
             cmd_str, (unsigned long)id, msg, (unsigned long)ts);
    sensorBoard_comm_send_json(buf);
}

/* ── Command: status ───────────────────────────────────────── */
static void handle_status(uint32_t id)
{
    char buf[SB_PROTO_MAX_JSON_PAYLOAD];
    uint32_t uptime_ms = (uint32_t)(esp_timer_get_time() / 1000);
    snprintf(buf, sizeof(buf),
             "{\"type\":\"resp\",\"cmd\":\"status\",\"id\":%lu,"
             "\"status\":\"ok\",\"device\":\"" SB_PROTO_DEVICE_NAME "\","
             "\"fw\":\"" SB_PROTO_FW_VERSION "\",\"uptime\":%lu}",
             (unsigned long)id, (unsigned long)uptime_ms);
    sensorBoard_comm_send_json(buf);
}

/* ── Dispatcher ────────────────────────────────────────────── */
void sensorBoard_cmd_handle(const uint8_t *payload, size_t len)
{
    /* Null-terminate for cJSON */
    char *json_str = strndup((const char *)payload, len);
    if (!json_str) return;

    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    if (!root) {
        ESP_LOGW(TAG, "JSON parse error");
        return;
    }

    cJSON *cmd = cJSON_GetObjectItemCaseSensitive(root, SB_JSON_CMD);
    uint32_t id = get_id(root);

    if (!cmd || !cJSON_IsString(cmd)) {
        send_error("?", id, "missing cmd field");
    } else if (strcmp(cmd->valuestring, SB_CMD_STATUS) == 0) {
        handle_status(id);
    } else {
        ESP_LOGW(TAG, "Unknown cmd: %s", cmd->valuestring);
        send_error(cmd->valuestring, id, "cmd not found");
    }

    cJSON_Delete(root);
}
```

- [ ] **Step 2: Verificar que compila**

```
cd SensorBoard && idf.py build
```

- [ ] **Step 3: Commit**

```
git add Firmware/SensorBoard/components/usb_comm/sensorBoard_cmd_handler.c
git commit -m "feat(sensorboard): command handler — status response and unknown cmd error"
```

---

## Task 11: main.c final + flash de integración

**Files:**
- Modify: `SensorBoard/main/main.c`

- [ ] **Step 1: Actualizar `main.c` con boot log completo**

```c
#include "esp_log.h"
#include "esp_timer.h"
#include "sensorBoard_comm.h"
#include "sensorBoard_comm_protocol.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "SensorBoard v%s booting", SB_PROTO_FW_VERSION);
    ESP_ERROR_CHECK(sensorBoard_comm_init());
    ESP_LOGI(TAG, "SensorBoard ready — USB CDC active");

    /* Heartbeat cada 30s para confirmar que el firmware está vivo */
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        char buf[128];
        uint32_t uptime = (uint32_t)(esp_timer_get_time() / 1000);
        snprintf(buf, sizeof(buf),
                 "{\"type\":\"event\",\"cmd\":\"heartbeat\",\"uptime\":%lu}",
                 (unsigned long)uptime);
        sensorBoard_comm_send_json(buf);
    }
}
```

- [ ] **Step 2: Build final**

```
cd SensorBoard && idf.py build
```

Resultado esperado: `Project build complete.` Sin warnings relevantes.

- [ ] **Step 3: Flash en el dispositivo**

```
idf.py -p COMx flash
```

- [ ] **Step 4: Verificar criterios de éxito**

Conectar la motherboard (o un PC con monitor serie) al USB del SensorBoard y verificar:

**a) Arranque — aparece frame JSON log:**
Abrir monitor serie en el puerto USB CDC del SensorBoard a cualquier baudrate.
Deben llegar bytes binarios. Decodificar el primer frame:
- Bytes `0xAB 0xCD` al inicio
- Tipo `0x00` (JSON)
- Payload: `{"type":"log",...,"msg":"SensorBoard v1.0.0 booting",...}`

**b) Comando status:**
Enviar el frame codificado de `{"type":"cmd","cmd":"status","id":1}`:

```python
# Script Python para generar el frame de prueba (ejecutar en PC)
import struct

def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = (crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1
        crc &= 0xFFFF
    return crc

payload = b'{"type":"cmd","cmd":"status","id":1}'
plen = len(payload)
header = bytes([0xAB, 0xCD, 0x00]) + struct.pack('<I', plen)
crc_input = header[2:] + payload
crc = crc16_ccitt(crc_input)
frame = header + payload + struct.pack('>H', crc)
print(frame.hex())
```

Enviar el `frame.hex()` (como bytes) al puerto serial. Respuesta esperada (dentro de un frame):
```json
{"type":"resp","cmd":"status","id":1,"status":"ok","device":"SensorBoard","fw":"1.0.0","uptime":XXXX}
```

**c) Comando desconocido:**
Enviar frame con payload `{"type":"cmd","cmd":"foo","id":2}`.
Respuesta esperada:
```json
{"type":"resp","cmd":"foo","id":2,"status":"error","msg":"cmd not found","ts":XXXX}
```

**d) Frame con CRC corrupto:**
Modificar el último byte del frame antes de enviarlo.
No debe llegar ninguna respuesta.

- [ ] **Step 5: Commit final**

```
git add Firmware/SensorBoard/main/main.c
git commit -m "feat(sensorboard): Phase 1 complete — USB CDC JSON protocol with status command"
```

---

## Criterios de éxito (checklist del spec)

- [ ] `idf.py build` sin errores
- [ ] SensorBoard aparece como dispositivo USB CDC
- [ ] `{"type":"cmd","cmd":"status","id":1}` devuelve respuesta con `"status":"ok"`
- [ ] Logs ESP_LOG llegan como frames JSON `"type":"log"`
- [ ] Comando desconocido devuelve `"status":"error"` con `"msg":"cmd not found"`
- [ ] Frame con CRC incorrecto es descartado sin respuesta
- [ ] Heartbeat `"type":"event","cmd":"heartbeat"` llega cada 30s
