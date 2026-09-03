#include "sb_json_codec.h"

#include <ArduinoJson.h>
#include <string.h>

#include "sb_protocol.h"

static void copy_msg(SbMessage *out, JsonVariantConst v) {
  const char *s = v | "";
  strncpy(out->msg, s, SB_MSG_MSG_MAX_CHARS);
  out->msg[SB_MSG_MSG_MAX_CHARS] = '\0';
}

// Rellena temp[3]/hum[3] desde un JsonArray; una posicion null (sensor
// caido, ADR-0002 del SensorBoard) queda en *_valid=false sin tocar el
// valor.
//
// Exige tipo NUMERICO, no solo "no null": as<float>() sobre una cadena la
// parsea sin fallar ("ERR" -> 0.0) y sobre un bool devuelve 0.0/1.0, asi que
// un campo emitido como texto se colaba como lectura valida de 0.0 grados.
// Estos valores gobiernan el PID de aire, no son telemetria decorativa.
static bool is_number(JsonVariantConst v) { return v.is<float>(); }

static void copy_optional_array3(JsonVariantConst arr, bool valid[3],
                                 float value[3]) {
  for (int i = 0; i < 3; i++) {
    valid[i] = false;
  }
  if (!arr.is<JsonArrayConst>()) return;
  JsonArrayConst a = arr.as<JsonArrayConst>();
  int i = 0;
  for (JsonVariantConst v : a) {
    if (i >= 3) break;
    if (is_number(v)) {
      valid[i] = true;
      value[i] = v.as<float>();
    }
    i++;
  }
}

bool sb_json_decode(const uint8_t *payload, size_t len, SbMessage *out) {
  memset(out, 0, sizeof(*out));
  out->kind = SB_MSG_UNKNOWN;

  // 1 KB para un payload de 256 B: deserializeJson() copia tambien las
  // claves y cadenas (la entrada es const), y el peor caso real
  // (sensor_data con dos arrays de 3) no cabia en 384 B.
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, payload, len);
  if (err) return false;

  const char *type = doc[SB_JSON_TYPE] | "";
  const char *cmd = doc[SB_JSON_CMD] | "";
  out->ts = doc[SB_JSON_TS] | 0u;

  if (strcmp(type, SB_JSON_TYPE_EVENT) == 0) {
    if (strcmp(cmd, SB_CMD_HEARTBEAT) == 0) {
      out->kind = SB_MSG_HEARTBEAT;
      // El heartbeat no lleva "ts" sino "uptime" (main.c del SensorBoard):
      // es el unico dato que permite detectar que la placa se ha reiniciado
      // sin que el USB llegue a caerse.
      JsonVariantConst up = doc[SB_JSON_UPTIME];
      if (is_number(up)) {
        out->uptime_valid = true;
        out->uptime = up.as<uint32_t>();
      }
    } else if (strcmp(cmd, SB_CMD_SENSOR_DATA) == 0) {
      out->kind = SB_MSG_SENSOR_DATA;
      JsonVariantConst data = doc[SB_JSON_DATA];
      copy_optional_array3(data["temp"], out->temp_valid, out->temp);
      copy_optional_array3(data["hum"], out->hum_valid, out->hum);
      JsonVariantConst lux = data["lux"];
      if (is_number(lux)) {
        out->lux_valid = true;
        out->lux = lux.as<float>();
      }
    } else if (strcmp(cmd, SB_CMD_DOOR_OPEN) == 0) {
      out->kind = SB_MSG_DOOR_OPEN;
    } else if (strcmp(cmd, SB_CMD_DOOR_CLOSED) == 0) {
      out->kind = SB_MSG_DOOR_CLOSED;
    } else if (strcmp(cmd, SB_CMD_SOUND_LEVEL) == 0) {
      out->kind = SB_MSG_SOUND_LEVEL;
      JsonVariantConst dba = doc[SB_JSON_DATA]["dba"];
      if (is_number(dba)) {
        out->dba_valid = true;
        out->dba = dba.as<float>();
      }
    }
  } else if (strcmp(type, SB_JSON_TYPE_LOG) == 0) {
    out->kind = SB_MSG_LOG;
    copy_msg(out, doc[SB_JSON_MSG]);
  } else if (strcmp(type, SB_JSON_TYPE_RESP) == 0) {
    const char *status = doc[SB_JSON_STATUS] | "";
    bool ok = strcmp(status, SB_JSON_STATUS_OK) == 0;
    out->resp_ok = ok;
    out->resp_id = doc[SB_JSON_ID] | 0u;
    if (strcmp(cmd, SB_CMD_STATUS) == 0) {
      out->kind = SB_MSG_STATUS_RESP;
    } else if (strcmp(cmd, SB_CMD_CAPTURE) == 0) {
      out->kind = SB_MSG_CAPTURE_RESP;
      out->capture_size = doc[SB_JSON_SIZE] | 0u;
    }
    if (!ok) copy_msg(out, doc[SB_JSON_MSG]);
  }

  return true;
}

static size_t encode_id_cmd(const char *cmd, uint32_t id, uint8_t *out,
                            size_t out_cap) {
  StaticJsonDocument<96> doc;
  doc[SB_JSON_TYPE] = SB_JSON_TYPE_CMD;
  doc[SB_JSON_CMD] = cmd;
  doc[SB_JSON_ID] = id;

  size_t needed = measureJson(doc);
  if (needed >= out_cap) return 0;  // hueco para el terminador
  size_t written = serializeJson(doc, (char *)out, out_cap);
  if (written != needed) return 0;
  return written;
}

size_t sb_json_encode_status_cmd(uint32_t id, uint8_t *out, size_t out_cap) {
  return encode_id_cmd(SB_CMD_STATUS, id, out, out_cap);
}

size_t sb_json_encode_capture_cmd(uint32_t id, uint8_t *out, size_t out_cap) {
  return encode_id_cmd(SB_CMD_CAPTURE, id, out, out_cap);
}
