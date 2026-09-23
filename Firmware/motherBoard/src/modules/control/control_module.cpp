#include "control_module.h"
#include "pid_wrapper.h"
#include "main.h"

// No hay control_module_init(): existia sin un solo llamante y lo unico que
// hacia era llamar a alarm_machine_init(). Eso lo convertia en un SEGUNDO
// punto de reset de la maquina de alarmas, ademas de initAlarms()
// (security.cpp, invocado una vez desde el arranque). Un reset de mas borra
// las condiciones ya declaradas por el autotest de arranque — el patron de
// tener dos puntos de reset ya provoco un fallo real en esta rama.

void control_module_security_update(void) {
  securityCheck();
}

void control_module_pid_update(void) {
  PIDHandler();
}
