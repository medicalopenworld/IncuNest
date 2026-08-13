#include <string.h>
#include <unity.h>

#include "modules/baby_profile/baby_profile_core.h"

void setUp(void) {}
void tearDown(void) {}

// ---------------- Weight-point dedup ----------------

void test_first_weight_point_appends(void) {
  TEST_ASSERT_TRUE(baby_weight_should_append(false, 0, 1200));
}

void test_changed_weight_appends(void) {
  TEST_ASSERT_TRUE(baby_weight_should_append(true, 1200, 1250));
}

void test_unchanged_weight_does_not_append(void) {
  TEST_ASSERT_FALSE(baby_weight_should_append(true, 1200, 1200));
}

void test_weight_point_roundtrip(void) {
  uint8_t buf[BABY_WEIGHT_POINT_SIZE];
  baby_weight_point_encode(1723200000u, 1450, buf);
  uint32_t ts = 0;
  uint16_t w = 0;
  baby_weight_point_decode(buf, &ts, &w);
  TEST_ASSERT_EQUAL_UINT32(1723200000u, ts);
  TEST_ASSERT_EQUAL_UINT16(1450, w);
}

// ---------------- Archive byte budget ----------------

void test_archive_total_bytes(void) {
  ArchiveEntry e[] = {{10, 600}, {11, 1200}, {12, 60}};
  TEST_ASSERT_EQUAL_UINT32(1860, baby_archive_total_bytes(e, 3));
  TEST_ASSERT_EQUAL_UINT32(0, baby_archive_total_bytes(e, 0));
}

void test_archive_pick_eviction_lowest_seq(void) {
  ArchiveEntry e[] = {{20, 600}, {11, 1200}, {32, 60}};
  TEST_ASSERT_EQUAL_INT(1, baby_archive_pick_eviction(e, 3));
  TEST_ASSERT_EQUAL_INT(-1, baby_archive_pick_eviction(e, 0));
}

void test_archive_over_budget(void) {
  TEST_ASSERT_FALSE(baby_archive_over_budget(1000, 24, 1024));
  TEST_ASSERT_TRUE(baby_archive_over_budget(1000, 25, 1024));
}

// ---------------- Unified eviction decisions ----------------

void test_no_eviction_when_both_stores_have_room(void) {
  TEST_ASSERT_EQUAL(BABY_EVICT_NONE,
                    baby_unified_eviction_check(10, BABY_HISTORY_CAP, 1000,
                                                600, 600u * 1024u));
}

void test_audit_cap_first_triggers_paired_eviction(void) {
  TEST_ASSERT_EQUAL(BABY_EVICT_AUDIT_CAP,
                    baby_unified_eviction_check(BABY_HISTORY_CAP,
                                                BABY_HISTORY_CAP, 1000, 600,
                                                600u * 1024u));
}

void test_weight_budget_first_triggers_tombstone_eviction(void) {
  // Audit log NOT yet full, but the incoming archive file busts the budget.
  TEST_ASSERT_EQUAL(BABY_EVICT_WEIGHT_BUDGET,
                    baby_unified_eviction_check(500, BABY_HISTORY_CAP,
                                                600u * 1024u - 100, 600,
                                                600u * 1024u));
}

void test_weight_budget_checked_before_audit_cap(void) {
  // Both would trigger; the budget branch must win so the byte pressure is
  // resolved first (audit overwrite then happens naturally on append).
  TEST_ASSERT_EQUAL(BABY_EVICT_WEIGHT_BUDGET,
                    baby_unified_eviction_check(BABY_HISTORY_CAP,
                                                BABY_HISTORY_CAP,
                                                600u * 1024u, 600,
                                                600u * 1024u));
}

// ---------------- Downsampling (design decision 11) ----------------

void test_downsample_passthrough_at_or_under_max(void) {
  uint32_t idx[BABY_WEIGHT_HISTORY_MAX_OUT];
  uint32_t n = baby_downsample_indices(50, BABY_WEIGHT_HISTORY_MAX_OUT, idx);
  TEST_ASSERT_EQUAL_UINT32(50, n);
  for (uint32_t i = 0; i < n; i++) TEST_ASSERT_EQUAL_UINT32(i, idx[i]);

  n = baby_downsample_indices(3, BABY_WEIGHT_HISTORY_MAX_OUT, idx);
  TEST_ASSERT_EQUAL_UINT32(3, n);
  TEST_ASSERT_EQUAL_UINT32(2, idx[2]);
}

void test_downsample_reduces_to_max_and_keeps_endpoints(void) {
  uint32_t idx[BABY_WEIGHT_HISTORY_MAX_OUT];
  uint32_t n =
      baby_downsample_indices(1000, BABY_WEIGHT_HISTORY_MAX_OUT, idx);
  TEST_ASSERT_EQUAL_UINT32(BABY_WEIGHT_HISTORY_MAX_OUT, n);
  TEST_ASSERT_EQUAL_UINT32(0, idx[0]);
  TEST_ASSERT_EQUAL_UINT32(999, idx[n - 1]);
  // Strictly increasing (no duplicates).
  for (uint32_t i = 1; i < n; i++) TEST_ASSERT_TRUE(idx[i] > idx[i - 1]);
}

void test_downsample_deterministic(void) {
  uint32_t a[BABY_WEIGHT_HISTORY_MAX_OUT];
  uint32_t b[BABY_WEIGHT_HISTORY_MAX_OUT];
  uint32_t na = baby_downsample_indices(777, BABY_WEIGHT_HISTORY_MAX_OUT, a);
  uint32_t nb = baby_downsample_indices(777, BABY_WEIGHT_HISTORY_MAX_OUT, b);
  TEST_ASSERT_EQUAL_UINT32(na, nb);
  for (uint32_t i = 0; i < na; i++) TEST_ASSERT_EQUAL_UINT32(a[i], b[i]);
}

void test_downsample_zero_points(void) {
  uint32_t idx[BABY_WEIGHT_HISTORY_MAX_OUT];
  TEST_ASSERT_EQUAL_UINT32(
      0, baby_downsample_indices(0, BABY_WEIGHT_HISTORY_MAX_OUT, idx));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_first_weight_point_appends);
  RUN_TEST(test_changed_weight_appends);
  RUN_TEST(test_unchanged_weight_does_not_append);
  RUN_TEST(test_weight_point_roundtrip);
  RUN_TEST(test_archive_total_bytes);
  RUN_TEST(test_archive_pick_eviction_lowest_seq);
  RUN_TEST(test_archive_over_budget);
  RUN_TEST(test_no_eviction_when_both_stores_have_room);
  RUN_TEST(test_audit_cap_first_triggers_paired_eviction);
  RUN_TEST(test_weight_budget_first_triggers_tombstone_eviction);
  RUN_TEST(test_weight_budget_checked_before_audit_cap);
  RUN_TEST(test_downsample_passthrough_at_or_under_max);
  RUN_TEST(test_downsample_reduces_to_max_and_keeps_endpoints);
  RUN_TEST(test_downsample_deterministic);
  RUN_TEST(test_downsample_zero_points);
  return UNITY_END();
}
