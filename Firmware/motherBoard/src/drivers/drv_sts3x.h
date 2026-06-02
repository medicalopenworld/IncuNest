#pragma once
#include <SensirionI2cSts3x.h>
#include <Wire.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct { float temperature; bool ok; } DrvSts3xResult;

bool           drv_sts3x_init(SensirionI2cSts3x *dev, TwoWire *bus, uint8_t addr);
DrvSts3xResult drv_sts3x_read(SensirionI2cSts3x *dev);
