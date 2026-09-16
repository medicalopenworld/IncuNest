#ifndef _ADAFRUIT_I2C_REGISTER_H_
#define _ADAFRUIT_I2C_REGISTER_H_

#include <Adafruit_BusIO_Register.h>
// PARCHE INCUNEST: Arduino -> capa de plataforma propia
#include "platform/plat_gpio.h"
#include "platform/plat_num.h"
#include "platform/plat_string.h"
#include "platform/plat_time.h"
#include "platform/plat_types.h"

typedef Adafruit_BusIO_Register Adafruit_I2CRegister;
typedef Adafruit_BusIO_RegisterBits Adafruit_I2CRegisterBits;

#endif
