#include <unity.h>

#include <vector>

#include "modules/trend/trend_decimate.h"

void setUp(void) {}
void tearDown(void) {}

namespace {

// Un anillo de 24 h, como en el firmware.
struct Fixture {
  std::vector<int16_t> block;
  TrendStore store;
  Fixture() : block(trend_store_bytes(TREND_CAPACITY) / 2) {
    trend_store_init(&store, block.data(), TREND_CAPACITY);
  }
  // Escribe `v` en la serie del aire en `slot` (las demas, sin dato).
  void put(int64_t slot, int16_t v) {
    TrendSample s = trend_empty_sample();
    s.v[TREND_AIR] = v;
    TEST_ASSERT_TRUE(trend_store_record(&store, slot, &s));
  }
};

int64_t floorMod(int64_t a, int64_t m) { return ((a % m) + m) % m; }

}  // namespace

void test_window_slots_are_4_12_and_24_hours(void) {
  TEST_ASSERT_EQUAL_INT(1440, trend_window_slots(0));
  TEST_ASSERT_EQUAL_INT(4320, trend_window_slots(1));
  TEST_ASSERT_EQUAL_INT(8640, trend_window_slots(2));
  TEST_ASSERT_EQUAL_INT(1440, trend_window_slots(-1));
  TEST_ASSERT_EQUAL_INT(1440, trend_window_slots(3));
}

void test_chart_geometry_is_constant_across_windows(void) {
  TEST_ASSERT_EQUAL_INT(721, TREND_BUCKETS);
  TEST_ASSERT_EQUAL_INT(1442, TREND_CHART_POINTS);
  for (int idx = 0; idx < TREND_WINDOW_COUNT; idx++) {
    const TrendWindow w = trend_window_plan(500000, idx);
    TEST_ASSERT_EQUAL_INT(TREND_BUCKETS, w.buckets);
    TEST_ASSERT_EQUAL_INT(trend_window_slots(idx) / TREND_BASE_BUCKETS,
                          w.slotsPerBucket);
    TEST_ASSERT_EQUAL_INT64((int64_t)w.buckets * w.slotsPerBucket, w.totalSlots);
  }
  TEST_ASSERT_EQUAL_INT(2, trend_window_plan(500000, 0).slotsPerBucket);
  TEST_ASSERT_EQUAL_INT(6, trend_window_plan(500000, 1).slotsPerBucket);
  TEST_ASSERT_EQUAL_INT(12, trend_window_plan(500000, 2).slotsPerBucket);
}

void test_window_covers_now_and_the_whole_requested_span(void) {
  for (int idx = 0; idx < TREND_WINDOW_COUNT; idx++) {
    for (int64_t now = 170000000; now < 170000040; now++) {
      const TrendWindow w = trend_window_plan(now, idx);
      // "ahora" cabe dentro del eje...
      TEST_ASSERT_TRUE(w.startSlot + w.totalSlots > now);
      // ...y el eje cubre al menos las ranuras pedidas.
      TEST_ASSERT_TRUE(w.startSlot <= now - trend_window_slots(idx) + 1);
    }
  }
}

void test_bucket_boundaries_are_aligned_to_absolute_multiples(void) {
  for (int idx = 0; idx < TREND_WINDOW_COUNT; idx++) {
    for (int64_t now = 170000000; now < 170000040; now++) {
      const TrendWindow w = trend_window_plan(now, idx);
      TEST_ASSERT_EQUAL_INT64(0, floorMod(w.startSlot, w.slotsPerBucket));
    }
  }
}

// Si los limites siguieran a "ahora", cada ranura nueva cambiaria el contenido
// de todos los cubos y la traza parpadearia al repintar.
void test_start_only_moves_when_a_bucket_boundary_is_crossed(void) {
  const TrendWindow base = trend_window_plan(170000000, 2);  // 12 ranuras/cubo
  int changes = 0;
  int64_t prev = base.startSlot;
  for (int64_t now = 170000000; now < 170000000 + 48; now++) {
    const TrendWindow w = trend_window_plan(now, 2);
    if (w.startSlot != prev) {
      changes++;
      TEST_ASSERT_EQUAL_INT64(prev + 12, w.startSlot);
      prev = w.startSlot;
    }
  }
  TEST_ASSERT_EQUAL_INT(4, changes);  // 48 ranuras = 4 cubos de 12
}

void test_plan_works_with_small_monotonic_slots_before_time(void) {
  // Recien arrancado y sin hora: now - n + 1 es negativo.
  const TrendWindow w = trend_window_plan(100, 2);
  TEST_ASSERT_TRUE(w.startSlot < 0);
  TEST_ASSERT_EQUAL_INT64(0, floorMod(w.startSlot, w.slotsPerBucket));
  TEST_ASSERT_TRUE(w.startSlot + w.totalSlots > 100);
  TEST_ASSERT_TRUE(w.startSlot <= 100 - 8640 + 1);
}

void test_a_lone_spike_survives_24h_decimation(void) {
  Fixture f;
  const TrendWindow w = trend_window_plan(1000, 2);
  // Un cubo concreto, el 700: 12 ranuras a 300 con un 400 en la 5.
  const int64_t b0 = w.startSlot + 700 * 12;
  // Las ranuras anteriores sin dato (hueco) y luego el cubo.
  for (int64_t s = b0; s < b0 + 12; s++) f.put(s, s == b0 + 5 ? 400 : 300);
  std::vector<int16_t> out(TREND_CHART_POINTS);
  trend_decimate(&f.store, TREND_AIR, &w, out.data());
  // Minimo (300, primera aparicion: idx 0) antes que maximo (400, idx 5).
  TEST_ASSERT_EQUAL_INT16(300, out[2 * 700]);
  TEST_ASSERT_EQUAL_INT16(400, out[2 * 700 + 1]);
}

void test_points_keep_the_order_in_which_extremes_happened(void) {
  Fixture f;
  const TrendWindow w = trend_window_plan(1000, 2);
  const int64_t b0 = w.startSlot + 700 * 12;
  // Pico al principio y caida despues: el maximo ocurre antes que el minimo.
  for (int64_t s = b0; s < b0 + 12; s++) f.put(s, s == b0 ? 400 : 300);
  std::vector<int16_t> out(TREND_CHART_POINTS);
  trend_decimate(&f.store, TREND_AIR, &w, out.data());
  TEST_ASSERT_EQUAL_INT16(400, out[2 * 700]);
  TEST_ASSERT_EQUAL_INT16(300, out[2 * 700 + 1]);
}

void test_4h_window_reproduces_the_data_in_time_order(void) {
  Fixture f;
  const TrendWindow w = trend_window_plan(1000, 0);  // 2 ranuras por cubo
  const int64_t b0 = w.startSlot + 100 * 2;
  f.put(b0, 350);      // sube
  f.put(b0 + 1, 360);
  f.put(b0 + 2, 380);  // baja
  f.put(b0 + 3, 370);
  f.put(b0 + 4, 365);  // igual
  f.put(b0 + 5, 365);
  std::vector<int16_t> out(TREND_CHART_POINTS);
  trend_decimate(&f.store, TREND_AIR, &w, out.data());
  const int16_t expect[] = {350, 360, 380, 370, 365, 365};
  for (int i = 0; i < 6; i++) TEST_ASSERT_EQUAL_INT16(expect[i], out[2 * 100 + i]);
}

void test_bucket_without_data_emits_no_data_in_both_points(void) {
  Fixture f;
  const TrendWindow w = trend_window_plan(1000, 2);
  f.put(w.startSlot + 300 * 12, 310);  // un cubo con dato; el resto, nada
  std::vector<int16_t> out(TREND_CHART_POINTS, 123);
  trend_decimate(&f.store, TREND_AIR, &w, out.data());
  TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA, out[2 * 299]);
  TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA, out[2 * 299 + 1]);
  TEST_ASSERT_EQUAL_INT16(310, out[2 * 300]);
  TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA, out[2 * 301]);
}

void test_partial_bucket_uses_only_the_valid_slots(void) {
  Fixture f;
  const TrendWindow w = trend_window_plan(1000, 2);
  const int64_t b0 = w.startSlot + 500 * 12;
  // Solo 2 de las 12 ranuras tienen dato; el resto, sin dato intercalado.
  TrendSample a = trend_empty_sample();
  a.v[TREND_AIR] = 305;
  TrendSample b = trend_empty_sample();
  b.v[TREND_AIR] = 295;
  trend_store_record(&f.store, b0 + 2, &a);
  trend_store_record(&f.store, b0 + 9, &b);
  std::vector<int16_t> out(TREND_CHART_POINTS);
  trend_decimate(&f.store, TREND_AIR, &w, out.data());
  // max (305) ocurre antes que min (295).
  TEST_ASSERT_EQUAL_INT16(305, out[2 * 500]);
  TEST_ASSERT_EQUAL_INT16(295, out[2 * 500 + 1]);
}

void test_single_valid_slot_gives_that_value_twice(void) {
  Fixture f;
  const TrendWindow w = trend_window_plan(1000, 1);  // 6 ranuras por cubo
  f.put(w.startSlot + 40 * 6 + 3, 333);
  std::vector<int16_t> out(TREND_CHART_POINTS);
  trend_decimate(&f.store, TREND_AIR, &w, out.data());
  TEST_ASSERT_EQUAL_INT16(333, out[2 * 40]);
  TEST_ASSERT_EQUAL_INT16(333, out[2 * 40 + 1]);
}

void test_each_series_decimates_independently(void) {
  Fixture f;
  const TrendWindow w = trend_window_plan(1000, 0);
  TrendSample s = trend_empty_sample();
  s.v[TREND_AIR] = 360;
  s.v[TREND_SP_AIR] = 370;
  trend_store_record(&f.store, w.startSlot + 50 * 2, &s);
  std::vector<int16_t> air(TREND_CHART_POINTS), sp(TREND_CHART_POINTS),
      skin(TREND_CHART_POINTS);
  trend_decimate(&f.store, TREND_AIR, &w, air.data());
  trend_decimate(&f.store, TREND_SP_AIR, &w, sp.data());
  trend_decimate(&f.store, TREND_SKIN, &w, skin.data());
  TEST_ASSERT_EQUAL_INT16(360, air[100]);
  TEST_ASSERT_EQUAL_INT16(370, sp[100]);
  TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA, skin[100]);
}

void test_data_older_than_the_ring_or_in_the_future_is_no_data(void) {
  Fixture f;
  const TrendWindow w = trend_window_plan(100000, 2);
  // Solo las 5 ultimas ranuras de la ventana tienen dato.
  for (int64_t s = 99995; s <= 100000; s++) f.put(s, 300);
  std::vector<int16_t> out(TREND_CHART_POINTS);
  trend_decimate(&f.store, TREND_AIR, &w, out.data());
  TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA, out[0]);
  TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA, out[2 * 360]);
  // El cubo que contiene 100000 tiene dato; el ultimo (cubo parcial a la
  // derecha) puede no tenerlo si "ahora" cae en el penultimo.
  const int bNow = (int)((100000 - w.startSlot) / w.slotsPerBucket);
  TEST_ASSERT_EQUAL_INT16(300, out[2 * bNow]);
}

void test_empty_store_decimates_to_all_no_data(void) {
  Fixture f;
  const TrendWindow w = trend_window_plan(1000, 0);
  std::vector<int16_t> out(TREND_CHART_POINTS, 7);
  trend_decimate(&f.store, TREND_AIR, &w, out.data());
  for (int i = 0; i < TREND_CHART_POINTS; i++)
    TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA, out[i]);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_window_slots_are_4_12_and_24_hours);
  RUN_TEST(test_chart_geometry_is_constant_across_windows);
  RUN_TEST(test_window_covers_now_and_the_whole_requested_span);
  RUN_TEST(test_bucket_boundaries_are_aligned_to_absolute_multiples);
  RUN_TEST(test_start_only_moves_when_a_bucket_boundary_is_crossed);
  RUN_TEST(test_plan_works_with_small_monotonic_slots_before_time);
  RUN_TEST(test_a_lone_spike_survives_24h_decimation);
  RUN_TEST(test_points_keep_the_order_in_which_extremes_happened);
  RUN_TEST(test_4h_window_reproduces_the_data_in_time_order);
  RUN_TEST(test_bucket_without_data_emits_no_data_in_both_points);
  RUN_TEST(test_partial_bucket_uses_only_the_valid_slots);
  RUN_TEST(test_single_valid_slot_gives_that_value_twice);
  RUN_TEST(test_each_series_decimates_independently);
  RUN_TEST(test_data_older_than_the_ring_or_in_the_future_is_no_data);
  RUN_TEST(test_empty_store_decimates_to_all_no_data);
  return UNITY_END();
}
