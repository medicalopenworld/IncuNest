#include <unity.h>

#include "modules/trend/trend_policy.h"

void setUp(void) {}
void tearDown(void) {}

// ActuationMode: 0 apagado, 1 temperatura, 2 humedad, 3 ambas.

void test_off_validates_no_setpoint(void) {
  for (int mode = 0; mode < 2; mode++) {
    const TrendSetpointValidity v = trend_setpoint_validity(0, mode != 0);
    TEST_ASSERT_FALSE(v.air);
    TEST_ASSERT_FALSE(v.skin);
    TEST_ASSERT_FALSE(v.hum);
  }
}

void test_temperature_in_air_mode_validates_only_air(void) {
  const TrendSetpointValidity v = trend_setpoint_validity(1, true);
  TEST_ASSERT_TRUE(v.air);
  TEST_ASSERT_FALSE(v.skin);
  TEST_ASSERT_FALSE(v.hum);
}

void test_temperature_in_skin_mode_validates_only_skin(void) {
  const TrendSetpointValidity v = trend_setpoint_validity(1, false);
  TEST_ASSERT_FALSE(v.air);
  TEST_ASSERT_TRUE(v.skin);
  TEST_ASSERT_FALSE(v.hum);
}

void test_humidity_only_validates_only_humidity_whatever_the_mode(void) {
  for (int mode = 0; mode < 2; mode++) {
    const TrendSetpointValidity v = trend_setpoint_validity(2, mode != 0);
    TEST_ASSERT_FALSE(v.air);
    TEST_ASSERT_FALSE(v.skin);
    TEST_ASSERT_TRUE(v.hum);
  }
}

void test_temperature_and_humidity_in_skin_mode(void) {
  const TrendSetpointValidity v = trend_setpoint_validity(3, false);
  TEST_ASSERT_FALSE(v.air);
  TEST_ASSERT_TRUE(v.skin);
  TEST_ASSERT_TRUE(v.hum);
}

void test_temperature_and_humidity_in_air_mode(void) {
  const TrendSetpointValidity v = trend_setpoint_validity(3, true);
  TEST_ASSERT_TRUE(v.air);
  TEST_ASSERT_FALSE(v.skin);
  TEST_ASSERT_TRUE(v.hum);
}

// Un valor que la placa no sabe producir (linea corrupta, version futura) no
// puede encender una linea de consigna.
void test_out_of_range_actuation_validates_nothing(void) {
  const int bad[] = {-1, 4, 99, 0x7FFFFFFF};
  for (int a : bad) {
    const TrendSetpointValidity v = trend_setpoint_validity(a, true);
    TEST_ASSERT_FALSE(v.air);
    TEST_ASSERT_FALSE(v.skin);
    TEST_ASSERT_FALSE(v.hum);
  }
}

void test_slot_to_px_maps_window_edges(void) {
  // 1440 ranuras sobre 672 px.
  TEST_ASSERT_EQUAL_INT(0, trend_slot_to_px(1000, 1000, 1440, 672));
  TEST_ASSERT_EQUAL_INT(336, trend_slot_to_px(1720, 1000, 1440, 672));
  TEST_ASSERT_EQUAL_INT(671, trend_slot_to_px(2439, 1000, 1440, 672));
}

void test_slot_to_px_rejects_outside_the_window(void) {
  TEST_ASSERT_EQUAL_INT(-1, trend_slot_to_px(999, 1000, 1440, 672));
  TEST_ASSERT_EQUAL_INT(-1, trend_slot_to_px(2440, 1000, 1440, 672));
  TEST_ASSERT_EQUAL_INT(-1, trend_slot_to_px(0, 1000, 1440, 672));
}

void test_slot_to_px_rejects_invalid_geometry(void) {
  TEST_ASSERT_EQUAL_INT(-1, trend_slot_to_px(5, 0, 0, 672));
  TEST_ASSERT_EQUAL_INT(-1, trend_slot_to_px(5, 0, 100, 0));
  TEST_ASSERT_EQUAL_INT(-1, trend_slot_to_px(5, 0, -3, 672));
}

void test_slot_to_px_works_with_epoch_sized_slots(void) {
  const int64_t start = 177000000;  // ~ ranuras de epoch/10 de 2026
  TEST_ASSERT_EQUAL_INT(0, trend_slot_to_px(start, start, 8640, 672));
  TEST_ASSERT_EQUAL_INT(671, trend_slot_to_px(start + 8639, start, 8640, 672));
}

void test_mark_kind_follows_priority_when_raised(void) {
  TEST_ASSERT_EQUAL_INT(TREND_MARK_LOW, trend_mark_kind(0, true));
  TEST_ASSERT_EQUAL_INT(TREND_MARK_MEDIUM, trend_mark_kind(1, true));
  TEST_ASSERT_EQUAL_INT(TREND_MARK_HIGH, trend_mark_kind(2, true));
}

void test_mark_kind_cleared_ignores_priority(void) {
  for (uint8_t p = 0; p < 3; p++)
    TEST_ASSERT_EQUAL_INT(TREND_MARK_CLEARED, trend_mark_kind(p, false));
}

// Prioridad desconocida: la marca mas severa, nunca una que parezca leve.
void test_mark_kind_unknown_priority_is_treated_as_high(void) {
  TEST_ASSERT_EQUAL_INT(TREND_MARK_HIGH, trend_mark_kind(3, true));
  TEST_ASSERT_EQUAL_INT(TREND_MARK_HIGH, trend_mark_kind(255, true));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_off_validates_no_setpoint);
  RUN_TEST(test_temperature_in_air_mode_validates_only_air);
  RUN_TEST(test_temperature_in_skin_mode_validates_only_skin);
  RUN_TEST(test_humidity_only_validates_only_humidity_whatever_the_mode);
  RUN_TEST(test_temperature_and_humidity_in_skin_mode);
  RUN_TEST(test_temperature_and_humidity_in_air_mode);
  RUN_TEST(test_out_of_range_actuation_validates_nothing);
  RUN_TEST(test_slot_to_px_maps_window_edges);
  RUN_TEST(test_slot_to_px_rejects_outside_the_window);
  RUN_TEST(test_slot_to_px_rejects_invalid_geometry);
  RUN_TEST(test_slot_to_px_works_with_epoch_sized_slots);
  RUN_TEST(test_mark_kind_follows_priority_when_raised);
  RUN_TEST(test_mark_kind_cleared_ignores_priority);
  RUN_TEST(test_mark_kind_unknown_priority_is_treated_as_high);
  return UNITY_END();
}
