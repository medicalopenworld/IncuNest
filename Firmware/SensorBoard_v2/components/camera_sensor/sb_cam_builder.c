#include "sb_cam_builder.h"
#include <stdio.h>

size_t sb_cam_build_capture_ok(char *buf, size_t buf_size, uint32_t id, size_t size_bytes,
                               uint32_t ts_ms)
{
    if (buf == NULL || buf_size == 0) {
        return 0;
    }
    int n = snprintf(buf, buf_size,
                     "{\"type\":\"resp\",\"cmd\":\"capture\",\"id\":%lu,"
                     "\"status\":\"ok\",\"size\":%u,\"ts\":%lu}",
                     (unsigned long)id, (unsigned)size_bytes, (unsigned long)ts_ms);
    return (n < 0 || n >= (int)buf_size) ? 0 : (size_t)n;
}

sb_cam_gate_t sb_cam_gate(int ready, int busy, uint32_t busy_elapsed_ms, uint32_t stall_ms)
{
    if (!ready) {
        return SB_CAM_GATE_NOT_READY;
    }
    if (busy) {
        return (busy_elapsed_ms > stall_ms) ? SB_CAM_GATE_STALLED : SB_CAM_GATE_BUSY;
    }
    return SB_CAM_GATE_ACCEPT;
}

size_t sb_cam_build_capture_err(char *buf, size_t buf_size, uint32_t id, const char *msg,
                                uint32_t ts_ms)
{
    if (buf == NULL || buf_size == 0 || msg == NULL) {
        return 0;
    }
    int n = snprintf(buf, buf_size,
                     "{\"type\":\"resp\",\"cmd\":\"capture\",\"id\":%lu,"
                     "\"status\":\"error\",\"msg\":\"%s\",\"ts\":%lu}",
                     (unsigned long)id, msg, (unsigned long)ts_ms);
    return (n < 0 || n >= (int)buf_size) ? 0 : (size_t)n;
}
