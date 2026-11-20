#include <unity.h>

#include "civil_time.h"

void setUp(void) {}
void tearDown(void) {}

void test_epoch_zero_reference(void) {
  TEST_ASSERT_EQUAL_INT64(0, civil_days_from_epoch(1970, 1, 1));
}

void test_known_dates(void) {
  // 2021-01-01 is 18628 days after the epoch.
  TEST_ASSERT_EQUAL_INT64(18628, civil_days_from_epoch(2021, 1, 1));
  // Leap-day handling.
  TEST_ASSERT_EQUAL_INT64(civil_days_from_epoch(2024, 2, 28) + 1,
                          civil_days_from_epoch(2024, 2, 29));
  TEST_ASSERT_EQUAL_INT64(civil_days_from_epoch(2024, 2, 29) + 1,
                          civil_days_from_epoch(2024, 3, 1));
  // 1900 is NOT a leap year, 2000 IS (the century rules).
  TEST_ASSERT_EQUAL_INT64(civil_days_from_epoch(1900, 2, 28) + 1,
                          civil_days_from_epoch(1900, 3, 1));
  TEST_ASSERT_EQUAL_INT64(civil_days_from_epoch(2000, 2, 28) + 2,
                          civil_days_from_epoch(2000, 3, 1));
}

void test_utc_conversion_no_offset(void) {
  uint32_t e = 0;
  // 2021-01-01T00:00:00Z
  TEST_ASSERT_TRUE(civil_to_unix_utc(2021, 1, 1, 0, 0, 0, 0, &e));
  TEST_ASSERT_EQUAL_UINT32(1609459200u, e);
}

void test_utc_conversion_applies_timezone(void) {
  uint32_t utc = 0, plus2 = 0;
  // Same instant: 12:00 UTC == 14:00 at UTC+2 (+8 quarter hours).
  TEST_ASSERT_TRUE(civil_to_unix_utc(2024, 6, 15, 12, 0, 0, 0, &utc));
  TEST_ASSERT_TRUE(civil_to_unix_utc(2024, 6, 15, 14, 0, 0, 8, &plus2));
  TEST_ASSERT_EQUAL_UINT32(utc, plus2);
}

void test_utc_conversion_negative_timezone(void) {
  uint32_t utc = 0, minus5 = 0;
  TEST_ASSERT_TRUE(civil_to_unix_utc(2024, 6, 15, 12, 0, 0, 0, &utc));
  TEST_ASSERT_TRUE(civil_to_unix_utc(2024, 6, 15, 7, 0, 0, -20, &minus5));
  TEST_ASSERT_EQUAL_UINT32(utc, minus5);
}

// The whole point of the 2021 floor: an unsynced SIM800 reports 2004-01-01.
void test_rejects_unsynced_modem_default_date(void) {
  uint32_t e = 0xDEADBEEF;
  TEST_ASSERT_FALSE(civil_to_unix_utc(2004, 1, 1, 0, 0, 0, 0, &e));
  TEST_ASSERT_EQUAL_UINT32(0xDEADBEEF, e); // untouched on failure
}

void test_rejects_out_of_range_fields(void) {
  uint32_t e = 0;
  TEST_ASSERT_FALSE(civil_to_unix_utc(2024, 0, 1, 0, 0, 0, 0, &e));
  TEST_ASSERT_FALSE(civil_to_unix_utc(2024, 13, 1, 0, 0, 0, 0, &e));
  TEST_ASSERT_FALSE(civil_to_unix_utc(2024, 1, 0, 0, 0, 0, 0, &e));
  TEST_ASSERT_FALSE(civil_to_unix_utc(2024, 1, 32, 0, 0, 0, 0, &e));
  TEST_ASSERT_FALSE(civil_to_unix_utc(2024, 1, 1, 24, 0, 0, 0, &e));
  TEST_ASSERT_FALSE(civil_to_unix_utc(2024, 1, 1, 0, 60, 0, 0, &e));
  TEST_ASSERT_FALSE(civil_to_unix_utc(2024, 1, 1, 0, 0, 0, 99, &e));
}

// --- Sentido inverso: epoch -> fecha civil --------------------------------
//
// Hace falta para ESCRIBIR el PCF8563 del HMI, que guarda campos civiles en
// BCD y no un contador de segundos.

static void check_roundtrip(uint32_t epoch) {
  int y = 0;
  unsigned mo = 0, d = 0, h = 0, mi = 0, s = 0, wd = 0;
  civil_from_unix_utc(epoch, &y, &mo, &d, &h, &mi, &s, &wd);
  uint32_t back = 0;
  TEST_ASSERT_TRUE(civil_to_unix_utc(y, mo, d, h, mi, s, 0, &back));
  TEST_ASSERT_EQUAL_UINT32(epoch, back);
}

void test_inverse_known_dates(void) {
  int y = 0;
  unsigned mo = 0, d = 0, h = 0, mi = 0, s = 0, wd = 0;

  // 2021-01-01T00:00:00Z, viernes. Es el suelo de la ventana valida.
  civil_from_unix_utc(1609459200u, &y, &mo, &d, &h, &mi, &s, &wd);
  TEST_ASSERT_EQUAL_INT(2021, y);
  TEST_ASSERT_EQUAL_UINT(1, mo);
  TEST_ASSERT_EQUAL_UINT(1, d);
  TEST_ASSERT_EQUAL_UINT(0, h);
  TEST_ASSERT_EQUAL_UINT(0, mi);
  TEST_ASSERT_EQUAL_UINT(0, s);
  TEST_ASSERT_EQUAL_UINT(5, wd); // viernes

  // 2024-02-29T23:59:59Z, jueves. Bisiesto, que es donde se rompen las
  // conversiones escritas a mano.
  civil_from_unix_utc(1709251199u, &y, &mo, &d, &h, &mi, &s, &wd);
  TEST_ASSERT_EQUAL_INT(2024, y);
  TEST_ASSERT_EQUAL_UINT(2, mo);
  TEST_ASSERT_EQUAL_UINT(29, d);
  TEST_ASSERT_EQUAL_UINT(23, h);
  TEST_ASSERT_EQUAL_UINT(59, mi);
  TEST_ASSERT_EQUAL_UINT(59, s);
  TEST_ASSERT_EQUAL_UINT(4, wd); // jueves
}

// 1970-01-01 fue jueves; si el +4 del calculo se pierde, esto lo caza.
void test_inverse_weekday_cycles(void) {
  unsigned wd = 0;
  for (unsigned i = 0; i < 7; i++) {
    civil_from_unix_utc(1609459200u + i * 86400u, nullptr, nullptr, nullptr,
                        nullptr, nullptr, nullptr, &wd);
    TEST_ASSERT_EQUAL_UINT((5 + i) % 7, wd);
  }
}

void test_inverse_roundtrips(void) {
  check_roundtrip(1609459200u); // 2021-01-01, suelo de la ventana
  check_roundtrip(1789000000u); // 2026-09-08
  check_roundtrip(2147483647u); // 2038-01-19, el desbordamiento de 32 bits
  check_roundtrip(2147483648u); // el segundo siguiente
  check_roundtrip(4102444799u); // 2099-12-31T23:59:59Z, techo de la ventana
}

// Un dia entero segundo a segundo cada hora: pilla un fallo de resto en la
// division del dia que un puñado de fechas sueltas dejaria pasar.
void test_inverse_covers_a_full_day(void) {
  const uint32_t base = 1789000000u - (1789000000u % 86400u);
  for (unsigned hour = 0; hour < 24; hour++) {
    check_roundtrip(base + hour * 3600u);
    check_roundtrip(base + hour * 3600u + 59u * 60u + 59u);
  }
}

// Los punteros de salida son opcionales: el driver del RTC no siempre los
// quiere todos.
void test_inverse_accepts_null_outputs(void) {
  unsigned d = 0;
  civil_from_unix_utc(1609459200u, nullptr, nullptr, &d, nullptr, nullptr,
                      nullptr, nullptr);
  TEST_ASSERT_EQUAL_UINT(1, d);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_epoch_zero_reference);
  RUN_TEST(test_known_dates);
  RUN_TEST(test_utc_conversion_no_offset);
  RUN_TEST(test_utc_conversion_applies_timezone);
  RUN_TEST(test_utc_conversion_negative_timezone);
  RUN_TEST(test_rejects_unsynced_modem_default_date);
  RUN_TEST(test_rejects_out_of_range_fields);
  RUN_TEST(test_inverse_known_dates);
  RUN_TEST(test_inverse_weekday_cycles);
  RUN_TEST(test_inverse_roundtrips);
  RUN_TEST(test_inverse_covers_a_full_day);
  RUN_TEST(test_inverse_accepts_null_outputs);
  return UNITY_END();
}
