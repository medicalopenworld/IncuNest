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

// --- El huso se ordena aparte del instante ---------------------------------
//
// Al meter el RTC del HMI en la jerarquia de fuentes de HORA, NITZ bajo al
// ultimo puesto: muchos operadores no lo emiten, o lo emiten con minutos de
// error. Pero para el HUSO sigue siendo la MEJOR fuente, porque la antena esta
// fisicamente donde esta el equipo. Son dos escalas distintas y este par de
// tests existe para que nadie las funda "simplificando".

// El caso cruzado que importa: un NITZ que NO puede fijar el reloj —porque ya
// lo fijo NTP, que gana— tiene que seguir aportando su offset.
void test_nitz_still_wins_the_zone_after_losing_the_clock(void) {
  TEST_ASSERT_TRUE(tz_source_set(8, TZ_SOURCE_IP));
  // El reloj lo puso NTP; aqui solo llega la zona del modem.
  TEST_ASSERT_TRUE(tz_source_set(4, TZ_SOURCE_NITZ));
  TEST_ASSERT_EQUAL_INT(4, tz_source_quarters());
  TEST_ASSERT_EQUAL_INT(TZ_SOURCE_NITZ, tz_source_origin());
}

// Y al reves: la escala del huso no conoce ni NTP ni RTC, asi que nada de lo
// que pase con el instante debe degradar un huso ya resuelto por NITZ.
void test_zone_survives_a_better_clock_source(void) {
  TEST_ASSERT_TRUE(tz_source_set(4, TZ_SOURCE_NITZ));
  TEST_ASSERT_FALSE(tz_source_set(8, TZ_SOURCE_IP));
  TEST_ASSERT_EQUAL_INT(4, tz_source_quarters());
  TEST_ASSERT_EQUAL_INT(TZ_SOURCE_NITZ, tz_source_origin());
}

// --- El huso manual va atado al epoch manual ------------------------------
//
// La hora manual es hora LOCAL guardada como si fuera UTC, con huso 0 y
// rango MANUAL. Ese huso 0 solo es verdad mientras el epoch siga siendo el
// tecleado. Cuando NTP o NITZ ponen el UTC de verdad, un huso MANUAL a 0
// dejaria la pantalla en UTC para siempre, porque nada pisa a MANUAL. En la
// 353 venia ademas sembrado desde la NVS del HMI en cada arranque.
void test_drop_manual_frees_the_zone(void) {
  TEST_ASSERT_TRUE(tz_source_set(0, TZ_SOURCE_MANUAL));
  tz_source_drop_manual();
  TEST_ASSERT_FALSE(tz_source_known());
  TEST_ASSERT_EQUAL_INT(TZ_SOURCE_NONE, tz_source_origin());
  TEST_ASSERT_TRUE(tz_source_set(4, TZ_SOURCE_IP));
  TEST_ASSERT_EQUAL_INT(4, tz_source_quarters());
}

void test_drop_manual_keeps_a_network_zone(void) {
  TEST_ASSERT_TRUE(tz_source_set(4, TZ_SOURCE_NITZ));
  tz_source_drop_manual();
  TEST_ASSERT_EQUAL_INT(TZ_SOURCE_NITZ, tz_source_origin());
  TEST_ASSERT_EQUAL_INT(4, tz_source_quarters());

  tz_source_reset();
  TEST_ASSERT_TRUE(tz_source_set(-12, TZ_SOURCE_IP));
  tz_source_drop_manual();
  TEST_ASSERT_EQUAL_INT(TZ_SOURCE_IP, tz_source_origin());
  TEST_ASSERT_EQUAL_INT(-12, tz_source_quarters());
}

// --- Unidades del huso del modem --------------------------------------------
//
// TinyGSM (TinyGsmTime.tpp) lee el AT+CCLK? en cuartos de hora y lo entrega
// ya DIVIDIDO por 4, en horas: "+08" llega como 2.0. GPRS.cpp lo pasaba con un
// (int) a civil_to_unix_utc() y a tz_source_set(), que esperan CUARTOS. En
// CEST eso restaba 30 min en vez de 2 h (reloj +1 h 30) y pintaba UTC+0:30.
void test_modem_hours_to_quarters(void) {
  int q = 99;
  TEST_ASSERT_TRUE(tz_quarters_from_modem_hours(2.0f, &q));
  TEST_ASSERT_EQUAL_INT(8, q);
  TEST_ASSERT_TRUE(tz_quarters_from_modem_hours(1.0f, &q));
  TEST_ASSERT_EQUAL_INT(4, q);
  TEST_ASSERT_TRUE(tz_quarters_from_modem_hours(0.0f, &q));
  TEST_ASSERT_EQUAL_INT(0, q);
  TEST_ASSERT_TRUE(tz_quarters_from_modem_hours(-3.5f, &q)); // Terranova
  TEST_ASSERT_EQUAL_INT(-14, q);
  TEST_ASSERT_TRUE(tz_quarters_from_modem_hours(5.75f, &q)); // Nepal
  TEST_ASSERT_EQUAL_INT(23, q);
  TEST_ASSERT_TRUE(tz_quarters_from_modem_hours(0.25f, &q));
  TEST_ASSERT_EQUAL_INT(1, q);
}

void test_modem_hours_out_of_range_rejected(void) {
  int q = 99;
  TEST_ASSERT_FALSE(tz_quarters_from_modem_hours(14.25f, &q));
  TEST_ASSERT_FALSE(tz_quarters_from_modem_hours(-12.25f, &q));
  TEST_ASSERT_EQUAL_INT(99, q);
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
  RUN_TEST(test_nitz_still_wins_the_zone_after_losing_the_clock);
  RUN_TEST(test_zone_survives_a_better_clock_source);
  RUN_TEST(test_drop_manual_frees_the_zone);
  RUN_TEST(test_drop_manual_keeps_a_network_zone);
  RUN_TEST(test_modem_hours_to_quarters);
  RUN_TEST(test_modem_hours_out_of_range_rejected);
  return UNITY_END();
}
