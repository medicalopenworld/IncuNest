#include <unity.h>

#include "modules/util/civil_time.h"

void setUp(void) {}
void tearDown(void) {}

void test_epoch_zero_reference(void) {
  TEST_ASSERT_EQUAL_INT64(0, civil_days_from_epoch(1970, 1, 1));
}

void test_known_dates(void) {
  // 2021-01-01 is 18628 days after the epoch.
  TEST_ASSERT_EQUAL_INT64(18628, civil_days_from_epoch(2021, 1, 1));
  // Leap-day handling.
  TEST_ASSERT_EQUAL_INT64(civil_days_from_epoch(2024, 2, 28) + 1,
                          civil_days_from_epoch(2024, 2, 29));
  TEST_ASSERT_EQUAL_INT64(civil_days_from_epoch(2024, 2, 29) + 1,
                          civil_days_from_epoch(2024, 3, 1));
  // 1900 is NOT a leap year, 2000 IS (the century rules).
  TEST_ASSERT_EQUAL_INT64(civil_days_from_epoch(1900, 2, 28) + 1,
                          civil_days_from_epoch(1900, 3, 1));
  TEST_ASSERT_EQUAL_INT64(civil_days_from_epoch(2000, 2, 28) + 2,
                          civil_days_from_epoch(2000, 3, 1));
}

void test_utc_conversion_no_offset(void) {
  uint32_t e = 0;
  // 2021-01-01T00:00:00Z
  TEST_ASSERT_TRUE(civil_to_unix_utc(2021, 1, 1, 0, 0, 0, 0, &e));
  TEST_ASSERT_EQUAL_UINT32(1609459200u, e);
}

void test_utc_conversion_applies_timezone(void) {
  uint32_t utc = 0, plus2 = 0;
  // Same instant: 12:00 UTC == 14:00 at UTC+2 (+8 quarter hours).
  TEST_ASSERT_TRUE(civil_to_unix_utc(2024, 6, 15, 12, 0, 0, 0, &utc));
  TEST_ASSERT_TRUE(civil_to_unix_utc(2024, 6, 15, 14, 0, 0, 8, &plus2));
  TEST_ASSERT_EQUAL_UINT32(utc, plus2);
}

void test_utc_conversion_negative_timezone(void) {
  uint32_t utc = 0, minus5 = 0;
  TEST_ASSERT_TRUE(civil_to_unix_utc(2024, 6, 15, 12, 0, 0, 0, &utc));
  TEST_ASSERT_TRUE(civil_to_unix_utc(2024, 6, 15, 7, 0, 0, -20, &minus5));
  TEST_ASSERT_EQUAL_UINT32(utc, minus5);
}

// The whole point of the 2021 floor: an unsynced SIM800 reports 2004-01-01.
void test_rejects_unsynced_modem_default_date(void) {
  uint32_t e = 0xDEADBEEF;
  TEST_ASSERT_FALSE(civil_to_unix_utc(2004, 1, 1, 0, 0, 0, 0, &e));
  TEST_ASSERT_EQUAL_UINT32(0xDEADBEEF, e); // untouched on failure
}

void test_rejects_out_of_range_fields(void) {
  uint32_t e = 0;
  TEST_ASSERT_FALSE(civil_to_unix_utc(2024, 0, 1, 0, 0, 0, 0, &e));
  TEST_ASSERT_FALSE(civil_to_unix_utc(2024, 13, 1, 0, 0, 0, 0, &e));
  TEST_ASSERT_FALSE(civil_to_unix_utc(2024, 1, 0, 0, 0, 0, 0, &e));
  TEST_ASSERT_FALSE(civil_to_unix_utc(2024, 1, 32, 0, 0, 0, 0, &e));
  TEST_ASSERT_FALSE(civil_to_unix_utc(2024, 1, 1, 24, 0, 0, 0, &e));
  TEST_ASSERT_FALSE(civil_to_unix_utc(2024, 1, 1, 0, 60, 0, 0, &e));
  TEST_ASSERT_FALSE(civil_to_unix_utc(2024, 1, 1, 0, 0, 0, 99, &e));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_epoch_zero_reference);
  RUN_TEST(test_known_dates);
  RUN_TEST(test_utc_conversion_no_offset);
  RUN_TEST(test_utc_conversion_applies_timezone);
  RUN_TEST(test_utc_conversion_negative_timezone);
  RUN_TEST(test_rejects_unsynced_modem_default_date);
  RUN_TEST(test_rejects_out_of_range_fields);
  return UNITY_END();
}
