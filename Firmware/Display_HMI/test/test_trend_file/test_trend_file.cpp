#include <unity.h>

#include <cstring>
#include <vector>

#include "modules/trend/trend_file.h"

void setUp(void) {}
void tearDown(void) {}

namespace {

constexpr int64_t BASE = 170000000;  // ~ ranuras de epoch/10 de 2026

struct Ring {
  std::vector<int16_t> block;
  TrendStore st;
  TrendEvents ev;
  explicit Ring(int cap) : block(trend_store_bytes(cap) / 2, 0x7777) {
    trend_store_init(&st, block.data(), cap);
    trend_events_init(&ev);
  }
};

// Valor reconocible por ranura y serie. `tag` distingue "fichero" de "vivo".
int16_t val(int64_t slot, int ser, int tag = 0) {
  return (int16_t)(1000 * ser + (slot % 500) + tag);
}

TrendSample sampleAt(int64_t slot, int tag = 0) {
  TrendSample s;
  for (int i = 0; i < TREND_SERIES; i++) s.v[i] = val(slot, i, tag);
  return s;
}

void fill(Ring &r, int64_t from, int64_t to, int tag = 0) {
  for (int64_t s = from; s <= to; s++) {
    const TrendSample x = sampleAt(s, tag);
    TEST_ASSERT_TRUE(trend_store_record(&r.st, s, &x));
  }
}

std::vector<uint8_t> serializeOf(const Ring &r) {
  std::vector<uint8_t> buf(trend_file_max_bytes(r.st.capacity));
  const size_t n = trend_file_serialize(&r.st, &r.ev, buf.data(), buf.size());
  TEST_ASSERT_TRUE(n > 0);
  buf.resize(n);
  return buf;
}

uint32_t rd32(const std::vector<uint8_t> &b, size_t o) {
  return (uint32_t)b[o] | ((uint32_t)b[o + 1] << 8) | ((uint32_t)b[o + 2] << 16) |
         ((uint32_t)b[o + 3] << 24);
}
uint16_t rd16(const std::vector<uint8_t> &b, size_t o) {
  return (uint16_t)(b[o] | (b[o + 1] << 8));
}
void wr32(std::vector<uint8_t> &b, size_t o, uint32_t v) {
  for (int i = 0; i < 4; i++) b[o + i] = (uint8_t)(v >> (8 * i));
}
void wr16(std::vector<uint8_t> &b, size_t o, uint16_t v) {
  b[o] = (uint8_t)v;
  b[o + 1] = (uint8_t)(v >> 8);
}
// Recalcula el CRC de cabecera tras tocar un campo, para probar que el
// rechazo viene del CAMPO y no del CRC.
void fixHeaderCrc(std::vector<uint8_t> &b) {
  wr32(b, 28, trend_crc32(b.data(), 28));
}

// Anillo de 16 ranuras con 6 muestras (una sin dato en medio) y dos eventos.
Ring sampleRing() {
  Ring r(16);
  fill(r, BASE, BASE + 2);
  TrendSample gap = trend_empty_sample();
  trend_store_record(&r.st, BASE + 4, &gap);  // BASE+3 queda sin dato
  fill(r, BASE + 5, BASE + 5);
  trend_events_add(&r.ev, BASE + 1, 5, 2, true);
  trend_events_add(&r.ev, BASE + 4, 5, 2, false);
  return r;
}

void assertSameSlots(const TrendStore &a, const TrendStore &b, int64_t from,
                     int64_t to) {
  for (int64_t s = from; s <= to; s++)
    for (int ser = 0; ser < TREND_SERIES; ser++)
      TEST_ASSERT_EQUAL_INT16(trend_store_value(&a, ser, s),
                              trend_store_value(&b, ser, s));
}

}  // namespace

void test_crc32_matches_the_ieee_check_value(void) {
  const uint8_t v[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, trend_crc32(v, sizeof(v)));
  TEST_ASSERT_EQUAL_HEX32(0u, trend_crc32(v, 0));
}

void test_max_bytes_for_24h_is_104224(void) {
  TEST_ASSERT_EQUAL_UINT32(32u + 6u * 8640u * 2u + 64u * 8u,
                           (uint32_t)trend_file_max_bytes(8640));
  TEST_ASSERT_EQUAL_UINT32(104224u, (uint32_t)trend_file_max_bytes(8640));
}

void test_header_layout_is_the_documented_one(void) {
  Ring r(16);
  fill(r, BASE, BASE + 4);
  const std::vector<uint8_t> b = serializeOf(r);
  TEST_ASSERT_EQUAL_UINT32(32u + 6u * 5u * 2u, (uint32_t)b.size());
  TEST_ASSERT_EQUAL_UINT8('I', b[0]);
  TEST_ASSERT_EQUAL_UINT8('T', b[1]);
  TEST_ASSERT_EQUAL_UINT8('R', b[2]);
  TEST_ASSERT_EQUAL_UINT8('N', b[3]);
  TEST_ASSERT_EQUAL_UINT16(1, rd16(b, 4));
  TEST_ASSERT_EQUAL_UINT16(32, rd16(b, 6));
  TEST_ASSERT_EQUAL_UINT32(10, rd32(b, 8));
  TEST_ASSERT_EQUAL_UINT32(5, rd32(b, 12));
  TEST_ASSERT_EQUAL_UINT32((uint32_t)((BASE + 4) * 10), rd32(b, 16));
  TEST_ASSERT_EQUAL_UINT16(6, rd16(b, 20));
  TEST_ASSERT_EQUAL_UINT16(0, rd16(b, 22));
  TEST_ASSERT_EQUAL_HEX32(trend_crc32(b.data() + 32, b.size() - 32), rd32(b, 24));
  TEST_ASSERT_EQUAL_HEX32(trend_crc32(b.data(), 28), rd32(b, 28));
}

void test_series_are_planar_oldest_first_little_endian(void) {
  Ring r(16);
  fill(r, BASE, BASE + 2);  // 3 ranuras
  const std::vector<uint8_t> b = serializeOf(r);
  // Serie 0 (aire): 3 valores; serie 1 empieza tras ellos.
  for (int i = 0; i < 3; i++)
    TEST_ASSERT_EQUAL_INT16(val(BASE + i, 0), (int16_t)rd16(b, 32 + 2 * i));
  for (int i = 0; i < 3; i++)
    TEST_ASSERT_EQUAL_INT16(val(BASE + i, 1), (int16_t)rd16(b, 32 + 2 * 3 + 2 * i));
}

void test_serialize_refuses_nothing_to_save_or_no_room(void) {
  Ring empty(16);
  std::vector<uint8_t> buf(1024);
  TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)trend_file_serialize(&empty.st, &empty.ev,
                                                              buf.data(), buf.size()));
  Ring r(16);
  fill(r, BASE, BASE + 4);
  TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)trend_file_serialize(&r.st, &r.ev,
                                                              buf.data(), 40));
  TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)trend_file_serialize(&r.st, &r.ev, nullptr, 1024));
}

// Sin hora el modelo usa ranuras monotonas pequenas: serian epochs de 1970 y se
// restaurarian en el sitio equivocado. El volcado exige epoch real.
void test_serialize_refuses_slots_that_are_not_epochs(void) {
  Ring r(16);
  fill(r, 5, 9);
  r.st.lastSlot = -3;
  std::vector<uint8_t> buf(1024);
  TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)trend_file_serialize(&r.st, &r.ev,
                                                              buf.data(), buf.size()));
  Ring far(16);
  fill(far, 5, 9);
  far.st.lastSlot = (int64_t)5000000000LL;  // > u32 en segundos
  TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)trend_file_serialize(&far.st, &far.ev,
                                                              buf.data(), buf.size()));
}

void test_only_events_inside_the_stored_range_are_saved(void) {
  Ring r(16);
  fill(r, BASE, BASE + 4);
  trend_events_add(&r.ev, BASE - 50, 1, 1, true);   // anterior al anillo
  trend_events_add(&r.ev, BASE + 2, 2, 0, true);
  const std::vector<uint8_t> b = serializeOf(r);
  TEST_ASSERT_EQUAL_UINT16(1, rd16(b, 22));
}

void test_roundtrip_restores_every_slot_and_event(void) {
  Ring src = sampleRing();
  const std::vector<uint8_t> b = serializeOf(src);

  Ring dst(16);
  int restored = -1;
  TEST_ASSERT_EQUAL_INT(TREND_RESTORE_OK,
                        trend_file_restore(b.data(), b.size(), BASE + 8, &dst.st,
                                           &dst.ev, &restored));
  TEST_ASSERT_EQUAL_INT(6, restored);
  TEST_ASSERT_EQUAL_INT64(BASE + 5, dst.st.lastSlot);
  TEST_ASSERT_EQUAL_INT(6, dst.st.count);
  assertSameSlots(src.st, dst.st, BASE, BASE + 5);
  TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA, trend_store_value(&dst.st, TREND_AIR, BASE + 3));

  TEST_ASSERT_EQUAL_INT(2, dst.ev.count);
  TrendEvent e0, e1;
  TEST_ASSERT_TRUE(trend_events_get(&dst.ev, 0, &e0));
  TEST_ASSERT_TRUE(trend_events_get(&dst.ev, 1, &e1));
  TEST_ASSERT_EQUAL_INT64(BASE + 1, e0.slot);
  TEST_ASSERT_EQUAL_UINT8(5, e0.id);
  TEST_ASSERT_EQUAL_UINT8(2, e0.priority);
  TEST_ASSERT_TRUE(e0.raised);
  TEST_ASSERT_EQUAL_INT64(BASE + 4, e1.slot);
  TEST_ASSERT_FALSE(e1.raised);
}

void test_validate_reports_the_header_fields(void) {
  Ring src = sampleRing();
  const std::vector<uint8_t> b = serializeOf(src);
  TrendFileInfo info = {};
  TEST_ASSERT_EQUAL_INT(TREND_FILE_OK, trend_file_validate(b.data(), b.size(), &info));
  TEST_ASSERT_EQUAL_INT(6, info.count);
  TEST_ASSERT_EQUAL_INT64(BASE + 5, info.lastSlot);
  TEST_ASSERT_EQUAL_INT(2, info.eventCount);
}

void test_validate_rejects_bad_lengths(void) {
  Ring src = sampleRing();
  std::vector<uint8_t> b = serializeOf(src);
  TEST_ASSERT_EQUAL_INT(TREND_FILE_BAD_LENGTH, trend_file_validate(b.data(), 0, nullptr));
  TEST_ASSERT_EQUAL_INT(TREND_FILE_BAD_LENGTH, trend_file_validate(b.data(), 31, nullptr));
  TEST_ASSERT_EQUAL_INT(TREND_FILE_BAD_LENGTH,
                        trend_file_validate(b.data(), b.size() - 1, nullptr));
  b.push_back(0);
  TEST_ASSERT_EQUAL_INT(TREND_FILE_BAD_LENGTH,
                        trend_file_validate(b.data(), b.size(), nullptr));
}

void test_validate_rejects_bad_magic(void) {
  Ring src = sampleRing();
  std::vector<uint8_t> b = serializeOf(src);
  b[0] ^= 0xFF;
  TEST_ASSERT_EQUAL_INT(TREND_FILE_BAD_MAGIC, trend_file_validate(b.data(), b.size(), nullptr));
}

void test_validate_rejects_other_versions_even_with_a_valid_header_crc(void) {
  Ring src = sampleRing();
  std::vector<uint8_t> b = serializeOf(src);
  wr16(b, 4, 2);
  fixHeaderCrc(b);
  TEST_ASSERT_EQUAL_INT(TREND_FILE_BAD_VERSION, trend_file_validate(b.data(), b.size(), nullptr));
}

void test_validate_rejects_absurd_header_fields(void) {
  Ring src = sampleRing();
  const std::vector<uint8_t> good = serializeOf(src);
  struct Case {
    size_t off;
    int bytes;
    uint32_t value;
  };
  const Case cases[] = {
      {6, 2, 40},    // headerSize
      {8, 4, 5},     // slotSeconds
      {20, 2, 5},    // series
      {12, 4, 0},    // count 0
  };
  for (const Case &c : cases) {
    std::vector<uint8_t> b = good;
    if (c.bytes == 2) wr16(b, c.off, (uint16_t)c.value);
    else wr32(b, c.off, c.value);
    fixHeaderCrc(b);
    TEST_ASSERT_EQUAL_INT(TREND_FILE_BAD_HEADER,
                          trend_file_validate(b.data(), b.size(), nullptr));
  }
}

void test_validate_rejects_any_flipped_bit_in_header_or_payload(void) {
  Ring src = sampleRing();
  const std::vector<uint8_t> good = serializeOf(src);
  // Cualquier byte alterado (sin arreglar los CRC) tiene que ser rechazado.
  for (size_t i = 0; i < good.size(); i++) {
    std::vector<uint8_t> b = good;
    b[i] ^= 0x01;
    TEST_ASSERT_NOT_EQUAL_MESSAGE(TREND_FILE_OK,
                                  trend_file_validate(b.data(), b.size(), nullptr),
                                  "un byte alterado paso la validacion");
  }
}

void test_restore_of_a_corrupt_file_changes_nothing(void) {
  Ring src = sampleRing();
  std::vector<uint8_t> b = serializeOf(src);
  b[40] ^= 0x10;  // carga
  Ring dst(16);
  fill(dst, BASE + 100, BASE + 102, 77);
  trend_events_add(&dst.ev, BASE + 101, 9, 1, true);
  TEST_ASSERT_EQUAL_INT(TREND_RESTORE_INVALID,
                        trend_file_restore(b.data(), b.size(), BASE + 110, &dst.st,
                                           &dst.ev, nullptr));
  TEST_ASSERT_EQUAL_INT(3, dst.st.count);
  TEST_ASSERT_EQUAL_INT64(BASE + 102, dst.st.lastSlot);
  TEST_ASSERT_EQUAL_INT(1, dst.ev.count);
}

void test_a_file_from_the_future_is_rejected_with_a_one_slot_tolerance(void) {
  Ring src = sampleRing();  // ultima ranura BASE+5
  const std::vector<uint8_t> b = serializeOf(src);
  Ring dst(16);
  TEST_ASSERT_EQUAL_INT(TREND_RESTORE_FUTURE,
                        trend_file_restore(b.data(), b.size(), BASE + 3, &dst.st,
                                           &dst.ev, nullptr));
  TEST_ASSERT_EQUAL_INT(0, dst.st.count);
  TEST_ASSERT_EQUAL_INT(0, dst.ev.count);
  // now+1 == last: dentro de la tolerancia de redondeo.
  TEST_ASSERT_EQUAL_INT(TREND_RESTORE_OK,
                        trend_file_restore(b.data(), b.size(), BASE + 4, &dst.st,
                                           &dst.ev, nullptr));
}

void test_the_power_off_gap_is_filled_with_no_data(void) {
  Ring src(64);
  fill(src, 1000 + BASE, 1019 + BASE);
  const std::vector<uint8_t> b = serializeOf(src);

  // 30 min apagado = 180 ranuras... aqui 30 ranuras para caber en el anillo.
  Ring dst(64);
  fill(dst, 1050 + BASE, 1050 + BASE, 500);  // primera muestra viva
  TEST_ASSERT_EQUAL_INT(TREND_RESTORE_OK,
                        trend_file_restore(b.data(), b.size(), 1050 + BASE, &dst.st,
                                           &dst.ev, nullptr));
  TEST_ASSERT_EQUAL_INT(20 + 30 + 1, dst.st.count);
  TEST_ASSERT_EQUAL_INT64(1050 + BASE, dst.st.lastSlot);
  for (int64_t s = 1020; s <= 1049; s++)
    for (int ser = 0; ser < TREND_SERIES; ser++)
      TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA, trend_store_value(&dst.st, ser, s + BASE));
  assertSameSlots(src.st, dst.st, 1000 + BASE, 1019 + BASE);
  TEST_ASSERT_EQUAL_INT16(val(1050 + BASE, TREND_AIR, 500),
                          trend_store_value(&dst.st, TREND_AIR, 1050 + BASE));
}

void test_live_samples_win_where_they_overlap_the_file(void) {
  Ring src(64);
  fill(src, 1000 + BASE, 1019 + BASE);
  const std::vector<uint8_t> b = serializeOf(src);

  Ring dst(64);
  fill(dst, 1015 + BASE, 1030 + BASE, 900);  // la sesion actual ya cubre 1015..1030
  TEST_ASSERT_EQUAL_INT(TREND_RESTORE_OK,
                        trend_file_restore(b.data(), b.size(), 1030 + BASE, &dst.st,
                                           &dst.ev, nullptr));
  TEST_ASSERT_EQUAL_INT(31, dst.st.count);  // 1000..1030
  for (int64_t s = 1000; s <= 1014; s++)
    TEST_ASSERT_EQUAL_INT16(val(s + BASE, TREND_AIR, 0),
                            trend_store_value(&dst.st, TREND_AIR, s + BASE));
  for (int64_t s = 1015; s <= 1030; s++)
    TEST_ASSERT_EQUAL_INT16(val(s + BASE, TREND_AIR, 900),
                            trend_store_value(&dst.st, TREND_AIR, s + BASE));
}

void test_what_does_not_fit_is_dropped_from_the_old_side(void) {
  Ring src(32);
  fill(src, BASE, BASE + 19);  // 20 ranuras
  const std::vector<uint8_t> b = serializeOf(src);

  Ring dst(8);
  int restored = 0;
  TEST_ASSERT_EQUAL_INT(TREND_RESTORE_OK,
                        trend_file_restore(b.data(), b.size(), BASE + 19, &dst.st,
                                           &dst.ev, &restored));
  TEST_ASSERT_EQUAL_INT(8, restored);
  TEST_ASSERT_EQUAL_INT(8, dst.st.count);
  TEST_ASSERT_EQUAL_INT64(BASE + 12, trend_store_first_slot(&dst.st));
  assertSameSlots(src.st, dst.st, BASE + 12, BASE + 19);
  TEST_ASSERT_EQUAL_INT16(TREND_NO_DATA, trend_store_value(&dst.st, TREND_AIR, BASE + 11));
}

void test_a_file_that_no_longer_falls_in_the_ring_is_stale(void) {
  Ring src(64);
  fill(src, BASE, BASE + 9);
  const std::vector<uint8_t> b = serializeOf(src);

  // now - last = 63 < capacidad: todavia cae en el anillo.
  Ring ok(64);
  TEST_ASSERT_EQUAL_INT(TREND_RESTORE_OK,
                        trend_file_restore(b.data(), b.size(), BASE + 9 + 63, &ok.st,
                                           &ok.ev, nullptr));
  // now - last = 64: ya no.
  Ring stale(64);
  TEST_ASSERT_EQUAL_INT(TREND_RESTORE_STALE,
                        trend_file_restore(b.data(), b.size(), BASE + 9 + 64, &stale.st,
                                           &stale.ev, nullptr));
  TEST_ASSERT_EQUAL_INT(0, stale.st.count);
}

void test_a_gap_too_big_for_the_free_room_is_stale_too(void) {
  Ring src(64);
  fill(src, BASE, BASE + 9);
  const std::vector<uint8_t> b = serializeOf(src);
  Ring dst(64);
  fill(dst, BASE + 5000, BASE + 5002);
  TEST_ASSERT_EQUAL_INT(TREND_RESTORE_STALE,
                        trend_file_restore(b.data(), b.size(), BASE + 5002, &dst.st,
                                           &dst.ev, nullptr));
  TEST_ASSERT_EQUAL_INT(3, dst.st.count);
}

void test_file_events_go_before_the_live_ones_without_duplicating(void) {
  Ring src(64);
  fill(src, BASE, BASE + 19);
  trend_events_add(&src.ev, BASE + 3, 5, 2, true);
  trend_events_add(&src.ev, BASE + 18, 6, 1, true);  // quedara solapado con uno vivo
  const std::vector<uint8_t> b = serializeOf(src);

  Ring dst(64);
  fill(dst, BASE + 15, BASE + 25, 900);
  trend_events_add(&dst.ev, BASE + 16, 6, 1, true);  // el mismo flanco, ya vivo
  TEST_ASSERT_EQUAL_INT(TREND_RESTORE_OK,
                        trend_file_restore(b.data(), b.size(), BASE + 25, &dst.st,
                                           &dst.ev, nullptr));
  TEST_ASSERT_EQUAL_INT(2, dst.ev.count);
  TrendEvent a, c;
  TEST_ASSERT_TRUE(trend_events_get(&dst.ev, 0, &a));
  TEST_ASSERT_TRUE(trend_events_get(&dst.ev, 1, &c));
  TEST_ASSERT_EQUAL_INT64(BASE + 3, a.slot);
  TEST_ASSERT_EQUAL_INT64(BASE + 16, c.slot);
}

void test_events_older_than_the_restored_range_are_dropped(void) {
  Ring src(32);
  fill(src, BASE, BASE + 19);
  trend_events_add(&src.ev, BASE + 2, 1, 0, true);    // fuera de las 8 que caben
  trend_events_add(&src.ev, BASE + 15, 2, 2, true);   // dentro
  const std::vector<uint8_t> b = serializeOf(src);
  Ring dst(8);
  TEST_ASSERT_EQUAL_INT(TREND_RESTORE_OK,
                        trend_file_restore(b.data(), b.size(), BASE + 19, &dst.st,
                                           &dst.ev, nullptr));
  TEST_ASSERT_EQUAL_INT(1, dst.ev.count);
  TrendEvent e;
  TEST_ASSERT_TRUE(trend_events_get(&dst.ev, 0, &e));
  TEST_ASSERT_EQUAL_INT64(BASE + 15, e.slot);
}

void test_prepending_events_respects_the_ring_capacity(void) {
  Ring src(32);
  fill(src, BASE, BASE + 19);
  for (int i = 0; i < 10; i++)
    trend_events_add(&src.ev, BASE + i, (uint8_t)(10 + i), 1, true);
  const std::vector<uint8_t> b = serializeOf(src);

  Ring dst(64);
  fill(dst, BASE + 40, BASE + 45, 900);
  for (int i = 0; i < 60; i++)
    trend_events_add(&dst.ev, BASE + 40 + (i % 5), (uint8_t)(50 + i), 1, true);
  TEST_ASSERT_EQUAL_INT(TREND_RESTORE_OK,
                        trend_file_restore(b.data(), b.size(), BASE + 45, &dst.st,
                                           &dst.ev, nullptr));
  TEST_ASSERT_EQUAL_INT(TREND_MAX_EVENTS, dst.ev.count);
  // Los 60 vivos se conservan; de los del fichero entran los 4 mas recientes.
  TrendEvent first, last;
  TEST_ASSERT_TRUE(trend_events_get(&dst.ev, 0, &first));
  TEST_ASSERT_TRUE(trend_events_get(&dst.ev, TREND_MAX_EVENTS - 1, &last));
  TEST_ASSERT_EQUAL_INT64(BASE + 6, first.slot);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)(50 + 59), last.id);
}

void test_a_full_24h_ring_roundtrips_within_the_max_size(void) {
  Ring src(TREND_CAPACITY);
  // Un dia entero con un hueco de 2 h.
  fill(src, BASE, BASE + 3000);
  TrendSample gap = trend_empty_sample();
  trend_store_record(&src.st, BASE + 3000 + 721, &gap);
  fill(src, BASE + 3000 + 722, BASE + 8639);
  const std::vector<uint8_t> b = serializeOf(src);
  TEST_ASSERT_TRUE(b.size() <= trend_file_max_bytes(TREND_CAPACITY));

  Ring dst(TREND_CAPACITY);
  TEST_ASSERT_EQUAL_INT(TREND_RESTORE_OK,
                        trend_file_restore(b.data(), b.size(), BASE + 8639, &dst.st,
                                           &dst.ev, nullptr));
  TEST_ASSERT_EQUAL_INT(src.st.count, dst.st.count);
  assertSameSlots(src.st, dst.st, trend_store_first_slot(&src.st), BASE + 8639);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_crc32_matches_the_ieee_check_value);
  RUN_TEST(test_max_bytes_for_24h_is_104224);
  RUN_TEST(test_header_layout_is_the_documented_one);
  RUN_TEST(test_series_are_planar_oldest_first_little_endian);
  RUN_TEST(test_serialize_refuses_nothing_to_save_or_no_room);
  RUN_TEST(test_serialize_refuses_slots_that_are_not_epochs);
  RUN_TEST(test_only_events_inside_the_stored_range_are_saved);
  RUN_TEST(test_roundtrip_restores_every_slot_and_event);
  RUN_TEST(test_validate_reports_the_header_fields);
  RUN_TEST(test_validate_rejects_bad_lengths);
  RUN_TEST(test_validate_rejects_bad_magic);
  RUN_TEST(test_validate_rejects_other_versions_even_with_a_valid_header_crc);
  RUN_TEST(test_validate_rejects_absurd_header_fields);
  RUN_TEST(test_validate_rejects_any_flipped_bit_in_header_or_payload);
  RUN_TEST(test_restore_of_a_corrupt_file_changes_nothing);
  RUN_TEST(test_a_file_from_the_future_is_rejected_with_a_one_slot_tolerance);
  RUN_TEST(test_the_power_off_gap_is_filled_with_no_data);
  RUN_TEST(test_live_samples_win_where_they_overlap_the_file);
  RUN_TEST(test_what_does_not_fit_is_dropped_from_the_old_side);
  RUN_TEST(test_a_file_that_no_longer_falls_in_the_ring_is_stale);
  RUN_TEST(test_a_gap_too_big_for_the_free_room_is_stale_too);
  RUN_TEST(test_file_events_go_before_the_live_ones_without_duplicating);
  RUN_TEST(test_events_older_than_the_restored_range_are_dropped);
  RUN_TEST(test_prepending_events_respects_the_ring_capacity);
  RUN_TEST(test_a_full_24h_ring_roundtrips_within_the_max_size);
  return UNITY_END();
}
