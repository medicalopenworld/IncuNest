#include <unity.h>

#include "drivers/rtc_write_policy.h"

void setUp(void) {}
void tearDown(void) {}

static const uint32_t E = 1789482645u; // 2026-09-16T14:30:45Z

// --- Mejora de rango ------------------------------------------------------

// El caso normal: el equipo arranco sembrado por su propio RTC y ahora
// aparece la red. Se escribe aunque la diferencia sea minima, porque lo que
// mejora es la CONFIANZA, no el valor.
void test_writes_when_source_improves(void) {
  TEST_ASSERT_TRUE(rtc_should_write(E + 1, PROTO_TIME_SOURCE_NTP, E,
                                    PROTO_TIME_SOURCE_RTC));
}

void test_writes_when_manual_beats_ntp(void) {
  TEST_ASSERT_TRUE(rtc_should_write(E + 1, PROTO_TIME_SOURCE_MANUAL, E,
                                    PROTO_TIME_SOURCE_NTP));
}

// Chip virgen o con VL puesto: cualquier fuente real mejora la nada.
void test_writes_into_empty_chip(void) {
  TEST_ASSERT_TRUE(
      rtc_should_write(E, PROTO_TIME_SOURCE_NITZ, 0, PROTO_TIME_SOURCE_NONE));
}

// --- Rango peor -----------------------------------------------------------

// NITZ va el ultimo: llega con minutos de error y no debe pisar una hora de
// NTP ya guardada, por mucho que sea mas reciente.
void test_does_not_write_when_source_is_worse(void) {
  TEST_ASSERT_FALSE(rtc_should_write(E + 3600, PROTO_TIME_SOURCE_NITZ, E,
                                     PROTO_TIME_SOURCE_NTP));
}

void test_rtc_does_not_overwrite_manual(void) {
  TEST_ASSERT_FALSE(rtc_should_write(E + 3600, PROTO_TIME_SOURCE_RTC, E,
                                     PROTO_TIME_SOURCE_MANUAL));
}

// --- Mismo rango: manda la deriva -----------------------------------------

// La difusion llega cada 10 s; sin umbral, el HMI escribiria el chip seis
// veces por minuto para nada, en un bus que comparte con el tactil.
void test_does_not_write_without_meaningful_drift(void) {
  TEST_ASSERT_FALSE(
      rtc_should_write(E, PROTO_TIME_SOURCE_NTP, E, PROTO_TIME_SOURCE_NTP));
  TEST_ASSERT_FALSE(
      rtc_should_write(E + 1, PROTO_TIME_SOURCE_NTP, E, PROTO_TIME_SOURCE_NTP));
  TEST_ASSERT_FALSE(
      rtc_should_write(E + 2, PROTO_TIME_SOURCE_NTP, E, PROTO_TIME_SOURCE_NTP));
}

void test_writes_past_the_drift_threshold(void) {
  TEST_ASSERT_TRUE(
      rtc_should_write(E + 3, PROTO_TIME_SOURCE_NTP, E, PROTO_TIME_SOURCE_NTP));
}

// La deriva cuenta en los dos sentidos: un RTC puede ir adelantado.
void test_drift_is_symmetric(void) {
  TEST_ASSERT_TRUE(
      rtc_should_write(E, PROTO_TIME_SOURCE_NTP, E + 3, PROTO_TIME_SOURCE_NTP));
  TEST_ASSERT_FALSE(
      rtc_should_write(E, PROTO_TIME_SOURCE_NTP, E + 2, PROTO_TIME_SOURCE_NTP));
}

// Mismo rango pero el chip no tiene hora: hay que sembrarlo igual.
void test_writes_when_chip_has_no_time(void) {
  TEST_ASSERT_TRUE(
      rtc_should_write(E, PROTO_TIME_SOURCE_RTC, 0, PROTO_TIME_SOURCE_RTC));
}

// --- Entradas que nunca escriben ------------------------------------------

// El caso mas frecuente de un arranque sin red: la motherBoard difunde
// epoch 0 cada 10 s mientras no sincroniza.
void test_never_writes_epoch_zero(void) {
  TEST_ASSERT_FALSE(
      rtc_should_write(0, PROTO_TIME_SOURCE_NTP, E, PROTO_TIME_SOURCE_NITZ));
  TEST_ASSERT_FALSE(
      rtc_should_write(0, PROTO_TIME_SOURCE_MANUAL, 0, PROTO_TIME_SOURCE_NONE));
}

void test_never_writes_out_of_window_epoch(void) {
  TEST_ASSERT_FALSE(rtc_should_write(1609459199u, PROTO_TIME_SOURCE_MANUAL, 0,
                                     PROTO_TIME_SOURCE_NONE));
  TEST_ASSERT_FALSE(rtc_should_write(4102444800u, PROTO_TIME_SOURCE_MANUAL, 0,
                                     PROTO_TIME_SOURCE_NONE));
}

// Una motherBoard anterior a este cambio no envia el campo `src`, asi que
// llega como NONE. No mejora nada, ni siquiera un chip vacio: esa hora podria
// venir de un NITZ malo y lo que el chip ya tenga es al menos trazable.
void test_unknown_source_never_writes(void) {
  TEST_ASSERT_FALSE(
      rtc_should_write(E, PROTO_TIME_SOURCE_NONE, 0, PROTO_TIME_SOURCE_NONE));
  TEST_ASSERT_FALSE(
      rtc_should_write(E, PROTO_TIME_SOURCE_NONE, E, PROTO_TIME_SOURCE_RTC));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_writes_when_source_improves);
  RUN_TEST(test_writes_when_manual_beats_ntp);
  RUN_TEST(test_writes_into_empty_chip);
  RUN_TEST(test_does_not_write_when_source_is_worse);
  RUN_TEST(test_rtc_does_not_overwrite_manual);
  RUN_TEST(test_does_not_write_without_meaningful_drift);
  RUN_TEST(test_writes_past_the_drift_threshold);
  RUN_TEST(test_drift_is_symmetric);
  RUN_TEST(test_writes_when_chip_has_no_time);
  RUN_TEST(test_never_writes_epoch_zero);
  RUN_TEST(test_never_writes_out_of_window_epoch);
  RUN_TEST(test_unknown_source_never_writes);
  return UNITY_END();
}
