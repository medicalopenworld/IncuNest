#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <stdint.h>

typedef struct {
  uint8_t buzzer;
  uint8_t heater;
  uint8_t fan;
  uint8_t fanCtl;
  uint8_t fanSpeedFeedback;
  uint8_t phototherapy;
  uint8_t actuatorsEn;
  uint8_t screenBacklight;
  uint8_t babyNtcPin;
  uint8_t babyTempEn;
  uint8_t onOffSwitch;
  uint8_t pwrEn;
  uint8_t gsmTxPin;
  uint8_t gsmRxPin;
  uint8_t uartMbTx;
  uint8_t uartMbRx;
  uint8_t i2cSda;
  uint8_t i2cScl;
  uint8_t i2c2Sda;
  uint8_t i2c2Scl;
  uint8_t afeMiso;
  uint8_t afeMosi;
  uint8_t afeSck;
  uint8_t afeAdcReady;
  uint8_t afeCs;
  uint8_t afeLedAlm;
  uint8_t usbEn;
  uint8_t usbFault;
} HalPinConfig;

typedef struct {
  uint32_t i2cSpeedHz;
  uint32_t i2c2SpeedHz;
} HalBusConfig;

extern const HalPinConfig  g_hal_pins;
extern const HalBusConfig  g_hal_buses;

void     hal_gpio_set_mode(uint8_t pin, uint8_t mode);
void     hal_gpio_write(uint8_t pin, bool value);
bool     hal_gpio_read(uint8_t pin);
void     hal_pwm_init(uint8_t channel, uint32_t freq_hz, uint8_t resolution_bits, uint8_t pin);
void     hal_pwm_write(uint8_t channel, uint32_t duty);
bool     hal_i2c_write(TwoWire *bus, uint8_t addr, const uint8_t *data, size_t len);
bool     hal_i2c_read (TwoWire *bus, uint8_t addr, uint8_t *buf,        size_t len);
uint32_t hal_adc_read_mv(uint8_t pin);
