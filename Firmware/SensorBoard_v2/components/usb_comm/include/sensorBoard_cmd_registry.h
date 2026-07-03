/* Registro de comandos por componente (patrón del registro de status):
 * el dispatcher despacha comandos de fases sin conocerlos. */
#pragma once
#include "esp_err.h"
#include <stdint.h>

#define SB_CMD_REG_MAX 4u
#define SB_CMD_REG_NAME_MAX 12u /* incluye NUL */

typedef void (*sb_cmd_handler_t)(uint32_t id);

/* Alta o reemplazo (idempotente por nombre). ESP_ERR_NO_MEM si está llena;
 * ESP_ERR_INVALID_ARG con nombre NULL/vacío/largo o handler NULL. */
esp_err_t sensorBoard_cmd_register(const char *cmd, sb_cmd_handler_t handler);

/* Interno (expuesto para tests): handler registrado o NULL. */
sb_cmd_handler_t sb_cmd_registry_find(const char *cmd);

/* Solo para tests. */
void sb_cmd_registry_reset(void);
