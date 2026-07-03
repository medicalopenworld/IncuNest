#include "sensorBoard_comm.h"
#include "sensorBoard_comm_protocol.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "CMD";

static uint32_t get_id(const cJSON *root)
{
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(root, SB_JSON_ID);
    return (cJSON_IsNumber(id)) ? (uint32_t)id->valuedouble : 0;
}

static void send_error(const char *cmd_str, uint32_t id, const char *msg)
{
    char buf[SB_PROTO_MAX_JSON_PAYLOAD];
    uint32_t ts = (uint32_t)(esp_timer_get_time() / 1000);
    snprintf(buf, sizeof(buf),
             "{\"type\":\"resp\",\"cmd\":\"%s\",\"id\":%lu,"
             "\"status\":\"error\",\"msg\":\"%s\",\"ts\":%lu}",
             cmd_str, (unsigned long)id, msg, (unsigned long)ts);
    sensorBoard_comm_send_json(buf);
}

static void handle_status(uint32_t id)
{
    char buf[SB_PROTO_MAX_JSON_PAYLOAD];
    uint32_t uptime_ms = (uint32_t)(esp_timer_get_time() / 1000);
    snprintf(buf, sizeof(buf),
             "{\"type\":\"resp\",\"cmd\":\"status\",\"id\":%lu,"
             "\"status\":\"ok\",\"device\":\"" SB_PROTO_DEVICE_NAME "\","
             "\"fw\":\"" SB_PROTO_FW_VERSION "\",\"uptime\":%lu}",
             (unsigned long)id, (unsigned long)uptime_ms);
    sensorBoard_comm_send_json(buf);
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
        ESP_LOGW(TAG, "Unknown cmd: %s", cmd->valuestring);
        send_error(cmd->valuestring, id, "cmd not found");
    }

    cJSON_Delete(root);
}
