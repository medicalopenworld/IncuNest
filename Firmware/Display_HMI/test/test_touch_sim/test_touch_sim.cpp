#include <unity.h>

#include "modules/debug/touch_sim.h"

void setUp(void) { touch_sim_reset(); }
void tearDown(void) {}

void test_idle_reports_no_finger(void) {
  TEST_ASSERT_FALSE(touch_sim_busy());
  TEST_ASSERT_FALSE(touch_sim_poll(0).pressed);
}

void test_tap_is_pressed_for_its_duration_then_released(void) {
  TEST_ASSERT_TRUE(touch_sim_push(100, 200, 80, 800, 480));
  TEST_ASSERT_TRUE(touch_sim_busy());
  TouchSimPoint p = touch_sim_poll(1000); // starts here
  TEST_ASSERT_TRUE(p.pressed);
  TEST_ASSERT_EQUAL_INT16(100, p.x);
  TEST_ASSERT_EQUAL_INT16(200, p.y);
  TEST_ASSERT_TRUE(touch_sim_poll(1079).pressed);
  p = touch_sim_poll(1080);
  TEST_ASSERT_FALSE(p.pressed);
  TEST_ASSERT_FALSE(touch_sim_busy());
}

void test_two_taps_have_a_release_between_them(void) {
  TEST_ASSERT_TRUE(touch_sim_push(10, 10, 50, 800, 480));
  TEST_ASSERT_TRUE(touch_sim_push(10, 10, 50, 800, 480));
  TEST_ASSERT_TRUE(touch_sim_poll(0).pressed);
  TEST_ASSERT_FALSE(touch_sim_poll(50).pressed); // first ends
  TEST_ASSERT_TRUE(touch_sim_busy());            // second still queued
  TEST_ASSERT_TRUE(touch_sim_poll(60).pressed);  // second starts
  TEST_ASSERT_TRUE(touch_sim_poll(109).pressed);
  TEST_ASSERT_FALSE(touch_sim_poll(110).pressed);
  TEST_ASSERT_FALSE(touch_sim_busy());
}

void test_long_press_survives_millis_wrap(void) {
  TEST_ASSERT_TRUE(touch_sim_push(400, 240, 2000, 800, 480));
  const uint32_t start = 0xFFFFFC00u; // 1024 ms before the wrap
  TEST_ASSERT_TRUE(touch_sim_poll(start).pressed);
  TEST_ASSERT_TRUE(touch_sim_poll(start + 1999u).pressed);
  TEST_ASSERT_FALSE(touch_sim_poll(start + 2000u).pressed);
}

void test_rejects_bad_requests(void) {
  TEST_ASSERT_FALSE(touch_sim_push(0, 0, 0, 800, 480));
  TEST_ASSERT_FALSE(touch_sim_push(0, 0, TOUCH_SIM_MAX_MS + 1, 800, 480));
  TEST_ASSERT_FALSE(touch_sim_push(-1, 0, 80, 800, 480));
  TEST_ASSERT_FALSE(touch_sim_push(800, 0, 80, 800, 480));
  TEST_ASSERT_FALSE(touch_sim_push(0, 480, 80, 800, 480));
  TEST_ASSERT_TRUE(touch_sim_push(799, 479, 80, 800, 480));
  TEST_ASSERT_TRUE(touch_sim_busy());
}

void test_queue_is_bounded(void) {
  for (uint8_t i = 0; i < TOUCH_SIM_QUEUE_LEN; i++)
    TEST_ASSERT_TRUE(touch_sim_push(1, 1, 80, 800, 480));
  TEST_ASSERT_FALSE(touch_sim_push(1, 1, 80, 800, 480));
  touch_sim_poll(0); // one leaves the queue and becomes active
  TEST_ASSERT_TRUE(touch_sim_push(1, 1, 80, 800, 480));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_idle_reports_no_finger);
  RUN_TEST(test_tap_is_pressed_for_its_duration_then_released);
  RUN_TEST(test_two_taps_have_a_release_between_them);
  RUN_TEST(test_long_press_survives_millis_wrap);
  RUN_TEST(test_rejects_bad_requests);
  RUN_TEST(test_queue_is_bounded);
  return UNITY_END();
}
