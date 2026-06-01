#pragma once
#include <SparkFun_SHTC3.h>
#include <Adafruit_SHT4x.h>
#include <Wire.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct { float temperature; float humidity; bool ok; } DrvShtc3Result;

bool           drv_shtc3_init(SHTC3 *dev, TwoWire *bus);
DrvShtc3Result drv_shtc3_read(SHTC3 *dev);

bool           drv_sht4x_init(Adafruit_SHT4x *dev);
DrvShtc3Result drv_sht4x_read(Adafruit_SHT4x *dev);
