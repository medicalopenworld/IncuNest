#include "sensorBoard_comm.h"
#include "sensorBoard_comm_protocol.h"
#include "sensorBoard_cmd_builder.h"
#include "sensorBoard_cmd_registry.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdint.h>
#include <string.h>

static const char *TAG = "CMD";

static uint32_t get_id(const cJSON *root)
{
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(root, SB_JSON_ID);
    if (!cJSON_IsNumber(id)) {
        return 0;
    }
    /* double→uint32 fuera de rango es UB en C; el emisor controla el valor.
     * NaN también cae en la primera comparación (NaN >= 0.0 es falso). */
    double v = id->valuedouble;
    if (!(v >= 0.0) || v > (double)UINT32_MAX) {
        return 0;
    }
    return (uint32_t)v;
}

static uint32_t now_ms(void)
{
    /* uint32 da la vuelta a los ~49.7 días de uptime continuo; la motherboard
     * no debe asumir que uptime/ts son monótonos indefinidamente. */
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void send_error(const char *cmd_str, uint32_t id, const char *msg)
{
    char buf[SB_PROTO_MAX_JSON_PAYLOAD];
    if (sb_cmd_build_error(buf, sizeof(buf), cmd_str, id, msg, now_ms()) > 0) {
        /* Contexto usb_rx: nunca bloquear la ruta de recepción por la salida
         * (un flood de comandos inválidos no debe frenar el decoder) */
        sensorBoard_comm_send_json_noblock(buf);
    }
}

static void handle_status(uint32_t id)
{
    char buf[SB_PROTO_MAX_JSON_PAYLOAD];
    if (sb_cmd_build_status(buf, sizeof(buf), id, now_ms()) > 0) {
        sensorBoard_comm_send_json_noblock(buf);
    } else {
        /* Fail-closed pero NO mudo: con demasiados sensores registrados el
         * status no cabría en 256B y el host solo vería un timeout. */
        ESP_LOGE(TAG, "status resp exceeds payload budget — not sent");
    }
}

void sensorBoard_cmd_handle(const uint8_t *payload, size_t len)
{
    if (payload == NULL || len == 0 || len > SB_PROTO_MAX_JSON_PAYLOAD) {
        return;
    }

    /* Copia con terminador para cJSON; el payload viene del buffer del decoder */
    char json_str[SB_PROTO_MAX_JSON_PAYLOAD + 1];
    memcpy(json_str, payload, len);
    json_str[len] = '\0';

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        ESP_LOGW(TAG, "JSON parse error");
        return; /* malformado: descarte sin respuesta */
    }

    const cJSON *cmd = cJSON_GetObjectItemCaseSensitive(root, SB_JSON_CMD);
    uint32_t id = get_id(root);

    if (!cJSON_IsString(cmd) || cmd->valuestring == NULL) {
        send_error("?", id, "missing cmd field");
    } else if (strcmp(cmd->valuestring, SB_CMD_STATUS) == 0) {
        handle_status(id);
    } else {
        /* Comandos de fases (Fase 5+): registrados sin que usb_comm los conozca */
        sb_cmd_handler_t handler = sb_cmd_registry_find(cmd->valuestring);
        if (handler != NULL) {
            handler(id);
        } else {
            ESP_LOGW(TAG, "Unknown cmd");
            send_error(cmd->valuestring, id, "cmd not found");
        }
    }

    cJSON_Delete(root);
}
