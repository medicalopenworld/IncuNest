#include <unity.h>

#include "modules/control/alarm_test.h"

void setUp(void) { alarm_test_init(); }
void tearDown(void) {}

void test_starts_idle(void) {
  TEST_ASSERT_FALSE(alarm_test_active());
  TEST_ASSERT_FALSE(alarm_test_audio_required());
  TEST_ASSERT_EQUAL_UINT8(ALARM_TEST_IDLE, alarm_test_priority());
}

// La secuencia sube de BAJA a MEDIA a ALTA. El orden importa: lo que el
// operador tiene que poder juzgar es que las tres suenan DISTINTO, y eso se
// aprecia oyendolas escalar.
void test_sequence_climbs_low_medium_high(void) {
  TEST_ASSERT_TRUE(alarm_test_start(0));
  TEST_ASSERT_EQUAL_UINT8(ALARM_PRIORITY_LOW, alarm_test_priority());
  TEST_ASSERT_TRUE(alarm_test_audio_required());

  // Hueco tras la rafaga de BAJA: sigue en marcha, pero sin audio.
  alarm_test_tick(ALARM_TEST_PHASE_MS_LOW);
  TEST_ASSERT_TRUE(alarm_test_active());
  TEST_ASSERT_FALSE(alarm_test_audio_required());
  TEST_ASSERT_EQUAL_UINT8(ALARM_TEST_IDLE, alarm_test_priority());

  alarm_test_tick(ALARM_TEST_PHASE_MS_LOW + ALARM_TEST_GAP_MS);
  TEST_ASSERT_EQUAL_UINT8(ALARM_PRIORITY_MEDIUM, alarm_test_priority());
  TEST_ASSERT_TRUE(alarm_test_audio_required());

  const uint32_t toHigh = ALARM_TEST_PHASE_MS_LOW + ALARM_TEST_GAP_MS +
                          ALARM_TEST_PHASE_MS_MEDIUM + ALARM_TEST_GAP_MS;
  alarm_test_tick(toHigh);
  TEST_ASSERT_EQUAL_UINT8(ALARM_PRIORITY_HIGH, alarm_test_priority());
  TEST_ASSERT_TRUE(alarm_test_audio_required());

  // Al terminar el ultimo tramo la prueba se apaga sola.
  alarm_test_tick(toHigh + ALARM_TEST_PHASE_MS_HIGH);
  TEST_ASSERT_FALSE(alarm_test_active());
  TEST_ASSERT_FALSE(alarm_test_audio_required());
  TEST_ASSERT_EQUAL_UINT8(ALARM_TEST_IDLE, alarm_test_priority());
}

// Un tick que se salta varios tramos de golpe no puede dejar la maquina en un
// tramo intermedio ni pasarse de largo del array.
void test_a_late_tick_does_not_overrun(void) {
  alarm_test_start(0);
  alarm_test_tick(1000000u);
  TEST_ASSERT_FALSE(alarm_test_active());
  TEST_ASSERT_EQUAL_UINT8(ALARM_TEST_IDLE, alarm_test_priority());
}

void test_start_is_refused_while_running(void) {
  TEST_ASSERT_TRUE(alarm_test_start(0));
  TEST_ASSERT_FALSE(alarm_test_start(10));
  // Y el rechazo no altera la secuencia en curso.
  TEST_ASSERT_EQUAL_UINT8(ALARM_PRIORITY_LOW, alarm_test_priority());
}

// Una alarma real cancela la prueba en el acto: es lo que hace
// driveAlarmBuzzer() en cuanto ve any_signalling().
void test_abort_stops_everything(void) {
  alarm_test_start(0);
  alarm_test_abort();
  TEST_ASSERT_FALSE(alarm_test_active());
  TEST_ASSERT_FALSE(alarm_test_audio_required());
  TEST_ASSERT_EQUAL_UINT8(ALARM_TEST_IDLE, alarm_test_priority());
  // Y el tick posterior no la resucita.
  alarm_test_tick(100);
  TEST_ASSERT_FALSE(alarm_test_active());
}

void test_abort_is_idempotent(void) {
  alarm_test_abort();
  alarm_test_abort();
  TEST_ASSERT_FALSE(alarm_test_active());
}

// El desbordamiento de millis() a los 24,86 dias ya rompio la maquina de
// alarmas una vez. Aqui la resta con signo tiene que aguantarlo.
void test_survives_the_millis_rollover(void) {
  const uint32_t nearWrap = 0xFFFFFF00u;
  alarm_test_start(nearWrap);
  TEST_ASSERT_EQUAL_UINT8(ALARM_PRIORITY_LOW, alarm_test_priority());
  // El reloj da la vuelta a mitad del primer tramo.
  alarm_test_tick(nearWrap + ALARM_TEST_PHASE_MS_LOW / 2u);
  TEST_ASSERT_EQUAL_UINT8(ALARM_PRIORITY_LOW, alarm_test_priority());
  // Y al cruzarlo del todo avanza como debe, sin quedarse colgada.
  alarm_test_tick(nearWrap + ALARM_TEST_PHASE_MS_LOW);
  TEST_ASSERT_TRUE(alarm_test_active());
  TEST_ASSERT_EQUAL_UINT8(ALARM_TEST_IDLE, alarm_test_priority());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_idle);
  RUN_TEST(test_sequence_climbs_low_medium_high);
  RUN_TEST(test_a_late_tick_does_not_overrun);
  RUN_TEST(test_start_is_refused_while_running);
  RUN_TEST(test_abort_stops_everything);
  RUN_TEST(test_abort_is_idempotent);
  RUN_TEST(test_survives_the_millis_rollover);
  return UNITY_END();
}
