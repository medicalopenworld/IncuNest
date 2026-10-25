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

// ---- prepend: eventos de un fichero antes de los vivos ----

namespace {
TrendEvent ev(int64_t slot, uint8_t id) { return TrendEvent{slot, id, 1, true}; }
}  // namespace

void test_prepend_onto_an_empty_ring_keeps_order(void) {
  TrendEvents e;
  trend_events_init(&e);
  const TrendEvent older[] = {ev(10, 1), ev(20, 2), ev(30, 3)};
  TEST_ASSERT_EQUAL_INT(3, trend_events_prepend(&e, older, 3, 0));
  TEST_ASSERT_EQUAL_INT(3, e.count);
  TrendEvent out;
  for (int i = 0; i < 3; i++) {
    TEST_ASSERT_TRUE(trend_events_get(&e, i, &out));
    TEST_ASSERT_EQUAL_INT64(older[i].slot, out.slot);
    TEST_ASSERT_EQUAL_UINT8(older[i].id, out.id);
  }
}

void test_prepend_goes_before_the_existing_events(void) {
  TrendEvents e;
  trend_events_init(&e);
  trend_events_add(&e, 100, 9, 2, true);
  const TrendEvent older[] = {ev(10, 1), ev(20, 2)};
  TEST_ASSERT_EQUAL_INT(2, trend_events_prepend(&e, older, 2, 0));
  TrendEvent out;
  TEST_ASSERT_EQUAL_INT(3, e.count);
  TEST_ASSERT_TRUE(trend_events_get(&e, 0, &out));
  TEST_ASSERT_EQUAL_INT64(10, out.slot);
  TEST_ASSERT_TRUE(trend_events_get(&e, 2, &out));
  TEST_ASSERT_EQUAL_INT64(100, out.slot);
  TEST_ASSERT_EQUAL_UINT8(9, out.id);
}

// El mismo flanco ya vivo no se duplica: solo entran los estrictamente
// anteriores al primer evento guardado.
void test_prepend_skips_events_not_older_than_the_first_existing(void) {
  TrendEvents e;
  trend_events_init(&e);
  trend_events_add(&e, 50, 9, 2, true);
  const TrendEvent older[] = {ev(40, 1), ev(45, 2), ev(50, 3), ev(55, 4)};
  TEST_ASSERT_EQUAL_INT(2, trend_events_prepend(&e, older, 4, 0));
  TEST_ASSERT_EQUAL_INT(3, e.count);
}

void test_prepend_skips_events_before_minSlot(void) {
  TrendEvents e;
  trend_events_init(&e);
  const TrendEvent older[] = {ev(5, 1), ev(15, 2), ev(25, 3)};
  TEST_ASSERT_EQUAL_INT(2, trend_events_prepend(&e, older, 3, 15));
  TrendEvent out;
  TEST_ASSERT_TRUE(trend_events_get(&e, 0, &out));
  TEST_ASSERT_EQUAL_INT64(15, out.slot);
}

// Los vivos nunca se pierden: si no cabe todo, se descartan los mas antiguos
// de los anteriores.
void test_prepend_never_evicts_existing_events(void) {
  TrendEvents e;
  trend_events_init(&e);
  for (int i = 0; i < TREND_MAX_EVENTS - 2; i++)
    trend_events_add(&e, 1000 + i, (uint8_t)i, 1, true);
  const TrendEvent older[] = {ev(1, 101), ev(2, 102), ev(3, 103), ev(4, 104),
                              ev(5, 105)};
  TEST_ASSERT_EQUAL_INT(2, trend_events_prepend(&e, older, 5, 0));
  TEST_ASSERT_EQUAL_INT(TREND_MAX_EVENTS, e.count);
  TrendEvent out;
  TEST_ASSERT_TRUE(trend_events_get(&e, 0, &out));
  TEST_ASSERT_EQUAL_UINT8(104, out.id);  // los 2 mas recientes de los anteriores
  TEST_ASSERT_TRUE(trend_events_get(&e, 2, &out));
  TEST_ASSERT_EQUAL_INT64(1000, out.slot);
  TEST_ASSERT_TRUE(trend_events_get(&e, TREND_MAX_EVENTS - 1, &out));
  TEST_ASSERT_EQUAL_INT64(1000 + TREND_MAX_EVENTS - 3, out.slot);
}

void test_prepend_handles_a_wrapped_ring(void) {
  TrendEvents e;
  trend_events_init(&e);
  for (int i = 0; i < TREND_MAX_EVENTS + 10; i++)  // el anillo ya dio la vuelta
    trend_events_add(&e, 1000 + i, (uint8_t)i, 1, true);
  // Lleno: no cabe ninguno.
  const TrendEvent older[] = {ev(1, 1)};
  TEST_ASSERT_EQUAL_INT(0, trend_events_prepend(&e, older, 1, 0));
  TEST_ASSERT_EQUAL_INT(TREND_MAX_EVENTS, e.count);
  TrendEvent out;
  TEST_ASSERT_TRUE(trend_events_get(&e, 0, &out));
  TEST_ASSERT_EQUAL_INT64(1010, out.slot);
}

void test_prepend_rejects_bad_arguments(void) {
  TrendEvents e;
  trend_events_init(&e);
  const TrendEvent older[] = {ev(1, 1)};
  TEST_ASSERT_EQUAL_INT(0, trend_events_prepend(nullptr, older, 1, 0));
  TEST_ASSERT_EQUAL_INT(0, trend_events_prepend(&e, nullptr, 1, 0));
  TEST_ASSERT_EQUAL_INT(0, trend_events_prepend(&e, older, 0, 0));
  TEST_ASSERT_EQUAL_INT(0, trend_events_prepend(&e, older, -1, 0));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_prepend_onto_an_empty_ring_keeps_order);
  RUN_TEST(test_prepend_goes_before_the_existing_events);
  RUN_TEST(test_prepend_skips_events_not_older_than_the_first_existing);
  RUN_TEST(test_prepend_skips_events_before_minSlot);
  RUN_TEST(test_prepend_never_evicts_existing_events);
  RUN_TEST(test_prepend_handles_a_wrapped_ring);
  RUN_TEST(test_prepend_rejects_bad_arguments);
  RUN_TEST(test_empty_ring_has_nothing);
  RUN_TEST(test_raise_then_clear_come_back_in_order_with_their_fields);
  RUN_TEST(test_window_bounds_are_inclusive);
  RUN_TEST(test_window_output_is_capped_by_max);
  RUN_TEST(test_ring_keeps_the_64_most_recent_in_chronological_order);
  RUN_TEST(test_shift_relabels_every_event);
  return UNITY_END();
}
