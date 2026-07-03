#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

esp_err_t sensorBoard_comm_init(void);
esp_err_t sensorBoard_comm_send_json(const char *json_str);
/* Variante sin espera para contextos que no deben bloquear (p. ej. handlers
 * de comando en usb_rx): bajo saturación la respuesta se descarta. */
esp_err_t sensorBoard_comm_send_json_noblock(const char *json_str);
esp_err_t sensorBoard_comm_send_binary(uint8_t type, uint8_t *buf, size_t len);

/* Called internally by RX task; implemented in sensorBoard_cmd_handler.c */
void sensorBoard_cmd_handle(const uint8_t *payload, size_t len);
