#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "modules/baby_profile/baby_profile_core.h"

void setUp(void) {}
void tearDown(void) {}

static BabyProfile mkProfile(uint32_t seq, const char *name, uint8_t gest,
                             uint16_t weight, uint32_t admission) {
  BabyProfile p;
  memset(&p, 0, sizeof(p));
  p.slotUsed = true;
  p.seq = seq;
  snprintf(p.name, BABY_NAME_LEN, "%s", name);
  p.gestWeeks = gest;
  p.weightGrams = weight;
  p.admissionEpoch = admission;
  p.dischargeEpoch = 0;
  p.outcome = BABY_OUTCOME_UNKNOWN;
  return p;
}

// ---------------- Slot selection ----------------

void test_find_free_slot_returns_first_free(void) {
  BabyProfile slots[BABY_ACTIVE_SLOTS];
  memset(slots, 0, sizeof(slots));
  slots[0] = mkProfile(1, "A", 30, 1200, 0);
  TEST_ASSERT_EQUAL_INT(1, baby_find_free_slot(slots));
}

void test_find_free_slot_returns_minus_one_when_full(void) {
  BabyProfile slots[BABY_ACTIVE_SLOTS] = {
      mkProfile(1, "A", 30, 1200, 0),
      mkProfile(2, "B", 32, 1500, 0),
      mkProfile(3, "C", 28, 900, 0),
  };
  TEST_ASSERT_EQUAL_INT(-1, baby_find_free_slot(slots));
}

void test_find_slot_by_seq(void) {
  BabyProfile slots[BABY_ACTIVE_SLOTS] = {
      mkProfile(7, "A", 30, 1200, 0),
      mkProfile(9, "B", 32, 1500, 0),
      mkProfile(8, "C", 28, 900, 0),
  };
  TEST_ASSERT_EQUAL_INT(1, baby_find_slot_by_seq(slots, 9));
  TEST_ASSERT_EQUAL_INT(-1, baby_find_slot_by_seq(slots, 99));
}

// ---------------- FIFO eviction with activeSeq protection ----------------

void test_eviction_picks_lowest_seq(void) {
  BabyProfile slots[BABY_ACTIVE_SLOTS] = {
      mkProfile(5, "A", 30, 1200, 0),
      mkProfile(3, "B", 32, 1500, 0),
      mkProfile(9, "C", 28, 900, 0),
  };
  TEST_ASSERT_EQUAL_INT(1, baby_pick_eviction_slot(slots, 0));
}

void test_eviction_skips_active_seq_even_if_lowest(void) {
  BabyProfile slots[BABY_ACTIVE_SLOTS] = {
      mkProfile(5, "A", 30, 1200, 0),
      mkProfile(3, "B", 32, 1500, 0),
      mkProfile(9, "C", 28, 900, 0),
  };
  // seq=3 is currently driving control -> next lowest (5) is evicted.
  TEST_ASSERT_EQUAL_INT(0, baby_pick_eviction_slot(slots, 3));
}

void test_eviction_rejects_when_no_eligible_slot(void) {
  BabyProfile slots[BABY_ACTIVE_SLOTS];
  memset(slots, 0, sizeof(slots));
  slots[0] = mkProfile(4, "A", 30, 1200, 0);
  // Only one used slot and it is the active one -> nothing eligible.
  TEST_ASSERT_EQUAL_INT(-1, baby_pick_eviction_slot(slots, 4));
}

// ---------------- Age derivation ----------------

void test_age_derivation_from_admission_epoch(void) {
  uint16_t days = 0xFFFF;
  // 9 full days later.
  TEST_ASSERT_TRUE(baby_derive_age_days(1000000u, 1000000u + 9u * 86400u,
                                        true, &days));
  TEST_ASSERT_EQUAL_UINT16(9, days);
}

void test_age_unknown_when_no_synced_time(void) {
  uint16_t days = 0xFFFF;
  TEST_ASSERT_FALSE(baby_derive_age_days(1000000u, 2000000u, false, &days));
}

void test_age_unknown_when_admission_epoch_zero(void) {
  uint16_t days = 0xFFFF;
  TEST_ASSERT_FALSE(baby_derive_age_days(0u, 2000000u, true, &days));
}

void test_age_unknown_when_clock_behind_admission(void) {
  uint16_t days = 0xFFFF;
  TEST_ASSERT_FALSE(baby_derive_age_days(2000000u, 1000000u, true, &days));
}

// ---------------- Audit record encode/decode ----------------

void test_history_record_roundtrip_including_discharge_outcome(void) {
  BabyProfile p = mkProfile(42, "Garcia", 31, 1450, 1723200000u);
  p.dischargeEpoch = 1723900000u;
  p.outcome = BABY_OUTCOME_SURVIVED;

  uint8_t rec[BABY_HISTORY_RECORD_SIZE];
  baby_history_encode(&p, true, rec);

  BabyProfile out;
  memset(&out, 0, sizeof(out));
  bool valid = baby_history_decode(rec, &out);

  TEST_ASSERT_TRUE(valid);
  TEST_ASSERT_EQUAL_UINT32(42, out.seq);
  TEST_ASSERT_EQUAL_STRING("Garcia", out.name);
  TEST_ASSERT_EQUAL_UINT8(31, out.gestWeeks);
  TEST_ASSERT_EQUAL_UINT16(1450, out.weightGrams);
  TEST_ASSERT_EQUAL_UINT32(1723200000u, out.admissionEpoch);
  TEST_ASSERT_EQUAL_UINT32(1723900000u, out.dischargeEpoch);
  TEST_ASSERT_EQUAL_UINT8(BABY_OUTCOME_SURVIVED, out.outcome);
}

void test_history_record_defaults_unknown_outcome(void) {
  BabyProfile p = mkProfile(7, "X", 29, 0, 0);
  uint8_t rec[BABY_HISTORY_RECORD_SIZE];
  baby_history_encode(&p, true, rec);
  BabyProfile out;
  baby_history_decode(rec, &out);
  TEST_ASSERT_EQUAL_UINT32(0, out.dischargeEpoch);
  TEST_ASSERT_EQUAL_UINT8(BABY_OUTCOME_UNKNOWN, out.outcome);
}

void test_history_record_roundtrips_kangaroo_fields(void) {
  BabyProfile p = mkProfile(11, "Kanga", 30, 1300, 1723200000u);
  p.kangarooCount = 7;
  p.lastKangarooEpoch = 1723999999u;

  uint8_t rec[BABY_HISTORY_RECORD_SIZE];
  baby_history_encode(&p, true, rec);
  BabyProfile out;
  memset(&out, 0, sizeof(out));
  TEST_ASSERT_TRUE(baby_history_decode(rec, &out));
  TEST_ASSERT_EQUAL_UINT16(7, out.kangarooCount);
  TEST_ASSERT_EQUAL_UINT32(1723999999u, out.lastKangarooEpoch);
  // The pre-existing fields must survive the layout growth unshifted.
  TEST_ASSERT_EQUAL_UINT32(11, out.seq);
  TEST_ASSERT_EQUAL_STRING("Kanga", out.name);
  TEST_ASSERT_EQUAL_UINT8(30, out.gestWeeks);
  TEST_ASSERT_EQUAL_UINT16(1300, out.weightGrams);
  TEST_ASSERT_EQUAL_UINT32(1723200000u, out.admissionEpoch);
}

void test_new_profile_has_no_kangaroo_events(void) {
  BabyProfile p = mkProfile(12, "New", 33, 0, 0);
  uint8_t rec[BABY_HISTORY_RECORD_SIZE];
  baby_history_encode(&p, true, rec);
  BabyProfile out;
  baby_history_decode(rec, &out);
  TEST_ASSERT_EQUAL_UINT16(0, out.kangarooCount);
  TEST_ASSERT_EQUAL_UINT32(0, out.lastKangarooEpoch);
}

void test_history_record_tombstone_flag(void) {
  BabyProfile p = mkProfile(7, "X", 29, 0, 0);
  uint8_t rec[BABY_HISTORY_RECORD_SIZE];
  baby_history_encode(&p, false, rec);
  BabyProfile out;
  TEST_ASSERT_FALSE(baby_history_decode(rec, &out));
}

// ---------------- Circular cursor ----------------

void test_circular_cursor_advances_and_saturates(void) {
  CircularCursor c = {0, 0};
  TEST_ASSERT_EQUAL_UINT32(0, circ_advance(&c, 4));
  TEST_ASSERT_EQUAL_UINT32(1, circ_advance(&c, 4));
  TEST_ASSERT_EQUAL_UINT32(2, circ_advance(&c, 4));
  TEST_ASSERT_EQUAL_UINT32(3, circ_advance(&c, 4));
  TEST_ASSERT_EQUAL_UINT32(4, c.count);
  // Wraparound: 5th write overwrites slot 0, count stays saturated.
  TEST_ASSERT_EQUAL_UINT32(0, circ_advance(&c, 4));
  TEST_ASSERT_EQUAL_UINT32(4, c.count);
  TEST_ASSERT_EQUAL_UINT32(1, c.writeIndex);
}

void test_circular_oldest_before_and_after_wrap(void) {
  CircularCursor c = {0, 0};
  circ_advance(&c, 4);
  circ_advance(&c, 4);
  TEST_ASSERT_EQUAL_UINT32(0, circ_oldest(&c, 4));
  circ_advance(&c, 4);
  circ_advance(&c, 4);
  circ_advance(&c, 4);  // wrapped once: oldest is now slot 1
  TEST_ASSERT_EQUAL_UINT32(1, circ_oldest(&c, 4));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_find_free_slot_returns_first_free);
  RUN_TEST(test_find_free_slot_returns_minus_one_when_full);
  RUN_TEST(test_find_slot_by_seq);
  RUN_TEST(test_eviction_picks_lowest_seq);
  RUN_TEST(test_eviction_skips_active_seq_even_if_lowest);
  RUN_TEST(test_eviction_rejects_when_no_eligible_slot);
  RUN_TEST(test_age_derivation_from_admission_epoch);
  RUN_TEST(test_age_unknown_when_no_synced_time);
  RUN_TEST(test_age_unknown_when_admission_epoch_zero);
  RUN_TEST(test_age_unknown_when_clock_behind_admission);
  RUN_TEST(test_history_record_roundtrip_including_discharge_outcome);
  RUN_TEST(test_history_record_defaults_unknown_outcome);
  RUN_TEST(test_history_record_roundtrips_kangaroo_fields);
  RUN_TEST(test_new_profile_has_no_kangaroo_events);
  RUN_TEST(test_history_record_tombstone_flag);
  RUN_TEST(test_circular_cursor_advances_and_saturates);
  RUN_TEST(test_circular_oldest_before_and_after_wrap);
  return UNITY_END();
}
