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

// ---- prepend: restauracion de un bloque anterior (series planares LE) ----

namespace {

// Bloque de `n` ranuras que termina en `lastSlot`, serie a serie, int16 LE.
std::vector<uint8_t> blockLE(int64_t lastSlot, int n, int tag = 0) {
  std::vector<uint8_t> b((size_t)TREND_SERIES * (size_t)n * 2);
  for (int ser = 0; ser < TREND_SERIES; ser++) {
    for (int i = 0; i < n; i++) {
      const int64_t slot = lastSlot - n + 1 + i;
      const int16_t v = (int16_t)(1000 * ser + (slot & 0xFF) + tag);
      const size_t o = ((size_t)ser * (size_t)n + (size_t)i) * 2;
      b[o] = (uint8_t)(v & 0xFF);
      b[o + 1] = (uint8_t)((uint16_t)v >> 8);
    }
  }
  return b;
}

int16_t blockVal(int64_t slot, int ser, int tag = 0) {
  return (int16_t)(1000 * ser + (slot & 0xFF) + tag);
}

}  // namespace

void test_prepend_onto_an_empty_store_becomes_its_content(void) {
  Fixture f;
  const std::vector<uint8_t> b = blockLE(200, 5);
  TEST_ASSERT_EQUAL_INT(5, trend_store_prepend(&f.store, 200, b.data(), 5));
  TEST_ASSERT_EQUAL_INT(5, f.store.count);
  TEST_ASSERT_EQUAL_INT64(200, f.store.lastSlot);
  TEST_ASSERT_EQUAL_INT64(196, trend_store_first_slot(&f.store));
  for (int64_t s = 196; s <= 200; s++)
    for (int ser = 0; ser < TREND_SERIES; ser++)
      TEST_ASSERT_EQUAL_INT16(blockVal(s, ser), trend_store_value(&f.store, ser, s));
  // Y el anillo sigue siendo escribible a continuacion.
  const TrendSample next = tagged(203);
  TEST_ASSERT_TRUE(trend_store_record(&f.store, 203, &next));
  TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA, trend_store_value(&f.store, TREND_AIR, 201));
  TEST_ASSERT_EQUAL_INT16(blockVal(200, 0), trend_store_value(&f.store, TREND_AIR, 200));
}

void test_prepend_before_live_data_fills_the_gap_with_no_data(void) {
  Fixture f(32);
  for (int64_t s = 100; s <= 102; s++) {
    const TrendSample x = tagged(s);
    trend_store_record(&f.store, s, &x);
  }
  const std::vector<uint8_t> b = blockLE(96, 3);  // 94..96
  TEST_ASSERT_EQUAL_INT(3, trend_store_prepend(&f.store, 96, b.data(), 3));
  TEST_ASSERT_EQUAL_INT(9, f.store.count);  // 94..102
  TEST_ASSERT_EQUAL_INT64(102, f.store.lastSlot);
  for (int64_t s = 94; s <= 96; s++)
    TEST_ASSERT_EQUAL_INT16(blockVal(s, TREND_SKIN),
                            trend_store_value(&f.store, TREND_SKIN, s));
  for (int64_t s = 97; s <= 99; s++)
    for (int ser = 0; ser < TREND_SERIES; ser++)
      TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA, trend_store_value(&f.store, ser, s));
  for (int64_t s = 100; s <= 102; s++)
    TEST_ASSERT_EQUAL_INT16(tagged(s).v[TREND_SKIN],
                            trend_store_value(&f.store, TREND_SKIN, s));
}

void test_prepend_does_not_overwrite_live_slots(void) {
  Fixture f(32);
  for (int64_t s = 100; s <= 105; s++) {
    const TrendSample x = tagged(s);
    trend_store_record(&f.store, s, &x);
  }
  const std::vector<uint8_t> b = blockLE(102, 5, 5000);  // 98..102, solapa 100..102
  TEST_ASSERT_EQUAL_INT(2, trend_store_prepend(&f.store, 102, b.data(), 5));
  TEST_ASSERT_EQUAL_INT(8, f.store.count);  // 98..105
  TEST_ASSERT_EQUAL_INT16(blockVal(98, TREND_AIR, 5000),
                          trend_store_value(&f.store, TREND_AIR, 98));
  TEST_ASSERT_EQUAL_INT16(blockVal(99, TREND_AIR, 5000),
                          trend_store_value(&f.store, TREND_AIR, 99));
  for (int64_t s = 100; s <= 105; s++)
    TEST_ASSERT_EQUAL_INT16(tagged(s).v[TREND_AIR],
                            trend_store_value(&f.store, TREND_AIR, s));
}

void test_prepend_of_a_block_that_is_not_older_adds_nothing(void) {
  Fixture f(32);
  const TrendSample x = tagged(100);
  trend_store_record(&f.store, 100, &x);
  const std::vector<uint8_t> b = blockLE(110, 4);
  TEST_ASSERT_EQUAL_INT(0, trend_store_prepend(&f.store, 110, b.data(), 4));
  const std::vector<uint8_t> c = blockLE(100, 4);  // 97..100: entran 97..99, no la 100 viva
  TEST_ASSERT_EQUAL_INT(3, trend_store_prepend(&f.store, 100, c.data(), 4));
}

void test_prepend_drops_what_does_not_fit_from_the_old_side(void) {
  Fixture f(8);
  for (int64_t s = 100; s <= 102; s++) {
    const TrendSample x = tagged(s);
    trend_store_record(&f.store, s, &x);
  }
  const std::vector<uint8_t> b = blockLE(99, 10);  // 90..99, caben 5
  TEST_ASSERT_EQUAL_INT(5, trend_store_prepend(&f.store, 99, b.data(), 10));
  TEST_ASSERT_EQUAL_INT(8, f.store.count);
  TEST_ASSERT_EQUAL_INT64(95, trend_store_first_slot(&f.store));
  for (int64_t s = 95; s <= 99; s++)
    TEST_ASSERT_EQUAL_INT16(blockVal(s, TREND_HUM), trend_store_value(&f.store, TREND_HUM, s));
  TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA, trend_store_value(&f.store, TREND_HUM, 94));
}

void test_prepend_with_a_gap_that_leaves_no_room_adds_nothing(void) {
  Fixture f(8);
  for (int64_t s = 100; s <= 102; s++) {
    const TrendSample x = tagged(s);
    trend_store_record(&f.store, s, &x);
  }
  const std::vector<uint8_t> b = blockLE(80, 5);
  TEST_ASSERT_EQUAL_INT(0, trend_store_prepend(&f.store, 80, b.data(), 5));
  TEST_ASSERT_EQUAL_INT(3, f.store.count);
  TEST_ASSERT_EQUAL_INT64(100, trend_store_first_slot(&f.store));
}

void test_prepend_into_a_full_store_adds_nothing(void) {
  Fixture f(4);
  for (int64_t s = 100; s <= 103; s++) {
    const TrendSample x = tagged(s);
    trend_store_record(&f.store, s, &x);
  }
  const std::vector<uint8_t> b = blockLE(99, 3);
  TEST_ASSERT_EQUAL_INT(0, trend_store_prepend(&f.store, 99, b.data(), 3));
  TEST_ASSERT_EQUAL_INT(4, f.store.count);
}

// El anillo ya tiene el cursor de escritura a mitad de camino: la zona anterior
// a la primera ranura da la vuelta por el final del bloque.
void test_prepend_wraps_around_the_end_of_the_ring(void) {
  Fixture f(8);
  for (int64_t s = 1; s <= 6; s++) {
    const TrendSample x = tagged(s);
    trend_store_record(&f.store, s, &x);
  }
  const std::vector<uint8_t> b = blockLE(0, 4);  // -3..0, caben 2
  TEST_ASSERT_EQUAL_INT(2, trend_store_prepend(&f.store, 0, b.data(), 4));
  TEST_ASSERT_EQUAL_INT(8, f.store.count);
  TEST_ASSERT_EQUAL_INT64(-1, trend_store_first_slot(&f.store));
  for (int64_t s = -1; s <= 0; s++)
    TEST_ASSERT_EQUAL_INT16(blockVal(s, TREND_SP_HUM),
                            trend_store_value(&f.store, TREND_SP_HUM, s));
  for (int64_t s = 1; s <= 6; s++)
    TEST_ASSERT_EQUAL_INT16(tagged(s).v[TREND_SP_HUM],
                            trend_store_value(&f.store, TREND_SP_HUM, s));
}

void test_prepend_rejects_bad_arguments(void) {
  Fixture f;
  const std::vector<uint8_t> b = blockLE(10, 3);
  TEST_ASSERT_EQUAL_INT(0, trend_store_prepend(nullptr, 10, b.data(), 3));
  TEST_ASSERT_EQUAL_INT(0, trend_store_prepend(&f.store, 10, nullptr, 3));
  TEST_ASSERT_EQUAL_INT(0, trend_store_prepend(&f.store, 10, b.data(), 0));
  TEST_ASSERT_EQUAL_INT(0, trend_store_prepend(&f.store, 10, b.data(), -2));
  TEST_ASSERT_EQUAL_INT(0, f.store.count);
}

// ---- generacion del anillo (C2) ----

void test_gen_changes_on_every_visible_mutation_and_only_then(void) {
  Fixture f;
  const uint32_t g0 = f.store.gen;
  const TrendSample a = tagged(1);
  TEST_ASSERT_TRUE(trend_store_record(&f.store, 10, &a));
  const uint32_t g1 = f.store.gen;
  TEST_ASSERT_NOT_EQUAL(g0, g1);
  TEST_ASSERT_FALSE(trend_store_record(&f.store, 10, &a));  // rechazada
  TEST_ASSERT_EQUAL_UINT32(g1, f.store.gen);
  trend_store_shift(&f.store, 5);
  const uint32_t g2 = f.store.gen;
  TEST_ASSERT_NOT_EQUAL(g1, g2);
  const std::vector<uint8_t> b = blockLE(14, 2);
  TEST_ASSERT_EQUAL_INT(2, trend_store_prepend(&f.store, 14, b.data(), 2));
  const uint32_t g3 = f.store.gen;
  TEST_ASSERT_NOT_EQUAL(g2, g3);
  const std::vector<uint8_t> none = blockLE(500, 2);  // posterior: no entra nada
  TEST_ASSERT_EQUAL_INT(0, trend_store_prepend(&f.store, 500, none.data(), 2));
  TEST_ASSERT_EQUAL_UINT32(g3, f.store.gen);
  trend_store_clear(&f.store);
  TEST_ASSERT_NOT_EQUAL(g3, f.store.gen);
}

// ---- prepend repartido en pasadas (C2) ----

namespace {
// Corre el trabajo hasta el final con tramos de `slice` ranuras; devuelve el
// numero de pasadas y el estado final.
int runJob(TrendPrependJob *j, int slice, TrendJobStatus *outStatus) {
  int steps = 0;
  for (;;) {
    steps++;
    const TrendJobStatus st = trend_prepend_step(j, slice);
    if (st != TREND_JOB_MORE) {
      *outStatus = st;
      return steps;
    }
    if (steps > 100000) {
      *outStatus = TREND_JOB_FAIL;
      return steps;
    }
  }
}
}  // namespace

void test_sliced_prepend_equals_the_one_shot_result(void) {
  Fixture a(64), b(64);
  for (int64_t s = 100; s <= 105; s++) {
    const TrendSample x = tagged(s);
    trend_store_record(&a.store, s, &x);
    trend_store_record(&b.store, s, &x);
  }
  const std::vector<uint8_t> blk = blockLE(90, 30);  // 61..90, hueco 91..99
  const int one = trend_store_prepend(&a.store, 90, blk.data(), 30);

  TrendPrependJob j;
  TEST_ASSERT_TRUE(trend_prepend_begin(&j, &b.store, 90, blk.data(), 30));
  TrendJobStatus st;
  const int steps = runJob(&j, 7, &st);
  TEST_ASSERT_EQUAL_INT(TREND_JOB_DONE, st);
  TEST_ASSERT_TRUE(steps > 1);
  TEST_ASSERT_EQUAL_INT(one, j.added);
  TEST_ASSERT_EQUAL_INT(a.store.count, b.store.count);
  TEST_ASSERT_EQUAL_INT64(a.store.lastSlot, b.store.lastSlot);
  for (int64_t s = 61; s <= 105; s++)
    for (int ser = 0; ser < TREND_SERIES; ser++)
      TEST_ASSERT_EQUAL_INT16(trend_store_value(&a.store, ser, s),
                              trend_store_value(&b.store, ser, s));
}

void test_a_sliced_prepend_is_invisible_until_the_last_slice(void) {
  Fixture f(64);
  const TrendSample x = tagged(100);
  trend_store_record(&f.store, 100, &x);
  const std::vector<uint8_t> blk = blockLE(99, 20);
  TrendPrependJob j;
  TEST_ASSERT_TRUE(trend_prepend_begin(&j, &f.store, 99, blk.data(), 20));
  TEST_ASSERT_EQUAL_INT(TREND_JOB_MORE, trend_prepend_step(&j, 5));
  TEST_ASSERT_EQUAL_INT(1, f.store.count);              // aun no se ve nada
  TEST_ASSERT_EQUAL_INT64(100, trend_store_first_slot(&f.store));
  TEST_ASSERT_EQUAL_INT(TREND_JOB_MORE, trend_prepend_step(&j, 5));
  TEST_ASSERT_EQUAL_INT(1, f.store.count);
  TEST_ASSERT_EQUAL_INT(TREND_JOB_MORE, trend_prepend_step(&j, 5));
  TEST_ASSERT_EQUAL_INT(TREND_JOB_DONE, trend_prepend_step(&j, 5));
  TEST_ASSERT_EQUAL_INT(21, f.store.count);
}

void test_a_live_sample_between_slices_restarts_without_touching_the_ring(void) {
  Fixture f(64);
  const TrendSample x = tagged(100);
  trend_store_record(&f.store, 100, &x);
  const std::vector<uint8_t> blk = blockLE(99, 20);
  TrendPrependJob j;
  TEST_ASSERT_TRUE(trend_prepend_begin(&j, &f.store, 99, blk.data(), 20));
  TEST_ASSERT_EQUAL_INT(TREND_JOB_MORE, trend_prepend_step(&j, 5));
  const TrendSample y = tagged(101);
  trend_store_record(&f.store, 101, &y);  // llega una muestra viva
  TEST_ASSERT_EQUAL_INT(TREND_JOB_RESTART, trend_prepend_step(&j, 5));
  TEST_ASSERT_EQUAL_INT(2, f.store.count);
  TEST_ASSERT_EQUAL_INT64(100, trend_store_first_slot(&f.store));
  // Y tras replanificar, el trabajo termina bien.
  TEST_ASSERT_TRUE(trend_prepend_begin(&j, &f.store, 99, blk.data(), 20));
  TrendJobStatus st;
  runJob(&j, 5, &st);
  TEST_ASSERT_EQUAL_INT(TREND_JOB_DONE, st);
  TEST_ASSERT_EQUAL_INT(22, f.store.count);
  TEST_ASSERT_EQUAL_INT16(tagged(101).v[TREND_AIR],
                          trend_store_value(&f.store, TREND_AIR, 101));
}

void test_prepend_begin_says_false_when_there_is_nothing_to_do(void) {
  Fixture f(8);
  for (int64_t s = 100; s <= 107; s++) {  // lleno
    const TrendSample x = tagged(s);
    trend_store_record(&f.store, s, &x);
  }
  const std::vector<uint8_t> blk = blockLE(99, 3);
  TrendPrependJob j;
  TEST_ASSERT_FALSE(trend_prepend_begin(&j, &f.store, 99, blk.data(), 3));
  TEST_ASSERT_EQUAL_INT(0, j.added);
  TEST_ASSERT_FALSE(trend_prepend_begin(&j, nullptr, 99, blk.data(), 3));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_gen_changes_on_every_visible_mutation_and_only_then);
  RUN_TEST(test_sliced_prepend_equals_the_one_shot_result);
  RUN_TEST(test_a_sliced_prepend_is_invisible_until_the_last_slice);
  RUN_TEST(test_a_live_sample_between_slices_restarts_without_touching_the_ring);
  RUN_TEST(test_prepend_begin_says_false_when_there_is_nothing_to_do);
  RUN_TEST(test_prepend_onto_an_empty_store_becomes_its_content);
  RUN_TEST(test_prepend_before_live_data_fills_the_gap_with_no_data);
  RUN_TEST(test_prepend_does_not_overwrite_live_slots);
  RUN_TEST(test_prepend_of_a_block_that_is_not_older_adds_nothing);
  RUN_TEST(test_prepend_drops_what_does_not_fit_from_the_old_side);
  RUN_TEST(test_prepend_with_a_gap_that_leaves_no_room_adds_nothing);
  RUN_TEST(test_prepend_into_a_full_store_adds_nothing);
  RUN_TEST(test_prepend_wraps_around_the_end_of_the_ring);
  RUN_TEST(test_prepend_rejects_bad_arguments);
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
