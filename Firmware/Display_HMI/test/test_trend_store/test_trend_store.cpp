#include <unity.h>

#include <cmath>
#include <vector>

#include "modules/trend/trend_store.h"

void setUp(void) {}
void tearDown(void) {}

namespace {

// Anillo pequeno para poder dar la vuelta con pocas muestras; la logica es la
// misma que con 1440 u 8640 ranuras.
constexpr int CAP = 8;

struct Fixture {
  std::vector<int16_t> block;
  TrendStore store;
  explicit Fixture(int cap = CAP) : block(trend_store_bytes(cap) / 2, 0x5A5A) {
    trend_store_init(&store, block.data(), cap);
  }
};

TrendSample sample(int16_t air, int16_t skin, int16_t hum, int16_t spAir,
                   int16_t spSkin, int16_t spHum) {
  TrendSample s;
  s.v[TREND_AIR] = air;
  s.v[TREND_SKIN] = skin;
  s.v[TREND_HUM] = hum;
  s.v[TREND_SP_AIR] = spAir;
  s.v[TREND_SP_SKIN] = spSkin;
  s.v[TREND_SP_HUM] = spHum;
  return s;
}

// Muestra reconocible por su ranura, para comprobar que cada valor sigue en
// su sitio tras dar la vuelta.
TrendSample tagged(int64_t slot) {
  const int16_t b = (int16_t)(slot & 0x3FFF);
  return sample(b, (int16_t)(b + 1), (int16_t)(b + 2), (int16_t)(b + 3),
                (int16_t)(b + 4), (int16_t)(b + 5));
}

}  // namespace

void test_bytes_is_six_planar_series(void) {
  TEST_ASSERT_EQUAL_UINT32(6u * 8640u * 2u, (uint32_t)trend_store_bytes(8640));
  TEST_ASSERT_EQUAL_UINT32(6u * 8u * 2u, (uint32_t)trend_store_bytes(8));
}

void test_empty_store_reads_nothing(void) {
  Fixture f;
  TrendSample out;
  TEST_ASSERT_EQUAL_INT(0, f.store.count);
  TEST_ASSERT_FALSE(trend_store_get(&f.store, 0, &out));
  TEST_ASSERT_FALSE(trend_store_get(&f.store, 100, &out));
  TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA, trend_store_value(&f.store, TREND_AIR, 5));
  TEST_ASSERT_EQUAL_INT64(0, trend_store_first_slot(&f.store));
}

void test_first_record_is_readable_in_all_series(void) {
  Fixture f;
  const TrendSample in = sample(367, 365, 550, 370, 366, 600);
  TEST_ASSERT_TRUE(trend_store_record(&f.store, 1000, &in));
  TEST_ASSERT_EQUAL_INT(1, f.store.count);
  TEST_ASSERT_EQUAL_INT64(1000, trend_store_first_slot(&f.store));
  TrendSample out;
  TEST_ASSERT_TRUE(trend_store_get(&f.store, 1000, &out));
  for (int s = 0; s < TREND_SERIES; s++) TEST_ASSERT_EQUAL_INT16(in.v[s], out.v[s]);
}

void test_consecutive_slots_do_not_fill_gaps(void) {
  Fixture f;
  for (int64_t s = 10; s < 14; s++) {
    const TrendSample in = tagged(s);
    TEST_ASSERT_TRUE(trend_store_record(&f.store, s, &in));
  }
  TEST_ASSERT_EQUAL_INT(4, f.store.count);
  for (int64_t s = 10; s < 14; s++) {
    TEST_ASSERT_EQUAL_INT16(tagged(s).v[TREND_HUM],
                            trend_store_value(&f.store, TREND_HUM, s));
  }
}

// Una ranura sin telemetria ocupa su sitio en el eje X, en las SEIS series
// (tambien la consigna): sin telemetria tampoco hay consigna conocida.
void test_gap_is_filled_with_no_data_in_every_series(void) {
  Fixture f;
  const TrendSample a = tagged(100);
  const TrendSample b = tagged(105);
  TEST_ASSERT_TRUE(trend_store_record(&f.store, 100, &a));
  TEST_ASSERT_TRUE(trend_store_record(&f.store, 105, &b));
  TEST_ASSERT_EQUAL_INT(6, f.store.count);
  for (int64_t s = 101; s <= 104; s++) {
    for (int ser = 0; ser < TREND_SERIES; ser++)
      TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA, trend_store_value(&f.store, ser, s));
  }
  TEST_ASSERT_EQUAL_INT16(a.v[TREND_AIR], trend_store_value(&f.store, TREND_AIR, 100));
  TEST_ASSERT_EQUAL_INT16(b.v[TREND_SP_HUM],
                          trend_store_value(&f.store, TREND_SP_HUM, 105));
}

void test_repeated_or_older_slot_is_ignored(void) {
  Fixture f;
  const TrendSample a = tagged(50);
  const TrendSample other = sample(1, 2, 3, 4, 5, 6);
  TEST_ASSERT_TRUE(trend_store_record(&f.store, 50, &a));
  TEST_ASSERT_FALSE(trend_store_record(&f.store, 50, &other));
  TEST_ASSERT_FALSE(trend_store_record(&f.store, 49, &other));
  TEST_ASSERT_EQUAL_INT(1, f.store.count);
  TEST_ASSERT_EQUAL_INT64(50, f.store.lastSlot);
  TEST_ASSERT_EQUAL_INT16(a.v[TREND_AIR], trend_store_value(&f.store, TREND_AIR, 50));
}

void test_ring_wraps_keeping_only_the_most_recent(void) {
  Fixture f;
  for (int64_t s = 1; s <= 20; s++) {
    const TrendSample in = tagged(s);
    TEST_ASSERT_TRUE(trend_store_record(&f.store, s, &in));
  }
  TEST_ASSERT_EQUAL_INT(CAP, f.store.count);
  TEST_ASSERT_EQUAL_INT64(13, trend_store_first_slot(&f.store));
  TrendSample out;
  TEST_ASSERT_FALSE(trend_store_get(&f.store, 12, &out));
  TEST_ASSERT_FALSE(trend_store_get(&f.store, 21, &out));
  for (int64_t s = 13; s <= 20; s++) {
    TEST_ASSERT_TRUE(trend_store_get(&f.store, s, &out));
    for (int ser = 0; ser < TREND_SERIES; ser++)
      TEST_ASSERT_EQUAL_INT16(tagged(s).v[ser], out.v[ser]);
  }
}

// Un hueco mayor que el anillo: lo unico legible es la muestra nueva y el
// "sin dato" que la precede, nunca datos viejos reetiquetados.
void test_gap_larger_than_capacity_leaves_only_no_data_before_the_sample(void) {
  Fixture f;
  const TrendSample a = tagged(1);
  const TrendSample b = tagged(1000);
  TEST_ASSERT_TRUE(trend_store_record(&f.store, 1, &a));
  TEST_ASSERT_TRUE(trend_store_record(&f.store, 1000, &b));
  TEST_ASSERT_EQUAL_INT(CAP, f.store.count);
  TEST_ASSERT_EQUAL_INT16(b.v[TREND_AIR], trend_store_value(&f.store, TREND_AIR, 1000));
  for (int64_t s = 1000 - (CAP - 1); s < 1000; s++)
    TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA, trend_store_value(&f.store, TREND_AIR, s));
  TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA, trend_store_value(&f.store, TREND_AIR, 1));
}

void test_shift_relabels_without_moving_data(void) {
  Fixture f;
  for (int64_t s = 5; s <= 7; s++) {
    const TrendSample in = tagged(s);
    trend_store_record(&f.store, s, &in);
  }
  trend_store_shift(&f.store, 100);
  TEST_ASSERT_EQUAL_INT64(107, f.store.lastSlot);
  TEST_ASSERT_EQUAL_INT64(105, trend_store_first_slot(&f.store));
  for (int64_t s = 5; s <= 7; s++)
    TEST_ASSERT_EQUAL_INT16(tagged(s).v[TREND_SKIN],
                            trend_store_value(&f.store, TREND_SKIN, s + 100));
  TrendSample out;
  TEST_ASSERT_FALSE(trend_store_get(&f.store, 5, &out));
  // Y se puede seguir escribiendo a continuacion.
  const TrendSample next = tagged(108);
  TEST_ASSERT_TRUE(trend_store_record(&f.store, 108, &next));
  TEST_ASSERT_EQUAL_INT(4, f.store.count);
}

void test_shift_backwards_is_allowed(void) {
  Fixture f;
  const TrendSample a = tagged(500);
  trend_store_record(&f.store, 500, &a);
  trend_store_shift(&f.store, -450);
  TEST_ASSERT_EQUAL_INT64(50, f.store.lastSlot);
  TEST_ASSERT_EQUAL_INT16(a.v[TREND_AIR], trend_store_value(&f.store, TREND_AIR, 50));
}

void test_clear_empties_the_store(void) {
  Fixture f;
  const TrendSample a = tagged(7);
  trend_store_record(&f.store, 7, &a);
  trend_store_clear(&f.store);
  TEST_ASSERT_EQUAL_INT(0, f.store.count);
  TrendSample out;
  TEST_ASSERT_FALSE(trend_store_get(&f.store, 7, &out));
  // Tras vaciar, cualquier ranura vuelve a ser aceptable (no queda lastSlot).
  TEST_ASSERT_TRUE(trend_store_record(&f.store, 3, &a));
}

void test_tenths_rounds_and_flags_no_data(void) {
  TEST_ASSERT_EQUAL_INT16(367, trend_tenths(36.7f, true));
  TEST_ASSERT_EQUAL_INT16(367, trend_tenths(36.74f, true));
  TEST_ASSERT_EQUAL_INT16(-25, trend_tenths(-2.5f, true));
  TEST_ASSERT_EQUAL_INT16(0, trend_tenths(0.0f, true));
  TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA, trend_tenths(36.7f, false));
  TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA, trend_tenths(NAN, true));
}

// El centinela de la placa (-999.0) o cualquier absurdo no puede colarse como
// "sin dato" ni desbordar: se satura dentro del rango legible.
void test_tenths_saturates_and_never_collides_with_no_data(void) {
  TEST_ASSERT_EQUAL_INT16(32767, trend_tenths(99999.0f, true));
  TEST_ASSERT_EQUAL_INT16(-32767, trend_tenths(-99999.0f, true));
  TEST_ASSERT_NOT_EQUAL(TREND_NO_DATA, trend_tenths(-99999.0f, true));
  // Un infinito no es una lectura: "sin dato", no una saturacion.
  TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA, trend_tenths(INFINITY, true));
  TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA, trend_tenths(-INFINITY, true));
}

void test_empty_sample_is_all_no_data(void) {
  const TrendSample s = trend_empty_sample();
  for (int i = 0; i < TREND_SERIES; i++) TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA, s.v[i]);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_bytes_is_six_planar_series);
  RUN_TEST(test_empty_store_reads_nothing);
  RUN_TEST(test_first_record_is_readable_in_all_series);
  RUN_TEST(test_consecutive_slots_do_not_fill_gaps);
  RUN_TEST(test_gap_is_filled_with_no_data_in_every_series);
  RUN_TEST(test_repeated_or_older_slot_is_ignored);
  RUN_TEST(test_ring_wraps_keeping_only_the_most_recent);
  RUN_TEST(test_gap_larger_than_capacity_leaves_only_no_data_before_the_sample);
  RUN_TEST(test_shift_relabels_without_moving_data);
  RUN_TEST(test_shift_backwards_is_allowed);
  RUN_TEST(test_clear_empties_the_store);
  RUN_TEST(test_tenths_rounds_and_flags_no_data);
  RUN_TEST(test_tenths_saturates_and_never_collides_with_no_data);
  RUN_TEST(test_empty_sample_is_all_no_data);
  return UNITY_END();
}
