#include <unity.h>
#include "modules/control/alarm_machine.h"

void setUp(void) { alarm_machine_init(); }
void tearDown(void) {}

void test_starts_inactive(void) {
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_INACTIVE,
                        alarm_machine_state(ALARM_FAN_FAILURE));
  TEST_ASSERT_EQUAL_UINT32(0u, alarm_machine_bitmask());
}

// Sin retardo de anuncio configurado, una condicion presente se anuncia ya.
void test_condition_becomes_active(void) {
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 1000);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_ACTIVE,
                        alarm_machine_state(ALARM_FAN_FAILURE));
  TEST_ASSERT_TRUE(alarm_machine_bitmask() & (1u << ALARM_FAN_FAILURE));
}

// Non-latching: al irse la condicion, la alarma se va sola.
void test_non_latching_clears_itself(void) {
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 1000);
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, false, 2000);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_INACTIVE,
                        alarm_machine_state(ALARM_HUMIDITY_DEVIATION));
  TEST_ASSERT_EQUAL_UINT32(0u, alarm_machine_bitmask());
}

// Repetir la misma condicion no debe reiniciar nada ni duplicar estado.
void test_repeated_condition_is_idempotent(void) {
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 1000);
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 1500);
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 2000);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_ACTIVE,
                        alarm_machine_state(ALARM_FAN_FAILURE));
}

// El corte de calefactor lo dicta la maquina, no el llamante.
void test_heater_cut_follows_policy(void) {
  TEST_ASSERT_FALSE(alarm_machine_heater_must_cut());
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 1000);
  TEST_ASSERT_FALSE(alarm_machine_heater_must_cut());
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 1000);
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());
}

// 201.12.3.101: una salida de aire obstruida debe cortar el calefactor.
void test_blocked_outlet_cuts_the_heater(void) {
  alarm_machine_condition(ALARM_AIR_OUTLET_BLOCKED, true, 1000);
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());
}

// 201.15.4.2.1 dd)/ee): por el lado frio el calefactor sigue encendido.
void test_cold_deviation_does_not_cut_the_heater(void) {
  alarm_machine_condition(ALARM_AIR_TEMP_DEVIATION_LOW, true, 1000);
  alarm_machine_condition(ALARM_SKIN_TEMP_DEVIATION_LOW, true, 1000);
  TEST_ASSERT_FALSE(alarm_machine_heater_must_cut());
}

// Con retardo configurado, la condicion se registra pero no se anuncia.
void test_delay_holds_in_pending(void) {
  alarm_machine_set_announce_delay(ALARM_HUMIDITY_DEVIATION, 1000);
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 0);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_PENDING,
                        alarm_machine_state(ALARM_HUMIDITY_DEVIATION));
  TEST_ASSERT_FALSE(alarm_machine_audio_required());
  // Pero la condicion ya cuenta como senalizada: el operador debe poder verla.
  TEST_ASSERT_TRUE(alarm_machine_bitmask() & (1u << ALARM_HUMIDITY_DEVIATION));
}

// Al expirar el retardo con la condicion viva, se anuncia. Que el retardo
// cancelase el aviso para siempre seria el fallo que tenia el codigo anterior.
void test_delay_expiry_announces(void) {
  alarm_machine_set_announce_delay(ALARM_HUMIDITY_DEVIATION, 1000);
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 0);
  alarm_machine_tick(999);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_PENDING,
                        alarm_machine_state(ALARM_HUMIDITY_DEVIATION));
  alarm_machine_tick(1000);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_ACTIVE,
                        alarm_machine_state(ALARM_HUMIDITY_DEVIATION));
  TEST_ASSERT_TRUE(alarm_machine_audio_required());
}

// Si la condicion se va durante el retardo, no llega a anunciarse nunca.
void test_condition_gone_during_delay_never_announces(void) {
  alarm_machine_set_announce_delay(ALARM_HUMIDITY_DEVIATION, 1000);
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 0);
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, false, 500);
  alarm_machine_tick(2000);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_INACTIVE,
                        alarm_machine_state(ALARM_HUMIDITY_DEVIATION));
}

// Un corte termico no admite retardo aunque se le configure uno: 201.15.4.2.1
// exige aviso en el instante en que se dispara.
void test_thermal_cutout_ignores_any_delay(void) {
  alarm_machine_set_announce_delay(ALARM_AIR_THERMAL_CUTOUT, 60000);
  alarm_machine_condition(ALARM_AIR_THERMAL_CUTOUT, true, 0);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_ACTIVE,
                        alarm_machine_state(ALARM_AIR_THERMAL_CUTOUT));
  TEST_ASSERT_TRUE(alarm_machine_audio_required());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_inactive);
  RUN_TEST(test_condition_becomes_active);
  RUN_TEST(test_non_latching_clears_itself);
  RUN_TEST(test_repeated_condition_is_idempotent);
  RUN_TEST(test_heater_cut_follows_policy);
  RUN_TEST(test_blocked_outlet_cuts_the_heater);
  RUN_TEST(test_cold_deviation_does_not_cut_the_heater);
  RUN_TEST(test_delay_holds_in_pending);
  RUN_TEST(test_delay_expiry_announces);
  RUN_TEST(test_condition_gone_during_delay_never_announces);
  RUN_TEST(test_thermal_cutout_ignores_any_delay);
  return UNITY_END();
}
