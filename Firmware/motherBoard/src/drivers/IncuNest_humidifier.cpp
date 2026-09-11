/*

        Arduino library for IncuNest humidifier.

        MIT License

        Copyright (c) 2020 Medical Open World, Pablo Sánchez Bergasa

        Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"), to
   deal in the Software without restriction, including without limitation the
   rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
   sell copies of the Software, and to permit persons to whom the Software is
        furnished to do so, subject to the following conditions:

        The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

        THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
        FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
        LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
   IN THE SOFTWARE.

*/

#include "IncuNest_humidifier.h"

#include "main.h"

int activationMode;
int controlPin;

void MAM_IncuNest_Humidifier::_read(IncuNestHum_param_t param, uint16_t *val) {
  // Porte a ESP-IDF: antes eran dos transacciones sueltas (escribir el
  // parametro, soltar el bus, pedir 2 bytes). writeRead() las une con START
  // repetido, sin soltar el bus entre medias — mas robusto y una transaccion
  // menos, pero OJO: si este esclavo necesitase el STOP intermedio habria que
  // volver a separarlas. Comprobar en banco la lectura de humedad.
  uint8_t rx[2] = {0, 0};
  const uint8_t reg = (uint8_t)param;
  const bool ok = _i2c->writeRead(_i2c_addr, &reg, 1, rx, sizeof(rx));

  if (ok) {
    *val = ((_i2c->read() << 8) | _i2c->read());
  }
}

void MAM_IncuNest_Humidifier::_write(IncuNestHum_param_t param, uint16_t *val) {
  // Mismo orden de bytes en el bus que antes: parametro, byte alto, byte bajo.
  const uint8_t payload[3] = {
      (uint8_t)param,                  // parameter
      (uint8_t)((*val >> 8) & 0xFF),   // Upper 8-bits
      (uint8_t)(*val & 0xFF),          // Lower 8-bits
  };
  _i2c->write(_i2c_addr, payload, sizeof(payload));
}

void MAM_IncuNest_Humidifier::begin(I2cBus *theWire) {
  _i2c = theWire;
  _i2c->begin();
  activationMode = HUMIDIFIER_I2C;
}

void MAM_IncuNest_Humidifier::begin(uint16_t mode, uint8_t pin) {
  activationMode = mode;
  controlPin = pin;
}

uint16_t MAM_IncuNest_Humidifier::getParam(IncuNestHum_param_t param) {
  uint16_t val = 0;
  _read(param, &val);
  return val;
}

void MAM_IncuNest_Humidifier::reset() {}

void MAM_IncuNest_Humidifier::turn(uint16_t mode) {
  int16_t val = 0;
  switch (activationMode) {
  case HUMIDIFIER_BINARY:
    pin_write(controlPin, mode);
    break;
  case HUMIDIFIER_PWM:
    // HUMIDIFIER_CTL to 115Khz
    pwm_write(HUMIDIFIER_PWM_CHANNEL, (PWM_MAX_VALUE / 2) * mode);
    break;
  case HUMIDIFIER_I2C:
  default:
    if (mode) {
      val = 1;
    }
    _write(IN3ATOR_HUM_ON, (uint16_t *)&val);
    logI("HUMIDIFIER I2C: " + String(mode));
    break;
  }
}
