#include <math.h>
#include <unity.h>

#include "modules/comm/vit_msg.h"

void setUp(void) {}
void tearDown(void) {}

// Sentinel to check that a rejected line does NOT touch the output.
static VitMsg poisoned(void) {
  VitMsg m;
  m.hr = 777;
  m.spo2 = 77;
  m.pi = 7.5f;
  return m;
}

static void assert_untouched(const VitMsg &m) {
  TEST_ASSERT_EQUAL_UINT16(777, m.hr);
  TEST_ASSERT_EQUAL_UINT8(77, m.spo2);
  TEST_ASSERT_EQUAL_FLOAT(7.5f, m.pi);
}

void test_parses_three_fields(void) {
  VitMsg m = poisoned();
  TEST_ASSERT_TRUE(vit_parse("CTRL,VIT,162,98,0.72", &m));
  TEST_ASSERT_EQUAL_UINT16(162, m.hr);
  TEST_ASSERT_EQUAL_UINT8(98, m.spo2);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.72f, m.pi);
}

// With one byte, 258 used to show as 2 (motherBoard 18.53.0 / HMI 4.5.0).
void test_hr_above_255_keeps_every_bit(void) {
  VitMsg m = poisoned();
  TEST_ASSERT_TRUE(vit_parse("CTRL,VIT,258,97,1.50", &m));
  TEST_ASSERT_EQUAL_UINT16(258, m.hr);
}

// Since motherBoard 18.57.0 a SpO2 comes with a PI below 0.5 % too: the parser
// keeps both, nothing here couples one to the other.
void test_spo2_with_low_pi(void) {
  VitMsg m = poisoned();
  TEST_ASSERT_TRUE(vit_parse("CTRL,VIT,140,95,0.31", &m));
  TEST_ASSERT_EQUAL_UINT8(95, m.spo2);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.31f, m.pi);
}

// "No value" on the wire: 0 for HR and SpO2, "nan" for PI (printf of NaN on
// the motherBoard).
void test_no_values(void) {
  VitMsg m = poisoned();
  TEST_ASSERT_TRUE(vit_parse("CTRL,VIT,0,0,nan", &m));
  TEST_ASSERT_EQUAL_UINT16(0, m.hr);
  TEST_ASSERT_EQUAL_UINT8(0, m.spo2);
  TEST_ASSERT_TRUE(isnan(m.pi));
}

// Older motherBoards sent no PI field.
void test_two_fields_without_pi(void) {
  VitMsg m = poisoned();
  TEST_ASSERT_TRUE(vit_parse("CTRL,VIT,140,97", &m));
  TEST_ASSERT_EQUAL_UINT16(140, m.hr);
  TEST_ASSERT_EQUAL_UINT8(97, m.spo2);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, m.pi);
}

// New fields are appended at the end; this HMI ignores them.
void test_ignores_trailing_fields(void) {
  VitMsg m = poisoned();
  TEST_ASSERT_TRUE(vit_parse("CTRL,VIT,140,97,1.20,9", &m));
  TEST_ASSERT_EQUAL_UINT16(140, m.hr);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.20f, m.pi);
}

void test_rejects_malformed(void) {
  const char *bad[] = {"CTRL,VIT", "CTRL,VIT,", "CTRL,VIT,140", "CTRL,VIT,abc,97,1.0",
                       "CTRL,VIT,,97,1.0", "CTRL,PPG,128"};
  for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
    VitMsg m = poisoned();
    TEST_ASSERT_FALSE_MESSAGE(vit_parse(bad[i], &m), bad[i]);
    assert_untouched(m);
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_parses_three_fields);
  RUN_TEST(test_hr_above_255_keeps_every_bit);
  RUN_TEST(test_spo2_with_low_pi);
  RUN_TEST(test_no_values);
  RUN_TEST(test_two_fields_without_pi);
  RUN_TEST(test_ignores_trailing_fields);
  RUN_TEST(test_rejects_malformed);
  return UNITY_END();
}
