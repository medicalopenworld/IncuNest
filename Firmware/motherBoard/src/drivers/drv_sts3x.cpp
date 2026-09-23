#include "drv_sts3x.h"

bool drv_sts3x_init(SensirionI2cSts3x *dev, TwoWire *bus, uint8_t addr) {
  dev->begin(*bus, addr);
  return true;
}

DrvSts3xResult drv_sts3x_read(SensirionI2cSts3x *dev) {
  DrvSts3xResult r = {0.0f, false};
  float t = 0.0f;
  int16_t err = dev->measureSingleShot(REPEATABILITY_HIGH, false, t);
  r.ok = (err == 0);
  r.temperature = r.ok ? t : 0.0f;
  return r;
}
