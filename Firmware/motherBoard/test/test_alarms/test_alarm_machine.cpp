#include <unity.h>
#include "modules/control/alarm_machine.h"

void setUp(void) { alarm_machine_init(); }
void tearDown(void) {}

void test_alarm_set_and_get(void) {
  alarm_machine_set(TEMPERATURE_ALARM, true);
  TEST_ASSERT_TRUE(alarm_machine_get(TEMPERATURE_ALARM));
}

void test_alarm_clear(void) {
  alarm_machine_set(TEMPERATURE_ALARM, true);
  alarm_machine_set(TEMPERATURE_ALARM, false);
  TEST_ASSERT_FALSE(alarm_machine_get(TEMPERATURE_ALARM));
}

void test_bitmask_consistency(void) {
  alarm_machine_set(HUMIDITY_ALARM, true);
  alarm_machine_set(FAN_ISSUE_ALARM, true);
  uint32_t mask = alarm_machine_bitmask();
  TEST_ASSERT_TRUE(mask & (1u << (int)HUMIDITY_ALARM));
  TEST_ASSERT_TRUE(mask & (1u << (int)FAN_ISSUE_ALARM));
  TEST_ASSERT_FALSE(mask & (1u << (int)TEMPERATURE_ALARM));
}

void test_critical_alarm_detected(void) {
  alarm_machine_set(FAN_ISSUE_ALARM, true);
  TEST_ASSERT_TRUE(alarm_machine_any_critical());
}

void test_non_critical_alarm_not_critical(void) {
  alarm_machine_set(HUMIDITY_ALARM, true);
  TEST_ASSERT_FALSE(alarm_machine_any_critical());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_alarm_set_and_get);
  RUN_TEST(test_alarm_clear);
  RUN_TEST(test_bitmask_consistency);
  RUN_TEST(test_critical_alarm_detected);
  RUN_TEST(test_non_critical_alarm_not_critical);
  return UNITY_END();
}
