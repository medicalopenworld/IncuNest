#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

esp_err_t sensorBoard_comm_init(void);
esp_err_t sensorBoard_comm_send_json(const char *json_str);
/* Variante sin espera para contextos que no deben bloquear (p. ej. handlers
 * de comando en usb_rx): bajo saturación la respuesta se descarta. */
esp_err_t sensorBoard_comm_send_json_noblock(const char *json_str);
esp_err_t sensorBoard_comm_send_binary(uint8_t type, uint8_t *buf, size_t len);

/* Espera hasta timeout_ms a que el host pase de ausente a presente (DTR
 * asertado con el bus listo). Devuelve true si ocurrió esa transición durante
 * la espera; false si venció el plazo o el transporte no está inicializado.
 * Pensada para el heartbeat: emitir uno en cuanto vuelve el host acorta la
 * recuperación del enlace en la motherboard de ≤30 s a ~1 s. */
bool sensorBoard_comm_wait_host_ready(uint32_t timeout_ms);

/* Called internally by RX task; implemented in sensorBoard_cmd_handler.c */
void sensorBoard_cmd_handle(const uint8_t *payload, size_t len);
