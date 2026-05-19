# Arduino → ESP-IDF Port — Display_HMI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port Display_HMI from pioarduino Arduino framework to pure ESP-IDF so that `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=n` can be applied via sdkconfig, eliminating OPI PSRAM bus contention between WiFi and the RGB LCD DMA, and restoring `BOUNCE_BUF_LINES=20`.

**Architecture:** A new `[env:idf_main]` env is added to `platformio.ini` while the existing `[env:main]` (Arduino) stays as fallback. Files are ported module by module; after each task the IDF env must compile cleanly. Arduino-specific APIs are replaced with IDF-native equivalents; the EEPROM layer uses an NVS-backed compatibility shim so call sites are untouched. LVGL, FreeRTOS, ArduinoJson, and the LCD driver (`esp_lcd_*`) are already IDF-native and need no changes.

**Tech Stack:** ESP-IDF 5.3.x, PlatformIO espressif32 platform, LVGL 8.3, ThingsBoard SDK v0.13 (`Espressif_MQTT_Client`), esp_http_server, esp_ota_ops, nvs_flash, i2c_master (new API), esp_wifi + esp_event.

---

## File map

| File | Action | What changes |
|---|---|---|
| `platformio.ini` | Modify | Add `[env:idf_main]` |
| `sdkconfig.defaults` | **Create** | PSRAM + WiFi SRAM config |
| `include/compat.h` | **Create** | `millis()`, `delay()`, `ESP.*` macros |
| `include/EEPROM.h` | Modify | Swap Arduino class for NVS shim |
| `src/EEPROM_IDF.cpp` | **Create** | NVS-backed EEPROM implementation |
| `src/EEPROM.cpp` | Keep (Arduino env only) | Guard with `#ifndef USE_IDF_EEPROM` |
| `src/main.cpp` | Modify | `setup()/loop()` → `app_main()`, Serial → ESP_LOG, Preferences → NVS |
| `include/main.h` | Modify | Guard Arduino includes with `#ifndef ESP_PLATFORM_IDF_ONLY` |
| `src/Wifi_OTA.cpp` | Modify | Arduino WiFi/WebServer/OTA/mDNS → IDF equivalents |
| `include/Wifi_OTA.h` | Modify | Remove Arduino WiFi/MQTT headers; add IDF headers |
| `src/UITask.cpp` | Modify | Wire I2C → i2c_master; String → std::string/char*; TAMC_GT911 → IDF driver |
| `src/buzzer.cpp` | Modify | Wire I2C → i2c_master |
| `src/CommTask.cpp` | Modify | COMM_SERIAL → ESP_LOG; EEPROM.commit guards |
| `src/gt911_idf.cpp` | **Create** | Minimal IDF GT911 touch driver (~80 lines) |
| `include/gt911_idf.h` | **Create** | GT911 IDF driver header |

---

## Task 1 — IDF environment + sdkconfig.defaults

**Files:**
- Modify: `platformio.ini`
- Create: `sdkconfig.defaults`

- [ ] **1.1 Add `[env:idf_main]` to `platformio.ini`**

```ini
; -------------------------------------------------------
; IDF-native build — enables CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=n
; -------------------------------------------------------
[env:idf_main]
platform = espressif32
board = esp32-s3-devkitc-1
framework = espidf

board_build.f_flash = 80000000L
board_build.flash_mode = qio
board_upload.flash_size = 16MB
board_build.partitions = IncuNest_display_v1_audio.csv
monitor_speed = 115200

build_flags =
    -D LV_LVGL_H_INCLUDE_SIMPLE
    -I./include
    -D BOARD_HAS_PSRAM
    -D CORE_DEBUG_LEVEL=3
    -D USE_IDF_FRAMEWORK

lib_deps =
    maxpromer/PCA9557-arduino@^1.0.0
    lvgl/lvgl@^8.3.11
    https://github.com/thingsboard/thingsboard-arduino-sdk.git#v0.13.0
    https://github.com/bblanchon/ArduinoStreamUtils.git
    bblanchon/ArduinoJson@6.21.5
    thingsboard/TBPubSubClient@2.9.4

lib_ignore = ArduinoHttpClient ArduinoMqttClient
```

- [ ] **1.2 Create `sdkconfig.defaults` in project root**

```
# OPI PSRAM (ESP32-S3 octal SPI, 80 MHz)
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_BOOT_INIT=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=4096
CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=n

# Flash
CONFIG_ESPTOOLPY_FLASHFREQ_80M=y
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y

# WiFi
CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=8
CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=32
CONFIG_ESP_WIFI_NVS_ENABLED=y

# Logging
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
CONFIG_LOG_MAXIMUM_LEVEL_DEBUG=y

# FreeRTOS
CONFIG_FREERTOS_HZ=1000
CONFIG_FREERTOS_UNICORE=n

# NVS
CONFIG_NVS_ENCRYPTION=n
```

- [ ] **1.3 Verify bare build succeeds (even with compile errors — just confirm PlatformIO picks up the env)**

```
cd Firmware/Display_HMI
pio run -e idf_main 2>&1 | head -40
```

Expected: IDF build starts, sdkconfig is generated, compilation begins (errors are expected at this stage).

- [ ] **1.4 Commit**

```bash
git add platformio.ini sdkconfig.defaults
git commit -m "build: add idf_main env with sdkconfig.defaults for SPIRAM_TRY_ALLOCATE_WIFI_LWIP=n"
```

---

## Task 2 — Compatibility shims (`compat.h`)

**Files:**
- Create: `include/compat.h`

This header provides drop-in replacements for Arduino runtime functions so the rest of the code compiles without changes.

- [ ] **2.1 Create `include/compat.h`**

```cpp
#pragma once
#ifdef USE_IDF_FRAMEWORK

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include <cstring>
#include <string>

// millis() / micros()
static inline uint32_t millis() {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}
static inline uint32_t micros() {
    return (uint32_t)(esp_timer_get_time());
}

// delay()
static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

// ESP.* equivalents
static inline void esp_arduino_restart() { esp_restart(); }
static inline uint32_t esp_get_free_heap()  {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
}
static inline uint32_t esp_get_psram_size() {
    return (uint32_t)heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
}
static inline bool psramFound() {
    return esp_psram_is_initialized();
}

// abs() — already in <cstdlib>, but guard for C context
#ifndef __cplusplus
#include <stdlib.h>
#endif

#endif // USE_IDF_FRAMEWORK
```

- [ ] **2.2 Add `#include "compat.h"` at top of `include/main.h` (inside `#ifdef USE_IDF_FRAMEWORK` guard)**

In `include/main.h`, find the includes block and add:
```cpp
#ifdef USE_IDF_FRAMEWORK
#include "compat.h"
#endif
```

- [ ] **2.3 Build — confirm no new errors introduced**

```
pio run -e idf_main 2>&1 | grep "error:" | head -20
```

- [ ] **2.4 Commit**

```bash
git add include/compat.h include/main.h
git commit -m "feat(idf): add compat.h shims for millis/delay/ESP.* in IDF mode"
```

---

## Task 3 — NVS-backed EEPROM shim

**Files:**
- Create: `src/EEPROM_IDF.cpp`
- Modify: `include/EEPROM.h` (guard original, add IDF class)

The shim stores the entire EEPROM content as a binary blob in NVS namespace `"eeprom"` key `"data"` — the same layout as the Arduino ESP32 EEPROM library. **Existing stored data is automatically compatible.**

- [ ] **3.1 Create `src/EEPROM_IDF.cpp`**

```cpp
#ifdef USE_IDF_FRAMEWORK
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <cstring>
#include <string>

static const char *TAG = "EEPROM_IDF";
static const char *NVS_NS  = "eeprom";
static const char *NVS_KEY = "data";

// ---- NVS-backed EEPROM implementation ----
class EEPROM_IDF_Class {
    uint8_t  *_buf  = nullptr;
    size_t    _size = 0;
    bool      _dirty = false;

public:
    bool begin(size_t size) {
        _size = size;
        if (_buf) delete[] _buf;
        _buf = new uint8_t[size]();

        esp_err_t err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
            err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            nvs_flash_erase();
            nvs_flash_init();
        }

        nvs_handle_t h;
        if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
            size_t len = size;
            nvs_get_blob(h, NVS_KEY, _buf, &len);
            nvs_close(h);
        }
        return true;
    }

    uint8_t read(int addr) const {
        if (!_buf || addr < 0 || (size_t)addr >= _size) return 0xFF;
        return _buf[addr];
    }

    void write(int addr, uint8_t val) {
        if (!_buf || addr < 0 || (size_t)addr >= _size) return;
        if (_buf[addr] != val) { _buf[addr] = val; _dirty = true; }
    }

    bool commit() {
        if (!_dirty) return true;
        nvs_handle_t h;
        if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
        esp_err_t e = nvs_set_blob(h, NVS_KEY, _buf, _size);
        if (e == ESP_OK) e = nvs_commit(h);
        nvs_close(h);
        _dirty = (e != ESP_OK);
        return (e == ESP_OK);
    }

    // Typed accessors
    float readFloat(int addr) const {
        float v; memcpy(&v, _buf + addr, sizeof(v)); return v;
    }
    void writeFloat(int addr, float v) {
        if (memcmp(_buf + addr, &v, sizeof(v)) != 0) {
            memcpy(_buf + addr, &v, sizeof(v)); _dirty = true;
        }
    }
    int readInt(int addr) const {
        int v; memcpy(&v, _buf + addr, sizeof(v)); return v;
    }
    void writeInt(int addr, int v) {
        if (memcmp(_buf + addr, &v, sizeof(v)) != 0) {
            memcpy(_buf + addr, &v, sizeof(v)); _dirty = true;
        }
    }
    uint16_t readUShort(int addr) const {
        uint16_t v; memcpy(&v, _buf + addr, sizeof(v)); return v;
    }
    void writeUShort(int addr, uint16_t v) {
        if (memcmp(_buf + addr, &v, sizeof(v)) != 0) {
            memcpy(_buf + addr, &v, sizeof(v)); _dirty = true;
        }
    }
    std::string readString(int addr) const {
        return std::string(reinterpret_cast<const char*>(_buf + addr));
    }
    void writeString(int addr, const std::string& s) {
        size_t len = s.size() + 1;
        if (memcmp(_buf + addr, s.c_str(), len) != 0) {
            memcpy(_buf + addr, s.c_str(), len); _dirty = true;
        }
    }

    // Arduino-compat put/get (for UITask lcd_set_freq_write)
    template<typename T>
    void put(int addr, const T& v) {
        if (memcmp(_buf + addr, &v, sizeof(v)) != 0) {
            memcpy(_buf + addr, &v, sizeof(v)); _dirty = true;
        }
    }
    template<typename T>
    void get(int addr, T& v) {
        memcpy(&v, _buf + addr, sizeof(v));
    }
};

EEPROM_IDF_Class EEPROM;
#endif // USE_IDF_FRAMEWORK
```

- [ ] **3.2 Guard `include/EEPROM.h` — conditionally include shim or original**

Wrap the existing `#include <EEPROM.h>` at the top of `include/main.h`:
```cpp
#ifdef USE_IDF_FRAMEWORK
// EEPROM_IDF.cpp provides the EEPROM global using NVS
class EEPROM_IDF_Class; // forward declaration
extern EEPROM_IDF_Class EEPROM;
#else
#include <EEPROM.h>
#endif
```

- [ ] **3.3 Guard `src/EEPROM.cpp` so it only compiles in Arduino mode**

Add at the very top of `src/EEPROM.cpp`:
```cpp
#ifndef USE_IDF_FRAMEWORK
```
And at the very bottom:
```cpp
#endif // USE_IDF_FRAMEWORK
```

- [ ] **3.4 Replace `Preferences` usage in `src/main.cpp`**

Find the Preferences block (lines 87-98) and replace:
```cpp
// BEFORE:
Preferences p;
p.begin("diag", false);
g_hmiBootCount = p.getUInt("boots", 0) + 1;
p.putUInt("boots", g_hmiBootCount);
// ...
p.end();

// AFTER (IDF):
#ifdef USE_IDF_FRAMEWORK
nvs_flash_init();
nvs_handle_t diag_h;
nvs_open("diag", NVS_READWRITE, &diag_h);
nvs_get_u32(diag_h, "boots", &g_hmiBootCount);
g_hmiBootCount++;
nvs_set_u32(diag_h, "boots", g_hmiBootCount);
esp_reset_reason_t rst = esp_reset_reason();
g_hmiLastRst = (int)rst;
nvs_set_i32(diag_h, "last_rst", g_hmiLastRst);
nvs_commit(diag_h);
nvs_close(diag_h);
g_hmiRestoreState = (rst != ESP_RST_POWERON && rst != ESP_RST_BROWNOUT);
#else
Preferences p;
// ... original code ...
#endif
```

- [ ] **3.5 Add required IDF includes to `src/main.cpp`**

```cpp
#ifdef USE_IDF_FRAMEWORK
#include "nvs_flash.h"
#include "nvs.h"
#endif
```

- [ ] **3.6 Build — verify EEPROM-related errors are resolved**

```
pio run -e idf_main 2>&1 | grep "error:" | grep -i eeprom
```

Expected: No EEPROM errors.

- [ ] **3.7 Commit**

```bash
git add src/EEPROM_IDF.cpp src/EEPROM.cpp include/main.h src/main.cpp
git commit -m "feat(idf): NVS-backed EEPROM shim compatible with existing stored data"
```

---

## Task 4 — I2C: Wire → i2c_master

**Files:**
- Create: `include/i2c_bus.h`
- Create: `src/i2c_bus.cpp`
- Modify: `src/UITask.cpp` (backlight I2C calls)
- Modify: `src/buzzer.cpp`
- Modify: `src/main.cpp` (Wire.begin)

The `i2c_master` new API (IDF 5.x) replaces `Wire`. We create a thin bus singleton.

- [ ] **4.1 Create `include/i2c_bus.h`**

```cpp
#pragma once
#ifdef USE_IDF_FRAMEWORK
#include "driver/i2c_master.h"

// Shared I2C master bus on SDA=TOUCH_SDA_PIN, SCL=TOUCH_SCL_PIN
// Call i2c_bus_init() once in main/setup.
// Then use i2c_bus_write_byte(addr, cmd) for simple single-byte writes.

void i2c_bus_init(int sda_pin, int scl_pin);
esp_err_t i2c_bus_write(uint8_t dev_addr, const uint8_t *data, size_t len);
esp_err_t i2c_bus_write_byte(uint8_t dev_addr, uint8_t cmd);
esp_err_t i2c_bus_read(uint8_t dev_addr, uint8_t reg,
                        uint8_t *out, size_t len);
i2c_master_bus_handle_t i2c_bus_get_handle();
#endif
```

- [ ] **4.2 Create `src/i2c_bus.cpp`**

```cpp
#ifdef USE_IDF_FRAMEWORK
#include "i2c_bus.h"
#include "esp_log.h"

static const char *TAG = "I2C_BUS";
static i2c_master_bus_handle_t s_bus = NULL;

void i2c_bus_init(int sda_pin, int scl_pin) {
    if (s_bus) return; // already initialized
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = (gpio_num_t)sda_pin,
        .scl_io_num = (gpio_num_t)scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = { .enable_internal_pullup = true },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &s_bus));
    ESP_LOGI(TAG, "I2C master initialized sda=%d scl=%d", sda_pin, scl_pin);
}

i2c_master_bus_handle_t i2c_bus_get_handle() { return s_bus; }

esp_err_t i2c_bus_write(uint8_t dev_addr, const uint8_t *data, size_t len) {
    i2c_master_dev_handle_t dev;
    i2c_device_config_t dcfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = dev_addr,
        .scl_speed_hz    = 100000,
    };
    esp_err_t e = i2c_master_bus_add_device(s_bus, &dcfg, &dev);
    if (e != ESP_OK) return e;
    e = i2c_master_transmit(dev, data, len, 100);
    i2c_master_bus_rm_device(dev);
    return e;
}

esp_err_t i2c_bus_write_byte(uint8_t dev_addr, uint8_t cmd) {
    return i2c_bus_write(dev_addr, &cmd, 1);
}

esp_err_t i2c_bus_read(uint8_t dev_addr, uint8_t reg,
                        uint8_t *out, size_t len) {
    i2c_master_dev_handle_t dev;
    i2c_device_config_t dcfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = dev_addr,
        .scl_speed_hz    = 100000,
    };
    esp_err_t e = i2c_master_bus_add_device(s_bus, &dcfg, &dev);
    if (e != ESP_OK) return e;
    e = i2c_master_transmit_receive(dev, &reg, 1, out, len, 100);
    i2c_master_bus_rm_device(dev);
    return e;
}
#endif // USE_IDF_FRAMEWORK
```

- [ ] **4.3 Replace `Wire.begin()` in `src/main.cpp`**

```cpp
#ifdef USE_IDF_FRAMEWORK
#include "i2c_bus.h"
// replace: Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);
i2c_bus_init(TOUCH_SDA_PIN, TOUCH_SCL_PIN);
#else
Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);
#endif
```

- [ ] **4.4 Replace I2C calls in `src/UITask.cpp` (backlight control at 0x30)**

Search for the three blocks that do `Wire.beginTransmission` / `Wire.write` / `Wire.endTransmission` targeting `I2C_ADDR_BACKLIGHT`. Replace each block:

```cpp
// BEFORE:
Wire.beginTransmission(I2C_ADDR_BACKLIGHT);
Wire.write(cmd);
Wire.endTransmission();

// AFTER:
#ifdef USE_IDF_FRAMEWORK
i2c_bus_write_byte(I2C_ADDR_BACKLIGHT, cmd);
#else
Wire.beginTransmission(I2C_ADDR_BACKLIGHT);
Wire.write(cmd);
Wire.endTransmission();
#endif
```

- [ ] **4.5 Replace I2C calls in `src/buzzer.cpp`**

Same pattern as 4.4. Replace all `Wire.beginTransmission` / `Wire.write` / `Wire.endTransmission` with `i2c_bus_write_byte(addr, cmd)`.

- [ ] **4.6 Add `#include "i2c_bus.h"` to `src/UITask.cpp` and `src/buzzer.cpp` (inside IDF guard)**

```cpp
#ifdef USE_IDF_FRAMEWORK
#include "i2c_bus.h"
#endif
```

- [ ] **4.7 Build — verify no I2C/Wire errors**

```
pio run -e idf_main 2>&1 | grep "error:" | grep -i "wire\|i2c" | head -20
```

- [ ] **4.8 Commit**

```bash
git add include/i2c_bus.h src/i2c_bus.cpp src/UITask.cpp src/buzzer.cpp src/main.cpp
git commit -m "feat(idf): replace Wire/I2C with i2c_master IDF API"
```

---

## Task 5 — WiFi + event system

**Files:**
- Modify: `src/Wifi_OTA.cpp`
- Modify: `include/Wifi_OTA.h`

Replace Arduino `WiFi.*` API with `esp_wifi.h` + `esp_event.h`. Keep same external interface (`wifiInit()`, `WIFIIsConnected()`, etc.).

- [ ] **5.1 Add IDF WiFi headers to `include/Wifi_OTA.h`**

```cpp
#ifdef USE_IDF_FRAMEWORK
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mdns.h"
#include "lwip/ip4_addr.h"
#else
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiClient.h>
#endif
```

Remove `#include <WiFi.h>`, `#include <ESPmDNS.h>`, `#include <WiFiClient.h>` from the non-IDF section.

- [ ] **5.2 Replace `wifiInit()` body in `src/Wifi_OTA.cpp`**

```cpp
#ifdef USE_IDF_FRAMEWORK
// IDF WiFi init — called once from OTATask
static esp_netif_t *s_netif_sta = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        auto *d = (wifi_event_sta_disconnected_t *)data;
        ESP_LOGW(TAG, "STA_DISCONNECTED reason=%d", d->reason);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto *e = (ip_event_got_ip_t *)data;
        char ip_str[16];
        esp_ip4addr_ntoa(&e->ip_info.ip, ip_str, sizeof(ip_str));
        ESP_LOGW(TAG, "STA_GOT_IP: %s  [HEAP] internal=%u PSRAM=%u",
                 ip_str,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        mdns_init();
        mdns_hostname_set(wifiHost);
        if (pendingSSID[0] != '\0') s_persistCredentials = true;
    }
}

void wifiInit(void) {
    ESP_LOGI(TAG, "Initializing WiFi (IDF)");
    String hostname = String(WIFI_NAME) + "-" + String(in3.serialNumber);
    strncpy(wifiHost, hostname.c_str(), sizeof(wifiHost) - 1);

    ESP_LOGW(TAG, "[HEAP] before WiFi.begin — internal=%u PSRAM=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    if (!s_netif_sta) {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        s_netif_sta = esp_netif_create_default_wifi_sta();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                wifi_event_handler, NULL);

    esp_wifi_set_mode(WIFI_MODE_STA);

    // Set hostname
    esp_netif_set_hostname(s_netif_sta, wifiHost);

    // Credentials
    wifi_config_t wcfg = {};
    std::string ssid, pass;
    if (pendingSSID[0] != '\0') {
        ssid = pendingSSID;
        pass = pendingPass;
    } else {
        ssid = EEPROM.readString(EEPROM_WIFI_SSID);
        pass = EEPROM.readString(EEPROM_WIFI_PASSWORD);
        if (ssid.empty()) { ssid = WIFI_SSID; pass = WIFI_PASSWORD; }
    }
    strncpy((char*)wcfg.sta.ssid,     ssid.c_str(), sizeof(wcfg.sta.ssid)-1);
    strncpy((char*)wcfg.sta.password, pass.c_str(), sizeof(wcfg.sta.password)-1);
    wcfg.sta.pmf_cfg.capable  = true;
    wcfg.sta.pmf_cfg.required = false;

    esp_wifi_set_config(WIFI_IF_STA, &wcfg);
    esp_wifi_start();
    esp_wifi_connect();
    esp_wifi_set_ps(WIFI_PS_NONE);
}
#else
// original Arduino wifiInit() unchanged
#endif
```

- [ ] **5.3 Replace `WIFIIsConnected()` in `src/Wifi_OTA.cpp`**

```cpp
bool WIFIIsConnected() {
#ifdef USE_IDF_FRAMEWORK
    wifi_ap_record_t ap;
    return (esp_wifi_sta_get_ap_info(&ap) == ESP_OK);
#else
    return WiFi.status() == WL_CONNECTED;
#endif
}
```

- [ ] **5.4 Replace `WiFi.status()`/`WiFi.disconnect()`/`WiFi.SSID()` occurrences in `src/UITask.cpp`**

Search for all WiFi references in UITask.cpp and wrap with IDF guards:
```cpp
// WiFi.status() == WL_CONNECTED:
#ifdef USE_IDF_FRAMEWORK
    WIFIIsConnected()
#else
    (WiFi.status() == WL_CONNECTED)
#endif

// WiFi.disconnect():
#ifdef USE_IDF_FRAMEWORK
    esp_wifi_disconnect()
#else
    WiFi.disconnect()
#endif

// WiFi.SSID():
#ifdef USE_IDF_FRAMEWORK
    [get SSID from wcfg or store it in a static char[]]
#else
    WiFi.SSID().c_str()
#endif
```

For `WiFi.SSID()` in UITask.cpp, add a helper in Wifi_OTA.cpp:
```cpp
const char* wifi_get_ssid() {
#ifdef USE_IDF_FRAMEWORK
    wifi_config_t cfg;
    esp_wifi_get_config(WIFI_IF_STA, &cfg);
    static char buf[33];
    strncpy(buf, (char*)cfg.sta.ssid, sizeof(buf)-1);
    return buf;
#else
    static std::string s = WiFi.SSID().c_str();
    return s.c_str();
#endif
}
```

- [ ] **5.5 Build — verify WiFi section compiles**

```
pio run -e idf_main 2>&1 | grep "error:" | grep -iv "webserver\|update\|mdns\|thingsboard" | head -20
```

- [ ] **5.6 Commit**

```bash
git add src/Wifi_OTA.cpp include/Wifi_OTA.h src/UITask.cpp
git commit -m "feat(idf): replace Arduino WiFi API with esp_wifi + esp_event"
```

---

## Task 6 — WebServer → esp_http_server

**Files:**
- Modify: `src/Wifi_OTA.cpp` (configWifiServer, handleClient)
- Modify: `include/Wifi_OTA.h` (WebServer global → httpd_handle_t)

- [ ] **6.1 Replace WebServer global in `include/Wifi_OTA.h`**

```cpp
#ifdef USE_IDF_FRAMEWORK
#include "esp_http_server.h"
extern httpd_handle_t wifiServer;
#else
#include <WebServer.h>
extern WebServer wifiServer;
#endif
```

- [ ] **6.2 Replace `configWifiServer()` in `src/Wifi_OTA.cpp`**

```cpp
#ifdef USE_IDF_FRAMEWORK
httpd_handle_t wifiServer = NULL;

// Basic-auth helper
static bool check_auth(httpd_req_t *req) {
    char auth[128] = {};
    if (httpd_req_get_hdr_value_str(req, "Authorization",
                                     auth, sizeof(auth)) != ESP_OK) return false;
    // Expected: "Basic aW4zYWRtaW46c2F2aW5nbGl2ZXM=" (in3admin:savinglives)
    return (strstr(auth, "aW4zYWRtaW46c2F2aW5nbGl2ZXM=") != NULL);
}

static void send_auth_required(httpd_req_t *req) {
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_hdr(req, "WWW-Authenticate",
                       "Basic realm=\"IncuNest\"");
    httpd_resp_send(req, "Unauthorized", 12);
}

// GET / — serve OTA page
static esp_err_t root_get_handler(httpd_req_t *req) {
    if (!check_auth(req)) { send_auth_required(req); return ESP_OK; }
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, serverIndex, strlen(serverIndex));
    return ESP_OK;
}

// GET /fw_version
static esp_err_t fw_version_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, FWversion);
    return ESP_OK;
}

// GET /freq
static esp_err_t freq_get_handler(httpd_req_t *req) {
    if (!check_auth(req)) { send_auth_required(req); return ESP_OK; }
    char buf[32];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)lcd_get_freq_write());
    httpd_resp_set_type(req, "application/json");
    char json[64];
    snprintf(json, sizeof(json), "{\"freq\":%s}", buf);
    httpd_resp_sendstr(req, json);
    return ESP_OK;
}

// POST /freq
static esp_err_t freq_post_handler(httpd_req_t *req) {
    if (!check_auth(req)) { send_auth_required(req); return ESP_OK; }
    char body[64] = {};
    int len = httpd_req_recv(req, body, sizeof(body)-1);
    if (len > 0) {
        // body: "freq=18000000"
        char *val = strstr(body, "freq=");
        if (val) {
            uint32_t hz = (uint32_t)strtoul(val + 5, NULL, 10);
            if (hz >= 8000000 && hz <= 30000000) {
                lcd_set_freq_write(hz); // triggers restart internally
            }
        }
    }
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

// POST /update — OTA upload
static esp_ota_handle_t  s_ota_handle    = 0;
static const esp_partition_t *s_ota_part = NULL;
static bool s_ota_started = false;

static esp_err_t ota_post_handler(httpd_req_t *req) {
    if (!check_auth(req)) { send_auth_required(req); return ESP_OK; }

    char buf[1024];
    int remaining = req->content_len;
    s_ota_part = esp_ota_get_next_update_partition(NULL);
    ESP_ERROR_CHECK(esp_ota_begin(s_ota_part,
                                   OTA_WITH_SEQUENTIAL_WRITES,
                                   &s_ota_handle));
    s_ota_started = true;

    while (remaining > 0) {
        int recv = httpd_req_recv(req, buf,
                                   MIN((int)sizeof(buf), remaining));
        if (recv <= 0) {
            if (recv == HTTPD_SOCK_ERR_TIMEOUT) continue;
            esp_ota_abort(s_ota_handle);
            s_ota_started = false;
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                "Recv failed");
            return ESP_FAIL;
        }
        esp_ota_write(s_ota_handle, buf, recv);
        remaining -= recv;
    }

    esp_err_t e = esp_ota_end(s_ota_handle);
    s_ota_started = false;
    if (e != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "OTA invalid");
        return ESP_FAIL;
    }
    esp_ota_set_boot_partition(s_ota_part);
    httpd_resp_sendstr(req, "OK — rebooting");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

void configWifiServer() {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 8;
    if (httpd_start(&wifiServer, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    httpd_uri_t uris[] = {
        { "/",           HTTP_GET,  root_get_handler,  NULL },
        { "/fw_version", HTTP_GET,  fw_version_handler, NULL },
        { "/freq",       HTTP_GET,  freq_get_handler,  NULL },
        { "/freq",       HTTP_POST, freq_post_handler, NULL },
        { "/update",     HTTP_POST, ota_post_handler,  NULL },
    };
    for (auto &u : uris) httpd_register_uri_handler(wifiServer, &u);
    ESP_LOGI(TAG, "[WiFi] HTTP server started on port 80");
}

// handleClient is a no-op in IDF (httpd runs in its own task)
void handleClient() {}

#else
// original Arduino WebServer implementation unchanged
#endif
```

- [ ] **6.3 Remove `Update.*` calls from Arduino OTA block in `src/Wifi_OTA.cpp`**

Wrap the existing `Update.begin/write/end` code in `#ifndef USE_IDF_FRAMEWORK` ... `#endif`.

- [ ] **6.4 Build — verify HTTP server compiles**

```
pio run -e idf_main 2>&1 | grep "error:" | grep -iv "thingsboard\|mqtt" | head -20
```

- [ ] **6.5 Commit**

```bash
git add src/Wifi_OTA.cpp include/Wifi_OTA.h
git commit -m "feat(idf): replace WebServer + OTA Update with esp_http_server + esp_ota_ops"
```

---

## Task 7 — ThingsBoard: switch to Espressif_MQTT_Client

**Files:**
- Modify: `src/Wifi_OTA.cpp`
- Modify: `include/Wifi_OTA.h`

- [ ] **7.1 Replace client declarations in `include/Wifi_OTA.h`**

```cpp
#ifdef USE_IDF_FRAMEWORK
#include <Espressif_MQTT_Client.h>
extern Espressif_MQTT_Client mqttClientWIFI;
#else
#include <Arduino_MQTT_Client.h>
#include <WiFiClient.h>
extern WiFiClient espClient;
extern Arduino_MQTT_Client mqttClientWIFI;
#endif
```

- [ ] **7.2 Replace global declarations in `src/Wifi_OTA.cpp`**

```cpp
#ifdef USE_IDF_FRAMEWORK
Espressif_MQTT_Client mqttClientWIFI;
#else
WiFiClient espClient;
Arduino_MQTT_Client mqttClientWIFI(espClient);
#endif
ThingsBoard tb_wifi(mqttClientWIFI, MAX_MESSAGE_SIZE);
```

`ThingsBoard` constructor signature is the same for both client types — no other changes needed.

- [ ] **7.3 Build — verify ThingsBoard compiles with Espressif_MQTT_Client**

```
pio run -e idf_main 2>&1 | grep "error:" | grep -i "thingsboard\|mqtt" | head -20
```

- [ ] **7.4 Commit**

```bash
git add src/Wifi_OTA.cpp include/Wifi_OTA.h
git commit -m "feat(idf): switch ThingsBoard to Espressif_MQTT_Client (IDF native MQTT)"
```

---

## Task 8 — `main.cpp`: setup/loop → app_main

**Files:**
- Modify: `src/main.cpp`

- [ ] **8.1 Wrap `setup()` and `loop()` for IDF mode**

In `src/main.cpp`, wrap the entire file content:

```cpp
#ifdef USE_IDF_FRAMEWORK
extern "C" void app_main() {
    // --- contents of setup() ---
    // (remove Serial.begin — logging goes via ESP_LOG)

    nvs_flash_init(); // must be first for NVS (EEPROM + diag)

    // ... paste setup() body here, replacing:
    //   Serial.begin(SERIAL_BAUD)  → remove (ESP_LOG handles logging)
    //   Wire.begin(...)            → i2c_bus_init(...)
    //   delay(STARTUP_DELAY_MS)    → vTaskDelay(pdMS_TO_TICKS(STARTUP_DELAY_MS))
    //   Preferences block          → NVS block (already done in Task 3)

    // --- replace loop() ---
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(MS_PER_SECOND));
    }
}
#else
void setup() { /* original */ }
void loop()  { /* original */ }
#endif
```

- [ ] **8.2 Remove `Serial.begin()` from IDF path**

In IDF mode, all output goes through `ESP_LOG*` macros (already in use). `Serial.begin()` must not be called.

- [ ] **8.3 Build — full compile attempt, check remaining errors**

```
pio run -e idf_main 2>&1 | grep "error:" | head -30
```

- [ ] **8.4 Commit**

```bash
git add src/main.cpp
git commit -m "feat(idf): replace setup()/loop() with app_main() for ESP-IDF framework"
```

---

## Task 9 — GT911 touch driver (IDF I2C)

**Files:**
- Create: `src/gt911_idf.cpp`
- Create: `include/gt911_idf.h`
- Modify: `src/UITask.cpp` (replace TAMC_GT911 with IDF driver)

The GT911 communicates over I2C at 0x5D or 0x14. In the current config, INT and RST pins are -1 (polling mode only). The driver reads touch points from registers 0x814E–0x8157.

- [ ] **9.1 Create `include/gt911_idf.h`**

```cpp
#pragma once
#ifdef USE_IDF_FRAMEWORK
#include <stdint.h>
#include <stdbool.h>

#define GT911_ADDR       0x5D
#define GT911_REG_STATUS 0x814E
#define GT911_REG_PT1    0x8150

typedef struct {
    uint16_t x, y;
    bool     pressed;
} GT911_Touch_t;

void     gt911_init(uint8_t i2c_addr);
bool     gt911_read(GT911_Touch_t *out);   // returns true if touch present
#endif
```

- [ ] **9.2 Create `src/gt911_idf.cpp`**

```cpp
#ifdef USE_IDF_FRAMEWORK
#include "gt911_idf.h"
#include "i2c_bus.h"
#include "esp_log.h"

static const char *TAG = "GT911";
static uint8_t s_addr = GT911_ADDR;

static esp_err_t gt911_write_reg(uint16_t reg, uint8_t val) {
    uint8_t buf[3] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF), val };
    return i2c_bus_write(s_addr, buf, 3);
}

static esp_err_t gt911_read_reg(uint16_t reg, uint8_t *out, size_t len) {
    uint8_t addr[2] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF) };
    return i2c_bus_read(s_addr, 0, out, 0); // placeholder — see below
}

// GT911 needs 16-bit register address write then read
static esp_err_t gt911_read16(uint16_t reg, uint8_t *buf, size_t len) {
    uint8_t addr[2] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF) };
    // Write register address
    esp_err_t e = i2c_bus_write(s_addr, addr, 2);
    if (e != ESP_OK) return e;
    // Read data
    return i2c_bus_read(s_addr, 0xFF, buf, len); // 0xFF = skip re-write in i2c_bus_read
}

void gt911_init(uint8_t i2c_addr) {
    s_addr = i2c_addr;
    // Reset status register
    gt911_write_reg(GT911_REG_STATUS, 0);
    ESP_LOGI(TAG, "GT911 initialized at 0x%02X", i2c_addr);
}

bool gt911_read(GT911_Touch_t *out) {
    uint8_t status = 0;
    // Read status register (0x814E)
    uint8_t addr[2] = { 0x81, 0x4E };
    i2c_bus_write(s_addr, addr, 2);
    // Read 1 byte status
    i2c_master_dev_handle_t dev;
    // Use raw read: re-use i2c_bus internals
    // Simpler: inline the I2C transaction
    uint8_t rx[8] = {};
    esp_err_t e = i2c_bus_read(s_addr, 0xFF, rx, 1);
    if (e != ESP_OK || (rx[0] & 0x80) == 0) {
        out->pressed = false;
        return false;
    }
    uint8_t pts = rx[0] & 0x0F;
    if (pts == 0) {
        gt911_write_reg(GT911_REG_STATUS, 0);
        out->pressed = false;
        return false;
    }
    // Read point 1: 8 bytes starting at 0x8150
    uint8_t pt_addr[2] = { 0x81, 0x50 };
    i2c_bus_write(s_addr, pt_addr, 2);
    uint8_t pt[8] = {};
    i2c_bus_read(s_addr, 0xFF, pt, 8);

    out->x       = pt[1] | ((uint16_t)pt[2] << 8);
    out->y       = pt[3] | ((uint16_t)pt[4] << 8);
    out->pressed = true;

    // Clear status
    gt911_write_reg(GT911_REG_STATUS, 0);
    return true;
}
#endif // USE_IDF_FRAMEWORK
```

**Note:** `i2c_bus_read()` needs a small fix: when called with reg=0xFF, skip the register-write step and just read. Update `src/i2c_bus.cpp`:

```cpp
esp_err_t i2c_bus_read(uint8_t dev_addr, uint8_t reg,
                        uint8_t *out, size_t len) {
    // ... create dev handle ...
    esp_err_t e;
    if (reg == 0xFF) {
        // plain read, no register write
        e = i2c_master_receive(dev, out, len, 100);
    } else {
        e = i2c_master_transmit_receive(dev, &reg, 1, out, len, 100);
    }
    i2c_master_bus_rm_device(dev);
    return e;
}
```

- [ ] **9.3 Replace TAMC_GT911 usage in `src/UITask.cpp`**

Find `my_touchpad_read()` function and replace:

```cpp
// BEFORE:
static void my_touchpad_read(lv_indev_drv_t *indev_driver,
                             lv_indev_data_t *data) {
    ts.read();
    if (ts.isTouched) {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = ts.points[0].x;
        data->point.y = ts.points[0].y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// AFTER:
static void my_touchpad_read(lv_indev_drv_t *indev_driver,
                             lv_indev_data_t *data) {
#ifdef USE_IDF_FRAMEWORK
    GT911_Touch_t touch;
    if (gt911_read(&touch)) {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = touch.x;
        data->point.y = touch.y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
#else
    ts.read();
    if (ts.isTouched) {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = ts.points[0].x;
        data->point.y = ts.points[0].y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
#endif
}
```

Replace touch controller initialization (where `ts.begin()` is called):

```cpp
#ifdef USE_IDF_FRAMEWORK
    gt911_init(GT911_ADDR);
    ESP_LOGI(TAG, "[UI] Touch controller initialized OK (IDF GT911)");
#else
    ts.begin();
    ts.setRotation(DISPLAY_ROTATE_180 ? ROTATION_INVERTED : ROTATION_NORMAL);
    ESP_LOGI(TAG, "[UI] Touch controller initialized OK");
#endif
```

- [ ] **9.4 Add includes to UITask.cpp (IDF guard)**

```cpp
#ifdef USE_IDF_FRAMEWORK
#include "gt911_idf.h"
#endif
```

- [ ] **9.5 Build — verify touch driver compiles**

```
pio run -e idf_main 2>&1 | grep "error:" | head -20
```

- [ ] **9.6 Commit**

```bash
git add include/gt911_idf.h src/gt911_idf.cpp src/i2c_bus.cpp src/UITask.cpp
git commit -m "feat(idf): minimal GT911 touch driver using IDF i2c_master"
```

---

## Task 10 — String class → std::string / char*

**Files:**
- Modify: `src/Wifi_OTA.cpp` (main consumer of Arduino String)
- Modify: `src/EEPROM_IDF.cpp` (returns std::string)

The Arduino `String` class is not available in pure IDF. Replace with `std::string` (C++11, available everywhere).

- [ ] **10.1 Replace `String` usage in `src/Wifi_OTA.cpp` (IDF path only)**

Wrap with `#ifdef USE_IDF_FRAMEWORK`. Key replacements:

```cpp
// String(WIFI_NAME) + "-" + String(in3.serialNumber):
std::string hostname = std::string(WIFI_NAME) + "-" + std::to_string(in3.serialNumber);

// ssid.length() > 0:
!ssid.empty()

// String.c_str():
ssid.c_str()  // same for std::string

// String concatenation for serverIndex HTML:
// serverIndex is already a const char* — no change needed
```

- [ ] **10.2 Replace `String` in `src/CommTask.cpp` serial macro (IDF path)**

`COMM_SERIAL` macro → `ESP_LOGI(TAG, ...)` equivalent in IDF path. Wrap:

```cpp
#ifdef USE_IDF_FRAMEWORK
#define COMM_SERIAL_PRINT(msg)   ESP_LOGI("COMM", "%s", (msg))
#define COMM_SERIAL_PRINTF(...)  ESP_LOGI("COMM", __VA_ARGS__)
#else
#define COMM_SERIAL_PRINT(msg)   COMM_SERIAL.print(msg)
#define COMM_SERIAL_PRINTF(...)  COMM_SERIAL.printf(__VA_ARGS__)
#endif
```

- [ ] **10.3 Build — full clean compile check**

```
pio run -e idf_main 2>&1 | grep "error:" | head -30
```

At this point the build should be near-clean (only minor remaining issues).

- [ ] **10.4 Fix any remaining compile errors iteratively**

For each error: identify the Arduino-specific API, apply the appropriate IDF replacement pattern from tasks 2-9.

- [ ] **10.5 Commit when clean**

```bash
git add -A
git commit -m "feat(idf): replace Arduino String and Serial macros — IDF build clean"
```

---

## Task 11 — Restore BOUNCE_BUF_LINES=20 and verify sdkconfig

**Files:**
- Modify: `src/UITask.cpp`

- [ ] **11.1 Restore bounce buffer to 20 lines in IDF build**

```cpp
#ifdef USE_IDF_FRAMEWORK
#define BOUNCE_BUF_LINES 20   // full 64KB — safe: WiFi uses SRAM (not PSRAM)
#else
#define BOUNCE_BUF_LINES  8   // reduced for Arduino: WiFi shares PSRAM bus
#endif
#define BOUNCE_BUF_SIZE_PX (DISPLAY_WIDTH * BOUNCE_BUF_LINES)
```

- [ ] **11.2 Verify sdkconfig was applied: check build output for CONFIG value**

```
pio run -e idf_main 2>&1 | grep -i "SPIRAM_TRY_ALLOCATE"
```

Expected output should confirm the value is `n`.

Also check the generated sdkconfig in `.pio/build/idf_main/`:
```
grep SPIRAM_TRY_ALLOCATE .pio/build/idf_main/sdkconfig
```

Expected: `# CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP is not set`

- [ ] **11.3 Flash IDF build and read serial**

```
pio run -e idf_main -t upload && pio device monitor -e idf_main
```

Watch for:
- `[HEAP] before WiFi.begin — internal=...` (should be higher than Arduino build if WiFi now uses SRAM)
- `STA_GOT_IP` appearing quickly (WPA2 handshake succeeds)
- `[LCD] RGB panel initialized OK [HEAP] internal=...`
- No `AUTH_EXPIRE reason=2` or `BEACON_TIMEOUT`
- No LCD glitches (20-line bounce buffer, clean framerate)

- [ ] **11.4 Commit**

```bash
git add src/UITask.cpp
git commit -m "fix: restore BOUNCE_BUF_LINES=20 in IDF build — WiFi uses SRAM not PSRAM"
```

---

## Task 12 — Final verification and cleanup

- [ ] **12.1 Smoke test checklist on hardware**

- [ ] WiFi connects and maintains connection under LCD activity
- [ ] LCD runs at full 20-line bounce buffer, no glitches
- [ ] Touch works (GT911)
- [ ] Backlight I2C control works
- [ ] Buzzer works
- [ ] ThingsBoard MQTT connects and sends telemetry
- [ ] OTA web server accessible at `http://incunest_display-<sn>.local`
- [ ] OTA firmware update completes successfully
- [ ] Reboot restores all settings from NVS (EEPROM data intact)
- [ ] `[HEAP] before WiFi.begin` shows internal SRAM not depleted

- [ ] **12.2 Verify `[env:main]` (Arduino) still compiles**

```
pio run -e main 2>&1 | tail -5
```

Expected: `[SUCCESS]`

- [ ] **12.3 Remove heap log lines added for diagnosis (optional, or leave for monitoring)**

The three `[HEAP]` log points in `Wifi_OTA.cpp` and `UITask.cpp` are useful production telemetry. Decision: keep them at `ESP_LOGW` level.

- [ ] **12.4 Update `platformio.ini` default env to `idf_main`**

```ini
[platformio]
src_dir = src
boards_dir = .
default_envs = idf_main
```

- [ ] **12.5 Final commit**

```bash
git add platformio.ini
git commit -m "build: set idf_main as default env — Arduino port to ESP-IDF complete"
```

---

## Self-review

**Spec coverage:**
- [x] CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=n → Task 1 sdkconfig.defaults
- [x] BOUNCE_BUF_LINES=20 restored → Task 11
- [x] All Arduino APIs inventoried and ported → Tasks 2-10
- [x] ThingsBoard Espressif_MQTT_Client → Task 7
- [x] Arduino env preserved as fallback → all tasks use `#ifdef USE_IDF_FRAMEWORK`
- [x] NVS data backward-compatible with Arduino EEPROM blob → Task 3

**Risks to watch:**
- `i2c_bus_read()` with GT911's 16-bit register addressing may need iteration — the driver in Task 9 shows the pattern but the exact I2C transaction sequence depends on hardware behaviour. Verify with logic analyser if touch is erratic.
- `Espressif_MQTT_Client` connection URI format may differ from ThingsBoard's expected format. Check ThingsBoard SDK examples for IDF in `.pio/libdeps/idf_main/ThingsBoard/examples/`.
- `esp_netif_init()` and `esp_event_loop_create_default()` must be called exactly once. If another component calls them, use `esp_event_loop_create_default()` return value check.
