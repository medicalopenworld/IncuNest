#include <unity.h>

#include "modules/trend/trend_events.h"

void setUp(void) {}
void tearDown(void) {}

void test_empty_ring_has_nothing(void) {
  TrendEvents e;
  trend_events_init(&e);
  TrendEvent out;
  TEST_ASSERT_EQUAL_INT(0, e.count);
  TEST_ASSERT_FALSE(trend_events_get(&e, 0, &out));
  TrendEvent buf[4];
  TEST_ASSERT_EQUAL_INT(0, trend_events_in_window(&e, 0, 1000000, buf, 4));
}

void test_raise_then_clear_come_back_in_order_with_their_fields(void) {
  TrendEvents e;
  trend_events_init(&e);
  trend_events_add(&e, 100, 5, 2, true);
  trend_events_add(&e, 160, 5, 2, false);

  TrendEvent buf[8];
  const int n = trend_events_in_window(&e, 0, 1000, buf, 8);
  TEST_ASSERT_EQUAL_INT(2, n);
  TEST_ASSERT_EQUAL_INT64(100, buf[0].slot);
  TEST_ASSERT_EQUAL_UINT8(5, buf[0].id);
  TEST_ASSERT_EQUAL_UINT8(2, buf[0].priority);
  TEST_ASSERT_TRUE(buf[0].raised);
  TEST_ASSERT_EQUAL_INT64(160, buf[1].slot);
  TEST_ASSERT_FALSE(buf[1].raised);
}

void test_window_bounds_are_inclusive(void) {
  TrendEvents e;
  trend_events_init(&e);
  trend_events_add(&e, 10, 1, 0, true);
  trend_events_add(&e, 20, 2, 1, true);
  trend_events_add(&e, 30, 3, 2, true);
  TrendEvent buf[8];
  TEST_ASSERT_EQUAL_INT(3, trend_events_in_window(&e, 10, 30, buf, 8));
  TEST_ASSERT_EQUAL_INT(1, trend_events_in_window(&e, 11, 29, buf, 8));
  TEST_ASSERT_EQUAL_UINT8(2, buf[0].id);
  TEST_ASSERT_EQUAL_INT(0, trend_events_in_window(&e, 31, 100, buf, 8));
  TEST_ASSERT_EQUAL_INT(0, trend_events_in_window(&e, 0, 9, buf, 8));
}

void test_window_output_is_capped_by_max(void) {
  TrendEvents e;
  trend_events_init(&e);
  for (int i = 0; i < 10; i++) trend_events_add(&e, i, (uint8_t)i, 0, true);
  TrendEvent buf[3];
  TEST_ASSERT_EQUAL_INT(3, trend_events_in_window(&e, 0, 100, buf, 3));
  TEST_ASSERT_EQUAL_UINT8(0, buf[0].id);
  TEST_ASSERT_EQUAL_UINT8(2, buf[2].id);
  TEST_ASSERT_EQUAL_INT(0, trend_events_in_window(&e, 0, 100, buf, 0));
}

void test_ring_keeps_the_64_most_recent_in_chronological_order(void) {
  TrendEvents e;
  trend_events_init(&e);
  const int total = TREND_MAX_EVENTS + 6;
  for (int i = 0; i < total; i++)
    trend_events_add(&e, 1000 + i, (uint8_t)(i % 200), 1, (i & 1) == 0);
  TEST_ASSERT_EQUAL_INT(TREND_MAX_EVENTS, e.count);
  TrendEvent out;
  TEST_ASSERT_TRUE(trend_events_get(&e, 0, &out));
  TEST_ASSERT_EQUAL_INT64(1006, out.slot);  // los 6 primeros se descartaron
  TEST_ASSERT_TRUE(trend_events_get(&e, TREND_MAX_EVENTS - 1, &out));
  TEST_ASSERT_EQUAL_INT64(1000 + total - 1, out.slot);
  TEST_ASSERT_FALSE(trend_events_get(&e, TREND_MAX_EVENTS, &out));
  TEST_ASSERT_FALSE(trend_events_get(&e, -1, &out));

  static TrendEvent buf[TREND_MAX_EVENTS + 8];
  const int n = trend_events_in_window(&e, 0, 1000000, buf, TREND_MAX_EVENTS + 8);
  TEST_ASSERT_EQUAL_INT(TREND_MAX_EVENTS, n);
  for (int i = 1; i < n; i++) TEST_ASSERT_TRUE(buf[i].slot > buf[i - 1].slot);
}

void test_shift_relabels_every_event(void) {
  TrendEvents e;
  trend_events_init(&e);
  trend_events_add(&e, 5, 1, 0, true);
  trend_events_add(&e, 9, 1, 0, false);
  trend_events_shift(&e, 1000);
  TrendEvent buf[4];
  TEST_ASSERT_EQUAL_INT(0, trend_events_in_window(&e, 0, 999, buf, 4));
  TEST_ASSERT_EQUAL_INT(2, trend_events_in_window(&e, 1005, 1009, buf, 4));
  trend_events_shift(&e, -1000);
  TEST_ASSERT_EQUAL_INT(2, trend_events_in_window(&e, 5, 9, buf, 4));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_empty_ring_has_nothing);
  RUN_TEST(test_raise_then_clear_come_back_in_order_with_their_fields);
  RUN_TEST(test_window_bounds_are_inclusive);
  RUN_TEST(test_window_output_is_capped_by_max);
  RUN_TEST(test_ring_keeps_the_64_most_recent_in_chronological_order);
  RUN_TEST(test_shift_relabels_every_event);
  return UNITY_END();
}
