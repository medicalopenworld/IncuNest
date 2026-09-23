/* Registro de comandos por componente (patrón del registro de status):
 * el dispatcher despacha comandos de fases sin conocerlos. */
#pragma once
#include "esp_err.h"
#include <stdint.h>

/* Presupuesto de comandos por fase: 8 entradas para las fases 5+ (capture
 * ocupa 1). Ampliar aquí Y en el test de tabla llena si una fase lo agota. */
#define SB_CMD_REG_MAX 8u
#define SB_CMD_REG_NAME_MAX 12u /* incluye NUL */

/* CONTRATO: el handler corre en el contexto de usb_rx_task (que también
 * decodifica las tramas entrantes) — debe ejecutar en tiempo acotado y sin
 * bloquear: validar + hand-off a una tarea propia; responder solo con
 * sensorBoard_comm_send_json_noblock(). */
typedef void (*sb_cmd_handler_t)(uint32_t id);

/* Alta o reemplazo (idempotente por nombre). ESP_ERR_NO_MEM si está llena;
 * ESP_ERR_INVALID_ARG con nombre NULL/vacío/largo o handler NULL. */
esp_err_t sensorBoard_cmd_register(const char *cmd, sb_cmd_handler_t handler);

/* Interno (expuesto para tests): handler registrado o NULL. */
sb_cmd_handler_t sb_cmd_registry_find(const char *cmd);

/* Solo para tests. */
void sb_cmd_registry_reset(void);
