#pragma once
#include "esp_err.h"

/* Configura IO47 (pull-up, ISR ANYEDGE), arranca door_task (prio 4),
 * publica el estado inicial y registra "door" en status. */
esp_err_t sb_door_sensor_init(void);
