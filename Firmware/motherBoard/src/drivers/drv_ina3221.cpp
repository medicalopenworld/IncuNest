#include "drv_ina3221.h"

bool drv_ina3221_init(Beastdevices_INA3221 *dev) {
  dev->begin();
  dev->reset();
  return true;
}

DrvIna3221Result drv_ina3221_read(Beastdevices_INA3221 *dev, ina3221_ch_t channel) {
  DrvIna3221Result r = {0.0f, 0.0f, true};
  r.current_ma = dev->getCurrent(channel) * 1000.0f;
  r.voltage_mv = dev->getVoltage(channel) * 1000.0f;
  return r;
}
