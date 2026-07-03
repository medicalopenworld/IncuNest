#include "sensorBoard_cmd_builder.h"
#include "sensorBoard_comm_protocol.h"
#include "sensorBoard_json.h"
#include <stdio.h>

size_t sb_cmd_build_status(char *buf, size_t buf_size, uint32_t id, uint32_t uptime_ms)
{
    int n = snprintf(buf, buf_size,
                     "{\"type\":\"resp\",\"cmd\":\"status\",\"id\":%lu,"
                     "\"status\":\"ok\",\"device\":\"" SB_PROTO_DEVICE_NAME "\","
                     "\"fw\":\"" SB_PROTO_FW_VERSION "\",\"uptime\":%lu}",
                     (unsigned long)id, (unsigned long)uptime_ms);
    return (n < 0 || n >= (int)buf_size) ? 0 : (size_t)n;
}

size_t sb_cmd_build_error(char *buf, size_t buf_size, const char *cmd, uint32_t id,
                          const char *msg, uint32_t ts_ms)
{
    /* Peor caso: SB_CMD_ECHO_MAX caracteres, todos escapados (2B cada uno) */
    char cmd_esc[2 * SB_CMD_ECHO_MAX + 1];
    sb_json_escape(cmd_esc, sizeof(cmd_esc), (cmd != NULL) ? cmd : "?", SB_CMD_ECHO_MAX);

    int n = snprintf(buf, buf_size,
                     "{\"type\":\"resp\",\"cmd\":\"%s\",\"id\":%lu,"
                     "\"status\":\"error\",\"msg\":\"%s\",\"ts\":%lu}",
                     cmd_esc, (unsigned long)id, msg, (unsigned long)ts_ms);
    return (n < 0 || n >= (int)buf_size) ? 0 : (size_t)n;
}
