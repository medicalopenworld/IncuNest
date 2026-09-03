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

// --- Reloj puesto a mano ---------------------------------------------------
// El ajuste manual de /config guarda LO QUE TECLEA EL OPERADOR como epoch, sin
// zona (civil_to_unix_utc con offset 0). Ese epoch YA es hora local, asi que
// sumarle luego un offset de NITZ o de IP lo desplazaria: en Espana, dos horas
// de error en la hora que fecha el historial de alarmas.
//
// Por eso el ajuste manual fija offset 0 y gana a todo: coincide con lo que
// ya decia system_clock.h — las fuentes automaticas no deben desplazar bajo
// los pies del operador una hora que ha puesto el a mano.

void test_manual_overrides_ip_and_nitz(void) {
  TEST_ASSERT_TRUE(tz_source_set(8, TZ_SOURCE_IP));
  TEST_ASSERT_TRUE(tz_source_set(0, TZ_SOURCE_MANUAL));
  TEST_ASSERT_EQUAL_INT(TZ_SOURCE_MANUAL, tz_source_origin());

  tz_source_reset();
  TEST_ASSERT_TRUE(tz_source_set(4, TZ_SOURCE_NITZ));
  TEST_ASSERT_TRUE(tz_source_set(0, TZ_SOURCE_MANUAL));
  TEST_ASSERT_EQUAL_INT(TZ_SOURCE_MANUAL, tz_source_origin());
  TEST_ASSERT_EQUAL_INT(0, tz_source_quarters());
}

// Lo critico: puesta la hora a mano, que NADA la desplace despues.
void test_nothing_overrides_manual(void) {
  TEST_ASSERT_TRUE(tz_source_set(0, TZ_SOURCE_MANUAL));
  TEST_ASSERT_FALSE(tz_source_set(8, TZ_SOURCE_NITZ));
  TEST_ASSERT_FALSE(tz_source_set(-20, TZ_SOURCE_IP));
  TEST_ASSERT_EQUAL_INT(0, tz_source_quarters());
  TEST_ASSERT_EQUAL_INT(TZ_SOURCE_MANUAL, tz_source_origin());
}

// Con hora manual el display SI tiene hora que pintar: el epoch ya es local.
void test_manual_counts_as_known(void) {
  TEST_ASSERT_TRUE(tz_source_set(0, TZ_SOURCE_MANUAL));
  TEST_ASSERT_TRUE(tz_source_known());
}

// --- Zona por defecto ------------------------------------------------------
// Una unidad que solo tiene GPRS y un operador que no manda NITZ se quedaba
// sin zona de por vida: la hora se pintaba en UTC, dos horas por debajo de la
// del reloj de la pared en Espana. El equipo se despliega con una zona de
// fabrica (+2) para que ese caso muestre algo razonable.
//
// Es la fuente de MENOR prioridad de todas, a proposito: es una suposicion,
// no un dato, y cualquiera que de verdad SEPA la zona tiene que poder
// corregirla sin esperar a un reinicio.

void test_default_sets_offset_when_nothing_known(void) {
  TEST_ASSERT_TRUE(tz_source_set(8, TZ_SOURCE_DEFAULT));
  TEST_ASSERT_TRUE(tz_source_known());
  TEST_ASSERT_EQUAL_INT(8, tz_source_quarters());
  TEST_ASSERT_EQUAL_INT(TZ_SOURCE_DEFAULT, tz_source_origin());
}

// Lo critico: la suposicion no puede bloquear al dato real, ni el de la
// antena, ni el de la IP, ni el que teclea el operador.
void test_every_real_source_overrides_default(void) {
  TEST_ASSERT_TRUE(tz_source_set(8, TZ_SOURCE_DEFAULT));
  TEST_ASSERT_TRUE(tz_source_set(4, TZ_SOURCE_NITZ));
  TEST_ASSERT_EQUAL_INT(4, tz_source_quarters());
  TEST_ASSERT_EQUAL_INT(TZ_SOURCE_NITZ, tz_source_origin());

  tz_source_reset();
  TEST_ASSERT_TRUE(tz_source_set(8, TZ_SOURCE_DEFAULT));
  TEST_ASSERT_TRUE(tz_source_set(-20, TZ_SOURCE_IP));
  TEST_ASSERT_EQUAL_INT(-20, tz_source_quarters());
  TEST_ASSERT_EQUAL_INT(TZ_SOURCE_IP, tz_source_origin());

  tz_source_reset();
  TEST_ASSERT_TRUE(tz_source_set(8, TZ_SOURCE_DEFAULT));
  TEST_ASSERT_TRUE(tz_source_set(0, TZ_SOURCE_MANUAL));
  TEST_ASSERT_EQUAL_INT(0, tz_source_quarters());
  TEST_ASSERT_EQUAL_INT(TZ_SOURCE_MANUAL, tz_source_origin());
}

// Y al reves: una vez hay dato real, la suposicion no vuelve a pisarlo. Sin
// esto, el refresco periodico de zona del GPRS reintroduciria el +2 encima
// del offset bueno cada vez que pasara por ahi.
void test_default_never_overrides_a_known_source(void) {
  TEST_ASSERT_TRUE(tz_source_set(4, TZ_SOURCE_NITZ));
  TEST_ASSERT_FALSE(tz_source_set(8, TZ_SOURCE_DEFAULT));
  TEST_ASSERT_EQUAL_INT(4, tz_source_quarters());
  TEST_ASSERT_EQUAL_INT(TZ_SOURCE_NITZ, tz_source_origin());

  tz_source_reset();
  TEST_ASSERT_TRUE(tz_source_set(-20, TZ_SOURCE_IP));
  TEST_ASSERT_FALSE(tz_source_set(8, TZ_SOURCE_DEFAULT));
  TEST_ASSERT_EQUAL_INT(-20, tz_source_quarters());

  tz_source_reset();
  TEST_ASSERT_TRUE(tz_source_set(0, TZ_SOURCE_MANUAL));
  TEST_ASSERT_FALSE(tz_source_set(8, TZ_SOURCE_DEFAULT));
  TEST_ASSERT_EQUAL_INT(TZ_SOURCE_MANUAL, tz_source_origin());
}

// Idempotente: aplicar el defecto dos veces no es un cambio de estado, asi
// que no debe reportarse como tal (quien llama lo usa para loguear una sola
// vez).
void test_default_over_default_is_not_a_change(void) {
  TEST_ASSERT_TRUE(tz_source_set(8, TZ_SOURCE_DEFAULT));
  TEST_ASSERT_FALSE(tz_source_set(8, TZ_SOURCE_DEFAULT));
  TEST_ASSERT_EQUAL_INT(8, tz_source_quarters());
}

// "Hay offset que aplicar" y "alguien nos ha dicho la zona de verdad" son dos
// preguntas distintas: la primera decide si el display pinta hora local, la
// segunda decide con que ritmo se sigue buscando la zona real. Con solo el
// defecto puesto, hay que seguir buscando deprisa.
void test_default_is_known_but_not_resolved(void) {
  TEST_ASSERT_TRUE(tz_source_set(8, TZ_SOURCE_DEFAULT));
  TEST_ASSERT_TRUE(tz_source_known());
  TEST_ASSERT_FALSE(tz_source_is_resolved());
}

void test_real_sources_are_resolved(void) {
  TEST_ASSERT_FALSE(tz_source_is_resolved()); // sin nada, tampoco

  TEST_ASSERT_TRUE(tz_source_set(4, TZ_SOURCE_NITZ));
  TEST_ASSERT_TRUE(tz_source_is_resolved());

  tz_source_reset();
  TEST_ASSERT_TRUE(tz_source_set(8, TZ_SOURCE_IP));
  TEST_ASSERT_TRUE(tz_source_is_resolved());

  tz_source_reset();
  TEST_ASSERT_TRUE(tz_source_set(0, TZ_SOURCE_MANUAL));
  TEST_ASSERT_TRUE(tz_source_is_resolved());
}

// El rango se valida igual que para cualquier otra fuente.
void test_default_respects_range(void) {
  TEST_ASSERT_FALSE(tz_source_set(TZ_QUARTER_MAX + 1, TZ_SOURCE_DEFAULT));
  TEST_ASSERT_FALSE(tz_source_known());
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
  RUN_TEST(test_manual_overrides_ip_and_nitz);
  RUN_TEST(test_nothing_overrides_manual);
  RUN_TEST(test_manual_counts_as_known);
  RUN_TEST(test_default_sets_offset_when_nothing_known);
  RUN_TEST(test_every_real_source_overrides_default);
  RUN_TEST(test_default_never_overrides_a_known_source);
  RUN_TEST(test_default_over_default_is_not_a_change);
  RUN_TEST(test_default_is_known_but_not_resolved);
  RUN_TEST(test_real_sources_are_resolved);
  RUN_TEST(test_default_respects_range);
  return UNITY_END();
}
