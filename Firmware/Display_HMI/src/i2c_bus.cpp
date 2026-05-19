#ifdef USE_IDF_FRAMEWORK
#include "i2c_bus.h"
#include "main.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "I2C";

static i2c_master_bus_handle_t s_bus = NULL;
static i2c_master_dev_handle_t s_bl_dev = NULL;

void i2c_bus_init(void) {
    if (s_bus) return;

    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = I2C_NUM_0;
    bus_cfg.sda_io_num = (gpio_num_t)TOUCH_SDA_PIN;
    bus_cfg.scl_io_num = (gpio_num_t)TOUCH_SCL_PIN;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;

    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus: %s", esp_err_to_name(err));
        return;
    }

    // Register backlight device (CrowPanel STC8H1K28 @ 0x30)
    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = (uint16_t)I2C_ADDR_BACKLIGHT;
    dev_cfg.scl_speed_hz = 100000;
    err = i2c_master_bus_add_device(s_bus, &dev_cfg, &s_bl_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_add_device (backlight): %s", esp_err_to_name(err));
    }
}

void i2c_send(uint8_t dev_addr, const uint8_t *data, size_t len) {
    if (!s_bus) return;
    if (dev_addr == (uint8_t)I2C_ADDR_BACKLIGHT && s_bl_dev) {
        i2c_master_transmit(s_bl_dev, data, len, pdMS_TO_TICKS(50));
        return;
    }
    // Generic path: add device on the fly (for future peripherals)
    i2c_master_dev_handle_t dev = NULL;
    i2c_device_config_t dcfg = {};
    dcfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dcfg.device_address = dev_addr;
    dcfg.scl_speed_hz = 100000;
    if (i2c_master_bus_add_device(s_bus, &dcfg, &dev) == ESP_OK) {
        i2c_master_transmit(dev, data, len, pdMS_TO_TICKS(50));
        i2c_master_bus_rm_device(dev);
    }
}

#endif // USE_IDF_FRAMEWORK
