#include <unity.h>
#include "nte_table.h"

void setUp(void) {}
void tearDown(void) {}

// --- Deterministic output for identical inputs ---
void test_deterministic_output_for_identical_inputs(void) {
  NteRange a = calculateNteRange(1300, 30, 5);
  NteRange b = calculateNteRange(1300, 30, 5);
  TEST_ASSERT_EQUAL_FLOAT(a.lo, b.lo);
  TEST_ASSERT_EQUAL_FLOAT(a.hi, b.hi);
  TEST_ASSERT_EQUAL_FLOAT(a.mid, b.mid);
  TEST_ASSERT_EQUAL(a.estimated, b.estimated);
}

// --- Known weight produces a bounded, ordered range ---
void test_known_weight_produces_bounded_ordered_range(void) {
  NteRange r = calculateNteRange(1300, 30, 5);
  TEST_ASSERT_FALSE(r.estimated);
  TEST_ASSERT_TRUE(r.lo <= r.mid);
  TEST_ASSERT_TRUE(r.mid <= r.hi);
  TEST_ASSERT_EQUAL_FLOAT((r.lo + r.hi) / 2.0f, r.mid);
}

// --- SKIP weight yields the unestimated sentinel range ---
void test_skip_weight_yields_sentinel_range(void) {
  NteRange r = calculateNteRange(0, 30, 5);
  TEST_ASSERT_TRUE(r.estimated);
  TEST_ASSERT_EQUAL_FLOAT(-1.0f, r.lo);
  TEST_ASSERT_EQUAL_FLOAT(-1.0f, r.hi);
  TEST_ASSERT_EQUAL_FLOAT(-1.0f, r.mid);
}

// --- Out-of-table inputs fail safe, never crash ---
void test_out_of_table_gest_weeks_below_minimum_fails_safe(void) {
  // Table only covers gestWeeks >= 24.
  NteRange r = calculateNteRange(1300, 10, 5);
  TEST_ASSERT_TRUE(r.estimated);
  TEST_ASSERT_EQUAL_FLOAT(-1.0f, r.lo);
  TEST_ASSERT_EQUAL_FLOAT(-1.0f, r.hi);
  TEST_ASSERT_EQUAL_FLOAT(-1.0f, r.mid);
}

void test_out_of_table_term_baby_beyond_week3_fails_safe(void) {
  // gestWeeks >= 36, ageDays >= 21 (W3+) is a documented "no aplica" cell.
  NteRange r = calculateNteRange(3000, 38, 25);
  TEST_ASSERT_TRUE(r.estimated);
  TEST_ASSERT_EQUAL_FLOAT(-1.0f, r.lo);
  TEST_ASSERT_EQUAL_FLOAT(-1.0f, r.hi);
}

void test_extreme_weight_does_not_crash_and_fails_safe_or_bounds(void) {
  // uint16_t max weight must not read out of bounds; must return a
  // well-defined result (either a valid bounded range from the top row, or
  // the sentinel), never garbage/UB.
  NteRange r = calculateNteRange(65535, 40, 2);
  if (!r.estimated) {
    TEST_ASSERT_TRUE(r.lo <= r.hi);
  } else {
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, r.lo);
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_deterministic_output_for_identical_inputs);
  RUN_TEST(test_known_weight_produces_bounded_ordered_range);
  RUN_TEST(test_skip_weight_yields_sentinel_range);
  RUN_TEST(test_out_of_table_gest_weeks_below_minimum_fails_safe);
  RUN_TEST(test_out_of_table_term_baby_beyond_week3_fails_safe);
  RUN_TEST(test_extreme_weight_does_not_crash_and_fails_safe_or_bounds);
  return UNITY_END();
}
