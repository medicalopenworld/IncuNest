#include "hal_hmi.h"
#include <Arduino.h>

// Pin values sourced from display_config.h and include/main.h:
//   Buzzer: controlled via I2C @ 0x30 (STC8H1K28) — no dedicated GPIO pin.
//           Using DISPLAY_PIN_BL (GPIO 2) as the I2C-backlight/buzzer bus pin.
//   I2C SDA/SCL: DISPLAY_TOUCH_SDA=15, DISPLAY_TOUCH_SCL=16 (main.h / display_config.h)
//   Touch INT/RST: -1 (managed via PCA9557 IO expander, not a direct GPIO)
//   Backlight: DISPLAY_PIN_BL=2 (display_config.h)
//   UART MB TX/RX: HMI communicates via USB CDC (Serial) — no dedicated HW UART pins.
//                  Placeholder value 0 used; actual comms handled by CommTask.

const HmiPinConfig g_hmi_pins = {
  .buzzer          = 2,   // DISPLAY_PIN_BL / I2C bus line (buzzer via I2C 0x30)
  .i2cSda          = 15,  // DISPLAY_TOUCH_SDA / TOUCH_SDA_PIN
  .i2cScl          = 16,  // DISPLAY_TOUCH_SCL / TOUCH_SCL_PIN
  .touchIrq        = (uint8_t)(-1), // DISPLAY_TOUCH_INT — not a direct GPIO
  .touchRst        = (uint8_t)(-1), // DISPLAY_TOUCH_RST — via PCA9557 expander
  .screenBacklight = 2,   // DISPLAY_PIN_BL / TFT_BL_PIN
  .uartMbTx        = 0,   // HMI uses USB CDC (Serial) — no dedicated UART TX pin
  .uartMbRx        = 0,   // HMI uses USB CDC (Serial) — no dedicated UART RX pin
};

const HmiBusConfig g_hmi_buses = {
  .i2cSpeedHz = 400000, // DISPLAY_I2C_FREQ_TOUCH
};

void     hmi_hal_gpio_write(uint8_t pin, bool value) { digitalWrite(pin, value ? HIGH : LOW); }
bool     hmi_hal_gpio_read(uint8_t pin)              { return digitalRead(pin) == HIGH; }
void     hmi_hal_gpio_set_mode(uint8_t pin, uint8_t mode) { pinMode(pin, mode); }
uint32_t hmi_hal_adc_read_mv(uint8_t pin)            { return analogReadMilliVolts(pin); }
