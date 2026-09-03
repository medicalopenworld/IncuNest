#pragma once
// Decodifica/codifica los payloads JSON del protocolo SensorBoard
// (SB_PROTO_TYPE_JSON). Pura: opera sobre bytes en memoria, sin tocar el
// dispositivo USB. Usa ArduinoJson igual que el resto de motherBoard
// (GPRS.cpp), no cJSON.
#include <stddef.h>
#include <stdint.h>

typedef enum {
  SB_MSG_UNKNOWN = 0,
  SB_MSG_HEARTBEAT,
  SB_MSG_SENSOR_DATA,
  SB_MSG_DOOR_OPEN,
  SB_MSG_DOOR_CLOSED,
  SB_MSG_SOUND_LEVEL,
  SB_MSG_LOG,
  SB_MSG_STATUS_RESP,
  SB_MSG_CAPTURE_RESP,
} SbMsgKind;

#define SB_MSG_MSG_MAX_CHARS 63

typedef struct {
  SbMsgKind kind;
  uint32_t ts;

  // SB_MSG_HEARTBEAT: el heartbeat lleva "uptime" y no "ts". Sirve para
  // detectar un reinicio del SensorBoard con el USB enumerado (uptime que
  // retrocede), caso en el que las lecturas anteriores dejan de valer.
  bool uptime_valid;
  uint32_t uptime;

  // SB_MSG_SENSOR_DATA: sensores.env_sensors reporta null por posicion
  // caida (ADR-0002 del SensorBoard) -- *_valid distingue "0.0" de "sin
  // lectura", la fusion/votacion queda fuera de este codec.
  bool temp_valid[3];
  float temp[3];
  bool hum_valid[3];
  float hum[3];
  bool lux_valid;
  float lux;

  // SB_MSG_SOUND_LEVEL
  bool dba_valid;
  float dba;

  // SB_MSG_STATUS_RESP / SB_MSG_CAPTURE_RESP
  bool resp_ok;
  uint32_t resp_id;
  uint32_t capture_size;

  // SB_MSG_LOG / mensajes de error de un resp
  char msg[SB_MSG_MSG_MAX_CHARS + 1];
} SbMessage;

// false si el payload no es JSON valido; un JSON valido de tipo/cmd
// desconocido decodifica a SB_MSG_UNKNOWN (true), nunca se descarta en
// silencio sin dejar rastro para quien llama.
bool sb_json_decode(const uint8_t *payload, size_t len, SbMessage *out);

// Codifica el comando saliente {"type":"cmd","cmd":"status","id":N}.
// Devuelve el numero de bytes escritos (0 si no cabe en out_cap).
size_t sb_json_encode_status_cmd(uint32_t id, uint8_t *out, size_t out_cap);

// Codifica {"type":"cmd","cmd":"capture","id":N}.
size_t sb_json_encode_capture_cmd(uint32_t id, uint8_t *out, size_t out_cap);

#define SB_STATUS_FW_MAX_CHARS 15

// Disponibilidad por recurso de la respuesta extendida de "status"
// (design.md D6, shared-factory-test). El test de fabrica (SB_STATUS) es el
// unico consumidor: el resto del firmware no necesita saber que sensores
// tiene la SensorBoard, solo si esta viva (sb_link_state.h).
typedef struct {
  char fw[SB_STATUS_FW_MAX_CHARS + 1];
  bool avail_sht[3];
  bool avail_als;
  bool avail_door;
  bool avail_cam;
  bool usb_swap;
} SbStatusResp;

// Decodifica {"type":"resp","cmd":"status","status":"ok","fw":"...",
// "sensors":{"sht0":bool,"sht1":bool,"sht2":bool,"als":bool,"door":bool,
// "cam":bool,"usb_swap":bool}}. false (sin tocar *out) si el payload no es
// JSON valido, si type/cmd no son resp/status, si "status" no es "ok" o si
// falta el objeto "sensors" completo -- una SHT40 mal soldada no debe poder
// colarse como "disponible" por un campo a medio inicializar.
bool sb_json_decode_status_resp(const uint8_t *payload, size_t len,
                                SbStatusResp *out);
