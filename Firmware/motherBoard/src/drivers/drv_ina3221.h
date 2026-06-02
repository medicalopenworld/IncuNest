#pragma once
#include <Beastdevices_INA3221.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct { float current_ma; float voltage_mv; bool ok; } DrvIna3221Result;

bool             drv_ina3221_init(Beastdevices_INA3221 *dev);
DrvIna3221Result drv_ina3221_read(Beastdevices_INA3221 *dev, ina3221_ch_t channel);
