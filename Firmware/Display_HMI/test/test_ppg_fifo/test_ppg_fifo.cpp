#include <unity.h>

#include "modules/comm/ppg_fifo.h"

void setUp(void) {}
void tearDown(void) {}

static PpgFifo fresh(void) {
  PpgFifo f = {{0}, 0, 0, 0};
  return f;
}

void test_pop_on_empty_fails_and_leaves_output(void) {
  PpgFifo f = fresh();
  uint8_t v = 0xAB;
  TEST_ASSERT_FALSE(ppg_fifo_pop(&f, &v));
  TEST_ASSERT_EQUAL_UINT8(0xAB, v);
}

void test_keeps_order(void) {
  PpgFifo f = fresh();
  for (uint8_t i = 0; i < 10; i++) TEST_ASSERT_TRUE(ppg_fifo_push(&f, (uint8_t)(100 + i)));
  for (uint8_t i = 0; i < 10; i++) {
    uint8_t v = 0;
    TEST_ASSERT_TRUE(ppg_fifo_pop(&f, &v));
    TEST_ASSERT_EQUAL_UINT8(100 + i, v);
  }
  uint8_t v = 0;
  TEST_ASSERT_FALSE(ppg_fifo_pop(&f, &v));
}

// A full buffer drops the NEW sample and counts it; what was queued survives.
void test_full_drops_newest_and_counts(void) {
  PpgFifo f = fresh();
  for (int i = 0; i < PPG_FIFO_LEN; i++) TEST_ASSERT_TRUE(ppg_fifo_push(&f, (uint8_t)i));
  TEST_ASSERT_FALSE(ppg_fifo_push(&f, 200));
  TEST_ASSERT_FALSE(ppg_fifo_push(&f, 201));
  TEST_ASSERT_EQUAL_UINT16(2, f.dropped);
  for (int i = 0; i < PPG_FIFO_LEN; i++) {
    uint8_t v = 0;
    TEST_ASSERT_TRUE(ppg_fifo_pop(&f, &v));
    TEST_ASSERT_EQUAL_UINT8(i, v);
  }
  // Room again once drained.
  TEST_ASSERT_TRUE(ppg_fifo_push(&f, 7));
}

// Capacity is the full length even when the data straddles the end of buf.
void test_capacity_across_the_end_of_the_array(void) {
  PpgFifo f = fresh();
  uint8_t v = 0;
  for (int i = 0; i < 20; i++) ppg_fifo_push(&f, (uint8_t)i);
  for (int i = 0; i < 10; i++) ppg_fifo_pop(&f, &v);
  for (int i = 0; i < PPG_FIFO_LEN - 10; i++) TEST_ASSERT_TRUE(ppg_fifo_push(&f, (uint8_t)(50 + i)));
  TEST_ASSERT_FALSE(ppg_fifo_push(&f, 99));
  for (int i = 10; i < 20; i++) {
    TEST_ASSERT_TRUE(ppg_fifo_pop(&f, &v));
    TEST_ASSERT_EQUAL_UINT8(i, v);
  }
  for (int i = 0; i < PPG_FIFO_LEN - 10; i++) {
    TEST_ASSERT_TRUE(ppg_fifo_pop(&f, &v));
    TEST_ASSERT_EQUAL_UINT8(50 + i, v);
  }
}

// head and tail are uint8_t and wrap every 256 samples (~5 s at 50 Hz): order
// and values must survive many wraps, at every fill level the UI might leave.
void test_survives_counter_wraparound(void) {
  PpgFifo f = fresh();
  uint8_t next_in = 0, next_out = 0, v = 0;
  for (int round = 0; round < 2000; round++) {
    const int burst = 1 + (round % 7);  // 1..7 samples per UI pass
    for (int i = 0; i < burst; i++) TEST_ASSERT_TRUE(ppg_fifo_push(&f, next_in++));
    const int drain = (round % 3 == 0) ? burst - 1 : burst;  // sometimes leave one queued
    for (int i = 0; i < drain; i++) {
      TEST_ASSERT_TRUE(ppg_fifo_pop(&f, &v));
      TEST_ASSERT_EQUAL_UINT8(next_out++, v);
    }
    if ((uint8_t)(f.head - f.tail) > PPG_FIFO_LEN / 2) {  // keep it from filling
      while (ppg_fifo_pop(&f, &v)) TEST_ASSERT_EQUAL_UINT8(next_out++, v);
    }
  }
  while (ppg_fifo_pop(&f, &v)) TEST_ASSERT_EQUAL_UINT8(next_out++, v);
  TEST_ASSERT_EQUAL_UINT8(next_in, next_out);
  TEST_ASSERT_EQUAL_UINT16(0, f.dropped);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_pop_on_empty_fails_and_leaves_output);
  RUN_TEST(test_keeps_order);
  RUN_TEST(test_full_drops_newest_and_counts);
  RUN_TEST(test_capacity_across_the_end_of_the_array);
  RUN_TEST(test_survives_counter_wraparound);
  return UNITY_END();
}
