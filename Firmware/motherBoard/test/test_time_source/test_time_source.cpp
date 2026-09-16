#include <unity.h>

#include "modules/util/time_source.h"

void setUp(void) { time_source_reset(); }
void tearDown(void) {}

// Epochs de prueba dentro de la ventana valida, separados entre si para que
// una confusion de variables se vea en el assert.
static const uint32_t E_2026 = 1789000000u; // 2026-09-08
static const uint32_t E_2027 = 1820000000u; // 2027-09-03

// --- Estado inicial -------------------------------------------------------

// Arrancar sin hora NO es lo mismo que arrancar en 1970: el display tiene que
// poder pintar el aviso "Sin hora" en vez de una fecha falsa.
void test_starts_unknown(void) {
  TEST_ASSERT_FALSE(time_source_known());
  TEST_ASSERT_EQUAL_INT(PROTO_TIME_SOURCE_NONE, time_source_origin());
}

void test_any_source_wins_over_nothing(void) {
  TEST_ASSERT_TRUE(time_source_accepts(PROTO_TIME_SOURCE_NITZ));
  TEST_ASSERT_TRUE(time_source_set(E_2026, PROTO_TIME_SOURCE_NITZ));
  TEST_ASSERT_TRUE(time_source_known());
  TEST_ASSERT_EQUAL_INT(PROTO_TIME_SOURCE_NITZ, time_source_origin());
}

// --- Politica de prioridad: manual > NTP > RTC > NITZ ----------------------

// El caso que motiva todo el cambio: el equipo arranca sembrado por su propio
// RTC y en cuanto aparece la red, NTP lo corrige. El RTC deriva minutos al
// mes, asi que NUNCA debe quedarse por encima de un NTP.
void test_ntp_overrides_rtc(void) {
  TEST_ASSERT_TRUE(time_source_set(E_2026, PROTO_TIME_SOURCE_RTC));
  TEST_ASSERT_TRUE(time_source_accepts(PROTO_TIME_SOURCE_NTP));
  TEST_ASSERT_TRUE(time_source_set(E_2027, PROTO_TIME_SOURCE_NTP));
  TEST_ASSERT_EQUAL_UINT32(E_2027, time_source_epoch());
  TEST_ASSERT_EQUAL_INT(PROTO_TIME_SOURCE_NTP, time_source_origin());
}

// NITZ va el ultimo a peticion expresa: muchos operadores no lo emiten, o lo
// emiten con minutos de error y a veces sin zona. Una semilla del RTC, aun
// derivada, es preferible.
void test_nitz_does_not_override_rtc(void) {
  TEST_ASSERT_TRUE(time_source_set(E_2026, PROTO_TIME_SOURCE_RTC));
  TEST_ASSERT_FALSE(time_source_accepts(PROTO_TIME_SOURCE_NITZ));
  TEST_ASSERT_FALSE(time_source_set(E_2027, PROTO_TIME_SOURCE_NITZ));
  TEST_ASSERT_EQUAL_UINT32(E_2026, time_source_epoch());
  TEST_ASSERT_EQUAL_INT(PROTO_TIME_SOURCE_RTC, time_source_origin());
}

void test_rtc_does_not_override_ntp(void) {
  TEST_ASSERT_TRUE(time_source_set(E_2026, PROTO_TIME_SOURCE_NTP));
  TEST_ASSERT_FALSE(time_source_set(E_2027, PROTO_TIME_SOURCE_RTC));
  TEST_ASSERT_EQUAL_UINT32(E_2026, time_source_epoch());
  TEST_ASSERT_EQUAL_INT(PROTO_TIME_SOURCE_NTP, time_source_origin());
}

// Lo que ya prometia system_clock.h y hay que seguir cumpliendo: una vez que
// el operador teclea la hora, nada la desplaza hasta el reinicio.
void test_nothing_overrides_manual(void) {
  TEST_ASSERT_TRUE(time_source_set(E_2026, PROTO_TIME_SOURCE_MANUAL));
  TEST_ASSERT_FALSE(time_source_set(E_2027, PROTO_TIME_SOURCE_NTP));
  TEST_ASSERT_FALSE(time_source_set(E_2027, PROTO_TIME_SOURCE_RTC));
  TEST_ASSERT_FALSE(time_source_set(E_2027, PROTO_TIME_SOURCE_NITZ));
  TEST_ASSERT_EQUAL_UINT32(E_2026, time_source_epoch());
  TEST_ASSERT_EQUAL_INT(PROTO_TIME_SOURCE_MANUAL, time_source_origin());
}

void test_manual_overrides_everything_else(void) {
  TEST_ASSERT_TRUE(time_source_set(E_2026, PROTO_TIME_SOURCE_NTP));
  TEST_ASSERT_TRUE(time_source_set(E_2027, PROTO_TIME_SOURCE_MANUAL));
  TEST_ASSERT_EQUAL_UINT32(E_2027, time_source_epoch());
  TEST_ASSERT_EQUAL_INT(PROTO_TIME_SOURCE_MANUAL, time_source_origin());
}

// Un NTP que vuelve a sincronizar tiene que poder corregir la deriva, o el
// primer valor quedaria congelado de por vida. Mismo criterio que
// tz_source_set() dentro de la misma fuente.
void test_same_rank_refreshes(void) {
  TEST_ASSERT_TRUE(time_source_set(E_2026, PROTO_TIME_SOURCE_NTP));
  TEST_ASSERT_TRUE(time_source_set(E_2027, PROTO_TIME_SOURCE_NTP));
  TEST_ASSERT_EQUAL_UINT32(E_2027, time_source_epoch());
}

// Incluso manual sobre manual: el operador puede corregirse.
void test_manual_can_be_re_entered(void) {
  TEST_ASSERT_TRUE(time_source_set(E_2026, PROTO_TIME_SOURCE_MANUAL));
  TEST_ASSERT_TRUE(time_source_set(E_2027, PROTO_TIME_SOURCE_MANUAL));
  TEST_ASSERT_EQUAL_UINT32(E_2027, time_source_epoch());
}

// --- Validacion de ventana ------------------------------------------------

// Misma ventana que civil_to_unix_utc(): un RTC con la pila agotada devuelve
// basura, y una parte de esa basura cae en 1970.
void test_rejects_epoch_before_2021(void) {
  TEST_ASSERT_FALSE(time_source_set(0, PROTO_TIME_SOURCE_RTC));
  TEST_ASSERT_FALSE(time_source_set(1609459199u, PROTO_TIME_SOURCE_NTP));
  TEST_ASSERT_FALSE(time_source_known());
}

void test_rejects_epoch_from_2100(void) {
  TEST_ASSERT_FALSE(time_source_set(4102444800u, PROTO_TIME_SOURCE_NTP));
  TEST_ASSERT_FALSE(time_source_known());
}

void test_accepts_window_extremes(void) {
  TEST_ASSERT_TRUE(time_source_set(1609459200u, PROTO_TIME_SOURCE_NTP));
  time_source_reset();
  TEST_ASSERT_TRUE(time_source_set(4102444799u, PROTO_TIME_SOURCE_NTP));
}

// Un epoch invalido de una fuente BUENA no debe tirar por tierra una hora
// valida ya obtenida de una fuente peor.
void test_invalid_epoch_does_not_clobber_good_time(void) {
  TEST_ASSERT_TRUE(time_source_set(E_2026, PROTO_TIME_SOURCE_RTC));
  TEST_ASSERT_FALSE(time_source_set(0, PROTO_TIME_SOURCE_MANUAL));
  TEST_ASSERT_EQUAL_UINT32(E_2026, time_source_epoch());
  TEST_ASSERT_EQUAL_INT(PROTO_TIME_SOURCE_RTC, time_source_origin());
}

// --- Reinicio -------------------------------------------------------------

// El rango vive solo en RAM a proposito, igual que la marca manual de hoy: un
// ciclo de alimentacion pierde el reloj de todas formas, asi que tras
// reiniciar las fuentes automaticas vuelven a tener via libre.
void test_reset_clears_rank(void) {
  TEST_ASSERT_TRUE(time_source_set(E_2026, PROTO_TIME_SOURCE_MANUAL));
  time_source_reset();
  TEST_ASSERT_FALSE(time_source_known());
  TEST_ASSERT_EQUAL_INT(PROTO_TIME_SOURCE_NONE, time_source_origin());
  TEST_ASSERT_TRUE(time_source_set(E_2027, PROTO_TIME_SOURCE_NITZ));
}

// --- accepts() no tiene efectos -------------------------------------------

// Se consulta antes de llamar a settimeofday(), asi que preguntarlo dos veces
// tiene que dar lo mismo.
void test_accepts_is_side_effect_free(void) {
  TEST_ASSERT_TRUE(time_source_set(E_2026, PROTO_TIME_SOURCE_RTC));
  TEST_ASSERT_TRUE(time_source_accepts(PROTO_TIME_SOURCE_NTP));
  TEST_ASSERT_TRUE(time_source_accepts(PROTO_TIME_SOURCE_NTP));
  TEST_ASSERT_EQUAL_INT(PROTO_TIME_SOURCE_RTC, time_source_origin());
  TEST_ASSERT_EQUAL_UINT32(E_2026, time_source_epoch());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_unknown);
  RUN_TEST(test_any_source_wins_over_nothing);
  RUN_TEST(test_ntp_overrides_rtc);
  RUN_TEST(test_nitz_does_not_override_rtc);
  RUN_TEST(test_rtc_does_not_override_ntp);
  RUN_TEST(test_nothing_overrides_manual);
  RUN_TEST(test_manual_overrides_everything_else);
  RUN_TEST(test_same_rank_refreshes);
  RUN_TEST(test_manual_can_be_re_entered);
  RUN_TEST(test_rejects_epoch_before_2021);
  RUN_TEST(test_rejects_epoch_from_2100);
  RUN_TEST(test_accepts_window_extremes);
  RUN_TEST(test_invalid_epoch_does_not_clobber_good_time);
  RUN_TEST(test_reset_clears_rank);
  RUN_TEST(test_accepts_is_side_effect_free);
  return UNITY_END();
}
