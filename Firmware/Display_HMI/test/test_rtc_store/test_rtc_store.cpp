#include <unity.h>

#include "drivers/rtc_store.h"

void setUp(void) {}
void tearDown(void) {}

static void roundtrip(int8_t q, uint8_t tzsrc, Proto_TimeSource src) {
  const RtcStoredTz in = {q, tzsrc, src};
  RtcStoredTz out = {99, 99, PROTO_TIME_SOURCE_MANUAL};
  TEST_ASSERT_TRUE(rtc_store_unpack(rtc_store_pack(&in), &out));
  TEST_ASSERT_EQUAL_INT(q, out.tzQuarters);
  TEST_ASSERT_EQUAL_UINT8(tzsrc, out.tzSource);
  TEST_ASSERT_EQUAL_INT(src, out.src);
}

void test_roundtrips_typical(void) {
  roundtrip(8, 1, PROTO_TIME_SOURCE_NTP);   // UTC+2, NITZ, NTP
  roundtrip(0, 3, PROTO_TIME_SOURCE_MANUAL); // puesto a mano
  roundtrip(4, 2, PROTO_TIME_SOURCE_RTC);
}

// Nepal (UTC+5:45) y los extremos reales del rango. El huso va en cuartos de
// hora justamente porque existen husos no enteros.
void test_roundtrips_range_extremes(void) {
  roundtrip(23, 1, PROTO_TIME_SOURCE_NTP);  // UTC+5:45
  roundtrip(-48, 1, PROTO_TIME_SOURCE_NTP); // UTC-12:00
  roundtrip(56, 1, PROTO_TIME_SOURCE_NTP);  // UTC+14:00
}

// El caso que justifica la marca de presencia: esta terna es legitima y todos
// sus campos valen 0. Sin marca seria indistinguible de una NVS virgen, y el
// equipo re-sembraria creyendo que no tiene nada guardado.
void test_all_zero_tuple_is_not_empty(void) {
  const RtcStoredTz in = {0, 0, PROTO_TIME_SOURCE_NONE};
  const uint32_t word = rtc_store_pack(&in);
  TEST_ASSERT_NOT_EQUAL_UINT32(RTC_STORE_EMPTY, word);
  RtcStoredTz out = {99, 99, PROTO_TIME_SOURCE_MANUAL};
  TEST_ASSERT_TRUE(rtc_store_unpack(word, &out));
  TEST_ASSERT_EQUAL_INT(0, out.tzQuarters);
  TEST_ASSERT_EQUAL_INT(PROTO_TIME_SOURCE_NONE, out.src);
}

// Una NVS que nunca se escribio devuelve 0.
void test_empty_word_is_rejected(void) {
  RtcStoredTz out = {99, 99, PROTO_TIME_SOURCE_MANUAL};
  TEST_ASSERT_FALSE(rtc_store_unpack(RTC_STORE_EMPTY, &out));
  TEST_ASSERT_EQUAL_INT(0, out.tzQuarters);
  TEST_ASSERT_EQUAL_UINT8(0, out.tzSource);
  TEST_ASSERT_EQUAL_INT(PROTO_TIME_SOURCE_NONE, out.src);
}

// Una NVS corrupta no debe producir un huso plausible pero falso: mejor pintar
// la hora sin offset, que es lo que el firmware ya hace sin zona conocida.
void test_corrupt_word_falls_back_to_unknown(void) {
  RtcStoredTz out = {99, 99, PROTO_TIME_SOURCE_MANUAL};
  TEST_ASSERT_FALSE(rtc_store_unpack(0xFFFFFFFFu, &out));
  TEST_ASSERT_EQUAL_INT(0, out.tzQuarters);
  TEST_ASSERT_EQUAL_INT(PROTO_TIME_SOURCE_NONE, out.src);
}

void test_rejects_out_of_range_timezone(void) {
  const RtcStoredTz tooLow = {-49, 1, PROTO_TIME_SOURCE_NTP};
  const RtcStoredTz tooHigh = {57, 1, PROTO_TIME_SOURCE_NTP};
  RtcStoredTz out;
  TEST_ASSERT_FALSE(rtc_store_unpack(rtc_store_pack(&tooLow), &out));
  TEST_ASSERT_FALSE(rtc_store_unpack(rtc_store_pack(&tooHigh), &out));
}

void test_rejects_null(void) {
  TEST_ASSERT_EQUAL_UINT32(RTC_STORE_EMPTY, rtc_store_pack(nullptr));
  TEST_ASSERT_FALSE(rtc_store_unpack(0x12345u, nullptr));
}

// Los tres campos tienen que sobrevivir juntos: si uno pisara a otro por un
// desplazamiento mal puesto, esto lo caza.
void test_fields_do_not_overlap(void) {
  for (int q = -48; q <= 56; q++) {
    roundtrip((int8_t)q, 3, PROTO_TIME_SOURCE_MANUAL);
    roundtrip((int8_t)q, 0, PROTO_TIME_SOURCE_NITZ);
  }
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_roundtrips_typical);
  RUN_TEST(test_roundtrips_range_extremes);
  RUN_TEST(test_all_zero_tuple_is_not_empty);
  RUN_TEST(test_empty_word_is_rejected);
  RUN_TEST(test_corrupt_word_falls_back_to_unknown);
  RUN_TEST(test_rejects_out_of_range_timezone);
  RUN_TEST(test_rejects_null);
  RUN_TEST(test_fields_do_not_overlap);
  return UNITY_END();
}
