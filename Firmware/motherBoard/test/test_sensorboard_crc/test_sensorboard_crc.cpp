#include <unity.h>

#include "modules/sensorboard_comm/sb_crc16.h"

void setUp(void) {}
void tearDown(void) {}

// Mismos vectores que SensorBoard_v2/test_apps/comm_test/main/test_main.c:
// el CRC tiene que dar el mismo valor en los dos lados del enlace.
void test_known_vector_123456789(void) {
  const uint8_t data[] = "123456789";
  TEST_ASSERT_EQUAL_HEX16(0x29B1, sb_crc16(data, 9));
}

void test_empty_data_returns_init_value(void) {
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, sb_crc16(NULL, 0));
}

void test_single_byte_zero(void) {
  const uint8_t data[] = {0x00};
  TEST_ASSERT_EQUAL_HEX16(0xE1F0, sb_crc16(data, 1));
}

void test_incremental_equals_batch(void) {
  const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
  uint16_t batch = sb_crc16(data, 4);
  uint16_t inc = 0xFFFFu;
  for (size_t i = 0; i < 4; i++) {
    inc = sb_crc16_byte(inc, data[i]);
  }
  TEST_ASSERT_EQUAL_HEX16(batch, inc);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_known_vector_123456789);
  RUN_TEST(test_empty_data_returns_init_value);
  RUN_TEST(test_single_byte_zero);
  RUN_TEST(test_incremental_equals_batch);
  return UNITY_END();
}
