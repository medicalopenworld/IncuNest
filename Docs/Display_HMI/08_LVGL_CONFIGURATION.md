# Configuración LVGL para ESP-IDF Nativo — CrowPanel 7"

> Basado en `include/lv_conf.h` actual + correcciones identificadas en el análisis.

## 1. lv_conf.h Completo y Comentado para ESP-IDF

```c
/**
 * lv_conf.h — Configuración LVGL 8.4.x para IncuNest HMI
 * Hardware: CrowPanel Advance 7" (ESP32-S3, 800x480, RGB565)
 * Framework: ESP-IDF 5.1+
 */

#if 1  /* Habilitar contenido */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* =====================
   CONFIGURACIÓN DE COLOR
   ===================== */

/* RGB565 — profundidad de color nativa del panel */
#define LV_COLOR_DEPTH 16

/* Sin swap de bytes — el bus RGB16 del ESP32-S3 no requiere inversión */
#define LV_COLOR_16_SWAP 0

/* Transparencia no necesaria para este HMI */
#define LV_COLOR_SCREEN_TRANSP 0

/* ========================
   CONFIGURACIÓN DE MEMORIA
   ======================== */

/* Usar allocator personalizado para dirigir objetos LVGL a PSRAM */
#define LV_MEM_CUSTOM 1
#if LV_MEM_CUSTOM
    #include <esp_heap_caps.h>
    /*
     * CAMBIO RESPECTO AL ACTUAL:
     * El código actual usa MALLOC_CAP_8BIT (SRAM interna).
     * Aquí usamos MALLOC_CAP_SPIRAM para liberar SRAM interna
     * para bounce buffers y variables críticas de tiempo real.
     * SPIRAM es suficientemente rápido para objetos estáticos LVGL.
     */
    #define LV_MEM_CUSTOM_INCLUDE <esp_heap_caps.h>
    #define LV_MEM_CUSTOM_ALLOC(size)       heap_caps_malloc((size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    #define LV_MEM_CUSTOM_FREE(ptr)         heap_caps_free(ptr)
    #define LV_MEM_CUSTOM_REALLOC(ptr,size) heap_caps_realloc((ptr), (size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#endif

#define LV_MEM_BUF_MAX_NUM 64

/* ========================
   CONFIGURACIÓN HAL
   ======================== */

/* Período de refresco 30ms = ~33 fps teórico */
#define LV_DISP_DEF_REFR_PERIOD 30

/* Polling del touch cada 30ms */
#define LV_INDEV_DEF_READ_PERIOD 30

/*
 * CAMBIO RESPECTO AL ACTUAL:
 * Usar esp_timer_get_time() en lugar de millis().
 * esp_timer es más preciso bajo carga FreeRTOS (millis() depende de
 * un timer de baja prioridad que puede desincronizarse).
 */
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE "esp_timer.h"
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR ((uint32_t)(esp_timer_get_time() / 1000LL))
#endif

#define LV_DPI_DEF 130

/* ========================
   DRAWING / RENDERING
   ======================== */

#define LV_DRAW_COMPLEX 1
#if LV_DRAW_COMPLEX
    #define LV_SHADOW_CACHE_SIZE 0
    #define LV_CIRCLE_CACHE_SIZE 4
#endif

/* Buffer para capas simples en SRAM interna (necesita ser rápido) */
#define LV_LAYER_SIMPLE_BUF_SIZE          (24 * 1024)
#define LV_LAYER_SIMPLE_FALLBACK_BUF_SIZE (3 * 1024)

#define LV_IMG_CACHE_DEF_SIZE 0
#define LV_GRADIENT_MAX_STOPS 2
#define LV_GRAD_CACHE_DEF_SIZE 0
#define LV_DITHER_GRADIENT 0
#define LV_DISP_ROT_MAX_BUF (10 * 1024)

/* ========================
   LOGGING
   ======================== */

/* Habilitar log LVGL para debug (deshabilitar en producción) */
#define LV_USE_LOG 0

/* ========================
   ASSERTS
   ======================== */

#define LV_USE_ASSERT_NULL    1
#define LV_USE_ASSERT_MALLOC  1
#define LV_USE_ASSERT_STYLE   0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ     0

/*
 * CAMBIO CRÍTICO RESPECTO AL ACTUAL:
 * El handler actual es while(1) que dispara el watchdog.
 * En IDF debemos llamar a esp_restart() con log previo para
 * reiniciar limpiamente y registrar el evento.
 */
#define LV_ASSERT_HANDLER_INCLUDE "esp_log.h"
#define LV_ASSERT_HANDLER \
    do { \
        ESP_LOGE("LVGL", "Assert failed at %s:%d", __FILE__, __LINE__); \
        esp_restart(); \
    } while(0)

/* ========================
   FUENTES
   ======================== */

/*
 * OPTIMIZACIÓN RESPECTO AL ACTUAL:
 * El código actual habilita TODOS los tamaños Montserrat (8-48pt).
 * Cada tamaño ocupa ~20-60 KB en flash. Habilitar solo los usados.
 * Revisar el código de UI para identificar cuáles se usan realmente.
 */
#define LV_FONT_MONTSERRAT_14 1  /* Default */
#define LV_FONT_MONTSERRAT_16 1  /* Labels de temperatura */
#define LV_FONT_MONTSERRAT_20 1  /* Setpoints */
#define LV_FONT_MONTSERRAT_24 1  /* Valores principales */
#define LV_FONT_MONTSERRAT_32 1  /* Temperaturas grandes */
#define LV_FONT_MONTSERRAT_48 1  /* Display principal */
/* Los demás tamaños: habilitar solo si se confirma su uso */

#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* ========================
   WIDGETS HABILITADOS
   ======================== */

/* Solo los widgets realmente usados en el HMI */
#define LV_USE_ARC       1  /* Progreso lock, arco encendido */
#define LV_USE_BAR       1  /* Barras de temperatura/humedad */
#define LV_USE_BTN       1  /* Botones de la UI */
#define LV_USE_BTNMATRIX 1
#define LV_USE_CANVAS    0  /* No usado actualmente */
#define LV_USE_CHECKBOX  1
#define LV_USE_DROPDOWN  1  /* Selector de idioma, rango histórico */
#define LV_USE_IMG       1  /* Iconos */
#define LV_USE_LABEL     1
#define LV_USE_LINE      1
#define LV_USE_ROLLER    0
#define LV_USE_SLIDER    1
#define LV_USE_SWITCH    1  /* Interruptores de control */
#define LV_USE_TEXTAREA  1  /* Campos WiFi SSID/Password */
#define LV_USE_TABLE     0
#define LV_USE_CHART     1  /* Gráficas históricas */
#define LV_USE_METER     0
#define LV_USE_MSGBOX    1  /* Diálogos de confirmación */
#define LV_USE_SPINNER   1  /* Indicador OTA en progreso */
#define LV_USE_GIF       1  /* Animaciones si se usan */
#define LV_USE_QRCODE    1  /* QR para configuración */
#define LV_USE_KEYBOARD  1  /* Teclado virtual WiFi */

/* Tema */
#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT
    #define LV_THEME_DEFAULT_DARK 0
    #define LV_THEME_DEFAULT_GROW 1
    #define LV_THEME_DEFAULT_TRANSITION_TIME 80
#endif

/* Layouts */
#define LV_USE_FLEX 1
#define LV_USE_GRID 1

#define LV_TXT_ENC LV_TXT_ENC_UTF8
#define LV_USE_USER_DATA 1

#endif /* LV_CONF_H */
#endif /* Fin del bloque activo */
```

---

## 2. Estrategia de Buffer de Dibujo

### 2.1 Configuración actual (problema)

```c
// ACTUAL — UITask.cpp:177
static lv_color_t disp_draw_buf[DISPLAY_WIDTH * DISPLAY_HEIGHT / COLOR_DIVISOR];
// = 800 × 480 / 16 = 24000 px = 48 KB en SRAM interna
```

Solo cubre 1/16 del frame. Requiere hasta 32 operaciones de flush para un repintado completo.

### 2.2 Configuración recomendada para ESP-IDF

```c
// RECOMENDADO: Buffer en PSRAM, 1/4 del frame
#define DRAW_BUF_SIZE_PX (DISPLAY_WIDTH * DISPLAY_HEIGHT / 4)
// = 800 × 480 / 4 = 96000 px = 192 KB

// Allocar en PSRAM (no compite con SRAM para bounce buffers)
static lv_color_t *draw_buf_1 = NULL;
static lv_color_t *draw_buf_2 = NULL;  // Segundo buffer para doble buffering

void ui_driver_init(void) {
    draw_buf_1 = heap_caps_malloc(DRAW_BUF_SIZE_PX * sizeof(lv_color_t),
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    draw_buf_2 = heap_caps_malloc(DRAW_BUF_SIZE_PX * sizeof(lv_color_t),
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(draw_buf_1 && draw_buf_2);
    
    lv_disp_draw_buf_init(&draw_buf, draw_buf_1, draw_buf_2, DRAW_BUF_SIZE_PX);
}
```

### 2.3 Comparativa de estrategias de buffer

| Estrategia | Tamaño | Ubicación | Flushes/frame | Latencia |
|---|---|---|---|---|
| Actual (1/16) | 48 KB | SRAM | ~32 | Alta |
| Recomendado (1/4) | 192 KB | PSRAM | ~8 | Baja |
| Óptimo (1/2) | 384 KB | PSRAM | ~4 | Muy baja |
| Full frame | 768 KB | PSRAM | 1 | Mínima |

> **Nota**: Con `fb_in_psram=1` y bounce buffers, el full-frame doble buffering en PSRAM es la estrategia óptima. Requiere IDF 5.1+ con soporte para `num_fbs=2`.

---

## 3. Integración con esp_lcd (Callback de Flush)

```c
// ui/ui_task.c

static esp_lcd_panel_handle_t lcd_panel = NULL;

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area,
                          lv_color_t *color_map) {
    esp_lcd_panel_draw_bitmap(lcd_panel,
                              area->x1, area->y1,
                              area->x2 + 1, area->y2 + 1,
                              color_map);
    lv_disp_flush_ready(drv);
}
```

---

## 4. Configuración del Tick LVGL en ESP-IDF

### 4.1 Método recomendado — esp_timer periódico

```c
// ui/ui_task.c

static void lvgl_tick_timer_cb(void *arg) {
    lv_tick_inc(2);  // Llamar cada 2ms → precisión ±2ms
}

void ui_driver_init(void) {
    // ...
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = lvgl_tick_timer_cb,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 2000)); // 2ms
}
```

### 4.2 Bucle principal UITask

```c
// ui/ui_task.c

void ui_task_fn(void *arg) {
    ui_driver_init();     // Display + touch + LVGL
    ui_create_screens();  // Crear todas las pantallas LVGL
    
    comm_send_ui_ready(); // HMI,UI_READY
    
    while (1) {
        // Procesar cola IPC de CommTask
        ui_event_t evt;
        while (xQueueReceive(g_ui_event_queue, &evt, 0) == pdTRUE) {
            ui_apply_event(&evt);
        }
        
        // Ejecutar timer handler LVGL
        uint32_t time_till_next = lv_timer_handler();
        
        // Dormir el tiempo mínimo que indica LVGL
        uint32_t delay_ms = (time_till_next < 5) ? 5 : time_till_next;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}
```

---

## 5. Driver Touch GT911 en ESP-IDF Nativo

```c
// drivers/touch_driver.c

#include "esp_lcd_touch_gt911.h"

static esp_lcd_touch_handle_t tp_handle = NULL;

void touch_driver_init(void) {
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    
    // Configurar I2C master bus
    i2c_master_bus_handle_t i2c_bus;
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = 16,   // DISPLAY_TOUCH_SCL
        .sda_io_num = 15,   // DISPLAY_TOUCH_SDA
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_bus));
    
    // Configurar IO del touch (protocolo I2C para gt911)
    esp_lcd_panel_io_i2c_config_t tp_io_config = 
        ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_config, &tp_io_handle));
    
    // Configurar touch
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = 800,
        .y_max = 480,
        .rst_gpio_num = -1,   // DISPLAY_TOUCH_RST = -1
        .int_gpio_num = -1,   // DISPLAY_TOUCH_INT = -1
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,    // Ajustar si DISPLAY_ROTATE_180 = 1
            .mirror_y = 0,
        },
    };
    ESP_ERROR_CHECK(esp_lcd_new_touch_gt911(tp_io_handle, &tp_cfg, &tp_handle));
}

// Callback LVGL
void lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    uint16_t touch_x[1], touch_y[1];
    uint8_t touch_cnt = 0;
    esp_lcd_touch_read_data(tp_handle);
    esp_lcd_touch_get_coordinates(tp_handle, touch_x, touch_y, NULL,
                                  &touch_cnt, 1);
    if (touch_cnt > 0) {
        data->point.x = touch_x[0];
        data->point.y = touch_y[0];
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}
```

---

## 6. Notas Específicas del CrowPanel Advance 7"

### 6.1 Backlight via I2C (no GPIO)

El CrowPanel Advance 7" controla el backlight a través del MCU auxiliar STC8H1K28 en I2C @0x30. No hay control por GPIO. En ESP-IDF:

```c
// drivers/backlight.c

#define BL_I2C_ADDR   0x30
#define BL_CMD_ON     10    // DISPLAY_BL_ON_VALUE
#define BL_CMD_OFF    245   // DISPLAY_BL_OFF_VALUE

void backlight_set(bool on) {
    uint8_t cmd = on ? BL_CMD_ON : BL_CMD_OFF;
    i2c_master_transmit(i2c_dev_handle, &cmd, 1, pdMS_TO_TICKS(100));
}
```

### 6.2 Buzzer via I2C

El buzzer de la placa también se controla via el STC8H1K28:

```c
#define BUZZER_ON_CMD   246  // I2C_CMD_BUZZER_ON
#define BUZZER_OFF_CMD  247  // I2C_CMD_BUZZER_OFF
#define SPEAKER_ON_CMD  248  // I2C_CMD_SPEAKER_ON
#define SPEAKER_OFF_CMD 249  // I2C_CMD_SPEAKER_OFF
```

### 6.3 Pixel Clock Óptimo

Probar al recuperar estabilidad sin I2S:
- 15 MHz (actual, garantizado estable)
- 18 MHz (objetivo intermedio)  
- 21 MHz (especificación oficial Elecrow Advance)

El nuevo proyecto no incluirá I2S de audio, por lo que debería ser posible volver a 21 MHz.
