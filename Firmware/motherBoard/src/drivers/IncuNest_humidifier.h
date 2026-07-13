/*

        MIT License

        Copyright (c) 2022 Medical Open World, Pablo Sánchez Bergasa

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

#ifndef MAM_IncuNest_humidifier_H
#define MAM_IncuNest_humidifier_H

#include "Arduino.h"
#include "Wire.h"

#define HUMIDIFIER_BINARY 0
#define HUMIDIFIER_PWM 1
#define HUMIDIFIER_I2C 2

typedef enum {
  DEFAULT_ADDRESS = 0x2,
} IncuNestHum_addr_t;

// parameters
typedef enum {
  IN3ATOR_HUM_ON = 0,
} IncuNestHum_param_t;

class MAM_IncuNest_Humidifier {
  // Arduino's I2C library
  TwoWire *_i2c;

  // I2C address
  IncuNestHum_addr_t _i2c_addr;

  // Reads 16 bytes from a parameter.
  void _read(IncuNestHum_param_t param, uint16_t *val);

  // Writes 16 bytes to a parameter.
  void _write(IncuNestHum_param_t param, uint16_t *val);

 public:
  MAM_IncuNest_Humidifier(IncuNestHum_addr_t addr) : _i2c_addr(addr){};

  // Initializes i2c humidifier
  void begin(TwoWire *theWire = &Wire);

  // Initializes i2c humidifier
  void begin(uint16_t mode, uint8_t pin);

  // Gets a parameter value.
  uint16_t getParam(IncuNestHum_param_t param);

  // Resets Humidifier
  void reset();

  // Sets humidifier ON
  void turn(uint16_t mode);
};

#endif