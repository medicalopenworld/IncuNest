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

/* Decisión pura del gate de capture (testable sin cámara):
 * - no ready → NOT_READY
 * - busy y la captura en curso excede stall_ms → STALLED (cámara colgada:
 *   se reporta el fallo real, no "busy" eterno)
 * - busy → BUSY
 * - si no → ACCEPT */
typedef enum {
    SB_CAM_GATE_ACCEPT = 0,
    SB_CAM_GATE_NOT_READY,
    SB_CAM_GATE_BUSY,
    SB_CAM_GATE_STALLED,
} sb_cam_gate_t;

sb_cam_gate_t sb_cam_gate(int ready, int busy, uint32_t busy_elapsed_ms, uint32_t stall_ms);
