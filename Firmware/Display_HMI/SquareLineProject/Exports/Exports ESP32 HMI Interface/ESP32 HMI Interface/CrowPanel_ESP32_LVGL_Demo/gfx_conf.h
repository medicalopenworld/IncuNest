#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <driver/i2c.h>

/*******************************************************************************
 * Please define the corresponding macros based on the board you have purchased.
 * CrowPanel_43 means CrowPanel 4.3inch Board
 * CrowPanel_50 means CrowPanel 5.0inch Board
 * CrowPanel_70 means CrowPanel 7.0inch Board
 ******************************************************************************/
#define CrowPanel_70
// #define CrowPanel_50
// #define CrowPanel_43

// --- Configuración Común y específica por Panel ---

#if defined (CrowPanel_70)
    #define SCREEN_WIDTH   800
    #define SCREEN_HEIGHT  480
    #define FREQ_WRITE     12000000
    
    // Pines Bus RGB
    #define PIN_D0  GPIO_NUM_15
    #define PIN_D1  GPIO_NUM_7
    #define PIN_D2  GPIO_NUM_6
    #define PIN_D3  GPIO_NUM_5
    #define PIN_D4  GPIO_NUM_4
    #define PIN_D5  GPIO_NUM_9
    #define PIN_D6  GPIO_NUM_46
    #define PIN_D7  GPIO_NUM_3
    #define PIN_D8  GPIO_NUM_8
    #define PIN_D9  GPIO_NUM_16
    #define PIN_D10 GPIO_NUM_1
    #define PIN_D11 GPIO_NUM_14
    #define PIN_D12 GPIO_NUM_21
    #define PIN_D13 GPIO_NUM_47
    #define PIN_D14 GPIO_NUM_48
    #define PIN_D15 GPIO_NUM_45

    #define PIN_HENABLE GPIO_NUM_41
    #define PIN_VSYNC   GPIO_NUM_40
    #define PIN_HSYNC   GPIO_NUM_39
    #define PIN_PCLK    GPIO_NUM_0

    // Tiempos
    #define H_FRONT_PORCH 40
    #define H_PULSE_WIDTH 48
    #define H_BACK_PORCH  40
    #define V_FRONT_PORCH 1
    #define V_PULSE_WIDTH 31
    #define V_BACK_PORCH  13
    #define PCLK_ACTIVE_NEG 1

    // Touch (GT911)
    #define USE_TOUCH_GT911
    #define TOUCH_SDA GPIO_NUM_19
    #define TOUCH_SCL GPIO_NUM_20

#elif defined (CrowPanel_50)
    #define SCREEN_WIDTH   800
    #define SCREEN_HEIGHT  480
    #define FREQ_WRITE     12000000

    #define PIN_D0  GPIO_NUM_8
    #define PIN_D1  GPIO_NUM_3
    #define PIN_D2  GPIO_NUM_46
    #define PIN_D3  GPIO_NUM_9
    #define PIN_D4  GPIO_NUM_1
    #define PIN_D5  GPIO_NUM_5
    #define PIN_D6  GPIO_NUM_6
    #define PIN_D7  GPIO_NUM_7
    #define PIN_D8  GPIO_NUM_15
    #define PIN_D9  GPIO_NUM_16
    #define PIN_D10 GPIO_NUM_4
    #define PIN_D11 GPIO_NUM_45
    #define PIN_D12 GPIO_NUM_48
    #define PIN_D13 GPIO_NUM_47
    #define PIN_D14 GPIO_NUM_21
    #define PIN_D15 GPIO_NUM_14

    #define PIN_HENABLE GPIO_NUM_40
    #define PIN_VSYNC   GPIO_NUM_41
    #define PIN_HSYNC   GPIO_NUM_39
    #define PIN_PCLK    GPIO_NUM_0

    #define H_FRONT_PORCH 8
    #define H_PULSE_WIDTH 4
    #define H_BACK_PORCH  43
    #define V_FRONT_PORCH 8
    #define V_PULSE_WIDTH 4
    #define V_BACK_PORCH  12
    #define PCLK_ACTIVE_NEG 1

    #define USE_TOUCH_GT911
    #define TOUCH_SDA GPIO_NUM_19
    #define TOUCH_SCL GPIO_NUM_20

#elif defined (CrowPanel_43)
    #define SCREEN_WIDTH   480
    #define SCREEN_HEIGHT  272
    #define FREQ_WRITE     8000000

    #define PIN_D0  GPIO_NUM_8
    #define PIN_D1  GPIO_NUM_3
    #define PIN_D2  GPIO_NUM_46
    #define PIN_D3  GPIO_NUM_9
    #define PIN_D4  GPIO_NUM_1
    #define PIN_D5  GPIO_NUM_5
    #define PIN_D6  GPIO_NUM_6
    #define PIN_D7  GPIO_NUM_7
    #define PIN_D8  GPIO_NUM_15
    #define PIN_D9  GPIO_NUM_16
    #define PIN_D10 GPIO_NUM_4
    #define PIN_D11 GPIO_NUM_45
    #define PIN_D12 GPIO_NUM_48
    #define PIN_D13 GPIO_NUM_47
    #define PIN_D14 GPIO_NUM_21
    #define PIN_D15 GPIO_NUM_14

    #define PIN_HENABLE GPIO_NUM_40
    #define PIN_VSYNC   GPIO_NUM_41
    #define PIN_HSYNC   GPIO_NUM_39
    #define PIN_PCLK    GPIO_NUM_42

    #define H_FRONT_PORCH 8
    #define H_PULSE_WIDTH 4
    #define H_BACK_PORCH  43
    #define V_FRONT_PORCH 8
    #define V_PULSE_WIDTH 4
    #define V_BACK_PORCH  12
    #define PCLK_ACTIVE_NEG 1

    #define USE_TOUCH_XPT2046
    #define TOUCH_SPI_SCLK GPIO_NUM_12
    #define TOUCH_SPI_MOSI GPIO_NUM_11
    #define TOUCH_SPI_MISO GPIO_NUM_13
    #define TOUCH_SPI_CS   GPIO_NUM_0
    #define TOUCH_INT      36

#endif

class LGFX : public lgfx::LGFX_Device
{
public:
    lgfx::Bus_RGB _bus_instance;
    lgfx::Panel_RGB _panel_instance;
    lgfx::Light_PWM _light_instance;
    
#if defined(USE_TOUCH_GT911)
    lgfx::Touch_GT911 _touch_instance;
#elif defined(USE_TOUCH_XPT2046)
    lgfx::Touch_XPT2046 _touch_instance;
#endif

    LGFX(void)
    {
        {
            auto cfg = _panel_instance.config();
            cfg.memory_width = SCREEN_WIDTH;
            cfg.memory_height = SCREEN_HEIGHT;
            cfg.panel_width = SCREEN_WIDTH;
            cfg.panel_height = SCREEN_HEIGHT;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            _panel_instance.config(cfg);
        }

        {
            auto cfg = _bus_instance.config();
            cfg.panel = &_panel_instance;

            cfg.pin_d0 = PIN_D0;
            cfg.pin_d1 = PIN_D1;
            cfg.pin_d2 = PIN_D2;
            cfg.pin_d3 = PIN_D3;
            cfg.pin_d4 = PIN_D4;
            cfg.pin_d5 = PIN_D5;
            cfg.pin_d6 = PIN_D6;
            cfg.pin_d7 = PIN_D7;
            cfg.pin_d8 = PIN_D8;
            cfg.pin_d9 = PIN_D9;
            cfg.pin_d10 = PIN_D10;
            cfg.pin_d11 = PIN_D11;
            cfg.pin_d12 = PIN_D12;
            cfg.pin_d13 = PIN_D13;
            cfg.pin_d14 = PIN_D14;
            cfg.pin_d15 = PIN_D15;

            cfg.pin_henable = PIN_HENABLE;
            cfg.pin_vsync = PIN_VSYNC;
            cfg.pin_hsync = PIN_HSYNC;
            cfg.pin_pclk = PIN_PCLK;
            cfg.freq_write = FREQ_WRITE;

            cfg.hsync_polarity    = 0;
            cfg.hsync_front_porch = H_FRONT_PORCH;
            cfg.hsync_pulse_width = H_PULSE_WIDTH;
            cfg.hsync_back_porch  = H_BACK_PORCH;
            
            cfg.vsync_polarity    = 0;
            cfg.vsync_front_porch = V_FRONT_PORCH;
            cfg.vsync_pulse_width = V_PULSE_WIDTH;
            cfg.vsync_back_porch  = V_BACK_PORCH;

            cfg.pclk_active_neg = PCLK_ACTIVE_NEG;
            cfg.de_idle_high = 0;
            cfg.pclk_idle_high = 0;

            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = GPIO_NUM_2;
            _light_instance.config(cfg);
            _panel_instance.light(&_light_instance);
        }

        {
            auto cfg = _touch_instance.config();
            
#if defined(USE_TOUCH_GT911)
            cfg.x_min      = 0;
            cfg.x_max      = SCREEN_WIDTH - 1;
            cfg.y_min      = 0;
            cfg.y_max      = SCREEN_HEIGHT - 1;
            cfg.pin_int    = -1;
            cfg.pin_rst    = -1;
            cfg.bus_shared = true;
            cfg.offset_rotation = 0;
            cfg.i2c_port   = I2C_NUM_1;
            cfg.pin_sda    = TOUCH_SDA;
            cfg.pin_scl    = TOUCH_SCL;
            cfg.freq       = 400000;
            cfg.i2c_addr   = 0x14;
#elif defined(USE_TOUCH_XPT2046)
            cfg.x_min      = 100;
            cfg.x_max      = 4000;
            cfg.y_min      = 100;
            cfg.y_max      = 4000;
            cfg.pin_int    = TOUCH_INT;
            cfg.bus_shared = true;
            cfg.offset_rotation = 0;
            cfg.spi_host   = SPI2_HOST;
            cfg.freq       = 1000000;
            cfg.pin_sclk   = TOUCH_SPI_SCLK;
            cfg.pin_mosi   = TOUCH_SPI_MOSI;
            cfg.pin_miso   = TOUCH_SPI_MISO;
            cfg.pin_cs     = TOUCH_SPI_CS;
#endif
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }
        setPanel(&_panel_instance);
    }
};

LGFX tft;


