// Vigilante de muestras del AFE4490. Ver modules/control/afe_watchdog.h.

#include <unity.h>

#include "modules/control/afe_watchdog.h"

static AfeWatchdog wd;
static uint32_t t0;

void setUp(void) {
  t0 = 100000; // lejos de 0
  afe_wd_init(&wd, t0);
}
void tearDown(void) {}

void test_no_restart_at_or_below_ten_seconds(void) {
  TEST_ASSERT_FALSE(afe_wd_should_restart(&wd, t0));
  TEST_ASSERT_FALSE(afe_wd_should_restart(&wd, t0 + AFE_WD_STALL_MS));
  TEST_ASSERT_EQUAL_UINT32(0, afe_wd_restarts(&wd));
}

void test_restart_just_above_ten_seconds(void) {
  TEST_ASSERT_TRUE(afe_wd_should_restart(&wd, t0 + AFE_WD_STALL_MS + 1));
  TEST_ASSERT_EQUAL_UINT32(1, afe_wd_restarts(&wd));
}

void test_a_sample_rearms_the_deadline(void) {
  afe_wd_sample(&wd, t0 + 9000);
  TEST_ASSERT_FALSE(afe_wd_should_restart(&wd, t0 + 9000 + AFE_WD_STALL_MS));
  TEST_ASSERT_TRUE(afe_wd_should_restart(&wd, t0 + 9000 + AFE_WD_STALL_MS + 1));
}

void test_continuous_samples_never_restart(void) {
  for (uint32_t t = t0; t < t0 + 120000; t += 2) {
    afe_wd_sample(&wd, t);
    TEST_ASSERT_FALSE(afe_wd_should_restart(&wd, t));
  }
}

void test_restart_rearms_sample_deadline(void) {
  const uint32_t r = t0 + AFE_WD_STALL_MS + 1;
  TEST_ASSERT_TRUE(afe_wd_should_restart(&wd, r));
  // Un instante despues sigue sin muestra, pero no se repite de inmediato.
  TEST_ASSERT_FALSE(afe_wd_should_restart(&wd, r + 1));
}

void test_at_most_one_restart_per_minute_while_stalled(void) {
  const uint32_t r = t0 + AFE_WD_STALL_MS + 1;
  TEST_ASSERT_TRUE(afe_wd_should_restart(&wd, r));
  // Sigue colgado: pasados otros 10 s toca, pero el limite de 60 s lo impide.
  TEST_ASSERT_FALSE(
      afe_wd_should_restart(&wd, r + AFE_WD_STALL_MS + 1));
  TEST_ASSERT_FALSE(
      afe_wd_should_restart(&wd, r + AFE_WD_MIN_RESTART_INTERVAL_MS - 1));
  TEST_ASSERT_TRUE(
      afe_wd_should_restart(&wd, r + AFE_WD_MIN_RESTART_INTERVAL_MS));
  TEST_ASSERT_EQUAL_UINT32(2, afe_wd_restarts(&wd));
}

void test_recovery_after_restart_stops_further_restarts(void) {
  const uint32_t r = t0 + AFE_WD_STALL_MS + 1;
  TEST_ASSERT_TRUE(afe_wd_should_restart(&wd, r));
  // El AFE volvio y sigue dando muestras hasta pasado el minuto.
  for (uint32_t t = r + 500; t <= r + AFE_WD_MIN_RESTART_INTERVAL_MS + 5000;
       t += 100) {
    afe_wd_sample(&wd, t);
    TEST_ASSERT_FALSE(afe_wd_should_restart(&wd, t));
  }
  TEST_ASSERT_FALSE(
      afe_wd_should_restart(&wd, r + AFE_WD_MIN_RESTART_INTERVAL_MS + 5000));
  TEST_ASSERT_EQUAL_UINT32(1, afe_wd_restarts(&wd));
}

void test_new_stall_soon_after_recovery_waits_for_the_minute(void) {
  const uint32_t r = t0 + AFE_WD_STALL_MS + 1;
  TEST_ASSERT_TRUE(afe_wd_should_restart(&wd, r));
  afe_wd_sample(&wd, r + 100);
  // Nuevo cuelgue a los 20 s del reinicio: pasan 10 s sin muestras, pero
  // aun no ha pasado un minuto desde el reinicio anterior.
  TEST_ASSERT_FALSE(afe_wd_should_restart(&wd, r + 100 + AFE_WD_STALL_MS + 1));
  TEST_ASSERT_TRUE(
      afe_wd_should_restart(&wd, r + AFE_WD_MIN_RESTART_INTERVAL_MS));
}

void test_survives_millis_wraparound(void) {
  const uint32_t near_wrap = 0xFFFFFFFFu - 2000u;
  afe_wd_init(&wd, near_wrap);
  const uint32_t after = near_wrap + AFE_WD_STALL_MS + 1; // ya dio la vuelta
  TEST_ASSERT_TRUE(after < near_wrap);
  TEST_ASSERT_TRUE(afe_wd_should_restart(&wd, after));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_no_restart_at_or_below_ten_seconds);
  RUN_TEST(test_restart_just_above_ten_seconds);
  RUN_TEST(test_a_sample_rearms_the_deadline);
  RUN_TEST(test_continuous_samples_never_restart);
  RUN_TEST(test_restart_rearms_sample_deadline);
  RUN_TEST(test_at_most_one_restart_per_minute_while_stalled);
  RUN_TEST(test_recovery_after_restart_stops_further_restarts);
  RUN_TEST(test_new_stall_soon_after_recovery_waits_for_the_minute);
  RUN_TEST(test_survives_millis_wraparound);
  return UNITY_END();
}
