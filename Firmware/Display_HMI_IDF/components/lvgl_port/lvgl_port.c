/**
 * @file lvgl_port.c
 * @brief LVGL 8.4 integration layer — display driver, touch input, task, mutex.
 *
 * Display strategy: single PSRAM framebuffer + SRAM bounce buffers.
 * bb_invalidate_cache=1 in the panel config ensures the CPU data cache is
 * flushed before DMA reads each bounce-buffer segment — no stale-data tearing.
 *
 * LVGL draw buffer: static SRAM partial buffer (LVGL_DRAW_BUF_LINES × H_RES).
 * LVGL renders dirty strips and calls flush_cb for each; flush_cb immediately
 * writes to the PSRAM FB via esp_lcd_panel_draw_bitmap and signals LVGL ready.
 * No VSYNC wait is needed — CONFIG_LCD_RGB_RESTART_IN_VSYNC handles DMA sync.
 */

#include "lvgl_port.h"
#include "display_driver.h"
#include "touch_driver.h"
#include "app_config.h"

#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "LVGL_PORT";

static SemaphoreHandle_t s_lvgl_mutex = NULL;

/* Diagnostic: count VSYNCs from ISR */
static volatile uint32_t s_vsync_count = 0;

/* Touch suppression: suppress all input until this timestamp (µs) expires.
 * Time-based — independent of GT911 hardware state to avoid stale-data race. */
static volatile int64_t s_suppress_until_us = 0;

/* SRAM draw buffer — 60 lines × H_RES pixels (1/8 of 800×480).
 * 60 lines = 8 flushes per frame ≈ 16ms, within one VSYNC period at 51Hz.
 * Keeps the progressive-render wipe below the persistence-of-vision threshold. */
#define LVGL_DRAW_BUF_LINES  60
static lv_color_t s_lvgl_buf[HW_LCD_H_RES * LVGL_DRAW_BUF_LINES];

/* ------------------------------------------------------------------------- */

static IRAM_ATTR bool s_vsync_isr(void)
{
    s_vsync_count++;
    return false;
}

/* ------------------------------------------------------------------------- */

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *buf)
{
    display_driver_flush(area->x1, area->y1, area->x2, area->y2, buf);
    lv_disp_flush_ready(drv);
}

/* ------------------------------------------------------------------------- */

static void lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    uint16_t x = 0, y = 0;
    bool pressed = false;
    touch_driver_read(&x, &y, &pressed);

    /* Time-based suppression: block all input for a fixed window after navigation.
     * Avoids dependence on GT911 hardware state (stale data / slow release). */
    if (esp_timer_get_time() < s_suppress_until_us) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    data->point.x = (lv_coord_t)x;
    data->point.y = (lv_coord_t)y;
    data->state   = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void lvgl_port_suppress_touch_for_ms(uint32_t ms)
{
    s_suppress_until_us = esp_timer_get_time() + (int64_t)ms * 1000LL;
}

/* ------------------------------------------------------------------------- */

static void lvgl_task(void *arg)
{
    ESP_LOGI(TAG, "UITask started (core %d, prio %d)",
             xPortGetCoreID(), TASK_LVGL_PRIORITY);

    uint32_t iter      = 0;
    int64_t  report_us = esp_timer_get_time();

    while (1) {
        uint32_t delay_ms = 10;
        if (lvgl_port_lock(10)) {
            delay_ms = lv_timer_handler();
            lvgl_port_unlock();
        }
        if (delay_ms > 50) delay_ms = 50;
        if (delay_ms < 1)  delay_ms = 1;

        iter++;
        int64_t now_us = esp_timer_get_time();

        if (now_us - report_us >= 2000000LL) {
            ESP_LOGI(TAG, "DIAG iter=%lu delay=%lums vsync_total=%lu heap=%lu",
                     (unsigned long)iter,
                     (unsigned long)delay_ms,
                     (unsigned long)s_vsync_count,
                     (unsigned long)esp_get_free_heap_size());
            report_us = now_us;
        }

        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

/* ------------------------------------------------------------------------- */

esp_err_t lvgl_port_init(void)
{
    s_lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    if (s_lvgl_mutex == NULL) {
        ESP_LOGE(TAG, "Mutex alloc failed");
        return ESP_ERR_NO_MEM;
    }

    lv_init();

    display_driver_set_vsync_notify(s_vsync_isr);

    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, s_lvgl_buf, NULL,
                          HW_LCD_H_RES * LVGL_DRAW_BUF_LINES);

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

    ESP_LOGI(TAG, "LVGL port OK — %dx%d  draw_buf=%d lines (%d px)",
             HW_LCD_H_RES, HW_LCD_V_RES,
             LVGL_DRAW_BUF_LINES, HW_LCD_H_RES * LVGL_DRAW_BUF_LINES);
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
