#include "drv_shtc3.h"

bool drv_shtc3_init(SHTC3 *dev, TwoWire *bus) {
  return dev->begin(*bus) == SHTC3_Status_Nominal;
}

DrvShtc3Result drv_shtc3_read(SHTC3 *dev) {
  DrvShtc3Result r = {0.0f, 0.0f, false};
  dev->update();
  if (dev->lastStatus == SHTC3_Status_Nominal) {
    r.temperature = dev->toDegC();
    r.humidity    = dev->toPercent();
    r.ok          = true;
  }
  return r;
}

bool drv_sht4x_init(Adafruit_SHT4x *dev) {
  return dev->begin();
}

DrvShtc3Result drv_sht4x_read(Adafruit_SHT4x *dev) {
  DrvShtc3Result r = {0.0f, 0.0f, false};
  sensors_event_t hum, temp;
  r.ok = dev->getEvent(&hum, &temp);
  if (r.ok) { r.temperature = temp.temperature; r.humidity = hum.relative_humidity; }
  return r;
}
