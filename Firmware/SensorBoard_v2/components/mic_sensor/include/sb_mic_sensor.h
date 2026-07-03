#pragma once
#include "esp_err.h"

/* Inicializa el canal I2S PDM RX (IO40 clk / IO39 data) y arranca
 * audio_task (prio 4). Registra "mic" en el status. */
esp_err_t sb_mic_sensor_init(void);
