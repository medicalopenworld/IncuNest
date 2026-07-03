#include "sb_cam_builder.h"

size_t sb_cam_build_capture_ok(char *buf, size_t buf_size, uint32_t id, size_t size_bytes,
                               uint32_t ts_ms)
{
    (void)buf;
    (void)buf_size;
    (void)id;
    (void)size_bytes;
    (void)ts_ms;
    return 0;
}

size_t sb_cam_build_capture_err(char *buf, size_t buf_size, uint32_t id, const char *msg,
                                uint32_t ts_ms)
{
    (void)buf;
    (void)buf_size;
    (void)id;
    (void)msg;
    (void)ts_ms;
    return 0;
}
