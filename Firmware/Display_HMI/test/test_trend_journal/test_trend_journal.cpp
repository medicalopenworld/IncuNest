#include <unity.h>

#include <vector>

#include "modules/trend/trend_file.h"
#include "modules/trend/trend_journal.h"

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

TrendSample sampleAt(int64_t slot) {
  TrendSample s;
  for (int i = 0; i < TREND_SERIES; i++) s.v[i] = (int16_t)(1000 * i + (slot % 500));
  return s;
}

void fill(Ring &r, int64_t from, int64_t to) {
  for (int64_t s = from; s <= to; s++) {
    const TrendSample x = sampleAt(s);
    TEST_ASSERT_TRUE(trend_store_record(&r.st, s, &x));
  }
}

std::vector<uint8_t> build(const Ring &r, int64_t after) {
  std::vector<uint8_t> buf((size_t)trend_jnl_pending_records(&r.st, &r.ev, after) *
                           TREND_JNL_REC_BYTES);
  if (buf.empty()) return buf;
  const size_t n = trend_jnl_build(&r.st, &r.ev, after, buf.data(), buf.size());
  TEST_ASSERT_EQUAL_UINT32(buf.size(), n);
  return buf;
}

// Aplica de una vez (pasadas de 7 registros, para ejercitar el troceo).
TrendJnlApply applyAll(Ring &r, const std::vector<uint8_t> &j, int64_t base) {
  TrendJnlApply a;
  trend_jnl_apply_begin(&a, &r.st, &r.ev, j.data(), j.size(), base);
  int guard = 0;
  while (trend_jnl_apply_step(&a, 7) == TREND_JOB_MORE) TEST_ASSERT_TRUE(++guard < 10000);
  return a;
}

void assertSameRing(const Ring &a, const Ring &b) {
  TEST_ASSERT_EQUAL_INT(a.st.count, b.st.count);
  TEST_ASSERT_EQUAL_INT64(a.st.lastSlot, b.st.lastSlot);
  for (int64_t s = trend_store_first_slot(&a.st); s <= a.st.lastSlot; s++)
    for (int i = 0; i < TREND_SERIES; i++)
      TEST_ASSERT_EQUAL_INT16(trend_store_value(&a.st, i, s), trend_store_value(&b.st, i, s));
  TEST_ASSERT_EQUAL_INT(a.ev.count, b.ev.count);
  for (int i = 0; i < a.ev.count; i++) {
    TrendEvent x, y;
    trend_events_get(&a.ev, i, &x);
    trend_events_get(&b.ev, i, &y);
    TEST_ASSERT_EQUAL_INT64(x.slot, y.slot);
    TEST_ASSERT_EQUAL_UINT8(x.id, y.id);
    TEST_ASSERT_EQUAL_UINT8(x.priority, y.priority);
    TEST_ASSERT_EQUAL(x.raised, y.raised);
  }
}

}  // namespace

// Lo que se guarda en una instantanea + el diario reconstruye el anillo vivo.
void test_snapshot_plus_journal_rebuilds_live_ring(void) {
  Ring live(500);
  fill(live, BASE, BASE + 99);
  trend_events_add(&live.ev, BASE + 50, 3, 2, true);
  const int64_t snapLast = live.st.lastSlot;

  // "Disco": copia del anillo en el momento de la instantanea.
  Ring disk(500);
  fill(disk, BASE, BASE + 99);
  trend_events_add(&disk.ev, BASE + 50, 3, 2, true);

  // Mas tarde: ranuras nuevas, un hueco sin dato y dos flancos.
  fill(live, BASE + 100, BASE + 120);
  fill(live, BASE + 125, BASE + 129);
  trend_events_add(&live.ev, BASE + 110, 3, 2, false);
  trend_events_add(&live.ev, BASE + 126, 7, 0, true);

  const std::vector<uint8_t> j = build(live, snapLast);
  const TrendJnlApply a = applyAll(disk, j, snapLast);
  TEST_ASSERT_FALSE(a.broken);
  assertSameRing(live, disk);
}

void test_pending_counts_samples_and_events_after_slot(void) {
  Ring r(100);
  fill(r, BASE, BASE + 9);
  trend_events_add(&r.ev, BASE + 2, 1, 1, true);
  trend_events_add(&r.ev, BASE + 8, 1, 1, false);
  TEST_ASSERT_EQUAL_INT(4 + 1, trend_jnl_pending_records(&r.st, &r.ev, BASE + 5));
  TEST_ASSERT_EQUAL_INT(0, trend_jnl_pending_records(&r.st, &r.ev, BASE + 9));
  // Sin nada nuevo no se escribe nada.
  uint8_t buf[TREND_JNL_REC_BYTES * 4];
  TEST_ASSERT_EQUAL_UINT32(0, trend_jnl_build(&r.st, &r.ev, BASE + 9, buf, sizeof buf));
}

void test_build_refuses_when_it_does_not_fit(void) {
  Ring r(100);
  fill(r, BASE, BASE + 9);
  uint8_t buf[TREND_JNL_REC_BYTES * 3];
  TEST_ASSERT_EQUAL_UINT32(0, trend_jnl_build(&r.st, &r.ev, BASE + 5, buf, sizeof buf));
}

// Un registro roto corta el diario: lo anterior entra, lo posterior no.
void test_corrupt_record_truncates_the_journal(void) {
  Ring live(100);
  fill(live, BASE, BASE + 9);
  Ring disk(100);
  fill(disk, BASE, BASE + 4);
  std::vector<uint8_t> j = build(live, BASE + 4);  // 5 registros
  j[2 * TREND_JNL_REC_BYTES + 9] ^= 0x01;           // el tercero, roto
  const TrendJnlApply a = applyAll(disk, j, BASE + 4);
  TEST_ASSERT_TRUE(a.broken);
  TEST_ASSERT_EQUAL_INT(2, a.applied);
  TEST_ASSERT_EQUAL_INT64(BASE + 6, disk.st.lastSlot);
}

// Un fichero que no es multiplo de 24 (corte a mitad) tambien corta.
void test_partial_tail_is_ignored(void) {
  Ring live(100);
  fill(live, BASE, BASE + 9);
  Ring disk(100);
  fill(disk, BASE, BASE + 4);
  std::vector<uint8_t> j = build(live, BASE + 4);
  j.resize(j.size() - 5);
  const TrendJnlApply a = applyAll(disk, j, BASE + 4);
  TEST_ASSERT_TRUE(a.broken);
  TEST_ASSERT_EQUAL_INT64(BASE + 8, disk.st.lastSlot);
}

// Diario que sobrevivio a una compactacion (corte entre el renombrado de la
// instantanea y el borrado del diario): sus registros ya estan en la
// instantanea y no se duplican.
void test_stale_journal_after_compaction_changes_nothing(void) {
  Ring live(100);
  fill(live, BASE, BASE + 19);
  trend_events_add(&live.ev, BASE + 15, 4, 1, true);
  const std::vector<uint8_t> j = build(live, BASE + 9);

  Ring disk(100);  // instantanea nueva: ya lo contiene todo
  fill(disk, BASE, BASE + 19);
  trend_events_add(&disk.ev, BASE + 15, 4, 1, true);
  const TrendJnlApply a = applyAll(disk, j, BASE + 19);
  TEST_ASSERT_FALSE(a.broken);
  assertSameRing(live, disk);
}

void test_last_sample_slot_skips_broken_tail_and_events(void) {
  Ring r(100);
  fill(r, BASE, BASE + 9);
  trend_events_add(&r.ev, BASE + 9, 2, 2, true);
  std::vector<uint8_t> j = build(r, BASE + 4);  // muestras 5..9 + 1 flanco
  int64_t slot = 0;
  TEST_ASSERT_TRUE(trend_jnl_last_sample_slot(j.data(), j.size(), &slot));
  TEST_ASSERT_EQUAL_INT64(BASE + 9, slot);
  // Ultima muestra rota: cuenta la anterior.
  j[4 * TREND_JNL_REC_BYTES] ^= 0x40;
  TEST_ASSERT_TRUE(trend_jnl_last_sample_slot(j.data(), j.size(), &slot));
  TEST_ASSERT_EQUAL_INT64(BASE + 8, slot);
  TEST_ASSERT_FALSE(trend_jnl_last_sample_slot(j.data(), 0, &slot));
}

// Una ranura que no es epoch (reloj sin sincronizar) no se escribe.
void test_build_rejects_non_epoch_slots(void) {
  Ring r(100);
  fill(r, -5, 4);
  uint8_t buf[TREND_JNL_REC_BYTES * 16];
  TEST_ASSERT_EQUAL_UINT32(0, trend_jnl_build(&r.st, &r.ev, -6, buf, sizeof buf));
}

// El limite de un anadido cabe en un bloque de LittleFS.
void test_append_limit_fits_one_block(void) {
  TEST_ASSERT_TRUE(TREND_JNL_APPEND_MAX_RECORDS * TREND_JNL_REC_BYTES <= 4096);
}

// Instantanea + diario serializados como fichero: lo que restaura el arranque.
void test_merged_image_restores_like_live(void) {
  Ring live(300);
  fill(live, BASE, BASE + 199);
  Ring disk(300);
  fill(disk, BASE, BASE + 149);
  const std::vector<uint8_t> j = build(live, BASE + 149);
  applyAll(disk, j, BASE + 149);
  std::vector<uint8_t> img(trend_file_max_bytes(300));
  const size_t n = trend_file_serialize(&disk.st, &disk.ev, img.data(), img.size());
  TEST_ASSERT_TRUE(n > 0);
  Ring boot(300);
  int restored = 0;
  TEST_ASSERT_EQUAL_INT(TREND_RESTORE_OK,
                        trend_file_restore(img.data(), n, BASE + 200, &boot.st, &boot.ev,
                                           &restored));
  TEST_ASSERT_EQUAL_INT(200, restored);
  assertSameRing(live, boot);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_snapshot_plus_journal_rebuilds_live_ring);
  RUN_TEST(test_pending_counts_samples_and_events_after_slot);
  RUN_TEST(test_build_refuses_when_it_does_not_fit);
  RUN_TEST(test_corrupt_record_truncates_the_journal);
  RUN_TEST(test_partial_tail_is_ignored);
  RUN_TEST(test_stale_journal_after_compaction_changes_nothing);
  RUN_TEST(test_last_sample_slot_skips_broken_tail_and_events);
  RUN_TEST(test_build_rejects_non_epoch_slots);
  RUN_TEST(test_append_limit_fits_one_block);
  RUN_TEST(test_merged_image_restores_like_live);
  return UNITY_END();
}
