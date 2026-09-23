#include "sensor_source.h"

#include "main.h"

extern bool roomSensorPresent[ROOM_SENSOR_POSIBILITIES];

// Por defecto I2C: es el comportamiento historico, asi que un equipo cuyo
// sondeo no llegue a correr se queda como estaba en vez de intentar hablar
// USB con una placa que no existe.
static SensorSource s_source = SENSOR_SOURCE_I2C;

SensorSource sensorSourceGet(void) { return s_source; }

void sensorSourceSet(SensorSource src) { s_source = src; }

bool roomSensorI2CDetected(void) {
  for (int i = 0; i < ROOM_SENSOR_POSIBILITIES; i++) {
    if (roomSensorPresent[i]) return true;
  }
  return false;
}
