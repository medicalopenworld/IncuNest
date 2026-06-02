#include <unity.h>
#include "modules/control/pid_wrapper.h"

void setUp(void) {}
void tearDown(void) {}

void test_pid_zero_error_zero_output(void) {
  PidWrapper pid;
  pid_init(&pid, 1.0f, 0.0f, 0.0f, 0.0f, 255.0f);
  float out = pid_compute(&pid, 36.5f, 36.5f, 100);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, out);
}

void test_pid_positive_output_below_setpoint(void) {
  PidWrapper pid;
  pid_init(&pid, 2.0f, 0.0f, 0.0f, 0.0f, 255.0f);
  float out = pid_compute(&pid, 36.5f, 35.0f, 100);
  TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, out);
}

void test_pid_output_clamped_at_max(void) {
  PidWrapper pid;
  pid_init(&pid, 100.0f, 0.0f, 0.0f, 0.0f, 255.0f);
  float out = pid_compute(&pid, 100.0f, 0.0f, 100);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 255.0f, out);
}

void test_pid_reset_clears_integral(void) {
  PidWrapper pid;
  pid_init(&pid, 0.0f, 1.0f, 0.0f, -1000.0f, 1000.0f);
  pid_compute(&pid, 10.0f, 0.0f, 1000);
  pid_reset(&pid);
  float out = pid_compute(&pid, 10.0f, 10.0f, 100);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, out);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_pid_zero_error_zero_output);
  RUN_TEST(test_pid_positive_output_below_setpoint);
  RUN_TEST(test_pid_output_clamped_at_max);
  RUN_TEST(test_pid_reset_clears_integral);
  return UNITY_END();
}
