#include <string.h>
#include <unity.h>

#include "modules/comm/sb_env.h"

void setUp(void) {}
void tearDown(void) {}

// Estado "centinela" para comprobar que un rechazo NO toca la salida.
static SbEnvMsg poisoned(void) {
  SbEnvMsg m;
  m.link = true;
  m.door = 77;
  m.lux = 1234;
  m.dba = 4321;
  return m;
}

static void assert_untouched(const SbEnvMsg &m) {
  TEST_ASSERT_TRUE(m.link);
  TEST_ASSERT_EQUAL_INT(77, m.door);
  TEST_ASSERT_EQUAL_INT(1234, m.lux);
  TEST_ASSERT_EQUAL_INT(4321, m.dba);
}

static void assert_rejected(const char *line) {
  SbEnvMsg m = poisoned();
  TEST_ASSERT_FALSE_MESSAGE(sbenv_parse(line, &m), line);
  assert_untouched(m);
}

// ---------------------------------------------------------------- parser

void test_linea_sana(void) {
  SbEnvMsg m = poisoned();
  TEST_ASSERT_TRUE(sbenv_parse("CTRL,ENV,1,1,320,62", &m));
  TEST_ASSERT_TRUE(m.link);
  TEST_ASSERT_EQUAL_INT(PROTO_ENV_DOOR_OPEN, m.door);
  TEST_ASSERT_EQUAL_INT(320, m.lux);
  TEST_ASSERT_EQUAL_INT(62, m.dba);
}

void test_centinelas_de_no_disponible(void) {
  SbEnvMsg m = poisoned();
  TEST_ASSERT_TRUE(sbenv_parse("CTRL,ENV,0,-1,-1,-1", &m));
  TEST_ASSERT_FALSE(m.link);
  TEST_ASSERT_EQUAL_INT(PROTO_ENV_DOOR_UNKNOWN, m.door);
  TEST_ASSERT_EQUAL_INT(PROTO_ENV_UNAVAILABLE, m.lux);
  TEST_ASSERT_EQUAL_INT(PROTO_ENV_UNAVAILABLE, m.dba);
}

void test_los_cuatro_estados_de_puerta(void) {
  SbEnvMsg m;
  TEST_ASSERT_TRUE(sbenv_parse("CTRL,ENV,1,0,1,1", &m));
  TEST_ASSERT_EQUAL_INT(PROTO_ENV_DOOR_CLOSED, m.door);
  TEST_ASSERT_TRUE(sbenv_parse("CTRL,ENV,1,1,1,1", &m));
  TEST_ASSERT_EQUAL_INT(PROTO_ENV_DOOR_OPEN, m.door);
  TEST_ASSERT_TRUE(sbenv_parse("CTRL,ENV,1,2,1,1", &m));
  TEST_ASSERT_EQUAL_INT(PROTO_ENV_DOOR_FAULT, m.door);
  TEST_ASSERT_TRUE(sbenv_parse("CTRL,ENV,1,-1,1,1", &m));
  TEST_ASSERT_EQUAL_INT(PROTO_ENV_DOOR_UNKNOWN, m.door);
}

void test_cero_valido_no_es_centinela(void) {
  SbEnvMsg m;
  TEST_ASSERT_TRUE(sbenv_parse("CTRL,ENV,1,0,0,0", &m));
  TEST_ASSERT_EQUAL_INT(0, m.lux);
  TEST_ASSERT_EQUAL_INT(0, m.dba);
}

void test_negativo_cualquiera_se_guarda_como_no_disponible(void) {
  SbEnvMsg m;
  TEST_ASSERT_TRUE(sbenv_parse("CTRL,ENV,1,0,-7,-3", &m));
  TEST_ASSERT_EQUAL_INT(PROTO_ENV_UNAVAILABLE, m.lux);
  TEST_ASSERT_EQUAL_INT(PROTO_ENV_UNAVAILABLE, m.dba);
}

void test_acepta_campos_extra_al_final(void) {
  SbEnvMsg m;
  TEST_ASSERT_TRUE(sbenv_parse("CTRL,ENV,1,1,320,62,9,otra", &m));
  TEST_ASSERT_EQUAL_INT(62, m.dba);
}

void test_rechaza_linea_truncada_en_cada_campo(void) {
  assert_rejected("CTRL,ENV");
  assert_rejected("CTRL,ENV,");
  assert_rejected("CTRL,ENV,1");
  assert_rejected("CTRL,ENV,1,1");
  assert_rejected("CTRL,ENV,1,1,32");
  assert_rejected("CTRL,ENV,1,1,32,");
}

void test_rechaza_campos_no_numericos(void) {
  assert_rejected("CTRL,ENV,1,x,320,62");
  assert_rejected("CTRL,ENV,a,1,320,62");
  assert_rejected("CTRL,ENV,1,1,3.5,62");
  assert_rejected("CTRL,ENV,1,1,320,62x");
  assert_rejected("CTRL,ENV,1,1,,62");
}

void test_rechaza_link_y_door_fuera_de_rango(void) {
  assert_rejected("CTRL,ENV,2,0,1,1");
  assert_rejected("CTRL,ENV,-1,0,1,1");
  assert_rejected("CTRL,ENV,1,3,1,1");
  assert_rejected("CTRL,ENV,1,-2,1,1");
}

void test_rechaza_otro_mensaje_con_prefijo_parecido(void) {
  assert_rejected("CTRL,ENVX,1,1,320,62");
  assert_rejected("CTRL,TEL,1,1,320,62");
  assert_rejected("");
}

void test_rechaza_puntero_nulo_sin_romper(void) {
  SbEnvMsg m = poisoned();
  TEST_ASSERT_FALSE(sbenv_parse(NULL, &m));
  TEST_ASSERT_FALSE(sbenv_parse("CTRL,ENV,1,1,1,1", NULL));
  assert_untouched(m);
}

void test_numero_desbordado_se_rechaza_o_se_acota_pero_no_rompe(void) {
  SbEnvMsg m = poisoned();
  const bool ok = sbenv_parse("CTRL,ENV,1,1,99999999999999999999,62", &m);
  if (!ok) {
    assert_untouched(m);
  } else {
    TEST_ASSERT_TRUE(m.lux >= PROTO_ENV_UNAVAILABLE);
  }
}

// ------------------------------------------------------- clasificacion

void test_reposo_medido_es_bajo(void) {
  TEST_ASSERT_EQUAL_INT(SBENV_NOISE_LOW, sbenv_noise_level(60));
}

void test_bordes_de_los_umbrales(void) {
  TEST_ASSERT_EQUAL_INT(SBENV_NOISE_LOW, sbenv_noise_level(64));
  TEST_ASSERT_EQUAL_INT(SBENV_NOISE_MID, sbenv_noise_level(65));
  TEST_ASSERT_EQUAL_INT(SBENV_NOISE_MID, sbenv_noise_level(74));
  TEST_ASSERT_EQUAL_INT(SBENV_NOISE_HIGH, sbenv_noise_level(75));
}

void test_los_umbrales_son_los_documentados(void) {
  // Ancla de los numeros que cita design.md D3: si cambian, cambia el diseno.
  TEST_ASSERT_EQUAL_INT(65, SBENV_NOISE_MID_DBA);
  TEST_ASSERT_EQUAL_INT(75, SBENV_NOISE_HIGH_DBA);
  TEST_ASSERT_TRUE(SBENV_NOISE_MID_DBA < SBENV_NOISE_HIGH_DBA);
}

void test_extremos(void) {
  TEST_ASSERT_EQUAL_INT(SBENV_NOISE_LOW, sbenv_noise_level(0));
  TEST_ASSERT_EQUAL_INT(SBENV_NOISE_HIGH, sbenv_noise_level(140));
  TEST_ASSERT_EQUAL_INT(SBENV_NOISE_HIGH, sbenv_noise_level(2000000000));
}

void test_no_disponible_no_tiene_nivel(void) {
  TEST_ASSERT_EQUAL_INT(SBENV_NOISE_NONE,
                        sbenv_noise_level(PROTO_ENV_UNAVAILABLE));
  TEST_ASSERT_EQUAL_INT(SBENV_NOISE_NONE, sbenv_noise_level(-50));
}

// ------------------------------------------------------------- la vista

static SbEnvMsg msg(bool link, int door, int dba) {
  SbEnvMsg m;
  m.link = link;
  m.door = door;
  m.lux = PROTO_ENV_UNAVAILABLE;
  m.dba = dba;
  return m;
}

static const uint32_t T0 = 100000u;

void test_sin_sensorboard_nunca_se_pinta_nada(void) {
  const SbEnvMsg m = msg(true, PROTO_ENV_DOOR_OPEN, 90);
  const SbEnvView v = sbenv_view(false, &m, 0, T0, false);
  TEST_ASSERT_FALSE(v.doorOpen);
  TEST_ASSERT_EQUAL_INT(SBENV_NOISE_NONE, v.noise);
}

void test_puerta_abierta_y_ruido_medio(void) {
  const SbEnvMsg m = msg(true, PROTO_ENV_DOOR_OPEN, 70);
  const SbEnvView v = sbenv_view(true, &m, T0, T0 + 1000u, false);
  TEST_ASSERT_TRUE(v.doorOpen);
  TEST_ASSERT_EQUAL_INT(SBENV_NOISE_MID, v.noise);
}

void test_puerta_cerrada_no_se_pinta(void) {
  const SbEnvMsg m = msg(true, PROTO_ENV_DOOR_CLOSED, 60);
  const SbEnvView v = sbenv_view(true, &m, T0, T0, false);
  TEST_ASSERT_FALSE(v.doorOpen);
  TEST_ASSERT_EQUAL_INT(SBENV_NOISE_LOW, v.noise);
}

void test_puerta_en_fallo_no_se_pinta_pero_el_ruido_si(void) {
  const SbEnvMsg m = msg(true, PROTO_ENV_DOOR_FAULT, 70);
  const SbEnvView v = sbenv_view(true, &m, T0, T0, false);
  TEST_ASSERT_FALSE(v.doorOpen);
  TEST_ASSERT_EQUAL_INT(SBENV_NOISE_MID, v.noise);
}

void test_puerta_desconocida_no_se_pinta(void) {
  const SbEnvMsg m = msg(true, PROTO_ENV_DOOR_UNKNOWN, 70);
  TEST_ASSERT_FALSE(sbenv_view(true, &m, T0, T0, false).doorOpen);
}

void test_enlace_sensorboard_caido_no_pinta_nada(void) {
  const SbEnvMsg m = msg(false, PROTO_ENV_DOOR_UNKNOWN, PROTO_ENV_UNAVAILABLE);
  const SbEnvView v = sbenv_view(true, &m, T0, T0, false);
  TEST_ASSERT_FALSE(v.doorOpen);
  TEST_ASSERT_EQUAL_INT(SBENV_NOISE_NONE, v.noise);
}

void test_enlace_caido_manda_aunque_los_valores_parezcan_buenos(void) {
  // Una linea incoherente (link=0 con valores) no debe pintar nada.
  const SbEnvMsg m = msg(false, PROTO_ENV_DOOR_OPEN, 90);
  const SbEnvView v = sbenv_view(true, &m, T0, T0, false);
  TEST_ASSERT_FALSE(v.doorOpen);
  TEST_ASSERT_EQUAL_INT(SBENV_NOISE_NONE, v.noise);
}

void test_ruido_no_disponible_con_puerta_abierta(void) {
  const SbEnvMsg m = msg(true, PROTO_ENV_DOOR_OPEN, PROTO_ENV_UNAVAILABLE);
  const SbEnvView v = sbenv_view(true, &m, T0, T0, false);
  TEST_ASSERT_TRUE(v.doorOpen);
  TEST_ASSERT_EQUAL_INT(SBENV_NOISE_NONE, v.noise);
}

void test_dato_rancio_no_pinta_nada(void) {
  const SbEnvMsg m = msg(true, PROTO_ENV_DOOR_OPEN, 90);
  const SbEnvView v = sbenv_view(true, &m, T0, T0 + SBENV_STALE_MS + 1u, false);
  TEST_ASSERT_FALSE(v.doorOpen);
  TEST_ASSERT_EQUAL_INT(SBENV_NOISE_NONE, v.noise);
}

void test_en_el_limite_de_frescura_aun_se_pinta(void) {
  const SbEnvMsg m = msg(true, PROTO_ENV_DOOR_OPEN, 90);
  const SbEnvView v = sbenv_view(true, &m, T0, T0 + SBENV_STALE_MS, false);
  TEST_ASSERT_TRUE(v.doorOpen);
  TEST_ASSERT_EQUAL_INT(SBENV_NOISE_HIGH, v.noise);
}

void test_frescura_son_tres_periodos(void) {
  TEST_ASSERT_EQUAL_UINT32(15000u, SBENV_STALE_MS);
}

void test_enlace_con_la_placa_caido_no_pinta_nada(void) {
  const SbEnvMsg m = msg(true, PROTO_ENV_DOOR_OPEN, 80);
  const SbEnvView v = sbenv_view(true, &m, T0, T0, true);
  TEST_ASSERT_FALSE(v.doorOpen);
  TEST_ASSERT_EQUAL_INT(SBENV_NOISE_NONE, v.noise);
}

void test_frescura_con_vuelta_del_contador(void) {
  const SbEnvMsg m = msg(true, PROTO_ENV_DOOR_OPEN, 70);
  // Recibida a 296 ms de la vuelta; ahora son 4000 ms tras ella: 4296 ms.
  SbEnvView v = sbenv_view(true, &m, 4294967000u, 4000u, false);
  TEST_ASSERT_TRUE(v.doorOpen);
  // Y 15297 ms despues ya esta rancia.
  v = sbenv_view(true, &m, 4294967000u, 15001u, false);
  TEST_ASSERT_FALSE(v.doorOpen);
}

void test_puntero_nulo_no_pinta_nada(void) {
  const SbEnvView v = sbenv_view(true, NULL, T0, T0, false);
  TEST_ASSERT_FALSE(v.doorOpen);
  TEST_ASSERT_EQUAL_INT(SBENV_NOISE_NONE, v.noise);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_linea_sana);
  RUN_TEST(test_centinelas_de_no_disponible);
  RUN_TEST(test_los_cuatro_estados_de_puerta);
  RUN_TEST(test_cero_valido_no_es_centinela);
  RUN_TEST(test_negativo_cualquiera_se_guarda_como_no_disponible);
  RUN_TEST(test_acepta_campos_extra_al_final);
  RUN_TEST(test_rechaza_linea_truncada_en_cada_campo);
  RUN_TEST(test_rechaza_campos_no_numericos);
  RUN_TEST(test_rechaza_link_y_door_fuera_de_rango);
  RUN_TEST(test_rechaza_otro_mensaje_con_prefijo_parecido);
  RUN_TEST(test_rechaza_puntero_nulo_sin_romper);
  RUN_TEST(test_numero_desbordado_se_rechaza_o_se_acota_pero_no_rompe);
  RUN_TEST(test_reposo_medido_es_bajo);
  RUN_TEST(test_bordes_de_los_umbrales);
  RUN_TEST(test_los_umbrales_son_los_documentados);
  RUN_TEST(test_extremos);
  RUN_TEST(test_no_disponible_no_tiene_nivel);
  RUN_TEST(test_sin_sensorboard_nunca_se_pinta_nada);
  RUN_TEST(test_puerta_abierta_y_ruido_medio);
  RUN_TEST(test_puerta_cerrada_no_se_pinta);
  RUN_TEST(test_puerta_en_fallo_no_se_pinta_pero_el_ruido_si);
  RUN_TEST(test_puerta_desconocida_no_se_pinta);
  RUN_TEST(test_enlace_sensorboard_caido_no_pinta_nada);
  RUN_TEST(test_enlace_caido_manda_aunque_los_valores_parezcan_buenos);
  RUN_TEST(test_ruido_no_disponible_con_puerta_abierta);
  RUN_TEST(test_dato_rancio_no_pinta_nada);
  RUN_TEST(test_en_el_limite_de_frescura_aun_se_pinta);
  RUN_TEST(test_frescura_son_tres_periodos);
  RUN_TEST(test_enlace_con_la_placa_caido_no_pinta_nada);
  RUN_TEST(test_frescura_con_vuelta_del_contador);
  RUN_TEST(test_puntero_nulo_no_pinta_nada);
  return UNITY_END();
}
