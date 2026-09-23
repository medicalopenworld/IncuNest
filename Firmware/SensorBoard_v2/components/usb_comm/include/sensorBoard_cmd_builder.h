/* Construcción pura (sin USB ni FreeRTOS) de las respuestas JSON del
 * dispatcher, separada del envío para poder verificar el contenido exacto
 * en tests Unity. Interno de usb_comm; en include/ solo para los tests. */
#pragma once
#include <stddef.h>
#include <stdint.h>

/* Máximo de caracteres del cmd entrante que se ecoan en una resp de error
 * (tras escapado); evita que un cmd de 256B trunque el JSON de respuesta. */
#define SB_CMD_ECHO_MAX 32u

/* Devuelven la longitud escrita, o 0 si no cabe (buf queda NUL-terminado). */
size_t sb_cmd_build_status(char *buf, size_t buf_size, uint32_t id, uint32_t uptime_ms);

/* cmd es dato externo: se escapa y trunca a SB_CMD_ECHO_MAX. msg debe ser
 * un literal interno (no se escapa). */
size_t sb_cmd_build_error(char *buf, size_t buf_size, const char *cmd, uint32_t id,
                          const char *msg, uint32_t ts_ms);
