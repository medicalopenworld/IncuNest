/**
 * @file lvgl_port.c
 * @brief LVGL 8.4 integration layer — display driver, touch input, task, mutex.
 *
 * @details Initializes LVGL with the RGB panel flush callback and GT911 touch
 *          input device. Creates the UITask pinned to Core 1. Provides a
 *          recursive mutex so other tasks can safely call LVGL APIs.
 *
 *          VSYNC synchronization: s_vsync_isr() is injected into display_driver
 *          so the VSYNC interrupt gives a semaphore that lvgl_flush_cb() waits
 *          on before calling lv_disp_flush_ready(). This prevents tearing.
 *
 * Normativa aplicable:
 * - RNF-001 — ≥20 fps minimum (lv_timer_handler capped at 50 ms = 20 fps floor)
 * - RNF-002 — touch latency ≤200 ms (indev polled every LV_INDEV_DEF_READ_PERIOD ms)
 * - RNF-007 — LVGL heap in PSRAM (MALLOC_CAP_SPIRAM in lv_conf.h)
 *
 * @author IncuNest Team
 * @date   2026-04-28
 */

#include "lvgl_port.h"
#include "display_driver.h"
#include "touch_driver.h"
#include "app_config.h"

#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "LVGL_PORT";

/* Draw buffer: 20 lines × 800 px × 2 bytes (RGB565) in PSRAM */
#define DRAW_BUF_LINES  20
#define DRAW_BUF_SIZE   (HW_LCD_H_RES * DRAW_BUF_LINES * sizeof(lv_color_t))

static SemaphoreHandle_t s_lvgl_mutex = NULL;
static SemaphoreHandle_t s_vsync_sem  = NULL;

/* ------------------------------------------------------------------------- */

/* Called from VSYNC ISR — must be IRAM-safe (fn pointer set via display_driver) */
static IRAM_ATTR bool s_vsync_isr(void)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_vsync_sem, &woken);
    return woken == pdTRUE;
}

/* ------------------------------------------------------------------------- */

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *buf)
{
    display_driver_flush(area->x1, area->y1, area->x2, area->y2, buf);
    /* Wait for VSYNC to avoid screen tearing (50 ms max — one frame at 20 fps) */
    xSemaphoreTake(s_vsync_sem, pdMS_TO_TICKS(50));
    lv_disp_flush_ready(drv);
}

/* ------------------------------------------------------------------------- */

static void lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    uint16_t x = 0, y = 0;
    bool pressed = false;
    touch_driver_read(&x, &y, &pressed);
    data->point.x = (lv_coord_t)x;
    data->point.y = (lv_coord_t)y;
    data->state   = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

/* ------------------------------------------------------------------------- */

static void lvgl_task(void *arg)
{
    ESP_LOGI(TAG, "UITask started (core %d, prio %d)",
             xPortGetCoreID(), TASK_LVGL_PRIORITY);
    while (1) {
        uint32_t delay_ms = 10;
        if (lvgl_port_lock(10)) {
            delay_ms = lv_timer_handler();
            lvgl_port_unlock();
        }
        /* Clamp: never sleep longer than 50 ms (20 fps floor) */
        if (delay_ms > 50) delay_ms = 50;
        if (delay_ms < 1)  delay_ms = 1;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

/* ------------------------------------------------------------------------- */

esp_err_t lvgl_port_init(void)
{
    s_vsync_sem  = xSemaphoreCreateBinary();
    s_lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    if (s_vsync_sem == NULL || s_lvgl_mutex == NULL) {
        ESP_LOGE(TAG, "Semaphore/mutex alloc failed");
        return ESP_ERR_NO_MEM;
    }

    lv_init();

    /* Inject VSYNC handler so flush_cb can synchronize with the panel DMA */
    display_driver_set_vsync_notify(s_vsync_isr);

    /* Allocate double draw buffers in PSRAM */
    lv_color_t *buf1 = heap_caps_malloc(DRAW_BUF_SIZE, MALLOC_CAP_SPIRAM);
    lv_color_t *buf2 = heap_caps_malloc(DRAW_BUF_SIZE, MALLOC_CAP_SPIRAM);
    if (buf1 == NULL || buf2 == NULL) {
        ESP_LOGE(TAG, "Draw buffer alloc failed (need %zu B × 2 in PSRAM)", DRAW_BUF_SIZE);
        return ESP_ERR_NO_MEM;
    }

    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, HW_LCD_H_RES * DRAW_BUF_LINES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = HW_LCD_H_RES;
    disp_drv.ver_res  = HW_LCD_V_RES;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_cb;
    lv_indev_drv_register(&indev_drv);

    xTaskCreatePinnedToCore(lvgl_task, "UITask",
                            TASK_LVGL_STACK_SIZE, NULL,
                            TASK_LVGL_PRIORITY, NULL, TASK_LVGL_CORE);

    ESP_LOGI(TAG, "LVGL port OK — %dx%d, bufs=%d lines×2 @ PSRAM",
             HW_LCD_H_RES, HW_LCD_V_RES, DRAW_BUF_LINES);
    return ESP_OK;
}

/* ------------------------------------------------------------------------- */

bool lvgl_port_lock(int timeout_ms)
{
    TickType_t ticks = (timeout_ms < 0) ? portMAX_DELAY
                                        : pdMS_TO_TICKS((uint32_t)timeout_ms);
    return xSemaphoreTakeRecursive(s_lvgl_mutex, ticks) == pdTRUE;
}

void lvgl_port_unlock(void)
{
    xSemaphoreGiveRecursive(s_lvgl_mutex);
}
