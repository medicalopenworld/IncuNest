#pragma once
#include <stdint.h>

#include "platform/plat_gpio.h"
#include "platform/plat_i2c.h"

// OJO: a fecha del porte a ESP-IDF, NADA de este fichero tiene consumidores
// (verificado con grep sobre src/ e include/: solo se referencia a si mismo).
// Se porta igualmente en vez de borrarlo, porque quitar codigo es una decision
// de producto y no algo que deba colarse dentro de un cambio de toolchain.
// Candidato a borrar en un commit aparte.

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
void     hmi_hal_gpio_set_mode(uint8_t pin, pin_mode_t mode);
uint32_t hmi_hal_adc_read_mv(uint8_t pin);
