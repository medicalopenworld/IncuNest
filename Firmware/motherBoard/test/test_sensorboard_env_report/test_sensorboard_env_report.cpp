#include <string.h>
#include <unity.h>

#include "modules/sensorboard_comm/sb_env_report.h"
#include "modules/sensorboard_comm/sb_protocol.h"
#include "protocol.h"

// Lo que se prueba es el CONTRATO del mensaje CTRL,ENV hacia el display: que
// forma tiene la linea, que valor sale cuando algo NO se sabe (nunca un cero) y
// cuando toca enviarla. El cableado a hmiSerial en CommTask.cpp queda fuera: no
// compila para host.

void setUp(void) {}
void tearDown(void) {}

static const uint32_t NOW = 100000u;

// Enlace vivo, puerta cerrada, luz y sonido al dia.
static SbSnapshot healthy(void) {
  SbSnapshot s = {};
  s.link_ok = true;
  s.env_seen = true;
  s.last_env_ms = NOW;
  s.lux_valid = true;
  s.lux = 320.4f;
  s.sound_seen = true;
  s.last_sound_ms = NOW;
  s.dba_valid = true;
  s.dba = 61.6f;
  s.door_known = true;
  s.last_door_ms = NOW;
  s.door_open = false;
  s.door_faulty = false;
  return s;
}

static SbEnvReport make(const SbSnapshot &s, uint32_t now) {
  SbEnvReport r;
  memset(&r, 0x5A, sizeof(r));  // que make() tenga que escribirlo todo
  sb_env_report_make(&r, &s, now);
  return r;
}

static void expect_line(const SbEnvReport &r, const char *expected) {
  char buf[PROTO_ENV_LINE_MAX + 2];
  const int n = sb_env_report_format(buf, sizeof(buf), &r);
  TEST_ASSERT_EQUAL_STRING(expected, buf);
  TEST_ASSERT_EQUAL_INT((int)strlen(expected), n);
}

// ---------------------------------------------------------------- make()

void test_sano_puerta_cerrada(void) {
  const SbEnvReport r = make(healthy(), NOW);
  TEST_ASSERT_TRUE(r.link);
  TEST_ASSERT_EQUAL_INT(PROTO_ENV_DOOR_CLOSED, r.door);
  TEST_ASSERT_EQUAL_INT32(320, r.lux);  // 320.4 redondea a 320
  TEST_ASSERT_EQUAL_INT32(62, r.dba);   // 61.6 redondea a 62
}

void test_puerta_abierta(void) {
  SbSnapshot s = healthy();
  s.door_open = true;
  TEST_ASSERT_EQUAL_INT(PROTO_ENV_DOOR_OPEN, make(s, NOW).door);
}

void test_enlace_caido_todo_a_centinela_aunque_haya_lecturas_buenas(void) {
  SbSnapshot s = healthy();
  s.link_ok = false;  // el snapshot sigue guardando lecturas de antes
  s.door_open = true;
  const SbEnvReport r = make(s, NOW);
  TEST_ASSERT_FALSE(r.link);
  TEST_ASSERT_EQUAL_INT(PROTO_ENV_DOOR_UNKNOWN, r.door);
  TEST_ASSERT_EQUAL_INT32(PROTO_ENV_UNAVAILABLE, r.lux);
  TEST_ASSERT_EQUAL_INT32(PROTO_ENV_UNAVAILABLE, r.dba);
}

void test_fallo_de_puerta_gana_al_ultimo_estado(void) {
  SbSnapshot s = healthy();
  s.door_faulty = true;
  s.door_open = true;
  TEST_ASSERT_EQUAL_INT(PROTO_ENV_DOOR_FAULT, make(s, NOW).door);
  s.door_open = false;
  TEST_ASSERT_EQUAL_INT(PROTO_ENV_DOOR_FAULT, make(s, NOW).door);
}

void test_fallo_de_puerta_sin_estado_conocido_sigue_siendo_fallo(void) {
  SbSnapshot s = healthy();
  s.door_known = false;
  s.door_faulty = true;
  TEST_ASSERT_EQUAL_INT(PROTO_ENV_DOOR_FAULT, make(s, NOW).door);
}

void test_puerta_nunca_vista_es_desconocida_no_cerrada(void) {
  SbSnapshot s = healthy();
  s.door_known = false;
  TEST_ASSERT_EQUAL_INT(PROTO_ENV_DOOR_UNKNOWN, make(s, NOW).door);
}

void test_puerta_rancia_es_desconocida(void) {
  SbSnapshot s = healthy();
  const uint32_t now = NOW + SB_DOOR_STALE_MS + 1u;
  s.last_env_ms = now;    // lo demas al dia
  s.last_sound_ms = now;
  TEST_ASSERT_EQUAL_INT(PROTO_ENV_DOOR_UNKNOWN, make(s, now).door);
}

void test_puerta_en_el_limite_de_frescura_aun_vale(void) {
  SbSnapshot s = healthy();
  TEST_ASSERT_EQUAL_INT(PROTO_ENV_DOOR_CLOSED,
                        make(s, NOW + SB_DOOR_STALE_MS).door);
}

void test_sonido_rancio_solo_afecta_a_dba(void) {
  SbSnapshot s = healthy();
  const uint32_t now = NOW + SB_SOUND_STALE_MS + 1u;
  s.last_env_ms = now;
  s.last_door_ms = now;
  const SbEnvReport r = make(s, now);
  TEST_ASSERT_EQUAL_INT32(PROTO_ENV_UNAVAILABLE, r.dba);
  TEST_ASSERT_EQUAL_INT32(320, r.lux);
  TEST_ASSERT_EQUAL_INT(PROTO_ENV_DOOR_CLOSED, r.door);
}

void test_sonido_nunca_visto_es_centinela(void) {
  SbSnapshot s = healthy();
  s.sound_seen = false;
  TEST_ASSERT_EQUAL_INT32(PROTO_ENV_UNAVAILABLE, make(s, NOW).dba);
}

void test_luz_rancia_es_centinela(void) {
  SbSnapshot s = healthy();
  const uint32_t now = NOW + SB_ENV_STALE_MS + 1u;
  s.last_sound_ms = now;
  s.last_door_ms = now;
  TEST_ASSERT_EQUAL_INT32(PROTO_ENV_UNAVAILABLE, make(s, now).lux);
}

void test_no_valido_es_centinela_nunca_cero(void) {
  SbSnapshot s = healthy();
  s.lux_valid = false;
  s.lux = 0.0f;
  s.dba_valid = false;
  s.dba = 0.0f;
  const SbEnvReport r = make(s, NOW);
  TEST_ASSERT_EQUAL_INT32(PROTO_ENV_UNAVAILABLE, r.lux);
  TEST_ASSERT_EQUAL_INT32(PROTO_ENV_UNAVAILABLE, r.dba);
}

void test_un_cero_valido_sigue_siendo_cero(void) {
  // Un lux 0 REAL (oscuridad) es una medida y tiene que viajar como 0; el
  // centinela es solo para lo que no se sabe.
  SbSnapshot s = healthy();
  s.lux = 0.0f;
  TEST_ASSERT_EQUAL_INT32(0, make(s, NOW).lux);
}

void test_valores_imposibles_son_centinela(void) {
  SbSnapshot s = healthy();
  s.lux = -5.0f;
  s.dba = -0.6f;
  const SbEnvReport r = make(s, NOW);
  TEST_ASSERT_EQUAL_INT32(PROTO_ENV_UNAVAILABLE, r.lux);
  TEST_ASSERT_EQUAL_INT32(PROTO_ENV_UNAVAILABLE, r.dba);
}

void test_valores_enormes_se_acotan_y_la_linea_cabe(void) {
  SbSnapshot s = healthy();
  s.lux = 3.0e9f;
  s.dba = 1.0e9f;
  s.door_faulty = true;
  const SbEnvReport r = make(s, NOW);
  TEST_ASSERT_TRUE(r.lux >= 0);
  TEST_ASSERT_TRUE(r.dba >= 0);
  char buf[PROTO_ENV_LINE_MAX + 2];
  const int n = sb_env_report_format(buf, sizeof(buf), &r);
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_TRUE(n <= PROTO_ENV_LINE_MAX + 1);  // + el '\n'
}

void test_frescura_con_vuelta_del_contador(void) {
  SbSnapshot s = healthy();
  s.last_sound_ms = 4294967000u;  // a 296 ms de la vuelta
  s.last_env_ms = 4294967000u;
  s.last_door_ms = 4294967000u;
  const SbEnvReport r = make(s, 1000u);  // pasaron 1296 ms
  TEST_ASSERT_EQUAL_INT32(62, r.dba);
  TEST_ASSERT_EQUAL_INT(PROTO_ENV_DOOR_CLOSED, r.door);
}

// -------------------------------------------------------------- format()

void test_linea_sana(void) {
  expect_line(make(healthy(), NOW), "CTRL,ENV,1,0,320,62\n");
}

void test_linea_puerta_abierta(void) {
  SbSnapshot s = healthy();
  s.door_open = true;
  expect_line(make(s, NOW), "CTRL,ENV,1,1,320,62\n");
}

void test_linea_enlace_caido(void) {
  SbSnapshot s = healthy();
  s.link_ok = false;
  expect_line(make(s, NOW), "CTRL,ENV,0,-1,-1,-1\n");
}

void test_linea_fallo_de_puerta(void) {
  SbSnapshot s = healthy();
  s.door_faulty = true;
  expect_line(make(s, NOW), "CTRL,ENV,1,2,320,62\n");
}

void test_linea_sin_sonido(void) {
  SbSnapshot s = healthy();
  s.dba_valid = false;
  expect_line(make(s, NOW), "CTRL,ENV,1,0,320,-1\n");
}

void test_format_no_cabe_devuelve_menos_uno_y_deja_cadena_vacia(void) {
  const SbEnvReport r = make(healthy(), NOW);
  char buf[8];
  memset(buf, 'x', sizeof(buf));
  TEST_ASSERT_EQUAL_INT(-1, sb_env_report_format(buf, sizeof(buf), &r));
  TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_format_con_buffer_de_tamano_cero_no_escribe(void) {
  const SbEnvReport r = make(healthy(), NOW);
  char buf[1] = {'x'};
  TEST_ASSERT_EQUAL_INT(-1, sb_env_report_format(buf, 0, &r));
  TEST_ASSERT_EQUAL_CHAR('x', buf[0]);
}

// -------------------------------------------------------------- pacer

void test_primer_envio_siempre_toca(void) {
  SbEnvPacer p;
  sb_env_pacer_init(&p);
  const SbEnvReport r = make(healthy(), NOW);
  TEST_ASSERT_TRUE(sb_env_pacer_due(&p, &r, NOW));
}

void test_dentro_del_periodo_sin_cambios_no_toca(void) {
  SbEnvPacer p;
  sb_env_pacer_init(&p);
  const SbEnvReport r = make(healthy(), NOW);
  sb_env_pacer_sent(&p, &r, NOW);
  TEST_ASSERT_FALSE(sb_env_pacer_due(&p, &r, NOW));
  TEST_ASSERT_FALSE(sb_env_pacer_due(&p, &r, NOW + PROTO_ENV_PERIOD_MS - 1u));
}

void test_cumplido_el_periodo_toca(void) {
  SbEnvPacer p;
  sb_env_pacer_init(&p);
  const SbEnvReport r = make(healthy(), NOW);
  sb_env_pacer_sent(&p, &r, NOW);
  TEST_ASSERT_TRUE(sb_env_pacer_due(&p, &r, NOW + PROTO_ENV_PERIOD_MS));
}

void test_cambio_de_puerta_toca_sin_esperar(void) {
  SbEnvPacer p;
  sb_env_pacer_init(&p);
  const SbEnvReport closed = make(healthy(), NOW);
  sb_env_pacer_sent(&p, &closed, NOW);
  SbSnapshot s = healthy();
  s.door_open = true;
  const SbEnvReport open = make(s, NOW + 100u);
  TEST_ASSERT_TRUE(sb_env_pacer_due(&p, &open, NOW + 100u));
}

void test_cambio_de_enlace_toca_sin_esperar(void) {
  SbEnvPacer p;
  sb_env_pacer_init(&p);
  const SbEnvReport up = make(healthy(), NOW);
  sb_env_pacer_sent(&p, &up, NOW);
  SbSnapshot s = healthy();
  s.link_ok = false;
  const SbEnvReport down = make(s, NOW + 100u);
  TEST_ASSERT_TRUE(sb_env_pacer_due(&p, &down, NOW + 100u));
}

void test_solo_cambia_el_sonido_no_toca(void) {
  SbEnvPacer p;
  sb_env_pacer_init(&p);
  const SbEnvReport quiet = make(healthy(), NOW);
  sb_env_pacer_sent(&p, &quiet, NOW);
  SbSnapshot s = healthy();
  s.dba = 90.0f;
  const SbEnvReport loud = make(s, NOW + 100u);
  TEST_ASSERT_FALSE(sb_env_pacer_due(&p, &loud, NOW + 100u));
}

void test_solo_cambia_la_luz_no_toca(void) {
  SbEnvPacer p;
  sb_env_pacer_init(&p);
  const SbEnvReport dim = make(healthy(), NOW);
  sb_env_pacer_sent(&p, &dim, NOW);
  SbSnapshot s = healthy();
  s.lux = 900.0f;
  const SbEnvReport bright = make(s, NOW + 100u);
  TEST_ASSERT_FALSE(sb_env_pacer_due(&p, &bright, NOW + 100u));
}

void test_enviar_reinicia_el_periodo(void) {
  SbEnvPacer p;
  sb_env_pacer_init(&p);
  const SbEnvReport r = make(healthy(), NOW);
  sb_env_pacer_sent(&p, &r, NOW);
  sb_env_pacer_sent(&p, &r, NOW + 3000u);
  TEST_ASSERT_FALSE(sb_env_pacer_due(&p, &r, NOW + 3000u + PROTO_ENV_PERIOD_MS - 1u));
  TEST_ASSERT_TRUE(sb_env_pacer_due(&p, &r, NOW + 3000u + PROTO_ENV_PERIOD_MS));
}

void test_periodo_con_vuelta_del_contador(void) {
  SbEnvPacer p;
  sb_env_pacer_init(&p);
  const SbEnvReport r = make(healthy(), NOW);
  sb_env_pacer_sent(&p, &r, 4294967000u);
  // 296 ms hasta la vuelta + 4800 = 5096 ms >= periodo.
  TEST_ASSERT_TRUE(sb_env_pacer_due(&p, &r, 4800u));
  // 296 + 1000 = 1296 ms: aun no.
  TEST_ASSERT_FALSE(sb_env_pacer_due(&p, &r, 1000u));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_sano_puerta_cerrada);
  RUN_TEST(test_puerta_abierta);
  RUN_TEST(test_enlace_caido_todo_a_centinela_aunque_haya_lecturas_buenas);
  RUN_TEST(test_fallo_de_puerta_gana_al_ultimo_estado);
  RUN_TEST(test_fallo_de_puerta_sin_estado_conocido_sigue_siendo_fallo);
  RUN_TEST(test_puerta_nunca_vista_es_desconocida_no_cerrada);
  RUN_TEST(test_puerta_rancia_es_desconocida);
  RUN_TEST(test_puerta_en_el_limite_de_frescura_aun_vale);
  RUN_TEST(test_sonido_rancio_solo_afecta_a_dba);
  RUN_TEST(test_sonido_nunca_visto_es_centinela);
  RUN_TEST(test_luz_rancia_es_centinela);
  RUN_TEST(test_no_valido_es_centinela_nunca_cero);
  RUN_TEST(test_un_cero_valido_sigue_siendo_cero);
  RUN_TEST(test_valores_imposibles_son_centinela);
  RUN_TEST(test_valores_enormes_se_acotan_y_la_linea_cabe);
  RUN_TEST(test_frescura_con_vuelta_del_contador);
  RUN_TEST(test_linea_sana);
  RUN_TEST(test_linea_puerta_abierta);
  RUN_TEST(test_linea_enlace_caido);
  RUN_TEST(test_linea_fallo_de_puerta);
  RUN_TEST(test_linea_sin_sonido);
  RUN_TEST(test_format_no_cabe_devuelve_menos_uno_y_deja_cadena_vacia);
  RUN_TEST(test_format_con_buffer_de_tamano_cero_no_escribe);
  RUN_TEST(test_primer_envio_siempre_toca);
  RUN_TEST(test_dentro_del_periodo_sin_cambios_no_toca);
  RUN_TEST(test_cumplido_el_periodo_toca);
  RUN_TEST(test_cambio_de_puerta_toca_sin_esperar);
  RUN_TEST(test_cambio_de_enlace_toca_sin_esperar);
  RUN_TEST(test_solo_cambia_el_sonido_no_toca);
  RUN_TEST(test_solo_cambia_la_luz_no_toca);
  RUN_TEST(test_enviar_reinicia_el_periodo);
  RUN_TEST(test_periodo_con_vuelta_del_contador);
  return UNITY_END();
}
