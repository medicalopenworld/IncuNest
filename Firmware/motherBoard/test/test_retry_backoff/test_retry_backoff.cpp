// Espera progresiva de reintento y limitador de log (modules/util/retry_backoff).
//
// Lo que se protege: que un cargador que se detecto pero no se configuro en el
// arranque se REINTENTE (sin reintento la bateria no carga y la red no se
// vigila), sin martillear el bus I2C ni llenar el log cada 5 s.

#include <unity.h>

#include "modules/util/retry_backoff.h"

static Backoff b;
static LogLimiter l;

void setUp(void) {
  backoff_init(&b);
  log_limiter_init(&l);
}
void tearDown(void) {}

// --- espera progresiva ---

void test_first_attempt_is_due_immediately(void) {
  TEST_ASSERT_TRUE(backoff_due(&b, 0));
  TEST_ASSERT_TRUE(backoff_due(&b, 123456));
}

void test_bounds_are_five_seconds_and_five_minutes(void) {
  TEST_ASSERT_EQUAL_UINT32(5000u, BACKOFF_MIN_MS);
  TEST_ASSERT_EQUAL_UINT32(300000u, BACKOFF_MAX_MS);
}

void test_after_a_failure_the_next_attempt_waits_the_minimum(void) {
  backoff_failed(&b, 100000);
  TEST_ASSERT_FALSE(backoff_due(&b, 100000));
  TEST_ASSERT_FALSE(backoff_due(&b, 104999));
  TEST_ASSERT_TRUE(backoff_due(&b, 105000));
}

void test_the_wait_doubles_up_to_the_maximum(void) {
  const uint32_t expected[] = {5000,  10000,  20000,  40000,
                               80000, 160000, 300000, 300000, 300000};
  uint32_t t = 1000;
  for (unsigned i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
    TEST_ASSERT_TRUE(backoff_due(&b, t));
    backoff_failed(&b, t);
    TEST_ASSERT_FALSE(backoff_due(&b, t + expected[i] - 1u));
    TEST_ASSERT_TRUE(backoff_due(&b, t + expected[i]));
    t += expected[i];
  }
}

void test_success_resets_the_wait(void) {
  backoff_failed(&b, 1000);
  backoff_failed(&b, 6000);
  backoff_failed(&b, 16000);
  backoff_ok(&b);
  TEST_ASSERT_TRUE(backoff_due(&b, 16000));
  backoff_failed(&b, 20000);  // vuelve a empezar en el minimo
  TEST_ASSERT_FALSE(backoff_due(&b, 24999));
  TEST_ASSERT_TRUE(backoff_due(&b, 25000));
}

void test_backoff_survives_millis_wraparound(void) {
  const uint32_t t = 0xFFFFFFFFu - 1000u;
  backoff_failed(&b, t);
  TEST_ASSERT_FALSE(backoff_due(&b, t + 4999u));
  TEST_ASSERT_TRUE(backoff_due(&b, t + 5000u));
}

// --- limitador de log: uno por episodio y luego cada 5 min ---

void test_first_log_of_an_episode_is_allowed(void) {
  TEST_ASSERT_TRUE(log_limiter_allow(&l, 1000));
}

void test_further_logs_inside_the_interval_are_dropped(void) {
  TEST_ASSERT_TRUE(log_limiter_allow(&l, 1000));
  TEST_ASSERT_FALSE(log_limiter_allow(&l, 6000));
  TEST_ASSERT_FALSE(log_limiter_allow(&l, 1000 + LOG_LIMITER_INTERVAL_MS - 1u));
}

void test_log_comes_back_every_five_minutes(void) {
  TEST_ASSERT_EQUAL_UINT32(300000u, LOG_LIMITER_INTERVAL_MS);
  TEST_ASSERT_TRUE(log_limiter_allow(&l, 1000));
  TEST_ASSERT_TRUE(log_limiter_allow(&l, 1000 + LOG_LIMITER_INTERVAL_MS));
  TEST_ASSERT_FALSE(log_limiter_allow(&l, 1000 + LOG_LIMITER_INTERVAL_MS + 5000u));
  TEST_ASSERT_TRUE(log_limiter_allow(&l, 1000 + 2u * LOG_LIMITER_INTERVAL_MS));
}

void test_a_new_episode_logs_again_immediately(void) {
  TEST_ASSERT_TRUE(log_limiter_allow(&l, 1000));
  log_limiter_reset(&l);  // el episodio termino (exito)
  TEST_ASSERT_TRUE(log_limiter_allow(&l, 2000));
}

void test_limiter_survives_millis_wraparound(void) {
  const uint32_t t = 0xFFFFFFFFu - 1000u;
  TEST_ASSERT_TRUE(log_limiter_allow(&l, t));
  TEST_ASSERT_FALSE(log_limiter_allow(&l, t + LOG_LIMITER_INTERVAL_MS - 1u));
  TEST_ASSERT_TRUE(log_limiter_allow(&l, t + LOG_LIMITER_INTERVAL_MS));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_first_attempt_is_due_immediately);
  RUN_TEST(test_bounds_are_five_seconds_and_five_minutes);
  RUN_TEST(test_after_a_failure_the_next_attempt_waits_the_minimum);
  RUN_TEST(test_the_wait_doubles_up_to_the_maximum);
  RUN_TEST(test_success_resets_the_wait);
  RUN_TEST(test_backoff_survives_millis_wraparound);
  RUN_TEST(test_first_log_of_an_episode_is_allowed);
  RUN_TEST(test_further_logs_inside_the_interval_are_dropped);
  RUN_TEST(test_log_comes_back_every_five_minutes);
  RUN_TEST(test_a_new_episode_logs_again_immediately);
  RUN_TEST(test_limiter_survives_millis_wraparound);
  return UNITY_END();
}
