/* Registro agnóstico de disponibilidad de sensores para la resp de status.
 * Las fases registran nombres opacos; usb_comm no sabe qué significan. */
#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

#define SB_STATUS_MAX_SENSORS 8u
#define SB_STATUS_NAME_MAX 12u /* incluye NUL */

/* Alta o actualización (idempotente por nombre). ESP_ERR_NO_MEM si la tabla
 * está llena; ESP_ERR_INVALID_ARG si el nombre es NULL/vacío/demasiado largo. */
esp_err_t sensorBoard_status_set_sensor(const char *name, bool available);

/* Interno (expuesto para tests): escribe `"sensors":{...}` en buf y devuelve
 * la longitud, o 0 si no hay sensores registrados o no cabe. */
size_t sb_status_build_sensors_json(char *buf, size_t buf_size);

/* Solo para tests: vacía la tabla. */
void sb_status_reset(void);
