/**
 * @file display_driver.c
 * @brief Initializes the RGB LCD panel and provides the flush callback for LVGL.
 *
 * @details Configures the CrowPanel Advance 7" (ESP32-S3, 800×480, RGB565) using
 *          esp_lcd_panel_rgb with PSRAM framebuffer + SRAM bounce buffers.
 *          Backlight is controlled via I2C to the STC8H1K28 MCU at I2C address 0x30.
 *          Phase 1: migrated from legacy i2c.h API to new i2c_master.h API so that
 *          esp_lcd_new_panel_io_i2c() (required by GT911 touch) can share the bus.
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
#include "driver/i2c_master.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"

static const char *TAG = "DISPLAY";

/* Module-private state ---------------------------------------------------- */

static esp_lcd_panel_handle_t  s_panel   = NULL;
static i2c_master_bus_handle_t s_i2c_bus = NULL;
static i2c_master_dev_handle_t s_bl_dev  = NULL;

/* Phase 0: VSYNC callback is a no-op.
 * Phase 1 (lvgl_port): replaced by a version that wakes the LVGL task. */
static bool (*s_vsync_notify_fn)(void) = NULL;

/* ------------------------------------------------------------------------- */

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

static esp_err_t s_i2c_write_byte(uint8_t value)
{
    return i2c_master_transmit(s_bl_dev, &value, 1, 100);
}

/* ------------------------------------------------------------------------- */

esp_err_t display_driver_init(void)
{
    esp_err_t ret;

    /* ------------------------------------------------------------------
     * Step 1 — Initialize I2C master bus (shared with GT911 touch driver)
     * Phase 1: switched to new i2c_master API so esp_lcd_new_panel_io_i2c()
     * can accept the bus handle (IDF 5.4 no longer accepts port numbers).
     * ------------------------------------------------------------------ */
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port            = HW_I2C_PORT,
        .sda_io_num          = HW_I2C_SDA_PIN,
        .scl_io_num          = HW_I2C_SCL_PIN,
        .clk_source          = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt   = 7,
        .flags.enable_internal_pullup = true,
    };
    ret = i2c_new_master_bus(&bus_cfg, &s_i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Add backlight controller (STC8H1K28) as a device on the bus */
    i2c_device_config_t bl_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = HW_BL_I2C_ADDR,
        .scl_speed_hz    = HW_I2C_FREQ_HZ,
    };
    ret = i2c_master_bus_add_device(s_i2c_bus, &bl_dev_cfg, &s_bl_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_master_bus_add_device (backlight) failed: %s",
                 esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "I2C bus initialized (SDA=%d SCL=%d %dHz)",
             HW_I2C_SDA_PIN, HW_I2C_SCL_PIN, HW_I2C_FREQ_HZ);

    /* ------------------------------------------------------------------
     * Step 2 — Initialize STC8H1K28 backlight controller (OFF first)
     * ------------------------------------------------------------------ */
    ret = s_i2c_write_byte(HW_BL_OFF_VALUE);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Backlight init write failed (0x%02X): %s — continuing",
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
            .flags.pclk_active_neg = 1,
        },
        .data_width            = HW_LCD_DATA_WIDTH,
        .bits_per_pixel        = HW_LCD_BIT_PER_PIXEL,
        .num_fbs               = 1,
        .bounce_buffer_size_px = HW_LCD_BOUNCE_BUF_PX,
        .hsync_gpio_num        = HW_LCD_PIN_HSYNC,
        .vsync_gpio_num        = HW_LCD_PIN_VSYNC,
        .de_gpio_num           = HW_LCD_PIN_DE,
        .pclk_gpio_num         = HW_LCD_PIN_PCLK,
        .disp_gpio_num         = HW_LCD_PIN_DISP,
        .data_gpio_nums = {
            HW_LCD_PIN_B0, HW_LCD_PIN_B1, HW_LCD_PIN_B2, HW_LCD_PIN_B3, HW_LCD_PIN_B4,
            HW_LCD_PIN_G0, HW_LCD_PIN_G1, HW_LCD_PIN_G2, HW_LCD_PIN_G3, HW_LCD_PIN_G4, HW_LCD_PIN_G5,
            HW_LCD_PIN_R0, HW_LCD_PIN_R1, HW_LCD_PIN_R2, HW_LCD_PIN_R3, HW_LCD_PIN_R4,
        },
        .flags.fb_in_psram = 1,
    };

    ret = esp_lcd_new_rgb_panel(&panel_cfg, &s_panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_rgb_panel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_lcd_rgb_panel_event_callbacks_t cbs = { .on_vsync = s_on_vsync };
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
     * Step 5 — Fill framebuffer with solid blue (Phase 0/1 proof-of-life)
     * ------------------------------------------------------------------ */
    void *fb = NULL;
    ret = esp_lcd_rgb_panel_get_frame_buffer(s_panel, 1, &fb);
    if (ret == ESP_OK && fb != NULL) {
        uint16_t *pixels = (uint16_t *)fb;
        const size_t total_px = HW_LCD_H_RES * HW_LCD_V_RES;
        for (size_t i = 0; i < total_px; i++) {
            pixels[i] = 0x001Fu;
        }
        ESP_LOGI(TAG, "Framebuffer filled blue (%zu px @ %p)", total_px, fb);
    } else {
        ESP_LOGW(TAG, "Could not get framebuffer — display may be blank");
    }

    ESP_LOGI(TAG, "Display init OK — %dx%d @ %luMHz bounce=%dpx",
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
    esp_lcd_panel_draw_bitmap(s_panel, x1, y1, x2 + 1, y2 + 1, color_data);
}

/* ------------------------------------------------------------------------- */

void display_driver_set_backlight(uint8_t brightness_pct)
{
    uint8_t cmd;
    if (brightness_pct == 0) {
        cmd = HW_BL_OFF_VALUE;
    } else if (brightness_pct < DISPLAY_BL_MIN_PCT) {
        ESP_LOGW(TAG, "Backlight %u%% below normative min (%u%%) — OFF",
                 brightness_pct, DISPLAY_BL_MIN_PCT);
        cmd = HW_BL_OFF_VALUE;
    } else {
        cmd = HW_BL_ON_VALUE;
    }
    esp_err_t ret = s_i2c_write_byte(cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Backlight set failed: %s", esp_err_to_name(ret));
    }
}

/* ------------------------------------------------------------------------- */

esp_lcd_panel_handle_t display_driver_get_panel(void)
{
    return s_panel;
}

i2c_master_bus_handle_t display_driver_get_i2c_bus(void)
{
    return s_i2c_bus;
}

/* ------------------------------------------------------------------------- */

bool display_driver_notify_vsync(void)
{
    return false;
}

void display_driver_set_vsync_notify(bool (*fn)(void))
{
    s_vsync_notify_fn = fn;
}
