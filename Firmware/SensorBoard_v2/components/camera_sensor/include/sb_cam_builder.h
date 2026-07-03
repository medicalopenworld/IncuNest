/* Builders puros de las resp de capture. Expuestos para tests Unity. */
#pragma once
#include <stddef.h>
#include <stdint.h>

/* {"type":"resp","cmd":"capture","id":N,"status":"ok","size":S,"ts":T} */
size_t sb_cam_build_capture_ok(char *buf, size_t buf_size, uint32_t id, size_t size_bytes,
                               uint32_t ts_ms);

/* {"type":"resp","cmd":"capture","id":N,"status":"error","msg":"...","ts":T}
 * msg debe ser literal interno. */
size_t sb_cam_build_capture_err(char *buf, size_t buf_size, uint32_t id, const char *msg,
                                uint32_t ts_ms);
