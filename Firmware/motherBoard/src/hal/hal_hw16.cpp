#include "hal.h"
#include "platform/plat_gpio.h"
#include "platform/plat_pwm.h"

const HalPinConfig g_hal_pins = {
  .buzzer           = 1,
  .heater           = 45,
  .fan              = 12,
  .fanCtl           = 11,
  .fanSpeedFeedback = 38,
  .phototherapy     = 13,
  .actuatorsEn      = 14,
  .screenBacklight  = 46,
  .babyNtcPin       = 8,
  .babyTempEn       = 18,
  .onOffSwitch      = 4,
  .pwrEn            = 2,
  .gsmTxPin         = 9,
  .gsmRxPin         = 10,
  .uartMbTx         = 15,
  .uartMbRx         = 16,
  .i2cSda           = 47,
  .i2cScl           = 48,
  .i2c2Sda          = 20,
  .i2c2Scl          = 19,
  .afeMiso          = 37,
  .afeMosi          = 35,
  .afeSck           = 36,
  .afeAdcReady      = 17,
  .afeCs            = 21,
  .afeLedAlm        = 0xFF,
  .usbEn            = 5,
  .usbFault         = 6,
};

const HalBusConfig g_hal_buses = {
  .i2cSpeedHz  = 10000,
  .i2c2SpeedHz = 10000,
};

void hal_gpio_set_mode(uint8_t pin, pin_mode_t mode) { pin_mode(pin, mode); }
void hal_gpio_write(uint8_t pin, bool value)       { pin_write(pin, value ? HIGH : LOW); }
bool hal_gpio_read(uint8_t pin)                    { return pin_read(pin) == HIGH; }

void hal_pwm_init(uint8_t ch, uint32_t freq, uint8_t res, uint8_t pin) {
  pwm_setup(ch, freq, res);
  pwm_attach(pin, ch);
}
void hal_pwm_write(uint8_t ch, uint32_t duty) { pwm_write(ch, duty); }

bool hal_i2c_write(I2cBus *bus, uint8_t addr, const uint8_t *data, size_t len) {
  return bus->write(addr, data, len);
}
bool hal_i2c_read(I2cBus *bus, uint8_t addr, uint8_t *buf, size_t len) {
  return bus->read(addr, buf, len);
}
uint32_t hal_adc_read_mv(uint8_t pin) { return adc_read_mv(pin); }
