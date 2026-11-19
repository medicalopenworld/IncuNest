// modules/util/vitals_out.h: where a quantity of incunest_afe4490 (finite, or
// NaN for "no value") becomes a field of CTRL,VIT or a telemetry value. The
// conversions to integer are undefined behaviour for NaN, infinity or a value
// out of the destination range, so every one of them goes through here.

#include <math.h>
#include <unity.h>

#include "modules/util/vitals_out.h"

void setUp(void) {}
void tearDown(void) {}

// --- is there a value? ---

void test_has_value_is_finite(void) {
  TEST_ASSERT_TRUE(vitals_has_value(97.0f));
  TEST_ASSERT_TRUE(vitals_has_value(0.0f));
  TEST_ASSERT_FALSE(vitals_has_value(NAN));
  TEST_ASSERT_FALSE(vitals_has_value(INFINITY));
  TEST_ASSERT_FALSE(vitals_has_value(-INFINITY));
}

// --- heart rate: nearest integer bpm, 0 = no value ---

void test_hr_rounds_to_nearest(void) {
  TEST_ASSERT_EQUAL_UINT16(134, vitals_hr_bpm(134.4f));
  TEST_ASSERT_EQUAL_UINT16(135, vitals_hr_bpm(134.5f));
  TEST_ASSERT_EQUAL_UINT16(30, vitals_hr_bpm(30.0f));
  TEST_ASSERT_EQUAL_UINT16(260, vitals_hr_bpm(260.0f)); // above one byte
}

void test_hr_no_value(void) {
  TEST_ASSERT_EQUAL_UINT16(0, vitals_hr_bpm(NAN));
  TEST_ASSERT_EQUAL_UINT16(0, vitals_hr_bpm(INFINITY));
  TEST_ASSERT_EQUAL_UINT16(0, vitals_hr_bpm(-INFINITY));
  TEST_ASSERT_EQUAL_UINT16(0, vitals_hr_bpm(0.0f)); // what a zeroed struct holds
  TEST_ASSERT_EQUAL_UINT16(0, vitals_hr_bpm(-5.0f));
}

void test_hr_bound(void) {
  TEST_ASSERT_EQUAL_UINT16(300, vitals_hr_bpm(VITALS_HR_MAX_BPM));
  TEST_ASSERT_EQUAL_UINT16(0, vitals_hr_bpm(300.1f));
  TEST_ASSERT_EQUAL_UINT16(0, vitals_hr_bpm(1.0e9f));
}

// --- SpO2: nearest integer %, 0 = no value ---

void test_spo2_rounds_to_nearest(void) {
  TEST_ASSERT_EQUAL_UINT16(97, vitals_spo2_pct(97.4f));
  TEST_ASSERT_EQUAL_UINT16(98, vitals_spo2_pct(97.5f));
  TEST_ASSERT_EQUAL_UINT16(100, vitals_spo2_pct(100.0f));
  TEST_ASSERT_EQUAL_UINT16(70, vitals_spo2_pct(70.0f));
}

void test_spo2_no_value(void) {
  TEST_ASSERT_EQUAL_UINT16(0, vitals_spo2_pct(NAN));
  TEST_ASSERT_EQUAL_UINT16(0, vitals_spo2_pct(INFINITY));
  TEST_ASSERT_EQUAL_UINT16(0, vitals_spo2_pct(0.0f));
  TEST_ASSERT_EQUAL_UINT16(0, vitals_spo2_pct(-1.0f));
  TEST_ASSERT_EQUAL_UINT16(0, vitals_spo2_pct(100.1f));
}

// --- perfusion index: %, 0 = no value ---

void test_pi_passes_a_value(void) {
  TEST_ASSERT_EQUAL_FLOAT(0.37f, vitals_pi_pct(0.37f));
  TEST_ASSERT_EQUAL_FLOAT(7.14f, vitals_pi_pct(7.14f));
  TEST_ASSERT_EQUAL_FLOAT(100.0f, vitals_pi_pct(VITALS_PI_MAX_PCT));
}

void test_pi_no_value(void) {
  // NaN used to travel as the text "nan"; an infinity would have travelled too.
  TEST_ASSERT_EQUAL_FLOAT(0.0f, vitals_pi_pct(NAN));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, vitals_pi_pct(INFINITY));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, vitals_pi_pct(-INFINITY));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, vitals_pi_pct(-0.2f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, vitals_pi_pct(100.5f));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_has_value_is_finite);
  RUN_TEST(test_hr_rounds_to_nearest);
  RUN_TEST(test_hr_no_value);
  RUN_TEST(test_hr_bound);
  RUN_TEST(test_spo2_rounds_to_nearest);
  RUN_TEST(test_spo2_no_value);
  RUN_TEST(test_pi_passes_a_value);
  RUN_TEST(test_pi_no_value);
  return UNITY_END();
}
