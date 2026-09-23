#include "hal.h"
#include <Arduino.h>

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
  .afeLedAlm        = 7,
  .usbEn            = 5,
  .usbFault         = 6,
};

const HalBusConfig g_hal_buses = {
  .i2cSpeedHz  = 10000,
  .i2c2SpeedHz = 10000,
};

void hal_gpio_set_mode(uint8_t pin, uint8_t mode) { pinMode(pin, mode); }
void hal_gpio_write(uint8_t pin, bool value)       { digitalWrite(pin, value ? HIGH : LOW); }
bool hal_gpio_read(uint8_t pin)                    { return digitalRead(pin) == HIGH; }

void hal_pwm_init(uint8_t ch, uint32_t freq, uint8_t res, uint8_t pin) {
  ledcSetup(ch, freq, res);
  ledcAttachPin(pin, ch);
}
void hal_pwm_write(uint8_t ch, uint32_t duty) { ledcWrite(ch, duty); }

bool hal_i2c_write(TwoWire *bus, uint8_t addr, const uint8_t *data, size_t len) {
  bus->beginTransmission(addr);
  bus->write(data, len);
  return bus->endTransmission() == 0;
}
bool hal_i2c_read(TwoWire *bus, uint8_t addr, uint8_t *buf, size_t len) {
  if (bus->requestFrom((uint8_t)addr, (uint8_t)len) != (uint8_t)len) return false;
  for (size_t i = 0; i < len; i++) buf[i] = bus->read();
  return true;
}
uint32_t hal_adc_read_mv(uint8_t pin) { return analogReadMilliVolts(pin); }
