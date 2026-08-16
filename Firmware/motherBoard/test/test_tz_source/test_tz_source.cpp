#include <unity.h>

#include "modules/util/tz_source.h"

void setUp(void) { tz_source_reset(); }
void tearDown(void) {}

// --- Estado inicial -------------------------------------------------------

// Sin fuente, el offset NO debe parecer un UTC+0 legitimo: "no lo se" y
// "estamos en Togo" son cosas distintas y el display tiene que poder
// distinguirlas para decidir si pinta la hora o el aviso.
void test_starts_unknown(void) {
  TEST_ASSERT_FALSE(tz_source_known());
  TEST_ASSERT_EQUAL_INT(TZ_SOURCE_NONE, tz_source_origin());
}

// --- Politica de prioridad ------------------------------------------------

void test_ip_sets_offset_when_nothing_known(void) {
  TEST_ASSERT_TRUE(tz_source_set(8, TZ_SOURCE_IP));
  TEST_ASSERT_TRUE(tz_source_known());
  TEST_ASSERT_EQUAL_INT(8, tz_source_quarters());
  TEST_ASSERT_EQUAL_INT(TZ_SOURCE_IP, tz_source_origin());
}

// La antena esta fisicamente donde esta el equipo; una IP puede ser de una
// VPN, un enlace satelital o la sede del operador en otro pais.
void test_nitz_overrides_ip(void) {
  TEST_ASSERT_TRUE(tz_source_set(8, TZ_SOURCE_IP));
  TEST_ASSERT_TRUE(tz_source_set(0, TZ_SOURCE_NITZ));
  TEST_ASSERT_EQUAL_INT(0, tz_source_quarters());
  TEST_ASSERT_EQUAL_INT(TZ_SOURCE_NITZ, tz_source_origin());
}

void test_ip_does_not_override_nitz(void) {
  TEST_ASSERT_TRUE(tz_source_set(4, TZ_SOURCE_NITZ));
  TEST_ASSERT_FALSE(tz_source_set(8, TZ_SOURCE_IP));
  TEST_ASSERT_EQUAL_INT(4, tz_source_quarters());
  TEST_ASSERT_EQUAL_INT(TZ_SOURCE_NITZ, tz_source_origin());
}

// Cruzar una frontera debe poder actualizar el offset dentro de la misma
// fuente, o el primer valor quedaria congelado de por vida.
void test_same_source_refreshes(void) {
  TEST_ASSERT_TRUE(tz_source_set(4, TZ_SOURCE_NITZ));
  TEST_ASSERT_TRUE(tz_source_set(8, TZ_SOURCE_NITZ));
  TEST_ASSERT_EQUAL_INT(8, tz_source_quarters());

  tz_source_reset();
  TEST_ASSERT_TRUE(tz_source_set(4, TZ_SOURCE_IP));
  TEST_ASSERT_TRUE(tz_source_set(-20, TZ_SOURCE_IP));
  TEST_ASSERT_EQUAL_INT(-20, tz_source_quarters());
}

// --- Validacion de rango --------------------------------------------------

void test_rejects_out_of_range_offset(void) {
  TEST_ASSERT_FALSE(tz_source_set(TZ_QUARTER_MIN - 1, TZ_SOURCE_NITZ));
  TEST_ASSERT_FALSE(tz_source_set(TZ_QUARTER_MAX + 1, TZ_SOURCE_NITZ));
  TEST_ASSERT_FALSE(tz_source_known());
}

// Un valor invalido no debe degradar un offset bueno ya obtenido.
void test_invalid_does_not_clobber_known_offset(void) {
  TEST_ASSERT_TRUE(tz_source_set(8, TZ_SOURCE_NITZ));
  TEST_ASSERT_FALSE(tz_source_set(999, TZ_SOURCE_NITZ));
  TEST_ASSERT_EQUAL_INT(8, tz_source_quarters());
  TEST_ASSERT_EQUAL_INT(TZ_SOURCE_NITZ, tz_source_origin());
}

void test_accepts_range_extremes(void) {
  TEST_ASSERT_TRUE(tz_source_set(TZ_QUARTER_MIN, TZ_SOURCE_NITZ));
  TEST_ASSERT_EQUAL_INT(TZ_QUARTER_MIN, tz_source_quarters());
  tz_source_reset();
  TEST_ASSERT_TRUE(tz_source_set(TZ_QUARTER_MAX, TZ_SOURCE_NITZ));
  TEST_ASSERT_EQUAL_INT(TZ_QUARTER_MAX, tz_source_quarters());
}

// --- Parseo de la respuesta de ip-api.com ---------------------------------
// El campo "offset" llega en SEGUNDOS (documentado por el servicio).

void test_parses_positive_offset(void) {
  int q = 0;
  TEST_ASSERT_TRUE(
      tz_parse_ipapi_offset("{\"status\":\"success\",\"offset\":7200}", &q));
  TEST_ASSERT_EQUAL_INT(8, q); // UTC+2
}

void test_parses_negative_offset(void) {
  int q = 0;
  TEST_ASSERT_TRUE(
      tz_parse_ipapi_offset("{\"status\":\"success\",\"offset\":-25200}", &q));
  TEST_ASSERT_EQUAL_INT(-28, q); // UTC-7
}

void test_parses_zero_offset(void) {
  int q = -1;
  TEST_ASSERT_TRUE(
      tz_parse_ipapi_offset("{\"status\":\"success\",\"offset\":0}", &q));
  TEST_ASSERT_EQUAL_INT(0, q); // UTC+0, el caso de Togo
}

// Nepal, UTC+5:45. Los husos no enteros son la razon de contar en cuartos.
void test_parses_quarter_hour_offset(void) {
  int q = 0;
  TEST_ASSERT_TRUE(
      tz_parse_ipapi_offset("{\"status\":\"success\",\"offset\":20700}", &q));
  TEST_ASSERT_EQUAL_INT(23, q);
}

void test_rejects_missing_offset_field(void) {
  int q = 0;
  TEST_ASSERT_FALSE(tz_parse_ipapi_offset("{\"status\":\"success\"}", &q));
}

void test_rejects_failed_lookup(void) {
  int q = 0;
  TEST_ASSERT_FALSE(tz_parse_ipapi_offset(
      "{\"status\":\"fail\",\"message\":\"private range\"}", &q));
}

// Truncada a mitad del numero: sin el terminador no hay valor completo que
// creerse. Politica de descarte silencioso de .claude/rules/security.md.
void test_rejects_truncated_json(void) {
  int q = 0;
  TEST_ASSERT_FALSE(
      tz_parse_ipapi_offset("{\"status\":\"success\",\"offset\":72", &q));
}

void test_rejects_offset_out_of_physical_range(void) {
  int q = 0;
  TEST_ASSERT_FALSE(
      tz_parse_ipapi_offset("{\"status\":\"success\",\"offset\":999999}", &q));
}

void test_rejects_garbage(void) {
  int q = 0;
  TEST_ASSERT_FALSE(tz_parse_ipapi_offset("", &q));
  TEST_ASSERT_FALSE(tz_parse_ipapi_offset("no soy json", &q));
  TEST_ASSERT_FALSE(
      tz_parse_ipapi_offset("{\"offset\":\"dos horas\"}", &q));
}

// Un campo que solo CONTIENE "offset" no es el campo offset.
void test_does_not_match_similar_field_names(void) {
  int q = 0;
  TEST_ASSERT_FALSE(
      tz_parse_ipapi_offset("{\"gmt_offset_hours\":2}", &q));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_unknown);
  RUN_TEST(test_ip_sets_offset_when_nothing_known);
  RUN_TEST(test_nitz_overrides_ip);
  RUN_TEST(test_ip_does_not_override_nitz);
  RUN_TEST(test_same_source_refreshes);
  RUN_TEST(test_rejects_out_of_range_offset);
  RUN_TEST(test_invalid_does_not_clobber_known_offset);
  RUN_TEST(test_accepts_range_extremes);
  RUN_TEST(test_parses_positive_offset);
  RUN_TEST(test_parses_negative_offset);
  RUN_TEST(test_parses_zero_offset);
  RUN_TEST(test_parses_quarter_hour_offset);
  RUN_TEST(test_rejects_missing_offset_field);
  RUN_TEST(test_rejects_failed_lookup);
  RUN_TEST(test_rejects_truncated_json);
  RUN_TEST(test_rejects_offset_out_of_physical_range);
  RUN_TEST(test_rejects_garbage);
  RUN_TEST(test_does_not_match_similar_field_names);
  return UNITY_END();
}
