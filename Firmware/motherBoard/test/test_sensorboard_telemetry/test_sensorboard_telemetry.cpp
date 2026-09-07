#include <unity.h>

#include "config/telemetry_keys.h"
#include "modules/sensorboard_comm/sb_protocol.h"
#include "modules/sensorboard_comm/sb_telemetry.h"

// Lo que se prueba aqui no es el redondeo, es QUE CLAVES aparecen en el JSON
// y cuales se omiten. sb_telemetry.cpp redondea con su propio helper, asi que
// este fichero no necesita enlazar nada de system/math.cpp.

void setUp(void) {}
void tearDown(void) {}

static const uint32_t NOW = 100000u;

// Enlace vivo, las tres posiciones de temperatura y humedad frescas y validas,
// luz/sonido/puerta al dia. Punto de partida de casi todos los casos.
static SbSnapshot healthy(void) {
  SbSnapshot s = {};
  s.link_ok = true;
  s.env_seen = true;
  s.last_env_ms = NOW;
  for (int i = 0; i < 3; i++) {
    s.temp.valid[i] = true;
    s.hum.valid[i] = true;
  }
  s.temp.value[0] = 36.0f;
  s.temp.value[1] = 36.25f;
  s.temp.value[2] = 36.5f;
  s.hum.value[0] = 55.0f;
  s.hum.value[1] = 55.5f;
  s.hum.value[2] = 60.0f;
  s.lux_valid = true;
  s.lux = 320.0f;
  s.sound_seen = true;
  s.last_sound_ms = NOW;
  s.dba_valid = true;
  s.dba = 48.0f;
  s.door_known = true;
  s.last_door_ms = NOW;
  s.door_open = false;
  s.door_faulty = false;
  return s;
}

void test_tres_posiciones_dan_seis_claves(void) {
  StaticJsonDocument<1024> doc;
  JsonObject json = doc.to<JsonObject>();
  const SbSnapshot s = healthy();
  sb_build_telemetry(json, s, NOW, 3);

  TEST_ASSERT_TRUE(json.containsKey(SB_TEMP0_KEY));
  TEST_ASSERT_TRUE(json.containsKey(SB_TEMP1_KEY));
  TEST_ASSERT_TRUE(json.containsKey(SB_TEMP2_KEY));
  TEST_ASSERT_TRUE(json.containsKey(SB_HUM0_KEY));
  TEST_ASSERT_TRUE(json.containsKey(SB_HUM1_KEY));
  TEST_ASSERT_TRUE(json.containsKey(SB_HUM2_KEY));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 36.0f, json[SB_TEMP0_KEY].as<float>());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 36.25f, json[SB_TEMP1_KEY].as<float>());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 36.5f, json[SB_TEMP2_KEY].as<float>());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 60.0f, json[SB_HUM2_KEY].as<float>());
}

// EL CASO QUE JUSTIFICA ESTE FICHERO. Un SHT40 caido tiene que DESAPARECER de
// la telemetria, no viajar como 0. Un 0 en la nube es indistinguible de una
// medida: el cuadro de mando pintaria un sensor vivo a 0 C y la deriva que hay
// que vigilar quedaria enterrada bajo un salto de 36 grados.
void test_posicion_caida_omite_su_clave_no_manda_cero(void) {
  StaticJsonDocument<1024> doc;
  JsonObject json = doc.to<JsonObject>();
  SbSnapshot s = healthy();
  s.temp.valid[1] = false;
  s.temp.value[1] = 0.0f;
  sb_build_telemetry(json, s, NOW, 2);

  TEST_ASSERT_FALSE(json.containsKey(SB_TEMP1_KEY));
  TEST_ASSERT_TRUE(json.containsKey(SB_TEMP0_KEY));
  TEST_ASSERT_TRUE(json.containsKey(SB_TEMP2_KEY));
  // La humedad de esa misma posicion es independiente: el SensorBoard puede
  // mandar temp null y hum valida (o al reves) en la misma posicion.
  TEST_ASSERT_TRUE(json.containsKey(SB_HUM1_KEY));
}

void test_humedad_caida_no_arrastra_a_su_temperatura(void) {
  StaticJsonDocument<1024> doc;
  JsonObject json = doc.to<JsonObject>();
  SbSnapshot s = healthy();
  s.hum.valid[0] = false;
  sb_build_telemetry(json, s, NOW, 3);

  TEST_ASSERT_FALSE(json.containsKey(SB_HUM0_KEY));
  TEST_ASSERT_TRUE(json.containsKey(SB_TEMP0_KEY));
}

void test_las_tres_caidas_dejan_las_seis_claves_fuera(void) {
  StaticJsonDocument<1024> doc;
  JsonObject json = doc.to<JsonObject>();
  SbSnapshot s = healthy();
  for (int i = 0; i < 3; i++) {
    s.temp.valid[i] = false;
    s.hum.valid[i] = false;
  }
  sb_build_telemetry(json, s, NOW, 0);

  TEST_ASSERT_FALSE(json.containsKey(SB_TEMP0_KEY));
  TEST_ASSERT_FALSE(json.containsKey(SB_TEMP1_KEY));
  TEST_ASSERT_FALSE(json.containsKey(SB_TEMP2_KEY));
  TEST_ASSERT_FALSE(json.containsKey(SB_HUM0_KEY));
  TEST_ASSERT_FALSE(json.containsKey(SB_HUM1_KEY));
  TEST_ASSERT_FALSE(json.containsKey(SB_HUM2_KEY));
  // sb_env_used si viaja, y a 0: es la senal de que no queda redundancia.
  TEST_ASSERT_EQUAL_INT(0, json[SB_ENV_USED_KEY].as<int>());
}

// Con el enlace caido no se publica ni un valor: los ultimos buenos se
// quedarian congelados en el cuadro de mando, que es peor que un hueco.
void test_enlace_caido_solo_publica_el_estado_del_enlace(void) {
  StaticJsonDocument<1024> doc;
  JsonObject json = doc.to<JsonObject>();
  SbSnapshot s = healthy();
  s.link_ok = false;
  sb_build_telemetry(json, s, NOW, 3);

  TEST_ASSERT_EQUAL_size_t(1, json.size());
  TEST_ASSERT_FALSE(json[SB_LINK_OK_KEY].as<bool>());
}

// Un dato rancio no es un dato. Con el enlace vivo pero sensor_data parado,
// las claves de temperatura tienen que caerse igual.
void test_env_rancio_no_publica_temperatura_ni_humedad_ni_luz(void) {
  StaticJsonDocument<1024> doc;
  JsonObject json = doc.to<JsonObject>();
  const SbSnapshot s = healthy();
  sb_build_telemetry(json, s, NOW + SB_ENV_STALE_MS + 1u, 3);

  TEST_ASSERT_FALSE(json.containsKey(SB_TEMP0_KEY));
  TEST_ASSERT_FALSE(json.containsKey(SB_HUM0_KEY));
  TEST_ASSERT_FALSE(json.containsKey(SB_LUX_KEY));
  // El sonido tiene su propio reloj y sigue fresco.
  TEST_ASSERT_TRUE(json.containsKey(SB_DB_KEY));
}

// Sin un solo sensor_data recibido no hay nada que publicar, aunque el
// heartbeat mantenga el enlace en pie.
void test_sin_env_visto_no_hay_claves_de_ambiente(void) {
  StaticJsonDocument<1024> doc;
  JsonObject json = doc.to<JsonObject>();
  SbSnapshot s = healthy();
  s.env_seen = false;
  sb_build_telemetry(json, s, NOW, 0);

  TEST_ASSERT_FALSE(json.containsKey(SB_TEMP0_KEY));
  TEST_ASSERT_FALSE(json.containsKey(SB_LUX_KEY));
  TEST_ASSERT_TRUE(json[SB_LINK_OK_KEY].as<bool>());
}

// sb_env_used es el aviso de que la redundancia se esta perdiendo: tiene que
// viajar siempre que el enlace este vivo, tambien cuando vale 3.
void test_env_used_viaja_siempre_con_enlace_vivo(void) {
  StaticJsonDocument<1024> doc;
  JsonObject json = doc.to<JsonObject>();
  const SbSnapshot s = healthy();
  sb_build_telemetry(json, s, NOW, 2);

  TEST_ASSERT_TRUE(json.containsKey(SB_ENV_USED_KEY));
  TEST_ASSERT_EQUAL_INT(2, json[SB_ENV_USED_KEY].as<int>());
}

void test_puerta_rancia_no_publica_estado_pero_si_averia(void) {
  StaticJsonDocument<1024> doc;
  JsonObject json = doc.to<JsonObject>();
  SbSnapshot s = healthy();
  s.door_faulty = true;
  sb_build_telemetry(json, s, NOW + SB_DOOR_STALE_MS + 1u, 3);

  TEST_ASSERT_FALSE(json.containsKey(SB_DOOR_OPEN_KEY));
  TEST_ASSERT_TRUE(json[SB_DOOR_FAULT_KEY].as<bool>());
}

// Una puerta cerrada es un false que SI tiene que viajar: distinguir
// "cerrada" de "no se sabe" es justo lo que da la omision de la clave.
void test_puerta_cerrada_publica_false_no_omite(void) {
  StaticJsonDocument<1024> doc;
  JsonObject json = doc.to<JsonObject>();
  const SbSnapshot s = healthy();
  sb_build_telemetry(json, s, NOW, 3);

  TEST_ASSERT_TRUE(json.containsKey(SB_DOOR_OPEN_KEY));
  TEST_ASSERT_FALSE(json[SB_DOOR_OPEN_KEY].as<bool>());
}

// Presupuesto de campos: el bloque sb_* se inserta ULTIMO en el JSON de
// telemetria, y ArduinoJson descarta las claves ultimas insertadas cuando se
// llena el pool (commit fb3a535). Si este bloque crece, se come el margen de
// THINGSBOARD_FIELDS_AMOUNT y las primeras victimas son estas claves.
void test_el_bloque_sb_no_pasa_de_doce_claves(void) {
  StaticJsonDocument<1024> doc;
  JsonObject json = doc.to<JsonObject>();
  const SbSnapshot s = healthy();
  sb_build_telemetry(json, s, NOW, 3);

  TEST_ASSERT_EQUAL_size_t(12, json.size());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_tres_posiciones_dan_seis_claves);
  RUN_TEST(test_posicion_caida_omite_su_clave_no_manda_cero);
  RUN_TEST(test_humedad_caida_no_arrastra_a_su_temperatura);
  RUN_TEST(test_las_tres_caidas_dejan_las_seis_claves_fuera);
  RUN_TEST(test_enlace_caido_solo_publica_el_estado_del_enlace);
  RUN_TEST(test_env_rancio_no_publica_temperatura_ni_humedad_ni_luz);
  RUN_TEST(test_sin_env_visto_no_hay_claves_de_ambiente);
  RUN_TEST(test_env_used_viaja_siempre_con_enlace_vivo);
  RUN_TEST(test_puerta_rancia_no_publica_estado_pero_si_averia);
  RUN_TEST(test_puerta_cerrada_publica_false_no_omite);
  RUN_TEST(test_el_bloque_sb_no_pasa_de_doce_claves);
  return UNITY_END();
}
