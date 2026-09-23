#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <stdint.h>

typedef struct {
  uint8_t buzzer;
  uint8_t i2cSda;
  uint8_t i2cScl;
  uint8_t touchIrq;
  uint8_t touchRst;
  uint8_t screenBacklight;
  uint8_t uartMbTx;
  uint8_t uartMbRx;
} HmiPinConfig;

typedef struct {
  uint32_t i2cSpeedHz;
} HmiBusConfig;

extern const HmiPinConfig g_hmi_pins;
extern const HmiBusConfig g_hmi_buses;

void     hmi_hal_gpio_write(uint8_t pin, bool value);
bool     hmi_hal_gpio_read(uint8_t pin);
void     hmi_hal_gpio_set_mode(uint8_t pin, uint8_t mode);
uint32_t hmi_hal_adc_read_mv(uint8_t pin);
