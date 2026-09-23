/* Interno entre componentes: acceso al bus I2C principal (IO4/IO5) que
 * env_sensors crea y que la Fase 5 (cámara, SCCB) debe compartir — el driver
 * i2c_master no permite dos buses en el mismo puerto. No es API para main. */
#pragma once
#include "driver/i2c_master.h"

/* NULL si el bus principal no llegó a inicializarse. */
i2c_master_bus_handle_t sb_env_get_main_i2c_bus(void);
