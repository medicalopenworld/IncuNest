// Detector de parada del AFE4490 y caducidad del dato. Ver
// modules/control/afe_watchdog.h.
//
// Historia: la primera version de este modulo ordenaba un reinicio en caliente
// de la libreria (stop/PDN/begin). Se retiro en la revision de mb-hw19 porque
// afe.stop() hace vTaskDelete de las tareas de la libreria en un punto
// arbitrario (ver docs/hw19.md y known_issues.md). Ahora solo DETECTA y dice si
// el dato sigue siendo fresco.

#include <unity.h>

#include "modules/control/afe_watchdog.h"

static AfeWatchdog wd;
static uint32_t t0;

void setUp(void) {
  t0 = 100000; // lejos de 0
  afe_wd_init(&wd, t0);
}
void tearDown(void) {}

// --- caducidad del dato (A2): >1 s sin muestra = no valido ---

void test_data_is_fresh_up_to_one_second(void) {
  TEST_ASSERT_TRUE(afe_wd_data_fresh(&wd, t0));
  TEST_ASSERT_TRUE(afe_wd_data_fresh(&wd, t0 + AFE_WD_DATA_STALE_MS));
}

void test_data_is_stale_just_above_one_second(void) {
  TEST_ASSERT_FALSE(afe_wd_data_fresh(&wd, t0 + AFE_WD_DATA_STALE_MS + 1));
}

void test_a_sample_makes_data_fresh_again(void) {
  const uint32_t late = t0 + 5000;
  TEST_ASSERT_FALSE(afe_wd_data_fresh(&wd, late));
  afe_wd_sample(&wd, late);
  TEST_ASSERT_TRUE(afe_wd_data_fresh(&wd, late));
  TEST_ASSERT_TRUE(afe_wd_data_fresh(&wd, late + AFE_WD_DATA_STALE_MS));
}

void test_freshness_survives_millis_wraparound(void) {
  const uint32_t near_wrap = 0xFFFFFFFFu - 300u;
  afe_wd_init(&wd, near_wrap);
  const uint32_t after = near_wrap + 800u; // ya dio la vuelta
  TEST_ASSERT_TRUE(after < near_wrap);
  TEST_ASSERT_TRUE(afe_wd_data_fresh(&wd, after));
  TEST_ASSERT_FALSE(afe_wd_data_fresh(&wd, near_wrap + AFE_WD_DATA_STALE_MS + 1));
}

// --- detector de parada: un solo evento por episodio ---

void test_no_stall_at_or_below_ten_seconds(void) {
  TEST_ASSERT_FALSE(afe_wd_stall_detected(&wd, t0));
  TEST_ASSERT_FALSE(afe_wd_stall_detected(&wd, t0 + AFE_WD_STALL_MS));
  TEST_ASSERT_EQUAL_UINT32(0, afe_wd_stalls(&wd));
}

void test_stall_detected_just_above_ten_seconds(void) {
  TEST_ASSERT_TRUE(afe_wd_stall_detected(&wd, t0 + AFE_WD_STALL_MS + 1));
  TEST_ASSERT_EQUAL_UINT32(1, afe_wd_stalls(&wd));
}

void test_one_event_per_episode(void) {
  TEST_ASSERT_TRUE(afe_wd_stall_detected(&wd, t0 + AFE_WD_STALL_MS + 1));
  // El episodio sigue abierto minutos: ni log ni contador repetidos.
  for (uint32_t t = t0 + 11000; t < t0 + 600000; t += 997) {
    TEST_ASSERT_FALSE(afe_wd_stall_detected(&wd, t));
  }
  TEST_ASSERT_EQUAL_UINT32(1, afe_wd_stalls(&wd));
}

void test_a_sample_closes_the_episode_and_a_new_one_counts(void) {
  TEST_ASSERT_TRUE(afe_wd_stall_detected(&wd, t0 + AFE_WD_STALL_MS + 1));
  const uint32_t back = t0 + 20000;
  afe_wd_sample(&wd, back);
  TEST_ASSERT_FALSE(afe_wd_stall_detected(&wd, back + AFE_WD_STALL_MS));
  TEST_ASSERT_TRUE(afe_wd_stall_detected(&wd, back + AFE_WD_STALL_MS + 1));
  TEST_ASSERT_EQUAL_UINT32(2, afe_wd_stalls(&wd));
}

void test_continuous_samples_never_stall(void) {
  for (uint32_t t = t0; t < t0 + 120000; t += 2) {
    afe_wd_sample(&wd, t);
    TEST_ASSERT_FALSE(afe_wd_stall_detected(&wd, t));
    TEST_ASSERT_TRUE(afe_wd_data_fresh(&wd, t));
  }
  TEST_ASSERT_EQUAL_UINT32(0, afe_wd_stalls(&wd));
}

void test_a_sample_before_the_deadline_rearms_it(void) {
  afe_wd_sample(&wd, t0 + 9000);
  TEST_ASSERT_FALSE(afe_wd_stall_detected(&wd, t0 + 9000 + AFE_WD_STALL_MS));
  TEST_ASSERT_TRUE(afe_wd_stall_detected(&wd, t0 + 9000 + AFE_WD_STALL_MS + 1));
}

void test_stall_survives_millis_wraparound(void) {
  const uint32_t near_wrap = 0xFFFFFFFFu - 2000u;
  afe_wd_init(&wd, near_wrap);
  const uint32_t after = near_wrap + AFE_WD_STALL_MS + 1;
  TEST_ASSERT_TRUE(after < near_wrap);
  TEST_ASSERT_TRUE(afe_wd_stall_detected(&wd, after));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_data_is_fresh_up_to_one_second);
  RUN_TEST(test_data_is_stale_just_above_one_second);
  RUN_TEST(test_a_sample_makes_data_fresh_again);
  RUN_TEST(test_freshness_survives_millis_wraparound);
  RUN_TEST(test_no_stall_at_or_below_ten_seconds);
  RUN_TEST(test_stall_detected_just_above_ten_seconds);
  RUN_TEST(test_one_event_per_episode);
  RUN_TEST(test_a_sample_closes_the_episode_and_a_new_one_counts);
  RUN_TEST(test_continuous_samples_never_stall);
  RUN_TEST(test_a_sample_before_the_deadline_rearms_it);
  RUN_TEST(test_stall_survives_millis_wraparound);
  return UNITY_END();
}
