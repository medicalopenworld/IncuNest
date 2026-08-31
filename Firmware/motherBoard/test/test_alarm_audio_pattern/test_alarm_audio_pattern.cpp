#include <unity.h>
#include "alarm_audio_pattern.h"
#include "alarm_ids.h"

void setUp(void) {}
void tearDown(void) {}

// Tabla 3, MEDIA: 3 pulsos de 150 ms separados por y = 200 ms.
//   pulso 1 [0, 150)   pulso 2 [350, 500)   pulso 3 [700, 850)
// Es el patron que emiten LOS DOS transductores para ALARM_HMI_LINK_LOST: el
// de la placa y el del display. Si estos instantes cambian, cambian los dos.
void test_medium_burst_pulse_boundaries(void) {
  TEST_ASSERT_TRUE(alarm_audio_pulse_on(0, ALARM_PRIORITY_MEDIUM));
  TEST_ASSERT_TRUE(alarm_audio_pulse_on(149, ALARM_PRIORITY_MEDIUM));
  TEST_ASSERT_FALSE(alarm_audio_pulse_on(150, ALARM_PRIORITY_MEDIUM));

  TEST_ASSERT_FALSE(alarm_audio_pulse_on(349, ALARM_PRIORITY_MEDIUM));
  TEST_ASSERT_TRUE(alarm_audio_pulse_on(350, ALARM_PRIORITY_MEDIUM));
  TEST_ASSERT_TRUE(alarm_audio_pulse_on(499, ALARM_PRIORITY_MEDIUM));
  TEST_ASSERT_FALSE(alarm_audio_pulse_on(500, ALARM_PRIORITY_MEDIUM));

  TEST_ASSERT_FALSE(alarm_audio_pulse_on(699, ALARM_PRIORITY_MEDIUM));
  TEST_ASSERT_TRUE(alarm_audio_pulse_on(700, ALARM_PRIORITY_MEDIUM));
  TEST_ASSERT_TRUE(alarm_audio_pulse_on(849, ALARM_PRIORITY_MEDIUM));
  TEST_ASSERT_FALSE(alarm_audio_pulse_on(850, ALARM_PRIORITY_MEDIUM));
}

// La rafaga MEDIA dura 850 ms y el periodo es de 25 s: el resto es silencio.
// Ese 3,4 % de ciclo de trabajo es lo que ACOTA el enmascaramiento de una
// alarma ALTA de la placa cuando el display suena a la vez (design.md D2). Si
// alguien "afina" el patron y lo hace mas denso, se carga esa mitigacion: por
// eso el silencio se comprueba, y no solo los pulsos.
void test_medium_burst_is_silent_until_the_period_closes(void) {
  TEST_ASSERT_EQUAL_UINT32(850u, ALARM_BURST_LEN_MS_MEDIUM);
  for (uint32_t t = 850; t < 25000; t += 250) {
    TEST_ASSERT_FALSE(alarm_audio_pulse_on(t, ALARM_PRIORITY_MEDIUM));
  }
  TEST_ASSERT_FALSE(alarm_audio_pulse_on(24999, ALARM_PRIORITY_MEDIUM));
}

// Tabla 3, ALTA: 10 pulsos con espaciado x = 100 ms, salvo entre el 5o y el 6o,
// donde el hueco vale 2x + y = 400 ms. Ese hueco parte la rafaga en dos grupos
// de cinco y es lo que la hace reconocible como ALTA.
void test_high_burst_splits_into_two_groups_of_five(void) {
  // Primer grupo: pulsos en 0, 250, 500, 750, 1000 (250 = 150 + x).
  for (uint32_t i = 0; i < 5; ++i) {
    const uint32_t start = i * (ALARM_PULSE_MS + ALARM_PULSE_SPACING_X_MS);
    TEST_ASSERT_TRUE(alarm_audio_pulse_on(start, ALARM_PRIORITY_HIGH));
    TEST_ASSERT_TRUE(alarm_audio_pulse_on(start + 149, ALARM_PRIORITY_HIGH));
    TEST_ASSERT_FALSE(alarm_audio_pulse_on(start + 150, ALARM_PRIORITY_HIGH));
  }

  // El quinto pulso acaba en 1150. El hueco de 2x + y = 400 ms lleva el sexto
  // a 1550, no a 1250 como los demas.
  TEST_ASSERT_EQUAL_UINT32(400u, ALARM_GROUP_GAP_MS);
  TEST_ASSERT_FALSE(alarm_audio_pulse_on(1250, ALARM_PRIORITY_HIGH));
  TEST_ASSERT_FALSE(alarm_audio_pulse_on(1549, ALARM_PRIORITY_HIGH));
  TEST_ASSERT_TRUE(alarm_audio_pulse_on(1550, ALARM_PRIORITY_HIGH));

  // Segundo grupo: cinco pulsos mas con espaciado x desde 1550.
  for (uint32_t i = 0; i < 5; ++i) {
    const uint32_t start =
        1550u + i * (ALARM_PULSE_MS + ALARM_PULSE_SPACING_X_MS);
    TEST_ASSERT_TRUE(alarm_audio_pulse_on(start, ALARM_PRIORITY_HIGH));
    TEST_ASSERT_FALSE(alarm_audio_pulse_on(start + 150, ALARM_PRIORITY_HIGH));
  }

  // La rafaga entera son 2700 ms; a partir de ahi, silencio.
  TEST_ASSERT_EQUAL_UINT32(2700u, ALARM_BURST_LEN_MS_HIGH);
  TEST_ASSERT_FALSE(alarm_audio_pulse_on(2700, ALARM_PRIORITY_HIGH));
  TEST_ASSERT_FALSE(alarm_audio_pulse_on(5000, ALARM_PRIORITY_HIGH));
}

// BAJA: un solo pulso de 150 ms.
void test_low_burst_is_a_single_pulse(void) {
  TEST_ASSERT_TRUE(alarm_audio_pulse_on(0, ALARM_PRIORITY_LOW));
  TEST_ASSERT_TRUE(alarm_audio_pulse_on(149, ALARM_PRIORITY_LOW));
  TEST_ASSERT_FALSE(alarm_audio_pulse_on(150, ALARM_PRIORITY_LOW));
  TEST_ASSERT_FALSE(alarm_audio_pulse_on(20000, ALARM_PRIORITY_LOW));
}

// El periodo de cada prioridad es el de la Tabla 3, y el orden que la tabla
// pide se mantiene: el intervalo entre rafagas crece al bajar la prioridad.
void test_burst_periods_follow_table_3(void) {
  TEST_ASSERT_EQUAL_UINT32(10000u,
                           alarm_audio_burst_period_ms(ALARM_PRIORITY_HIGH));
  TEST_ASSERT_EQUAL_UINT32(25000u,
                           alarm_audio_burst_period_ms(ALARM_PRIORITY_MEDIUM));
  TEST_ASSERT_EQUAL_UINT32(30000u,
                           alarm_audio_burst_period_ms(ALARM_PRIORITY_LOW));

  const uint32_t gapHigh = 10000u - ALARM_BURST_LEN_MS_HIGH;
  const uint32_t gapMedium = 25000u - ALARM_BURST_LEN_MS_MEDIUM;
  const uint32_t gapLow = 30000u - ALARM_PULSE_MS;
  TEST_ASSERT_TRUE(gapMedium >= gapHigh);
  TEST_ASSERT_TRUE(gapLow >= gapMedium);
}

// Una prioridad que no se reconoce se trata como ALTA: sobreestimar la
// urgencia es la opcion seguridad-primero, igual que hace alarm_priority().
void test_unknown_priority_falls_back_to_high(void) {
  TEST_ASSERT_EQUAL_UINT32(10000u, alarm_audio_burst_period_ms(99));
  TEST_ASSERT_TRUE(alarm_audio_pulse_on(1550, 99));
}

// 6.10: el audio no cesa por haber sonado suficiente. La funcion es sin estado
// respecto al numero de rafagas emitidas, asi que el llamante que aplique el
// modulo del periodo obtiene el mismo patron a los 30 minutos que al principio.
void test_pattern_does_not_wear_out(void) {
  const uint32_t halfHour = 30u * 60u * 1000u;
  for (uint32_t burst = 0; burst * 25000u < halfHour; ++burst) {
    const uint32_t base = burst * 25000u;
    TEST_ASSERT_TRUE(
        alarm_audio_pulse_on(base % 25000u, ALARM_PRIORITY_MEDIUM));
    TEST_ASSERT_TRUE(
        alarm_audio_pulse_on((base + 700u) % 25000u, ALARM_PRIORITY_MEDIUM));
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_medium_burst_pulse_boundaries);
  RUN_TEST(test_medium_burst_is_silent_until_the_period_closes);
  RUN_TEST(test_high_burst_splits_into_two_groups_of_five);
  RUN_TEST(test_low_burst_is_a_single_pulse);
  RUN_TEST(test_burst_periods_follow_table_3);
  RUN_TEST(test_unknown_priority_falls_back_to_high);
  RUN_TEST(test_pattern_does_not_wear_out);
  return UNITY_END();
}
