# Análisis del Driver de Display y Touch

> Fuentes: `include/display_config.h`, `src/UITask.cpp:2800-2930`, `include/lv_conf.h`, `lib/TAMC_GT911_Fixed/`

## 1. Driver de Display Actual

### 1.1 Interfaz

El CrowPanel Advance 7" usa un bus RGB paralelo de 16 bits (RGB565) sin controlador de display externo. El ESP32-S3 genera directamente las señales de sincronización y datos para el panel LCD.

El driver se implementa usando el componente **`esp_lcd_panel_rgb`** de ESP-IDF, accedido a través de la capa Arduino-ESP32.

### 1.2 Configuración completa del panel RGB

```c
// src/UITask.cpp:2813
esp_lcd_rgb_panel_config_t panel_cfg = {};

panel_cfg.clk_src = LCD_CLK_SRC_DEFAULT;
panel_cfg.timings.pclk_hz = 15000000UL;   // 15 MHz (ajustable vía EEPROM)
panel_cfg.timings.h_res = 800;
panel_cfg.timings.v_res = 480;

// Sincronización horizontal
panel_cfg.timings.hsync_pulse_width = 4;
panel_cfg.timings.hsync_back_porch  = 8;
panel_cfg.timings.hsync_front_porch = 8;

// Sincronización vertical
panel_cfg.timings.vsync_pulse_width = 4;
panel_cfg.timings.vsync_back_porch  = 8;
panel_cfg.timings.vsync_front_porch = 8;

// Polaridades
panel_cfg.timings.flags.hsync_idle_low  = 0;  // HSYNC polarity 1 → idle high
panel_cfg.timings.flags.vsync_idle_low  = 0;  // VSYNC polarity 1 → idle high
panel_cfg.timings.flags.pclk_idle_high  = 1;
panel_cfg.timings.flags.pclk_active_neg = 1;  // Latch en flanco descendente
panel_cfg.timings.flags.de_idle_high    = 1;

// Memoria y buffers
panel_cfg.data_width = 16;              // RGB565
panel_cfg.num_fbs = 1;                  // 1 framebuffer
panel_cfg.bounce_buffer_size_px = 16000; // 20 líneas × 800px en SRAM
panel_cfg.psram_trans_align = 64;
panel_cfg.sram_trans_align = 4;
panel_cfg.flags.fb_in_psram = 1;        // Framebuffer en PSRAM
panel_cfg.flags.bb_invalidate_cache = 1;
```

### 1.3 Estrategia de Framebuffer y Bounce Buffers

```
┌─────────────────────────────────────────────────────────────────┐
│                     ARQUITECTURA DE MEMORIA                     │
│                                                                 │
│  PSRAM (8MB)                  SRAM Interna                     │
│  ┌────────────────────┐      ┌───────────────┐                 │
│  │   Framebuffer RGB  │      │  Bounce Buf 0 │                 │
│  │   800×480×2 bytes  │      │  800×20×2=32KB│                 │
│  │   = 768 KB         │      ├───────────────┤                 │
│  │                    │◄─DMA─│  Bounce Buf 1 │                 │
│  │   (fb_in_psram=1)  │      │  800×20×2=32KB│                 │
│  └────────────────────┘      └───────────────┘                 │
│                                                                 │
│  SRAM Interna                                                   │
│  ┌────────────────────┐                                         │
│  │   LVGL Draw Buf    │                                         │
│  │   800×480/16×2=48KB│  ← COLOR_DIVISOR=16                    │
│  │   (MALLOC_CAP_8BIT)│                                         │
│  └────────────────────┘                                         │
└─────────────────────────────────────────────────────────────────┘
```

Los **bounce buffers** (2 × 20 líneas × 800 px = 64 KB SRAM) desacoplan el DMA del LCD del acceso a la PSRAM, eliminando el "tearing" y los parpadeos durante transferencias DMA.

### 1.4 Pines RGB (definidos en `include/display_config.h`)

| Canal | GPIO | Canal | GPIO |
|---|---|---|---|
| B0 (LSB) | 21 | G0 (LSB) | 9 |
| B1 | 47 | G1 | 10 |
| B2 | 48 | G2 | 11 |
| B3 | 45 | G3 | 12 |
| B4 (MSB) | 38 | G4 | 13 |
| R0 (LSB) | 7 | G5 (MSB) | 14 |
| R1 | 17 | HSYNC | 40 |
| R2 | 18 | VSYNC | 41 |
| R3 | 3 | DE | 42 |
| R4 (MSB) | 46 | PCLK | 39 |

### 1.5 Pixel Clock — Historial de cambios

La frecuencia del pixel clock ha evolucionado durante el desarrollo:

| Versión | Frecuencia | Motivo |
|---|---|---|
| Original (Elecrow Advance) | 21 MHz | Factory code oficial |
| Reducido para estabilidad | 18 MHz | Primer ajuste |
| Actual producción | 15 MHz | Conflicto I2S audio DMA |
| Mínimo admitido | 12 MHz | `DISPLAY_FREQ_MIN` |

> **Riesgo**: A 15 MHz, el frame rate visual baja a ~16 fps teórico. La reducción fue necesaria para eliminar el conflicto entre el DMA del bus RGB y el DMA del I2S (audio).

### 1.6 Inconsistencia de pines — Problema crítico

Existe una discrepancia entre dos archivos del mismo proyecto:

| Señal | `display_config.h` (CORRECTO) | `main.h` (OBSOLETO) |
|---|---|---|
| HSYNC | GPIO 40 | GPIO 39 |
| VSYNC | GPIO 41 | GPIO 40 |
| DE | GPIO 42 | GPIO 41 (`PIN_HENABLE`) |
| PCLK | GPIO 39 | GPIO 42 |

El archivo `main.h` contiene los valores originales invertidos que ya fueron corregidos en `display_config.h` v2.7. La nueva implementación ESP-IDF debe usar **exclusivamente** los valores de `display_config.h`.

---

## 2. Driver del Touch Controller (GT911)

### 2.1 Librería actual

Se usa la librería local `TAMC_GT911_Fixed` (en `lib/TAMC_GT911_Fixed/`), que es un fork corregido de la librería Arduino de tamc-electronics para el controlador táctil Goodix GT911.

**Inicialización** (ver `src/UITask.cpp:170`):
```cpp
TAMC_GT911 ts = TAMC_GT911(
    DISPLAY_TOUCH_SDA,  // GPIO 15
    DISPLAY_TOUCH_SCL,  // GPIO 16
    DISPLAY_TOUCH_INT,  // -1 (sin interrupción)
    DISPLAY_TOUCH_RST,  // -1 (reset vía STC8)
    DISPLAY_WIDTH,      // 800
    DISPLAY_HEIGHT      // 480
);
```

**Lectura en callback LVGL** (`src/UITask.cpp:204`):
```cpp
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    ts.read();
    if (ts.isTouched) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = ts.points[0].x;
        data->point.y = ts.points[0].y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}
```

### 2.2 Rotación del touch

Controlada por la macro `DISPLAY_ROTATE_180` en `display_config.h`:

| `DISPLAY_ROTATE_180` | `TOUCH_ROTATION` | `esp_lcd_panel_mirror()` |
|---|---|---|
| 0 (actual) | 1 | `mirror(false, false)` |
| 1 | 3 | `mirror(true, true)` |

> **Nota**: El reset del GT911 no está conectado al ESP32-S3 directamente — se gestiona via el STC8H1K28. El `DISPLAY_TOUCH_RST = -1` suprime la secuencia de reset hardware, que en alguna PCB causa que el touch no responda tras cold boot. Ver `src/UITask.cpp:2887` — hay 3 reintentos de inicialización con 500ms de espera.

---

## 3. Configuración LVGL Actual

### 3.1 Parámetros principales

| Parámetro | Valor en producción |
|---|---|
| Versión LVGL | 8.3.11 |
| `LV_COLOR_DEPTH` | 16 (RGB565) |
| `LV_COLOR_16_SWAP` | 0 |
| `LV_DISP_DEF_REFR_PERIOD` | 30 ms |
| `LV_INDEV_DEF_READ_PERIOD` | 30 ms |
| `LV_TICK_CUSTOM` | 1 (usa `millis()`) |
| `LV_MEM_CUSTOM` | 1 |
| `LV_MEM_CUSTOM_ALLOC` | `heap_caps_malloc(size, MALLOC_CAP_8BIT)` |
| `LV_MEM_BUF_MAX_NUM` | 64 |
| `LV_USE_LOG` | 0 (deshabilitado) |
| `LV_ASSERT_HANDLER` | `while(1)` — **CAUSA WATCHDOG RESET** |

### 3.2 Problema crítico: memoria LVGL en SRAM interna

El `lv_conf.h` actual asigna la memoria dinámica de LVGL en **SRAM interna** (`MALLOC_CAP_8BIT`), no en PSRAM:

```c
// include/lv_conf.h:68
#define LV_MEM_CUSTOM_ALLOC(size) heap_caps_malloc(size, MALLOC_CAP_8BIT)
```

El fragmento comentado (líneas 64-66) habría usado PSRAM (`MALLOC_CAP_SPIRAM`). La versión activa usa SRAM, lo cual limita la memoria disponible para objetos LVGL y puede causar fallos de asignación.

### 3.3 Tamaño del buffer de dibujo

```cpp
// src/UITask.cpp:177
static lv_color_t disp_draw_buf[DISPLAY_WIDTH * DISPLAY_HEIGHT / COLOR_DIVISOR];
// = 800 * 480 / 16 = 24000 pixels = 48 KB
```

Esto es ~6.25% del tamaño total del frame (768 KB). Cada "flush" de LVGL transfiere este buffer al framebuffer en PSRAM.

**Valor recomendado**: `DISPLAY_WIDTH * DISPLAY_HEIGHT / 8` = 48000 pixels = 96 KB (requiere más SRAM interna).

### 3.4 Widgets LVGL habilitados

Todos los widgets principales están activos: arc, bar, btn, btnmatrix, canvas, checkbox, dropdown, img, label, line, roller, slider, switch, textarea, table, chart, colorwheel, imgbtn, keyboard, led, list, menu, meter, msgbox, span, spinbox, spinner, tabview, tileview, win.

Todos los tamaños de fuente Montserrat (8–48 pt) están habilitados, lo cual ocupa considerable espacio en flash.

---

## 4. Comparativa: Implementación actual vs. Referencia Fabricante

| Aspecto | Implementación Actual (Arduino) | Referencia Fabricante (IDF_Code) |
|---|---|---|
| Framework | Arduino 3.x (PlatformIO) | ESP-IDF nativo |
| Driver display | `esp_lcd_panel_rgb` via Arduino-ESP32 | `esp_lcd_panel_rgb` IDF nativo |
| LVGL | 8.3.11 via librería Arduino | 8.x via idf_component_manager |
| Tick LVGL | `millis()` (Arduino) | `esp_timer_get_time()` / FreeRTOS tick |
| Touch | TAMC_GT911_Fixed (Arduino) | `espressif/esp_lcd_touch_gt911` (IDF component) |
| Pixel clock | 15 MHz (reducido por I2S) | 21 MHz (especificación oficial) |
| PSRAM | `MALLOC_CAP_SPIRAM` | Mismo |
| FB location | PSRAM | PSRAM |
| Bounce buffers | 20 líneas (32 KB × 2) | TBD — requiere verificación del código IDF |
| Mutex LVGL | `xSemaphoreCreateRecursiveMutex()` | `xSemaphoreCreateMutex()` |

### 4.1 Ventajas de la migración a ESP-IDF nativo

1. **Sin overhead Arduino**: La capa Arduino-ESP32 añade ~30 KB de código y múltiples inicializaciones por defecto que no son necesarias.
2. **Control directo del pixel clock**: En IDF nativo se puede configurar la fuente de clock con más precisión, potencialmente permitiendo volver a 21 MHz.
3. **Mejor gestión de DMA**: Sin conflicto con el stack WiFi/BT de Arduino que puede interferir con el DMA del RGB.
4. **I2S nativo sin capa Arduino**: El driver de audio I2S en IDF nativo tiene mejor aislamiento del bus RGB.
5. **LVGL tick con `esp_timer`**: Más preciso que `millis()` bajo carga FreeRTOS.
