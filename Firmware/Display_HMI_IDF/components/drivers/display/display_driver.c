/**
 * @file display_driver.c
 * @brief Initializes the RGB LCD panel and provides the flush callback for LVGL.
 *
 * @details Configures the CrowPanel Advance 7" (ESP32-S3, 800×480, RGB565) using
 *          esp_lcd_panel_rgb with PSRAM framebuffer + SRAM bounce buffers.
 *          Backlight is controlled via I2C to the STC8H1K28 MCU at I2C address 0x30.
 *          This file does NOT depend on LVGL — the lvgl_port component registers
 *          display_driver_flush() as the flush callback.
 *
 * Normativa aplicable:
 * - IEC 60601-2-19 §201.9.6.2.1.101 — display minimum backlight brightness
 *
 * @author IncuNest Team
 * @date   2026-04-28
 */

#include "display_driver.h"
#include "app_config.h"
#include "medical_constants.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "DISPLAY";

/* Module-private state ---------------------------------------------------- */

static esp_lcd_panel_handle_t s_panel = NULL;

/* Phase 0: VSYNC callback is a no-op.
 * Phase 1 (lvgl_port): replaced by a version that wakes the LVGL task.
 * The vsync_notify_fn pointer lets lvgl_port inject its own handler. */
static bool (*s_vsync_notify_fn)(void) = NULL;

/* ------------------------------------------------------------------------- */

/**
 * @brief VSYNC ISR callback — must run from IRAM.
 * Delegates to s_vsync_notify_fn if one has been registered (Phase 1+).
 */
IRAM_ATTR static bool s_on_vsync(esp_lcd_panel_handle_t panel,
                                  const esp_lcd_rgb_panel_event_data_t *edata,
                                  void *user_ctx)
{
    if (s_vsync_notify_fn) {
        return s_vsync_notify_fn();
    }
    return false;
}

/* ------------------------------------------------------------------------- */

/**
 * @brief Write a single byte to an I2C device (blocking, 100 ms timeout).
 */
static esp_err_t s_i2c_write_byte(uint8_t dev_addr, uint8_t value)
{
    return i2c_master_write_to_device(HW_I2C_PORT, dev_addr,
                                      &value, 1,
                                      pdMS_TO_TICKS(100));
}

/* ------------------------------------------------------------------------- */

esp_err_t display_driver_init(void)
{
    esp_err_t ret;

    /* ------------------------------------------------------------------
     * Step 1 — Initialize I2C bus (shared with touch_driver and backlight)
     * ------------------------------------------------------------------ */
    i2c_config_t i2c_cfg = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = HW_I2C_SDA_PIN,
        .scl_io_num       = HW_I2C_SCL_PIN,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = HW_I2C_FREQ_HZ,
    };
    ret = i2c_param_config(HW_I2C_PORT, &i2c_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = i2c_driver_install(HW_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "I2C port %d initialized (SDA=%d, SCL=%d, %d Hz)",
             HW_I2C_PORT, HW_I2C_SDA_PIN, HW_I2C_SCL_PIN, HW_I2C_FREQ_HZ);

    /* ------------------------------------------------------------------
     * Step 2 — Initialize STC8H1K28 backlight controller
     * Send OFF command first to ensure clean state before panel init.
     * ------------------------------------------------------------------ */
    ret = s_i2c_write_byte(HW_BL_I2C_ADDR, HW_BL_OFF_VALUE);
    if (ret != ESP_OK) {
        /* Non-fatal: backlight might still work. Log and continue. */
        ESP_LOGW(TAG, "Backlight init write failed (addr=0x%02X): %s — continuing",
                 HW_BL_I2C_ADDR, esp_err_to_name(ret));
    }
    vTaskDelay(pdMS_TO_TICKS(20));

    /* ------------------------------------------------------------------
     * Step 3 — Configure and create the RGB panel
     * ------------------------------------------------------------------ */
    esp_lcd_rgb_panel_config_t panel_cfg = {
        .clk_src   = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz           = HW_LCD_PCLK_HZ,
            .h_res             = HW_LCD_H_RES,
            .v_res             = HW_LCD_V_RES,
            .hsync_pulse_width = HW_LCD_HSYNC_PULSE_W,
            .hsync_back_porch  = HW_LCD_HSYNC_BACK_PORCH,
            .hsync_front_porch = HW_LCD_HSYNC_FRONT_PORCH,
            .vsync_pulse_width = HW_LCD_VSYNC_PULSE_W,
            .vsync_back_porch  = HW_LCD_VSYNC_BACK_PORCH,
            .vsync_front_porch = HW_LCD_VSYNC_FRONT_PORCH,
            .flags = {
                .pclk_active_neg = 1,   /**< Latch on falling edge */
            },
        },
        .data_width            = HW_LCD_DATA_WIDTH,
        .bits_per_pixel        = HW_LCD_BIT_PER_PIXEL,
        .num_fbs               = 1,
        .bounce_buffer_size_px = HW_LCD_BOUNCE_BUF_PX,
        .sram_trans_align      = 4,
        .psram_trans_align     = 64,
        .hsync_gpio_num        = HW_LCD_PIN_HSYNC,
        .vsync_gpio_num        = HW_LCD_PIN_VSYNC,
        .de_gpio_num           = HW_LCD_PIN_DE,
        .pclk_gpio_num         = HW_LCD_PIN_PCLK,
        .disp_gpio_num         = HW_LCD_PIN_DISP,
        .data_gpio_nums = {
            /* Data bus order matches RGB565 bit layout:
             * DATA[0..4]  = B[0..4] (blue LSB→MSB)
             * DATA[5..10] = G[0..5] (green LSB→MSB)
             * DATA[11..15]= R[0..4] (red LSB→MSB) */
            HW_LCD_PIN_B0, HW_LCD_PIN_B1, HW_LCD_PIN_B2, HW_LCD_PIN_B3, HW_LCD_PIN_B4,
            HW_LCD_PIN_G0, HW_LCD_PIN_G1, HW_LCD_PIN_G2, HW_LCD_PIN_G3, HW_LCD_PIN_G4, HW_LCD_PIN_G5,
            HW_LCD_PIN_R0, HW_LCD_PIN_R1, HW_LCD_PIN_R2, HW_LCD_PIN_R3, HW_LCD_PIN_R4,
        },
        .flags = {
            .fb_in_psram = 1,   /**< Framebuffer in 8 MB PSRAM */
        },
    };

    ret = esp_lcd_new_rgb_panel(&panel_cfg, &s_panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_rgb_panel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Register VSYNC event callback (ISR context — must be IRAM).
     * Phase 0: no-op. Phase 1: lvgl_port calls display_driver_set_vsync_notify(). */
    esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_vsync = s_on_vsync,
    };
    ret = esp_lcd_rgb_panel_register_event_callbacks(s_panel, &cbs, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "register_event_callbacks failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ------------------------------------------------------------------
     * Step 4 — Initialize panel (starts DMA streaming from PSRAM FB)
     * ------------------------------------------------------------------ */
    ret = esp_lcd_panel_init(s_panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_panel_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ------------------------------------------------------------------
     * Step 5 — Fill framebuffer with solid blue as Phase 0 proof-of-life
     * RGB565 blue: R=0, G=0, B=31 → 0x001F
     * The DMA reads the FB continuously; no explicit draw_bitmap needed.
     * ------------------------------------------------------------------ */
    void *fb = NULL;
    ret = esp_lcd_rgb_panel_get_frame_buffer(s_panel, 1, &fb);
    if (ret == ESP_OK && fb != NULL) {
        uint16_t *pixels = (uint16_t *)fb;
        const size_t total_px = HW_LCD_H_RES * HW_LCD_V_RES;
        for (size_t i = 0; i < total_px; i++) {
            pixels[i] = 0x001Fu;   /* RGB565 solid blue */
        }
        ESP_LOGI(TAG, "Framebuffer filled with blue (%zu px @ %p)", total_px, fb);
    } else {
        ESP_LOGW(TAG, "Could not get framebuffer pointer — display may be blank");
    }

    ESP_LOGI(TAG, "Display init OK — %dx%d @ %lu MHz, bounce_buf=%d px",
             HW_LCD_H_RES, HW_LCD_V_RES,
             (unsigned long)(HW_LCD_PCLK_HZ / 1000000),
             HW_LCD_BOUNCE_BUF_PX);

    return ESP_OK;
}

/* ------------------------------------------------------------------------- */

void display_driver_flush(int x1, int y1, int x2, int y2, const void *color_data)
{
    if (s_panel == NULL) {
        ESP_LOGE(TAG, "flush called before init");
        return;
    }
    /* esp_lcd_panel_draw_bitmap copies color_data into the PSRAM framebuffer.
     * x2+1 and y2+1 because the API takes exclusive end coordinates. */
    esp_lcd_panel_draw_bitmap(s_panel, x1, y1, x2 + 1, y2 + 1, color_data);
}

/* ------------------------------------------------------------------------- */

void display_driver_set_backlight(uint8_t brightness_pct)
{
    uint8_t cmd;

    if (brightness_pct == 0) {
        cmd = HW_BL_OFF_VALUE;
    } else if (brightness_pct < DISPLAY_BL_MIN_PCT) {
        /* IEC 60601-2-19 §201.9.6.2.1.101 — cannot set below normative minimum.
         * Treat values 1–19 as "off" since we cannot guarantee legibility. */
        ESP_LOGW(TAG, "Backlight %u%% is below normative minimum (%u%%) — turning OFF",
                 brightness_pct, DISPLAY_BL_MIN_PCT);
        cmd = HW_BL_OFF_VALUE;
    } else {
        cmd = HW_BL_ON_VALUE;
    }

    esp_err_t ret = s_i2c_write_byte(HW_BL_I2C_ADDR, cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Backlight set failed: %s", esp_err_to_name(ret));
    }
}

/* ------------------------------------------------------------------------- */

esp_lcd_panel_handle_t display_driver_get_panel(void)
{
    return s_panel;
}

/* ------------------------------------------------------------------------- */

bool display_driver_notify_vsync(void)
{
    /* Phase 0: no-op. Phase 1: lvgl_port installs a real handler via
     * display_driver_set_vsync_notify(). */
    return false;
}

/**
 * @brief Register the VSYNC notify function (called by lvgl_port in Phase 1).
 * @param[in] fn  Function to call from VSYNC ISR — must be IRAM-safe.
 */
void display_driver_set_vsync_notify(bool (*fn)(void))
{
    s_vsync_notify_fn = fn;
}
