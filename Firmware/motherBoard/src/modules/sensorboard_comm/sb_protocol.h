#pragma once
// Constantes del protocolo USB-CDC del SensorBoard, lado motherboard.
// Fuente de verdad del wire format: SensorBoard_v2/README.md y
// SensorBoard_v2/components/usb_comm/include/sensorBoard_comm_protocol.h.
// Este header es una copia deliberada, no un #include cruzado: los dos
// firmwares se compilan y versionan por separado (ESP-IDF vs PlatformIO), y
// SB_PROTO_FW_VERSION en el otro lado es quien decide cuando diverge.
#include <stdint.h>
#include <stddef.h>

#define SB_PROTO_MAGIC_0 0xABu
#define SB_PROTO_MAGIC_1 0xCDu

#define SB_PROTO_TYPE_JSON 0x00u
#define SB_PROTO_TYPE_JPEG 0x01u

#define SB_PROTO_FRAME_HEADER_SIZE 7u  // 2 magic + 1 type + 4 len (LE)
#define SB_PROTO_FRAME_CRC_SIZE 2u     // CRC16 (BE en el frame)

#define SB_PROTO_MAX_JSON_PAYLOAD 256u
#define SB_PROTO_MAX_BINARY_PAYLOAD (128u * 1024u)

// ── Campos y valores JSON ─────────────────────────────────────────
#define SB_JSON_TYPE "type"
#define SB_JSON_CMD "cmd"
#define SB_JSON_ID "id"
#define SB_JSON_STATUS "status"
#define SB_JSON_TS "ts"
#define SB_JSON_DATA "data"
#define SB_JSON_MSG "msg"
#define SB_JSON_SIZE "size"

#define SB_JSON_TYPE_CMD "cmd"
#define SB_JSON_TYPE_RESP "resp"
#define SB_JSON_TYPE_EVENT "event"
#define SB_JSON_TYPE_LOG "log"

#define SB_JSON_STATUS_OK "ok"

#define SB_CMD_STATUS "status"
#define SB_CMD_CAPTURE "capture"
#define SB_CMD_HEARTBEAT "heartbeat"
#define SB_CMD_SENSOR_DATA "sensor_data"
#define SB_CMD_DOOR_OPEN "door_open"
#define SB_CMD_DOOR_CLOSED "door_closed"
#define SB_CMD_SOUND_LEVEL "sound_level"

// Contrato de fail-safe (SensorBoard_v2/README.md #Protocolo): heartbeat
// cada 30 s, enlace se declara perdido a los 3 periodos sin recibir uno.
#define SB_LINK_HEARTBEAT_PERIOD_MS 30000u
#define SB_LINK_TIMEOUT_MS (3u * SB_LINK_HEARTBEAT_PERIOD_MS)
