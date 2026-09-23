#pragma once

// Sin control_module_init(): la maquina de alarmas se inicializa en un unico
// sitio, initAlarms() (security.cpp). Ver el comentario de control_module.cpp.
void control_module_security_update(void);
void control_module_pid_update(void);
