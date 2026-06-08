#pragma once
#include <stdint.h>

/* ── Frame framing ─────────────────────────────────────────── */
#define SB_PROTO_MAGIC_0            0xABu
#define SB_PROTO_MAGIC_1            0xCDu

#define SB_PROTO_TYPE_JSON          0x00u
#define SB_PROTO_TYPE_JPEG          0x01u   /* Phase 5 */

/* Header: 2 magic + 1 type + 4 len */
#define SB_PROTO_FRAME_HEADER_SIZE  7u
#define SB_PROTO_FRAME_CRC_SIZE     2u
#define SB_PROTO_FRAME_OVERHEAD     (SB_PROTO_FRAME_HEADER_SIZE + SB_PROTO_FRAME_CRC_SIZE)

#define SB_PROTO_MAX_JSON_PAYLOAD   256u
/* Max frame buffer needed for JSON: */
#define SB_PROTO_MAX_JSON_FRAME     (SB_PROTO_FRAME_OVERHEAD + SB_PROTO_MAX_JSON_PAYLOAD)

/* ── Device identity ───────────────────────────────────────── */
#define SB_PROTO_FW_VERSION         "1.0.0"
#define SB_PROTO_DEVICE_NAME        "SensorBoard"

/* ── JSON field names ──────────────────────────────────────── */
#define SB_JSON_TYPE        "type"
#define SB_JSON_CMD         "cmd"
#define SB_JSON_ID          "id"
#define SB_JSON_STATUS      "status"
#define SB_JSON_TS          "ts"
#define SB_JSON_DATA        "data"
#define SB_JSON_MSG         "msg"
#define SB_JSON_LEVEL       "level"
#define SB_JSON_TAG         "tag"
#define SB_JSON_DEVICE      "device"
#define SB_JSON_FW          "fw"
#define SB_JSON_UPTIME      "uptime"

/* ── JSON type values ──────────────────────────────────────── */
#define SB_JSON_TYPE_CMD    "cmd"
#define SB_JSON_TYPE_RESP   "resp"
#define SB_JSON_TYPE_EVENT  "event"
#define SB_JSON_TYPE_LOG    "log"

/* ── JSON status values ────────────────────────────────────── */
#define SB_JSON_STATUS_OK   "ok"
#define SB_JSON_STATUS_ERR  "error"

/* ── Commands ──────────────────────────────────────────────── */
#define SB_CMD_STATUS       "status"
