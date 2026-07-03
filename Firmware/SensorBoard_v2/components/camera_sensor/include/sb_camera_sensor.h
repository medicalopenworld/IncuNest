#pragma once
#include "esp_err.h"

/* Inicializa la OV2640 (DVP + SCCB en el bus I2C principal) y registra el
 * comando "capture". Registra "cam" en el status. */
esp_err_t sb_camera_sensor_init(void);
