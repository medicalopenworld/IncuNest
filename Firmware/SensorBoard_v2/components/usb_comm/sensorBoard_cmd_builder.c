#include "sensorBoard_cmd_builder.h"
#include "sensorBoard_comm_protocol.h"
#include "sensorBoard_json.h"
#include "sensorBoard_status.h"
#include <stdio.h>

size_t sb_cmd_build_status(char *buf, size_t buf_size, uint32_t id, uint32_t uptime_ms)
{
    int n = snprintf(buf, buf_size,
                     "{\"type\":\"resp\",\"cmd\":\"status\",\"id\":%lu,"
                     "\"status\":\"ok\",\"device\":\"" SB_PROTO_DEVICE_NAME "\","
                     "\"fw\":\"" SB_PROTO_FW_VERSION "\",\"uptime\":%lu",
                     (unsigned long)id, (unsigned long)uptime_ms);
    if (n < 0 || n >= (int)buf_size) {
        return 0;
    }

    /* Fases 2-5: disponibilidad registrada vía sensorBoard_status_set_sensor */
    char sensors[SB_STATUS_MAX_SENSORS * (SB_STATUS_NAME_MAX + 10) + 16];
    if (sb_status_build_sensors_json(sensors, sizeof(sensors)) > 0) {
        int m = snprintf(buf + n, buf_size - (size_t)n, ",%s", sensors);
        if (m < 0 || m >= (int)(buf_size - (size_t)n)) {
            return 0;
        }
        n += m;
    }

    if ((size_t)n + 2 > buf_size) {
        return 0;
    }
    buf[n++] = '}';
    buf[n] = '\0';
    return (size_t)n;
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
