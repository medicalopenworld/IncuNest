/**
 * @file touch_driver.c
 * @brief Initializes and reads the Goodix GT911 capacitive touch controller.
 *
 * @details Uses espressif/esp_lcd_touch_gt911 IDF managed component.
 *          I2C bus is shared with the backlight controller — initialized by
 *          display_driver_init(). touch_driver_init() must be called after
 *          display_driver_init() to obtain the shared bus handle.
 *
 * Normativa aplicable:
 * - IEC 62366-1 §5.1 — touch latency ≤200 ms (RNF-002)
 *
 * @author IncuNest Team
 * @date   2026-04-28
 */

#include "touch_driver.h"
#include "display_driver.h"
#include "app_config.h"

#include "esp_lcd_touch_gt911.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "TOUCH";

static esp_lcd_touch_handle_t s_touch = NULL;

/* ------------------------------------------------------------------------- */

esp_err_t touch_driver_init(void)
{
    i2c_master_bus_handle_t i2c_bus = display_driver_get_i2c_bus();
    if (i2c_bus == NULL) {
        ESP_LOGE(TAG, "I2C bus not ready — call display_driver_init() first");
        return ESP_ERR_INVALID_STATE;
    }

    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_cfg.dev_addr = HW_TOUCH_I2C_ADDR;   /* 0x14 per CrowPanel v1.3/v1.4 */

    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_i2c_v2(i2c_bus, &tp_io_cfg, &tp_io),
        TAG, "esp_lcd_new_panel_io_i2c_v2 failed"
    );

    esp_lcd_touch_config_t tp_cfg = {
        .x_max        = HW_LCD_H_RES,
        .y_max        = HW_LCD_V_RES,
        .rst_gpio_num = HW_TOUCH_RST_PIN,   /* -1: managed by STC8H1K28 */
        .int_gpio_num = HW_TOUCH_INT_PIN,   /* -1: not wired to ESP32-S3 */
        .flags.swap_xy  = 0,
        .flags.mirror_x = 0,
        .flags.mirror_y = 0,
    };

    ESP_RETURN_ON_ERROR(
        esp_lcd_touch_new_i2c_gt911(tp_io, &tp_cfg, &s_touch),
        TAG, "esp_lcd_touch_new_i2c_gt911 failed"
    );

    ESP_LOGI(TAG, "GT911 initialized (addr=0x%02X, %dx%d)",
             HW_TOUCH_I2C_ADDR, HW_LCD_H_RES, HW_LCD_V_RES);
    return ESP_OK;
}

/* ------------------------------------------------------------------------- */

void touch_driver_read(uint16_t *x, uint16_t *y, bool *pressed)
{
    if (s_touch == NULL) {
        if (x)       *x       = 0;
        if (y)       *y       = 0;
        if (pressed) *pressed = false;
        return;
    }

    esp_lcd_touch_read_data(s_touch);

    uint16_t touch_x[1] = {0};
    uint16_t touch_y[1] = {0};
    uint8_t  touch_cnt  = 0;
    bool got = esp_lcd_touch_get_coordinates(s_touch, touch_x, touch_y,
                                              NULL, &touch_cnt, 1);

    if (got && touch_cnt > 0) {
        ESP_LOGD(TAG, "TOUCH x=%d y=%d", touch_x[0], touch_y[0]);
    }

    if (x)       *x       = (got && touch_cnt > 0) ? touch_x[0] : 0;
    if (y)       *y       = (got && touch_cnt > 0) ? touch_y[0] : 0;
    if (pressed) *pressed = (got && touch_cnt > 0);
}

/* ------------------------------------------------------------------------- */

touch_handle_t touch_driver_get_handle(void)
{
    return (touch_handle_t)s_touch;
}
