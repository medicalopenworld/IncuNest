#include <unity.h>
#include "alarm_ids.h"
#include "alarm_policy.h"

void setUp(void) {}
void tearDown(void) {}

// IEC 60601-1-8 Tabla 1: muerte o lesion irreversible con onset "prompt" -> ALTA.
void test_high_priority_set(void) {
  const AlarmId high[] = {
      ALARM_AIR_THERMAL_CUTOUT,  ALARM_SKIN_THERMAL_CUTOUT,
      ALARM_AIR_SENSOR_FAULT,    ALARM_SKIN_SENSOR_FAULT_SKIN_MODE,
      ALARM_FAN_FAILURE,         ALARM_AIR_OUTLET_BLOCKED,
      ALARM_MAINS_INTERRUPTION};
  for (unsigned i = 0; i < sizeof(high) / sizeof(high[0]); ++i) {
    TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_HIGH, alarm_priority(high[i]));
  }
}

// Lesion reversible con onset "prompt" -> MEDIA.
void test_medium_priority_set(void) {
  const AlarmId medium[] = {
      ALARM_AIR_TEMP_DEVIATION_HIGH,  ALARM_AIR_TEMP_DEVIATION_LOW,
      ALARM_SKIN_TEMP_DEVIATION_HIGH, ALARM_SKIN_TEMP_DEVIATION_LOW,
      ALARM_HEATER_FAULT,             ALARM_SUPPLY_UNDERVOLTAGE,
      ALARM_HMI_LINK_LOST};
  for (unsigned i = 0; i < sizeof(medium) / sizeof(medium[0]); ++i) {
    TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_MEDIUM, alarm_priority(medium[i]));
  }
}

// Lesion menor o molestia con onset "delayed" -> BAJA.
void test_low_priority_set(void) {
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_LOW,
                        alarm_priority(ALARM_SKIN_SENSOR_FAULT_AIR_MODE));
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_LOW,
                        alarm_priority(ALARM_HUMIDITY_DEVIATION));
}

// El reparto declarado en la spec: 7 ALTA / 8 MEDIA / 2 BAJA.
//
// La MEDIA subio de 7 a 8 al separar ALARM_HEATER_SENSOR_FAULT de
// ALARM_HEATER_FAULT: son averias distintas (calefactor frente a su sensor de
// corriente) con acciones distintas para el operador, pero la misma urgencia.
void test_priority_distribution(void) {
  int counts[3] = {0, 0, 0};
  for (int id = ALARM_NONE + 1; id < ALARM_COUNT; ++id) {
    counts[alarm_priority((AlarmId)id)]++;
  }
  TEST_ASSERT_EQUAL_INT(2, counts[ALARM_PRIORITY_LOW]);
  TEST_ASSERT_EQUAL_INT(8, counts[ALARM_PRIORITY_MEDIUM]);
  TEST_ASSERT_EQUAL_INT(7, counts[ALARM_PRIORITY_HIGH]);
}

// Las dos averias de calefactor comparten urgencia y comparten corte, pero no
// identidad: son alarmas separadas para que el operador sepa que revisar.
void test_heater_and_its_sensor_are_separate_conditions(void) {
  TEST_ASSERT_NOT_EQUAL(ALARM_HEATER_FAULT, ALARM_HEATER_SENSOR_FAULT);
  TEST_ASSERT_EQUAL_INT(alarm_priority(ALARM_HEATER_FAULT),
                        alarm_priority(ALARM_HEATER_SENSOR_FAULT));
  // Sin medida de consumo no se deja calentar sin vigilancia: separar las
  // alarmas no puede relajar el corte que ya habia.
  TEST_ASSERT_TRUE(alarm_cuts_heater(ALARM_HEATER_SENSOR_FAULT));
  // Ninguna de las dos es latching: solo lo son los cortes termicos.
  TEST_ASSERT_FALSE(alarm_is_latching(ALARM_HEATER_SENSOR_FAULT));
}

// Un id fuera de rango no debe devolver basura: se degrada a la mas urgente,
// porque equivocarse hacia arriba es seguro y hacia abajo no.
void test_out_of_range_defaults_to_high(void) {
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_HIGH, alarm_priority(ALARM_COUNT));
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_HIGH, alarm_priority((AlarmId)999));
}

// 201.15.4.2.1 aa) y bb): el corte termico auto-rearmable exige que "la alarma
// opere continuamente hasta reset manual". Es la unica familia latching.
void test_only_thermal_cutouts_latch(void) {
  TEST_ASSERT_TRUE(alarm_is_latching(ALARM_AIR_THERMAL_CUTOUT));
  TEST_ASSERT_TRUE(alarm_is_latching(ALARM_SKIN_THERMAL_CUTOUT));
  for (int id = ALARM_NONE + 1; id < ALARM_COUNT; ++id) {
    if (id == ALARM_AIR_THERMAL_CUTOUT || id == ALARM_SKIN_THERMAL_CUTOUT) {
      continue;
    }
    TEST_ASSERT_FALSE(alarm_is_latching((AlarmId)id));
  }
}

// 201.12.3.101 (ventilador y salida obstruida), 201.12.3.102 (sonda de piel),
// 201.15.4.2.1 dd)/ee) (desviacion por el lado caliente).
void test_conditions_that_must_cut_the_heater(void) {
  const AlarmId cuts[] = {
      ALARM_AIR_THERMAL_CUTOUT,       ALARM_SKIN_THERMAL_CUTOUT,
      ALARM_AIR_SENSOR_FAULT,         ALARM_SKIN_SENSOR_FAULT_SKIN_MODE,
      ALARM_FAN_FAILURE,              ALARM_AIR_OUTLET_BLOCKED,
      ALARM_AIR_TEMP_DEVIATION_HIGH,  ALARM_SKIN_TEMP_DEVIATION_HIGH,
      ALARM_HEATER_FAULT};
  for (unsigned i = 0; i < sizeof(cuts) / sizeof(cuts[0]); ++i) {
    TEST_ASSERT_TRUE(alarm_cuts_heater(cuts[i]));
  }
}

// dd) y ee) son explicitas: por el lado frio el calefactor DEBE seguir
// encendido. Cortarlo ahi enfriaria a un bebe que ya esta por debajo.
void test_cold_side_deviation_never_cuts_the_heater(void) {
  TEST_ASSERT_FALSE(alarm_cuts_heater(ALARM_AIR_TEMP_DEVIATION_LOW));
  TEST_ASSERT_FALSE(alarm_cuts_heater(ALARM_SKIN_TEMP_DEVIATION_LOW));
}

// La unica condicion de prioridad ALTA que NO corta el calefactor, y por eso
// merece su propio assert: no hay condicion fisica que cortar. 201.12.3.103
// pide avisar de la interrupcion de red; cortar el calefactor al recuperarla
// solo enfriaria al bebe. Todas las demas ALTA si cortan, asi que sin fijar
// esto un "cortan todas las ALTA" pasaria desapercibido.
void test_mains_interruption_does_not_cut_the_heater(void) {
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_HIGH,
                        alarm_priority(ALARM_MAINS_INTERRUPTION));
  TEST_ASSERT_FALSE(alarm_cuts_heater(ALARM_MAINS_INTERRUPTION));
  // Y es la unica ALTA en esa situacion.
  for (int id = ALARM_NONE + 1; id < ALARM_COUNT; ++id) {
    if (alarm_priority((AlarmId)id) != ALARM_PRIORITY_HIGH ||
        id == ALARM_MAINS_INTERRUPTION) {
      continue;
    }
    TEST_ASSERT_TRUE(alarm_cuts_heater((AlarmId)id));
  }
}

void test_notify_only_conditions_do_not_cut_the_heater(void) {
  TEST_ASSERT_FALSE(alarm_cuts_heater(ALARM_SUPPLY_UNDERVOLTAGE));
  TEST_ASSERT_FALSE(alarm_cuts_heater(ALARM_HMI_LINK_LOST));
  TEST_ASSERT_FALSE(alarm_cuts_heater(ALARM_HUMIDITY_DEVIATION));
  TEST_ASSERT_FALSE(alarm_cuts_heater(ALARM_SKIN_SENSOR_FAULT_AIR_MODE));
}

// 201.15.4.2.1 aa): el corte por aire no puede exceder 38 C.
void test_air_cutout_is_capped_at_38(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 38.0f, alarm_clamp_air_cutout(45.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 38.0f, alarm_clamp_air_cutout(38.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 36.5f, alarm_clamp_air_cutout(36.5f));
}

// 201.15.4.2.1 bb): el corte de piel no puede exceder 40 C.
void test_skin_cutout_is_capped_at_40(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 40.0f, alarm_clamp_skin_cutout(50.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 37.5f, alarm_clamp_skin_cutout(37.5f));
}

// Un valor absurdo por abajo dejaria el equipo alarmando siempre; se acota a
// un minimo clinicamente util.
void test_cutouts_have_a_floor(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 34.0f, alarm_clamp_air_cutout(0.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 34.0f, alarm_clamp_skin_cutout(-5.0f));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_high_priority_set);
  RUN_TEST(test_medium_priority_set);
  RUN_TEST(test_low_priority_set);
  RUN_TEST(test_priority_distribution);
  RUN_TEST(test_heater_and_its_sensor_are_separate_conditions);
  RUN_TEST(test_out_of_range_defaults_to_high);
  RUN_TEST(test_only_thermal_cutouts_latch);
  RUN_TEST(test_conditions_that_must_cut_the_heater);
  RUN_TEST(test_cold_side_deviation_never_cuts_the_heater);
  RUN_TEST(test_mains_interruption_does_not_cut_the_heater);
  RUN_TEST(test_notify_only_conditions_do_not_cut_the_heater);
  RUN_TEST(test_air_cutout_is_capped_at_38);
  RUN_TEST(test_skin_cutout_is_capped_at_40);
  RUN_TEST(test_cutouts_have_a_floor);
  return UNITY_END();
}
