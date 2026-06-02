#include "sensors_module.h"
#include "main.h"

// Sensors module — thin facade over existing sensor functions.
// Calls existing acquisition functions; future phases will inline
// the logic here and remove the originals.

void sensors_module_init(void) {
  initRoomSensor();
  initAmbientSensor();
  initSkinSensor();
}

void sensors_module_update(void) {
  measureSkinSensor();
  updateRoomSensor();
  updateAmbientSensor();
  powerMonitor();
  fanSpeedHandler();
}
