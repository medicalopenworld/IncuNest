#include "control_module.h"
#include "alarm_machine.h"
#include "pid_wrapper.h"
#include "main.h"

void control_module_init(void) {
  alarm_machine_init();
}

void control_module_security_update(void) {
  securityCheck();
}

void control_module_pid_update(void) {
  PIDHandler();
}
