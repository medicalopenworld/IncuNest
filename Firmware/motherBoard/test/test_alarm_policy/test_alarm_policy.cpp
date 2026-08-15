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

// El reparto declarado en la spec: 7 ALTA / 7 MEDIA / 2 BAJA.
void test_priority_distribution(void) {
  int counts[3] = {0, 0, 0};
  for (int id = ALARM_NONE + 1; id < ALARM_COUNT; ++id) {
    counts[alarm_priority((AlarmId)id)]++;
  }
  TEST_ASSERT_EQUAL_INT(2, counts[ALARM_PRIORITY_LOW]);
  TEST_ASSERT_EQUAL_INT(7, counts[ALARM_PRIORITY_MEDIUM]);
  TEST_ASSERT_EQUAL_INT(7, counts[ALARM_PRIORITY_HIGH]);
}

// Un id fuera de rango no debe devolver basura: se degrada a la mas urgente,
// porque equivocarse hacia arriba es seguro y hacia abajo no.
void test_out_of_range_defaults_to_high(void) {
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_HIGH, alarm_priority(ALARM_COUNT));
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_HIGH, alarm_priority((AlarmId)999));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_high_priority_set);
  RUN_TEST(test_medium_priority_set);
  RUN_TEST(test_low_priority_set);
  RUN_TEST(test_priority_distribution);
  RUN_TEST(test_out_of_range_defaults_to_high);
  return UNITY_END();
}
