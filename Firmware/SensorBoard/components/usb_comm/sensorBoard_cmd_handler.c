#include "sensorBoard_comm.h"
#include "sensorBoard_comm_protocol.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static const char *TAG = "CMD";

static void send_error(uint32_t id, const char *cmd_name, const char *msg)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) { return; }
    cJSON_AddStringToObject(root, "type", "resp");
    cJSON_AddStringToObject(root, "cmd", cmd_name ? cmd_name : "unknown");
    cJSON_AddNumberToObject(root, "id", (double)id);
    cJSON_AddStringToObject(root, "status", "error");
    cJSON_AddStringToObject(root, "msg", msg);
    cJSON_AddNumberToObject(root, "ts", (double)(esp_timer_get_time() / 1000ULL));

    char buf[SB_PROTO_MAX_JSON_PAYLOAD];
    if (cJSON_PrintPreallocated(root, buf, sizeof(buf), 0)) {
        sensorBoard_comm_send_json(buf);
    }
    cJSON_Delete(root);
}

static void handle_status(uint32_t id)
{
    char buf[SB_PROTO_MAX_JSON_PAYLOAD];
    uint32_t uptime_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    int n = snprintf(buf, sizeof(buf),
        "{\"type\":\"resp\",\"cmd\":\"status\",\"id\":%lu,\"status\":\"ok\","
        "\"device\":\"" SB_PROTO_DEVICE_NAME "\",\"fw\":\"" SB_PROTO_FW_VERSION "\","
        "\"uptime\":%lu}",
        (unsigned long)id,
        (unsigned long)uptime_ms);
    if (n > 0 && n < (int)sizeof(buf)) {
        sensorBoard_comm_send_json(buf);
    } else {
        ESP_LOGE(TAG, "status response too large");
    }
}

void sensorBoard_cmd_handle(const uint8_t *payload, size_t len)
{
    if (!payload || len == 0) {
        return;
    }

    /* Copy to a local null-terminated buffer.
     * The frame decoder drops frames where payload_len > payload_buf_size,
     * but does NOT guarantee payload_len < payload_buf_size (equality is
     * possible), so we cannot safely write payload[len] into the original
     * buffer. Use a local stack copy with room for the null terminator. */
    char json_str[SB_PROTO_MAX_JSON_PAYLOAD + 1];
    if (len > SB_PROTO_MAX_JSON_PAYLOAD) {
        len = SB_PROTO_MAX_JSON_PAYLOAD;
    }
    memcpy(json_str, payload, len);
    json_str[len] = '\0';

    cJSON *root = cJSON_ParseWithLength(json_str, len);
    if (!root) {
        ESP_LOGW(TAG, "invalid JSON");
        return;
    }

    uint32_t id = 0;
    const cJSON *id_item = cJSON_GetObjectItemCaseSensitive(root, "id");
    if (cJSON_IsNumber(id_item)) {
        id = (uint32_t)id_item->valuedouble;
    }

    const cJSON *cmd_item = cJSON_GetObjectItemCaseSensitive(root, "cmd");
    const char *cmd = cJSON_IsString(cmd_item) ? cmd_item->valuestring : NULL;

    if (!cmd) {
        send_error(id, "unknown", "missing cmd field");
        cJSON_Delete(root);
        return;
    }

    if (strcmp(cmd, SB_CMD_STATUS) == 0) {
        handle_status(id);
    } else {
        ESP_LOGW(TAG, "unknown cmd: %s", cmd);
        send_error(id, cmd, "cmd not found");
    }

    cJSON_Delete(root);
}
