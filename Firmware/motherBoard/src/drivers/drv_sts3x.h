#pragma once
#include <SensirionI2cSts3x.h>
#include "platform/plat_i2c.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct { float temperature; bool ok; } DrvSts3xResult;

bool           drv_sts3x_init(SensirionI2cSts3x *dev, I2cBus *bus, uint8_t addr);
DrvSts3xResult drv_sts3x_read(SensirionI2cSts3x *dev);
