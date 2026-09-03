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
#define SB_JSON_UPTIME "uptime"

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

// ── Cadencias del SensorBoard y su caducidad ──────────────────────
// ATADAS a su Kconfig: CONFIG_SB_ENV_POLL_PERIOD_S,
// CONFIG_SB_MIC_PUBLISH_PERIOD_S y CONFIG_SB_DOOR_REASSERT_S. Cambiar una
// sola de las dos partes rompe la cadena de abajo.
//
// CADENA DE SEGURIDAD DE LA TEMPERATURA DE AIRE (equipo con SensorBoard):
//   publica cada 1 s -> se considera rancia a los 3 s (3 periodos, mismo
//   criterio de 3 periodos que el heartbeat) -> al estar rancia deja de
//   refrescarse lastSuccesfullSensorUpdate[ROOM_DIGITAL_TEMP_SENSOR] -> a los
//   5 s de ese sello (MINIMUM_SUCCESSFULL_AIR_SENSOR_UPDATE, security.cpp)
//   salta ALARM_AIR_SENSOR_FAULT, que es ALTA y corta el calefactor.
// Con la cadencia de 5 s que traia el SensorBoard originalmente, una sola
// publicacion perdida (las descarta sin reintento bajo backpressure) cortaba
// el calefactor: de ahi que la cadencia tenga que ser 1 s.
// Periodo MEDIDO en banco (2026-09-02, SensorBoard fw 1.0.0): 1034 ms de
// media, min 1034 / max 1036 sobre 20 muestras. No son 1000 clavados: el
// polling arrastra el tiempo de conversion de los tres SHT40.
#define SB_ENV_PUBLISH_PERIOD_MS 1034u
// 3.5 periodos reales. Con los 3000 ms de "3 x 1000" que habia antes, perder
// DOS publicaciones seguidas (3 x 1034 = 3102 ms) ya declaraba el dato rancio
// y acababa cortando el calefactor; con 3600 hacen falta tres. La cuenta
// completa hasta el corte es: ultimo dato bueno -> 3.6 s rancio -> deja de
// refrescarse el sello -> +5 s (MINIMUM_SUCCESSFULL_AIR_SENSOR_UPDATE) ->
// ALARM_AIR_SENSOR_FAULT. Unos 8.6 s, muy por debajo de la constante termica
// de la incubadora.
#define SB_ENV_STALE_MS 3600u

#define SB_SOUND_PUBLISH_PERIOD_MS 5000u
#define SB_SOUND_STALE_MS (3u * SB_SOUND_PUBLISH_PERIOD_MS)

// La puerta solo emite en cambios estables, pero re-afirma su estado cada
// 30 s: esa re-asercion es lo que permite detectar que ha dejado de hablar.
#define SB_DOOR_REASSERT_PERIOD_MS 30000u
#define SB_DOOR_STALE_MS (3u * SB_DOOR_REASSERT_PERIOD_MS)
