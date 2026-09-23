#pragma once
#include "esp_err.h"

/* Inicializa buses I2C, ADC del ALS y arranca sensor_task (prio 4).
 * Registra sht0/sht1/sht2/als en el status de usb_comm. */
esp_err_t sb_env_sensors_init(void);
