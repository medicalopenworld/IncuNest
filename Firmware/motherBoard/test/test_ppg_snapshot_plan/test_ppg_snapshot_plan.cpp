#include <unity.h>

#include "modules/util/ppg_snapshot_plan.h"

#define INTERVAL (5UL * 60UL * 1000UL)
#define RETRY 10000UL

static PpgAutoCapture st;

void setUp(void) { st = PpgAutoCapture{}; }
void tearDown(void) {}

// --- Troceado -------------------------------------------------------------

void test_chunk_end_takes_a_full_chunk(void) {
  TEST_ASSERT_EQUAL_UINT16(25, ppg_chunk_end(0, 400, 25));
  TEST_ASSERT_EQUAL_UINT16(400, ppg_chunk_end(375, 400, 25));
}

void test_chunk_end_clamps_the_last_partial_chunk(void) {
  TEST_ASSERT_EQUAL_UINT16(10, ppg_chunk_end(0, 10, 25));
  TEST_ASSERT_EQUAL_UINT16(401, ppg_chunk_end(400, 401, 25));
}

void test_chunks_cover_every_sample_exactly_once(void) {
  uint16_t next = 0, chunks = 0;
  while (next < 400) {
    uint16_t end = ppg_chunk_end(next, 400, 25);
    TEST_ASSERT_TRUE(end > next);
    next = end;
    chunks++;
  }
  TEST_ASSERT_EQUAL_UINT16(400, next);
  TEST_ASSERT_EQUAL_UINT16(16, chunks);
}

void test_chunk_end_never_passes_n_even_near_uint16_max(void) {
  TEST_ASSERT_EQUAL_UINT16(65535, ppg_chunk_end(65530, 65535, 25));
}

// --- Timestamps -----------------------------------------------------------

void test_last_sample_is_now_and_first_goes_back(void) {
  const uint64_t last = 1790199209000ULL;
  TEST_ASSERT_TRUE(ppg_sample_ts_ms(last, 400, 399, 20) == last);
  TEST_ASSERT_TRUE(ppg_sample_ts_ms(last, 400, 0, 20) == last - 399ULL * 20ULL);
}

void test_timestamps_are_contiguous_across_a_chunk_boundary(void) {
  // Los trozos se mandan en momentos distintos, pero el ts sale de la muestra,
  // no del reloj al mandar: la onda queda continua en TB.
  const uint64_t last = 1790199209000ULL;
  TEST_ASSERT_TRUE(ppg_sample_ts_ms(last, 400, 25, 20) -
                       ppg_sample_ts_ms(last, 400, 24, 20) ==
                   20ULL);
}

// --- Tiempo con vuelta de millis() ----------------------------------------

void test_elapsed_basic(void) {
  TEST_ASSERT_FALSE(ppg_elapsed(1000, 900, 250));
  TEST_ASSERT_TRUE(ppg_elapsed(1150, 900, 250));
}

void test_elapsed_survives_millis_wraparound(void) {
  TEST_ASSERT_TRUE(ppg_elapsed(100, 0xFFFFFF00u, 250));
  TEST_ASSERT_FALSE(ppg_elapsed(10, 0xFFFFFF00u, 1000));
}

// --- Captura automática ---------------------------------------------------

void test_first_attempt_is_immediate_after_boot(void) {
  TEST_ASSERT_TRUE(ppg_autocapture_due(&st, 0, INTERVAL, RETRY));
  TEST_ASSERT_TRUE(ppg_autocapture_due(&st, 3000, INTERVAL, RETRY));
}

void test_after_a_capture_waits_the_full_interval(void) {
  ppg_autocapture_record(&st, 1000, true);
  TEST_ASSERT_FALSE(ppg_autocapture_due(&st, 1000 + RETRY, INTERVAL, RETRY));
  TEST_ASSERT_FALSE(ppg_autocapture_due(&st, 1000 + INTERVAL - 1, INTERVAL, RETRY));
  TEST_ASSERT_TRUE(ppg_autocapture_due(&st, 1000 + INTERVAL, INTERVAL, RETRY));
}

void test_without_signal_retries_soon_not_a_full_interval(void) {
  ppg_autocapture_record(&st, 1000, false);
  TEST_ASSERT_FALSE(ppg_autocapture_due(&st, 1000 + RETRY - 1, INTERVAL, RETRY));
  TEST_ASSERT_TRUE(ppg_autocapture_due(&st, 1000 + RETRY, INTERVAL, RETRY));
}

void test_failed_retries_do_not_shorten_the_interval_after_a_capture(void) {
  // Captura a t=1000; si luego se quita el dedo, los reintentos sin señal no
  // deben adelantar la siguiente captura: sigue tocando a 1000 + INTERVAL.
  ppg_autocapture_record(&st, 1000, true);
  const uint32_t t = 1000 + INTERVAL;
  TEST_ASSERT_TRUE(ppg_autocapture_due(&st, t, INTERVAL, RETRY));
  ppg_autocapture_record(&st, t, false);
  TEST_ASSERT_FALSE(ppg_autocapture_due(&st, t + RETRY - 1, INTERVAL, RETRY));
  TEST_ASSERT_TRUE(ppg_autocapture_due(&st, t + RETRY, INTERVAL, RETRY));
}

void test_autocapture_survives_millis_wraparound(void) {
  ppg_autocapture_record(&st, 0xFFFFF000u, true);
  TEST_ASSERT_FALSE(ppg_autocapture_due(&st, 0x00000100u, INTERVAL, RETRY));
  TEST_ASSERT_TRUE(
      ppg_autocapture_due(&st, 0xFFFFF000u + INTERVAL, INTERVAL, RETRY));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_chunk_end_takes_a_full_chunk);
  RUN_TEST(test_chunk_end_clamps_the_last_partial_chunk);
  RUN_TEST(test_chunks_cover_every_sample_exactly_once);
  RUN_TEST(test_chunk_end_never_passes_n_even_near_uint16_max);
  RUN_TEST(test_last_sample_is_now_and_first_goes_back);
  RUN_TEST(test_timestamps_are_contiguous_across_a_chunk_boundary);
  RUN_TEST(test_elapsed_basic);
  RUN_TEST(test_elapsed_survives_millis_wraparound);
  RUN_TEST(test_first_attempt_is_immediate_after_boot);
  RUN_TEST(test_after_a_capture_waits_the_full_interval);
  RUN_TEST(test_without_signal_retries_soon_not_a_full_interval);
  RUN_TEST(test_failed_retries_do_not_shorten_the_interval_after_a_capture);
  RUN_TEST(test_autocapture_survives_millis_wraparound);
  return UNITY_END();
}
