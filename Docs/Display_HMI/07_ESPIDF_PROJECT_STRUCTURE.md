# Estructura de Proyecto ESP-IDF Recomendada

## 1. Árbol de Directorios

```
incunest_hmi/
├── CMakeLists.txt                 # Raíz del proyecto
├── sdkconfig.defaults             # Configuración IDF por defecto
├── partitions.csv                 # Tabla de particiones flash (igual que v1_audio)
├── idf_component.yml              # Dependencias IDF Component Manager
│
├── main/
│   ├── CMakeLists.txt             # Componente principal
│   ├── main.c                     # Punto de entrada (app_main)
│   ├── main.h                     # Configuración global, constantes
│   │
│   ├── comm/
│   │   ├── comm_task.c            # Tarea CommTask: parse UART, envío HMI
│   │   ├── comm_task.h            # API pública y estructuras de datos
│   │   └── comm_protocol.h        # Constantes del protocolo ASCII
│   │
│   ├── ui/
│   │   ├── ui_task.c              # Tarea UITask: init display, touch, LVGL loop
│   │   ├── ui_task.h              # API pública UITask + LVGL mutex
│   │   ├── screens/
│   │   │   ├── screen_intro.c     # Pantalla splash logo
│   │   │   ├── screen_lock.c      # Pantalla bloqueo con progreso
│   │   │   ├── screen_main.c      # Dashboard principal
│   │   │   ├── screen_alarms.c    # Lista de alarmas activas
│   │   │   ├── screen_charts.c    # Gráficas históricas
│   │   │   ├── screen_pulseoxi.c  # Pulse oximetría
│   │   │   └── screen_settings.c  # Configuración del sistema
│   │   ├── widgets/
│   │   │   ├── alarm_panel.c      # Widget de panel de alarma
│   │   │   ├── temp_bar.c         # Widget barra de temperatura
│   │   │   └── chart_utils.c      # Utilidades para gráficas históricas
│   │   ├── ui_events.c            # Callbacks de eventos táctiles
│   │   ├── ui_update.c            # Actualización de labels y datos
│   │   └── ui_i18n.c              # Internacionalización (ES/EN/FR)
│   │
│   ├── drivers/
│   │   ├── display_driver.c       # Init esp_lcd_panel_rgb con bounce buffers
│   │   ├── display_driver.h       # API del driver de display
│   │   ├── touch_driver.c         # Init GT911 via esp_lcd_touch_gt911
│   │   ├── touch_driver.h         # API del driver de touch
│   │   ├── backlight.c            # Control backlight via I2C @0x30
│   │   └── backlight.h
│   │
│   ├── storage/
│   │   ├── nvs_storage.c          # Abstracción NVS (equivale a EEPROM actual)
│   │   └── nvs_storage.h          # Claves NVS y API
│   │
│   ├── ota/
│   │   ├── ota_task.c             # OTA WiFi via esp_http_server
│   │   └── ota_task.h
│   │
│   └── assets/                    # Imágenes LVGL (arrays C generados por LVGL Image Converter)
│       ├── img_incunest_logo.c
│       ├── img_lock_icon.c
│       ├── img_baby_icon.c
│       └── ...
│
└── components/
    └── lv_conf/
        └── lv_conf.h              # Configuración LVGL personalizada
```

---

## 2. CMakeLists.txt Raíz

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(incunest_hmi VERSION 3.0.0)
```

---

## 3. CMakeLists.txt del Componente Principal

```cmake
idf_component_register(
    SRCS
        "main.c"
        "comm/comm_task.c"
        "ui/ui_task.c"
        "ui/screens/screen_intro.c"
        "ui/screens/screen_lock.c"
        "ui/screens/screen_main.c"
        "ui/screens/screen_alarms.c"
        "ui/screens/screen_charts.c"
        "ui/screens/screen_pulseoxi.c"
        "ui/screens/screen_settings.c"
        "ui/widgets/alarm_panel.c"
        "ui/widgets/temp_bar.c"
        "ui/widgets/chart_utils.c"
        "ui/ui_events.c"
        "ui/ui_update.c"
        "ui/ui_i18n.c"
        "drivers/display_driver.c"
        "drivers/touch_driver.c"
        "drivers/backlight.c"
        "storage/nvs_storage.c"
        "ota/ota_task.c"
        "assets/img_incunest_logo.c"
        "assets/img_lock_icon.c"
        "assets/img_baby_icon.c"
    INCLUDE_DIRS
        "."
        "comm"
        "ui"
        "ui/screens"
        "ui/widgets"
        "drivers"
        "storage"
        "ota"
        "assets"
    REQUIRES
        esp_lcd
        esp_wifi
        esp_http_server
        esp_ota_ops
        nvs_flash
        driver
        freertos
        esp_log
        lvgl
        esp_lcd_touch_gt911
)
```

---

## 4. idf_component.yml

```yaml
## IDF Component Manager Manifest
dependencies:
  lvgl/lvgl:
    version: "^8.4.0"
    rules:
      - if: "idf_version >= 5.1"
  espressif/esp_lcd_touch_gt911:
    version: "^1.0.0"
  idf:
    version: ">=5.1.0"
```

---

## 5. Particiones (partitions.csv)

```csv
# Name,   Type, SubType, Offset,   Size,   Flags
nvs,      data, nvs,     0x9000,   0x5000,
otadata,  data, ota,     0xe000,   0x2000,
app0,     app,  ota_0,   0x10000,  0x300000,
app1,     app,  ota_1,   0x310000, 0x300000,
spiffs,   data, spiffs,  0x610000, 0x800000,
coredump, data, coredump,0xE10000, 0x10000,
```

---

## 6. sdkconfig.defaults

```ini
# ===== CPU / FLASH / PSRAM =====
CONFIG_ESP32S3_DEFAULT_CPU_FREQ_MHZ=240
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y
CONFIG_ESPTOOLPY_FLASHFREQ_80M=y

# ===== PSRAM OPI =====
CONFIG_SPIRAM_SUPPORT=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
# Importante: instrucciones y datos de solo lectura en PSRAM
# reduce presión sobre SRAM interna
CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y
CONFIG_SPIRAM_RODATA=y

# ===== FreeRTOS =====
CONFIG_FREERTOS_HZ=1000
# Dual core activo (Core 0: sistema, Core 1: app)
CONFIG_FREERTOS_UNICORE=n
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192

# ===== LCD RGB =====
# Reiniciar el panel RGB en el pulso VSYNC para evitar tearing
CONFIG_LCD_RGB_RESTART_IN_VSYNC=y

# ===== UART / USB CDC =====
CONFIG_ESP_CONSOLE_USB_CDC=y

# ===== OTA =====
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y

# ===== Logging =====
CONFIG_LOG_DEFAULT_LEVEL_INFO=y

# ===== Gestión de energía =====
# Deshabilitar PM para evitar interferencias con DMA del display
CONFIG_PM_ENABLE=n

# ===== Watchdog =====
CONFIG_ESP_TASK_WDT_TIMEOUT_S=10
CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0=n
CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1=n
```

---

## 7. Estructura de Tareas FreeRTOS Recomendada

El diseño propuesto **elimina el acceso directo LVGL desde CommTask** usando una cola de mensajes IPC:

```
app_main (Core 1)
  │
  ├── UITask          Core 1, Prio 5, Stack 20480  — ÚNICO escritor LVGL
  │   │                 Ejecuta lv_timer_handler() en bucle
  │   │                 Lee IPC queue → aplica actualizaciones UI
  │   └── Drivers: esp_lcd_panel_rgb, esp_lcd_touch_gt911
  │
  ├── CommTask        Core 1, Prio 4, Stack 8192   — SIN acceso LVGL directo
  │   │                 Parse UART → publica en IPC queue
  │   │                 Lee events queue → envía HMI,... via UART
  │   └── Serial (USB CDC)
  │
  └── OTA_Task        Core 0, Prio 3, Stack 8192   — Aislado en Core 0
      │                 WiFi AP + HTTP server
      │                 Solo interactúa con UITask via IPC queue (evento OTA_START)
      └── esp_wifi, esp_http_server
```

### 7.1 Mecanismo IPC — Cola de mensajes UI

```c
// En lugar de CommTask → LVGL_Lock() → lv_label_set_text():

// CommTask publica evento:
ui_event_t evt = {
    .type = UI_EVT_TELEMETRY,
    .air_temp_det = 35.2f,
    .skin_temp_det = 36.8f,
    .hum_det = 55
};
xQueueSend(g_ui_event_queue, &evt, 0);

// UITask consume en su bucle:
while (xQueueReceive(g_ui_event_queue, &evt, 0) == pdTRUE) {
    ui_apply_event(&evt);  // Llamadas LVGL en contexto UITask
}
lv_timer_handler();
vTaskDelay(pdMS_TO_TICKS(5));
```

### 7.2 Razón del cambio de arquitectura

| Problema actual | Solución IPC queue |
|---|---|
| CommTask (Prio 3) bloquea UITask (Prio 2) via mutex LVGL | CommTask solo hace `xQueueSend()` no bloqueante |
| Inversión de prioridad entre OTA (Prio 4) y CommTask (Prio 3) con mutex | Sin mutex compartido entre tareas |
| Touch no responde durante aplicación de estado completo | `ui_apply_event()` solo corre en UITask |

---

## 8. Plantilla main.c

```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "comm/comm_task.h"
#include "ui/ui_task.h"
#include "ota/ota_task.h"

static const char *TAG = "Main";

void app_main(void) {
    ESP_LOGI(TAG, "IncuNest HMI v3.0.0 starting...");

    // Inicializar NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Crear cola IPC UI
    ui_task_init();

    // Crear tareas
    xTaskCreatePinnedToCore(ui_task_fn,   "UITask",   20480, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(comm_task_fn, "CommTask",  8192, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(ota_task_fn,  "OTATask",   8192, NULL, 3, NULL, 0);
}
```
