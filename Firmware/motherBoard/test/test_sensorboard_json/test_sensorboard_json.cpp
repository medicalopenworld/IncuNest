#include <unity.h>
#include <string.h>

#include "modules/sensorboard_comm/sb_json_codec.h"

void setUp(void) {}
void tearDown(void) {}

static bool decode_str(const char *json, SbMessage *out) {
  return sb_json_decode((const uint8_t *)json, strlen(json), out);
}

void test_heartbeat_event(void) {
  SbMessage m;
  TEST_ASSERT_TRUE(
      decode_str("{\"type\":\"event\",\"cmd\":\"heartbeat\",\"uptime\":5200}", &m));
  TEST_ASSERT_EQUAL(SB_MSG_HEARTBEAT, m.kind);
  // El heartbeat trae "uptime", no "ts": es la unica pista de un reinicio del
  // SensorBoard con el USB todavia enumerado.
  TEST_ASSERT_TRUE(m.uptime_valid);
  TEST_ASSERT_EQUAL_UINT32(5200, m.uptime);
}

// as<float>() no falla nunca: parsea cadenas y convierte booleanos, asi que
// sin comprobacion de tipo un "ERR" entraba como lectura valida de 0.0 grados
// -- y estas lecturas gobiernan el PID de aire.
void test_non_numeric_readings_are_not_valid(void) {
  SbMessage m;
  TEST_ASSERT_TRUE(decode_str(
      "{\"type\":\"event\",\"cmd\":\"sensor_data\","
      "\"data\":{\"temp\":[\"ERR\",true,36.8],\"hum\":[{},\"55\",54.5],"
      "\"lux\":\"320\"},\"ts\":1}",
      &m));
  TEST_ASSERT_EQUAL(SB_MSG_SENSOR_DATA, m.kind);
  TEST_ASSERT_FALSE(m.temp_valid[0]);  // "ERR"
  TEST_ASSERT_FALSE(m.temp_valid[1]);  // true
  TEST_ASSERT_TRUE(m.temp_valid[2]);   // 36.8
  TEST_ASSERT_FALSE(m.hum_valid[0]);   // {}
  TEST_ASSERT_FALSE(m.hum_valid[1]);   // "55"
  TEST_ASSERT_TRUE(m.hum_valid[2]);    // 54.5
  TEST_ASSERT_FALSE(m.lux_valid);      // "320"
}

void test_non_numeric_sound_level_is_not_valid(void) {
  SbMessage m;
  TEST_ASSERT_TRUE(decode_str(
      "{\"type\":\"event\",\"cmd\":\"sound_level\",\"data\":{\"dba\":\"nan\"}}",
      &m));
  TEST_ASSERT_EQUAL(SB_MSG_SOUND_LEVEL, m.kind);
  TEST_ASSERT_FALSE(m.dba_valid);
}

// Los enteros sin decimales tambien son lecturas validas.
void test_integer_readings_are_valid(void) {
  SbMessage m;
  TEST_ASSERT_TRUE(decode_str(
      "{\"type\":\"event\",\"cmd\":\"sensor_data\","
      "\"data\":{\"temp\":[36,37,36],\"hum\":[55,54,60],\"lux\":320},\"ts\":1}",
      &m));
  TEST_ASSERT_TRUE(m.temp_valid[0]);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 36.0f, m.temp[0]);
  TEST_ASSERT_TRUE(m.lux_valid);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 320.0f, m.lux);
}

// El decoder recibe (puntero, longitud), no una cadena C: un payload de 256 B
// llena el buffer del parser sin terminador y nada debe leer mas alla.
void test_decoder_respects_the_length_and_not_a_terminator(void) {
  const char *json = "{\"type\":\"event\",\"cmd\":\"door_open\",\"ts\":7}";
  uint8_t buf[128];
  memset(buf, 0xFF, sizeof(buf));  // sin terminador nulo detras
  const size_t n = strlen(json);
  memcpy(buf, json, n);

  SbMessage m;
  TEST_ASSERT_TRUE(sb_json_decode(buf, n, &m));
  TEST_ASSERT_EQUAL(SB_MSG_DOOR_OPEN, m.kind);
  TEST_ASSERT_EQUAL_UINT32(7, m.ts);
}

// Ejemplo literal de SensorBoard_v2/README.md #Telemetria.
void test_sensor_data_with_dead_sensor(void) {
  SbMessage m;
  TEST_ASSERT_TRUE(decode_str(
      "{\"type\":\"event\",\"cmd\":\"sensor_data\","
      "\"data\":{\"temp\":[36.5,null,36.8],\"hum\":[55.0,54.5,60.1],"
      "\"lux\":320.5},\"ts\":5200}",
      &m));
  TEST_ASSERT_EQUAL(SB_MSG_SENSOR_DATA, m.kind);
  TEST_ASSERT_EQUAL_UINT32(5200, m.ts);
  TEST_ASSERT_TRUE(m.temp_valid[0]);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 36.5f, m.temp[0]);
  TEST_ASSERT_FALSE(m.temp_valid[1]);  // sensor caido -> null
  TEST_ASSERT_TRUE(m.temp_valid[2]);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 36.8f, m.temp[2]);
  TEST_ASSERT_TRUE(m.hum_valid[1]);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 54.5f, m.hum[1]);
  TEST_ASSERT_TRUE(m.lux_valid);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 320.5f, m.lux);
}

void test_door_open_and_closed(void) {
  SbMessage m;
  TEST_ASSERT_TRUE(
      decode_str("{\"type\":\"event\",\"cmd\":\"door_open\",\"ts\":10}", &m));
  TEST_ASSERT_EQUAL(SB_MSG_DOOR_OPEN, m.kind);

  TEST_ASSERT_TRUE(
      decode_str("{\"type\":\"event\",\"cmd\":\"door_closed\",\"ts\":40}", &m));
  TEST_ASSERT_EQUAL(SB_MSG_DOOR_CLOSED, m.kind);
}

void test_sound_level_event(void) {
  SbMessage m;
  TEST_ASSERT_TRUE(decode_str(
      "{\"type\":\"event\",\"cmd\":\"sound_level\",\"data\":{\"dba\":42.3},"
      "\"ts\":9000}",
      &m));
  TEST_ASSERT_EQUAL(SB_MSG_SOUND_LEVEL, m.kind);
  TEST_ASSERT_TRUE(m.dba_valid);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 42.3f, m.dba);
}

void test_capture_resp_ok(void) {
  SbMessage m;
  TEST_ASSERT_TRUE(decode_str(
      "{\"type\":\"resp\",\"cmd\":\"capture\",\"id\":7,\"status\":\"ok\","
      "\"size\":18432,\"ts\":1000}",
      &m));
  TEST_ASSERT_EQUAL(SB_MSG_CAPTURE_RESP, m.kind);
  TEST_ASSERT_TRUE(m.resp_ok);
  TEST_ASSERT_EQUAL_UINT32(7, m.resp_id);
  TEST_ASSERT_EQUAL_UINT32(18432, m.capture_size);
}

void test_capture_resp_error_busy(void) {
  SbMessage m;
  TEST_ASSERT_TRUE(decode_str(
      "{\"type\":\"resp\",\"cmd\":\"capture\",\"id\":8,\"status\":\"error\","
      "\"msg\":\"busy\"}",
      &m));
  TEST_ASSERT_EQUAL(SB_MSG_CAPTURE_RESP, m.kind);
  TEST_ASSERT_FALSE(m.resp_ok);
  TEST_ASSERT_EQUAL_STRING("busy", m.msg);
}

void test_status_resp(void) {
  SbMessage m;
  TEST_ASSERT_TRUE(decode_str(
      "{\"type\":\"resp\",\"cmd\":\"status\",\"id\":3,\"status\":\"ok\"}", &m));
  TEST_ASSERT_EQUAL(SB_MSG_STATUS_RESP, m.kind);
  TEST_ASSERT_TRUE(m.resp_ok);
  TEST_ASSERT_EQUAL_UINT32(3, m.resp_id);
}

void test_log_event(void) {
  SbMessage m;
  TEST_ASSERT_TRUE(decode_str(
      "{\"type\":\"log\",\"ts\":1,\"msg\":\"boot ok\"}", &m));
  TEST_ASSERT_EQUAL(SB_MSG_LOG, m.kind);
  TEST_ASSERT_EQUAL_STRING("boot ok", m.msg);
}

void test_unknown_cmd_decodes_as_unknown_not_failure(void) {
  SbMessage m;
  TEST_ASSERT_TRUE(
      decode_str("{\"type\":\"event\",\"cmd\":\"future_thing\"}", &m));
  TEST_ASSERT_EQUAL(SB_MSG_UNKNOWN, m.kind);
}

void test_malformed_json_fails(void) {
  SbMessage m;
  TEST_ASSERT_FALSE(decode_str("{not json", &m));
}

void test_encode_status_cmd(void) {
  uint8_t buf[96];
  size_t n = sb_json_encode_status_cmd(5, buf, sizeof(buf));
  TEST_ASSERT_TRUE(n > 0);
  SbMessage m;
  TEST_ASSERT_TRUE(sb_json_decode(buf, n, &m));
  // El propio comando no se decodifica como resp/evento (no lleva "status"
  // de respuesta), pero debe ser JSON valido y round-trip-eable.
  TEST_ASSERT_TRUE(strstr((char *)buf, "\"cmd\":\"status\"") != NULL);
  TEST_ASSERT_TRUE(strstr((char *)buf, "\"id\":5") != NULL);
}

void test_encode_capture_cmd(void) {
  uint8_t buf[96];
  size_t n = sb_json_encode_capture_cmd(9, buf, sizeof(buf));
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_TRUE(strstr((char *)buf, "\"cmd\":\"capture\"") != NULL);
  TEST_ASSERT_TRUE(strstr((char *)buf, "\"id\":9") != NULL);
}

void test_encode_returns_zero_when_buffer_too_small(void) {
  uint8_t buf[4];
  TEST_ASSERT_EQUAL_UINT32(0, sb_json_encode_status_cmd(5, buf, sizeof(buf)));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_heartbeat_event);
  RUN_TEST(test_non_numeric_readings_are_not_valid);
  RUN_TEST(test_non_numeric_sound_level_is_not_valid);
  RUN_TEST(test_integer_readings_are_valid);
  RUN_TEST(test_decoder_respects_the_length_and_not_a_terminator);
  RUN_TEST(test_sensor_data_with_dead_sensor);
  RUN_TEST(test_door_open_and_closed);
  RUN_TEST(test_sound_level_event);
  RUN_TEST(test_capture_resp_ok);
  RUN_TEST(test_capture_resp_error_busy);
  RUN_TEST(test_status_resp);
  RUN_TEST(test_log_event);
  RUN_TEST(test_unknown_cmd_decodes_as_unknown_not_failure);
  RUN_TEST(test_malformed_json_fails);
  RUN_TEST(test_encode_status_cmd);
  RUN_TEST(test_encode_capture_cmd);
  RUN_TEST(test_encode_returns_zero_when_buffer_too_small);
  return UNITY_END();
}
