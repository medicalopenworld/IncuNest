#include <unity.h>

#include "modules/util/time_protocol.h"

// Centinelas: si el parser toca una salida en un caso que debe rechazar, el
// assert lo caza. Nada de comprobar solo el bool de retorno — el peligro no es
// que devuelva true de mas, es que deje un dato a medias.
static uint32_t epoch;
static int tzq;
static int tzsrc;

void setUp(void) {
  epoch = 0xDEADBEEFu;
  tzq = 99;
  tzsrc = 99;
}
void tearDown(void) {}

static void assert_untouched(void) {
  TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFu, epoch);
  TEST_ASSERT_EQUAL_INT(99, tzq);
  TEST_ASSERT_EQUAL_INT(99, tzsrc);
}

// --- Linea buena ----------------------------------------------------------

void test_parses_valid_line(void) {
  TEST_ASSERT_TRUE(time_protocol_parse_rtc_seed(
      "HMI,RTC_TIME,1789000000,8,1", &epoch, &tzq, &tzsrc));
  TEST_ASSERT_EQUAL_UINT32(1789000000u, epoch);
  TEST_ASSERT_EQUAL_INT(8, tzq);
  TEST_ASSERT_EQUAL_INT(1, tzsrc);
}

// El enlace entrega las lineas con el salto incluido segun por donde se lea.
void test_accepts_trailing_newline(void) {
  TEST_ASSERT_TRUE(time_protocol_parse_rtc_seed(
      "HMI,RTC_TIME,1789000000,-20,2\n", &epoch, &tzq, &tzsrc));
  TEST_ASSERT_EQUAL_UINT32(1789000000u, epoch);
  TEST_ASSERT_EQUAL_INT(-20, tzq);
  TEST_ASSERT_EQUAL_INT(2, tzsrc);
}

void test_accepts_crlf(void) {
  TEST_ASSERT_TRUE(time_protocol_parse_rtc_seed(
      "HMI,RTC_TIME,1789000000,0,0\r\n", &epoch, &tzq, &tzsrc));
  TEST_ASSERT_EQUAL_UINT32(1789000000u, epoch);
}

// El caso normal de un HMI que tiene hora guardada pero no huso: su NVS esta
// vacia porque nunca llego a resolverse la zona.
void test_accepts_zero_timezone(void) {
  TEST_ASSERT_TRUE(time_protocol_parse_rtc_seed(
      "HMI,RTC_TIME,1789000000,0,0", &epoch, &tzq, &tzsrc));
  TEST_ASSERT_EQUAL_INT(0, tzq);
  TEST_ASSERT_EQUAL_INT(0, tzsrc);
}

// --- Prefijo --------------------------------------------------------------

void test_rejects_other_message(void) {
  TEST_ASSERT_FALSE(time_protocol_parse_rtc_seed(
      "HMI,SET_TIME,2026,9,16,12,0", &epoch, &tzq, &tzsrc));
  assert_untouched();
}

// Una linea cortada a media palabra no debe leer fuera del buffer ni colarse.
void test_rejects_truncated_prefix(void) {
  TEST_ASSERT_FALSE(
      time_protocol_parse_rtc_seed("HMI,RTC_TI", &epoch, &tzq, &tzsrc));
  TEST_ASSERT_FALSE(time_protocol_parse_rtc_seed("", &epoch, &tzq, &tzsrc));
  assert_untouched();
}

void test_rejects_null_line(void) {
  TEST_ASSERT_FALSE(
      time_protocol_parse_rtc_seed(nullptr, &epoch, &tzq, &tzsrc));
  assert_untouched();
}

// --- Numero de campos -----------------------------------------------------

void test_rejects_missing_field(void) {
  TEST_ASSERT_FALSE(time_protocol_parse_rtc_seed("HMI,RTC_TIME,1789000000,8",
                                                 &epoch, &tzq, &tzsrc));
  assert_untouched();
}

void test_rejects_no_fields_at_all(void) {
  TEST_ASSERT_FALSE(
      time_protocol_parse_rtc_seed("HMI,RTC_TIME,", &epoch, &tzq, &tzsrc));
  assert_untouched();
}

// Sobrar campos tambien se rechaza: aqui no aplica la tolerancia de CTRL,TIME,
// porque esto no es un mensaje con campos opcionales sino uno de longitud fija.
void test_rejects_extra_field(void) {
  TEST_ASSERT_FALSE(time_protocol_parse_rtc_seed(
      "HMI,RTC_TIME,1789000000,8,1,7", &epoch, &tzq, &tzsrc));
  assert_untouched();
}

void test_rejects_empty_field(void) {
  TEST_ASSERT_FALSE(time_protocol_parse_rtc_seed("HMI,RTC_TIME,,8,1", &epoch,
                                                 &tzq, &tzsrc));
  assert_untouched();
}

// --- Campos no numericos --------------------------------------------------

void test_rejects_non_numeric_field(void) {
  TEST_ASSERT_FALSE(time_protocol_parse_rtc_seed("HMI,RTC_TIME,abc,8,1",
                                                 &epoch, &tzq, &tzsrc));
  assert_untouched();
}

// El caso que sscanf("%d") dejaria pasar sin decir nada, leyendo 1789000000 y
// tirando el resto. Con un enlace sin CRC, "basura pegada a un numero bueno"
// es exactamente la forma que tiene la corrupcion de parecer valida.
void test_rejects_trailing_garbage_in_number(void) {
  TEST_ASSERT_FALSE(time_protocol_parse_rtc_seed(
      "HMI,RTC_TIME,1789000000abc,8,1", &epoch, &tzq, &tzsrc));
  assert_untouched();
}

void test_rejects_lone_sign(void) {
  TEST_ASSERT_FALSE(time_protocol_parse_rtc_seed("HMI,RTC_TIME,1789000000,-,1",
                                                 &epoch, &tzq, &tzsrc));
  assert_untouched();
}

// --- Rangos ---------------------------------------------------------------

// Lo que devuelve un PCF8563 con la pila agotada.
void test_rejects_epoch_of_1970(void) {
  TEST_ASSERT_FALSE(
      time_protocol_parse_rtc_seed("HMI,RTC_TIME,0,0,0", &epoch, &tzq, &tzsrc));
  assert_untouched();
}

void test_rejects_epoch_before_window(void) {
  TEST_ASSERT_FALSE(time_protocol_parse_rtc_seed("HMI,RTC_TIME,1609459199,0,0",
                                                 &epoch, &tzq, &tzsrc));
  assert_untouched();
}

void test_rejects_epoch_from_2100(void) {
  TEST_ASSERT_FALSE(time_protocol_parse_rtc_seed("HMI,RTC_TIME,4102444800,0,0",
                                                 &epoch, &tzq, &tzsrc));
  assert_untouched();
}

void test_accepts_window_extremes(void) {
  TEST_ASSERT_TRUE(time_protocol_parse_rtc_seed("HMI,RTC_TIME,1609459200,0,0",
                                                &epoch, &tzq, &tzsrc));
  TEST_ASSERT_EQUAL_UINT32(1609459200u, epoch);
  TEST_ASSERT_TRUE(time_protocol_parse_rtc_seed("HMI,RTC_TIME,4102444799,0,0",
                                                &epoch, &tzq, &tzsrc));
  TEST_ASSERT_EQUAL_UINT32(4102444799u, epoch);
}

void test_rejects_timezone_out_of_range(void) {
  TEST_ASSERT_FALSE(time_protocol_parse_rtc_seed("HMI,RTC_TIME,1789000000,57,0",
                                                 &epoch, &tzq, &tzsrc));
  TEST_ASSERT_FALSE(time_protocol_parse_rtc_seed(
      "HMI,RTC_TIME,1789000000,-49,0", &epoch, &tzq, &tzsrc));
  assert_untouched();
}

// Nepal, UTC+5:45. El huso va en cuartos de hora justamente por esto.
void test_accepts_non_integer_timezone(void) {
  TEST_ASSERT_TRUE(time_protocol_parse_rtc_seed("HMI,RTC_TIME,1789000000,23,1",
                                                &epoch, &tzq, &tzsrc));
  TEST_ASSERT_EQUAL_INT(23, tzq);
}

void test_rejects_tzsrc_out_of_range(void) {
  TEST_ASSERT_FALSE(time_protocol_parse_rtc_seed("HMI,RTC_TIME,1789000000,0,4",
                                                 &epoch, &tzq, &tzsrc));
  TEST_ASSERT_FALSE(time_protocol_parse_rtc_seed("HMI,RTC_TIME,1789000000,0,-1",
                                                 &epoch, &tzq, &tzsrc));
  assert_untouched();
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_parses_valid_line);
  RUN_TEST(test_accepts_trailing_newline);
  RUN_TEST(test_accepts_crlf);
  RUN_TEST(test_accepts_zero_timezone);
  RUN_TEST(test_rejects_other_message);
  RUN_TEST(test_rejects_truncated_prefix);
  RUN_TEST(test_rejects_null_line);
  RUN_TEST(test_rejects_missing_field);
  RUN_TEST(test_rejects_no_fields_at_all);
  RUN_TEST(test_rejects_extra_field);
  RUN_TEST(test_rejects_empty_field);
  RUN_TEST(test_rejects_non_numeric_field);
  RUN_TEST(test_rejects_trailing_garbage_in_number);
  RUN_TEST(test_rejects_lone_sign);
  RUN_TEST(test_rejects_epoch_of_1970);
  RUN_TEST(test_rejects_epoch_before_window);
  RUN_TEST(test_rejects_epoch_from_2100);
  RUN_TEST(test_accepts_window_extremes);
  RUN_TEST(test_rejects_timezone_out_of_range);
  RUN_TEST(test_accepts_non_integer_timezone);
  RUN_TEST(test_rejects_tzsrc_out_of_range);
  return UNITY_END();
}
