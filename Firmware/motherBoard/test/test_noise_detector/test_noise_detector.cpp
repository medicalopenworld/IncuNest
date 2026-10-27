// Detector de ruido elevado (ALARM_NOISE_HIGH).
//
// Lo que se protege: que un ruido mantenido levante la alarma en 10 s y que
// un golpe suelto no; que se retire con 10 s de silencio y no oscile con un
// ruido que ronda el umbral; que el pitido de otra alarma no cuente como
// ruido; y que perder el microfono retire la condicion. Ver
// modules/control/noise_detector.h.

#include <unity.h>

#include "modules/control/noise_detector.h"

static NoiseDetector d;

void setUp(void) {
  const NoiseDetector init = NOISE_DETECTOR_INITIALIZER;
  d = init;
}
void tearDown(void) {}

// Lecturas cada `period` ms desde `t0` hasta `t1` incluido; devuelve el
// instante de la ultima.
static uint32_t feed(float dba, uint32_t t0, uint32_t t1, uint32_t period,
                     bool suppressed = false) {
  uint32_t t = t0;
  for (; t <= t1; t += period) {
    noise_detector_sample(&d, true, dba, suppressed, t);
  }
  return t - period;
}

void test_quiet_never_raises(void) {
  const uint32_t t = feed(50.0f, 0, 60000, 1000);
  TEST_ASSERT_FALSE(noise_detector_condition(&d, t));
}

void test_sustained_noise_raises_after_ten_seconds(void) {
  feed(70.0f, 0, 9000, 1000);
  TEST_ASSERT_FALSE(noise_detector_condition(&d, 9000));
  noise_detector_sample(&d, true, 70.0f, false, 10000);
  TEST_ASSERT_TRUE(noise_detector_condition(&d, 10000));
}

void test_also_works_with_five_second_readings(void) {
  // La SensorBoard publicaba cada 5 s: 0, 5 y 10 s.
  feed(70.0f, 0, 5000, 5000);
  TEST_ASSERT_FALSE(noise_detector_condition(&d, 5000));
  noise_detector_sample(&d, true, 70.0f, false, 10000);
  TEST_ASSERT_TRUE(noise_detector_condition(&d, 10000));
}

void test_a_single_loud_reading_does_not_raise(void) {
  feed(50.0f, 0, 5000, 1000);
  noise_detector_sample(&d, true, 90.0f, false, 6000);
  const uint32_t t = feed(50.0f, 7000, 30000, 1000);
  TEST_ASSERT_FALSE(noise_detector_condition(&d, t));
}

void test_a_quiet_gap_restarts_the_count(void) {
  feed(70.0f, 0, 8000, 1000);
  noise_detector_sample(&d, true, 50.0f, false, 9000);
  feed(70.0f, 10000, 18000, 1000);
  TEST_ASSERT_FALSE(noise_detector_condition(&d, 18000));
  noise_detector_sample(&d, true, 70.0f, false, 20000);
  TEST_ASSERT_TRUE(noise_detector_condition(&d, 20000));
}

void test_clears_after_ten_seconds_below(void) {
  feed(70.0f, 0, 10000, 1000);
  TEST_ASSERT_TRUE(noise_detector_condition(&d, 10000));
  feed(55.0f, 11000, 20000, 1000);
  TEST_ASSERT_TRUE(noise_detector_condition(&d, 20000));
  noise_detector_sample(&d, true, 55.0f, false, 21000);
  TEST_ASSERT_FALSE(noise_detector_condition(&d, 21000));
}

void test_hysteresis_band_does_not_clear(void) {
  feed(70.0f, 0, 10000, 1000);
  // 64 dB: por debajo del umbral de disparo pero no del de retirada.
  const uint32_t t = feed(64.0f, 11000, 60000, 1000);
  TEST_ASSERT_TRUE(noise_detector_condition(&d, t));
}

void test_hysteresis_band_does_not_raise(void) {
  const uint32_t t = feed(64.0f, 0, 60000, 1000);
  TEST_ASSERT_FALSE(noise_detector_condition(&d, t));
}

void test_another_alarm_sounding_is_not_noise(void) {
  const uint32_t t = feed(85.0f, 0, 30000, 1000, /*suppressed=*/true);
  TEST_ASSERT_FALSE(noise_detector_condition(&d, t));
}

void test_suppressed_readings_freeze_the_clear_count(void) {
  feed(70.0f, 0, 10000, 1000);
  feed(55.0f, 11000, 15000, 1000);
  // Suena otra alarma 30 s: ni retira ni rompe la racha de silencio.
  feed(85.0f, 16000, 45000, 1000, /*suppressed=*/true);
  TEST_ASSERT_TRUE(noise_detector_condition(&d, 45000));
}

void test_stale_microphone_clears(void) {
  feed(70.0f, 0, 10000, 1000);
  TEST_ASSERT_TRUE(noise_detector_condition(&d, 10000));
  TEST_ASSERT_TRUE(noise_detector_condition(&d, 10000 + NOISE_STALE_MS));
  TEST_ASSERT_FALSE(noise_detector_condition(&d, 10001 + NOISE_STALE_MS));
}

void test_after_a_stale_gap_it_starts_over(void) {
  feed(70.0f, 0, 10000, 1000);
  // Vuelve el microfono tras el hueco: la racha previa no cuenta.
  noise_detector_sample(&d, true, 70.0f, false, 40000);
  TEST_ASSERT_FALSE(noise_detector_condition(&d, 40000));
}

void test_invalid_readings_change_nothing(void) {
  feed(70.0f, 0, 9000, 1000);
  noise_detector_sample(&d, false, 0.0f, false, 9500);
  noise_detector_sample(&d, true, 70.0f, false, 10000);
  TEST_ASSERT_TRUE(noise_detector_condition(&d, 10000));
}

void test_survives_millis_wraparound(void) {
  const uint32_t t0 = 0xFFFFFFFFu - 4000u;
  for (uint32_t i = 0; i <= 10; i++) {
    noise_detector_sample(&d, true, 70.0f, false, t0 + i * 1000u);
  }
  TEST_ASSERT_TRUE(noise_detector_condition(&d, t0 + 10000u));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_quiet_never_raises);
  RUN_TEST(test_sustained_noise_raises_after_ten_seconds);
  RUN_TEST(test_also_works_with_five_second_readings);
  RUN_TEST(test_a_single_loud_reading_does_not_raise);
  RUN_TEST(test_a_quiet_gap_restarts_the_count);
  RUN_TEST(test_clears_after_ten_seconds_below);
  RUN_TEST(test_hysteresis_band_does_not_clear);
  RUN_TEST(test_hysteresis_band_does_not_raise);
  RUN_TEST(test_another_alarm_sounding_is_not_noise);
  RUN_TEST(test_suppressed_readings_freeze_the_clear_count);
  RUN_TEST(test_stale_microphone_clears);
  RUN_TEST(test_after_a_stale_gap_it_starts_over);
  RUN_TEST(test_invalid_readings_change_nothing);
  RUN_TEST(test_survives_millis_wraparound);
  return UNITY_END();
}
