#include <unity.h>

#include <vector>

#include "modules/trend/trend_clock.h"
#include "modules/trend/trend_model.h"

void setUp(void) {}
void tearDown(void) {}

namespace {

constexpr int64_t SLOT_MS = TREND_SLOT_MS;

TrendSample tagged(int16_t base) {
  TrendSample s;
  for (int i = 0; i < TREND_SERIES; i++) s.v[i] = (int16_t)(base + i);
  return s;
}

}  // namespace

void test_without_time_the_slot_is_the_monotonic_one(void) {
  TrendClock c;
  trend_clock_init(&c);
  TEST_ASSERT_FALSE(c.synced);
  TEST_ASSERT_EQUAL_INT64(0, trend_clock_slot(&c, 0));
  TEST_ASSERT_EQUAL_INT64(0, trend_clock_slot(&c, 9999));
  TEST_ASSERT_EQUAL_INT64(1, trend_clock_slot(&c, 10000));
  TEST_ASSERT_EQUAL_INT64(360, trend_clock_slot(&c, 3600000));
}

void test_epoch_zero_means_no_time_and_changes_nothing(void) {
  TrendClock c;
  trend_clock_init(&c);
  TEST_ASSERT_EQUAL_INT64(0, trend_clock_update(&c, 65000, 0));
  TEST_ASSERT_FALSE(c.synced);
  TEST_ASSERT_EQUAL_INT64(0, c.offsetSlots);
}

void test_first_time_sets_the_offset_and_returns_the_full_shift(void) {
  TrendClock c;
  trend_clock_init(&c);
  // 65 s de arranque = ranura monotona 6; epoch 1_700_000_005 / 10 = 170000000.
  const int64_t shift = trend_clock_update(&c, 65000, 1700000005u);
  TEST_ASSERT_TRUE(c.synced);
  TEST_ASSERT_EQUAL_INT64(170000000 - 6, shift);
  TEST_ASSERT_EQUAL_INT64(shift, c.offsetSlots);
  // La ranura "ahora" ya ES epoch/10, y avanza con el reloj monotono.
  TEST_ASSERT_EQUAL_INT64(170000000, trend_clock_slot(&c, 65000));
  TEST_ASSERT_EQUAL_INT64(170000001, trend_clock_slot(&c, 75000));
}

void test_rounding_of_one_slot_or_less_does_not_move_the_axis(void) {
  TrendClock c;
  trend_clock_init(&c);
  trend_clock_update(&c, 65000, 1700000005u);
  const int64_t offset = c.offsetSlots;
  // Mismo instante, hora leida con +/- 1 ranura de diferencia.
  TEST_ASSERT_EQUAL_INT64(0, trend_clock_update(&c, 65000, 1700000015u));
  TEST_ASSERT_EQUAL_INT64(0, trend_clock_update(&c, 65000, 1699999995u));
  TEST_ASSERT_EQUAL_INT64(offset, c.offsetSlots);
}

void test_a_correction_of_more_than_one_slot_returns_the_difference(void) {
  TrendClock c;
  trend_clock_init(&c);
  trend_clock_update(&c, 65000, 1700000005u);
  const int64_t offset = c.offsetSlots;
  // NTP corrige la hora 25 s hacia delante (2,5 ranuras -> 3 con el redondeo).
  const int64_t fwd = trend_clock_update(&c, 65000, 1700000030u);
  TEST_ASSERT_EQUAL_INT64(3, fwd);
  TEST_ASSERT_EQUAL_INT64(offset + 3, c.offsetSlots);
  // Y despues un salto atras de una hora.
  const int64_t back = trend_clock_update(&c, 65000, 1700000030u - 3600u);
  TEST_ASSERT_EQUAL_INT64(-360, back);
}

void test_time_disappearing_after_sync_keeps_the_offset(void) {
  TrendClock c;
  trend_clock_init(&c);
  trend_clock_update(&c, 65000, 1700000005u);
  const int64_t offset = c.offsetSlots;
  TEST_ASSERT_EQUAL_INT64(0, trend_clock_update(&c, 70000, 0));
  TEST_ASSERT_TRUE(c.synced);
  TEST_ASSERT_EQUAL_INT64(offset, c.offsetSlots);
}

// ---- Modelo: la transicion esp_timer -> epoch no pierde ni duplica nada ----

struct ModelFixture {
  std::vector<int16_t> block;
  TrendModel m;
  explicit ModelFixture(int cap = 64) : block(trend_store_bytes(cap) / 2) {
    trend_model_init(&m, block.data(), cap);
  }
};

void test_model_starts_empty_without_time(void) {
  ModelFixture f;
  TEST_ASSERT_EQUAL_INT(0, f.m.store.count);
  TEST_ASSERT_EQUAL_INT(0, f.m.events.count);
  TEST_ASSERT_FALSE(f.m.clock.synced);
  TEST_ASSERT_EQUAL_INT64(0, trend_model_newest_slot(&f.m));
}

void test_model_records_on_monotonic_slots_before_time(void) {
  ModelFixture f;
  const TrendSample a = tagged(100);
  TEST_ASSERT_TRUE(trend_model_record(&f.m, 20000, 0, &a));   // ranura 2
  TEST_ASSERT_TRUE(trend_model_record(&f.m, 50000, 0, &a));   // ranura 5
  TEST_ASSERT_EQUAL_INT64(5, trend_model_newest_slot(&f.m));
  TEST_ASSERT_EQUAL_INT(4, f.m.store.count);  // 2,3,4,5 (3 y 4 sin dato)
}

void test_model_rejects_a_second_sample_in_the_same_slot(void) {
  ModelFixture f;
  const TrendSample a = tagged(100);
  TEST_ASSERT_TRUE(trend_model_record(&f.m, 20000, 0, &a));
  TEST_ASSERT_FALSE(trend_model_record(&f.m, 25000, 0, &a));
  TEST_ASSERT_EQUAL_INT(1, f.m.store.count);
}

void test_first_time_relabels_samples_and_events_keeping_their_spacing(void) {
  ModelFixture f;
  const TrendSample a = tagged(100);
  const TrendSample b = tagged(200);
  trend_model_record(&f.m, 20000, 0, &a);  // ranura 2
  trend_model_record(&f.m, 50000, 0, &b);  // ranura 5
  trend_model_edge(&f.m, 31000, 7, 2, true);  // ranura 3

  // Llega la hora: 65 s de arranque, epoch 1_700_000_005 -> ranura 170000000.
  const int64_t now = trend_model_tick(&f.m, 65000, 1700000005u);
  TEST_ASSERT_EQUAL_INT64(170000000, now);

  // La muestra de la ranura mono 5 (a 15 s de "ahora") queda 1 ranura antes...
  // 65000 ms -> mono 6 -> 170000000; mono 5 -> 169999999; mono 2 -> 169999996.
  TEST_ASSERT_EQUAL_INT64(169999999, f.m.store.lastSlot);
  TEST_ASSERT_EQUAL_INT16(200, trend_store_value(&f.m.store, TREND_AIR, 169999999));
  TEST_ASSERT_EQUAL_INT16(100, trend_store_value(&f.m.store, TREND_AIR, 169999996));
  TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA,
                          trend_store_value(&f.m.store, TREND_AIR, 169999997));
  TrendEvent ev;
  TEST_ASSERT_TRUE(trend_events_get(&f.m.events, 0, &ev));
  TEST_ASSERT_EQUAL_INT64(169999997, ev.slot);
}

void test_after_time_new_samples_continue_on_the_same_axis(void) {
  ModelFixture f;
  const TrendSample a = tagged(100);
  trend_model_record(&f.m, 50000, 0, &a);  // ranura mono 5
  trend_model_tick(&f.m, 65000, 1700000005u);
  const TrendSample b = tagged(300);
  // 10 s despues: epoch +10 -> ranura 170000001
  TEST_ASSERT_TRUE(trend_model_record(&f.m, 75000, 1700000015u, &b));
  TEST_ASSERT_EQUAL_INT64(170000001, trend_model_newest_slot(&f.m));
  // Entre la muestra pre-hora (169999999) y esta hay una ranura sin dato.
  TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA,
                          trend_store_value(&f.m.store, TREND_AIR, 170000000));
  TEST_ASSERT_EQUAL_INT16(100, trend_store_value(&f.m.store, TREND_AIR, 169999999));
}

void test_edge_uses_the_instant_of_the_edge_not_of_the_drain(void) {
  ModelFixture f;
  trend_model_tick(&f.m, 65000, 1700000005u);
  // El flanco ocurrio hace 30 s (mono 35 s) y se drena ahora.
  trend_model_edge(&f.m, 35000, 3, 1, false);
  TrendEvent ev;
  TEST_ASSERT_TRUE(trend_events_get(&f.m.events, 0, &ev));
  TEST_ASSERT_EQUAL_INT64(170000000 - 3, ev.slot);
  TEST_ASSERT_FALSE(ev.raised);
  TEST_ASSERT_EQUAL_UINT8(3, ev.id);
}

void test_a_big_time_correction_shifts_data_and_events_together(void) {
  ModelFixture f;
  trend_model_tick(&f.m, 65000, 1700000005u);
  const TrendSample a = tagged(100);
  trend_model_record(&f.m, 65000, 1700000005u, &a);
  trend_model_edge(&f.m, 65000, 1, 2, true);
  // La hora se corrige una hora hacia atras.
  trend_model_tick(&f.m, 65000, 1700000005u - 3600u);
  TEST_ASSERT_EQUAL_INT64(170000000 - 360, f.m.store.lastSlot);
  TrendEvent ev;
  TEST_ASSERT_TRUE(trend_events_get(&f.m.events, 0, &ev));
  TEST_ASSERT_EQUAL_INT64(170000000 - 360, ev.slot);
}

// ---- clockShifts: la persistencia detecta correcciones de hora (A3) ----

void test_clock_shifts_count_first_sync_and_real_corrections_only(void) {
  ModelFixture f;
  TEST_ASSERT_EQUAL_UINT32(0, f.m.clockShifts);
  trend_model_tick(&f.m, 20000, 0);  // sin hora: nada
  TEST_ASSERT_EQUAL_UINT32(0, f.m.clockShifts);
  trend_model_tick(&f.m, 65000, 1700000005u);  // primera sincronia
  TEST_ASSERT_EQUAL_UINT32(1, f.m.clockShifts);
  trend_model_tick(&f.m, 65000, 1700000015u);  // +-1 ranura: redondeo
  TEST_ASSERT_EQUAL_UINT32(1, f.m.clockShifts);
  trend_model_tick(&f.m, 65000, 1700000005u - 3600u);  // correccion real
  TEST_ASSERT_EQUAL_UINT32(2, f.m.clockShifts);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_clock_shifts_count_first_sync_and_real_corrections_only);
  RUN_TEST(test_without_time_the_slot_is_the_monotonic_one);
  RUN_TEST(test_epoch_zero_means_no_time_and_changes_nothing);
  RUN_TEST(test_first_time_sets_the_offset_and_returns_the_full_shift);
  RUN_TEST(test_rounding_of_one_slot_or_less_does_not_move_the_axis);
  RUN_TEST(test_a_correction_of_more_than_one_slot_returns_the_difference);
  RUN_TEST(test_time_disappearing_after_sync_keeps_the_offset);
  RUN_TEST(test_model_starts_empty_without_time);
  RUN_TEST(test_model_records_on_monotonic_slots_before_time);
  RUN_TEST(test_model_rejects_a_second_sample_in_the_same_slot);
  RUN_TEST(test_first_time_relabels_samples_and_events_keeping_their_spacing);
  RUN_TEST(test_after_time_new_samples_continue_on_the_same_axis);
  RUN_TEST(test_edge_uses_the_instant_of_the_edge_not_of_the_drain);
  RUN_TEST(test_a_big_time_correction_shifts_data_and_events_together);
  return UNITY_END();
}
