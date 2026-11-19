#include <unity.h>

#include "drivers/hum_en_policy.h"

void setUp(void) {}
void tearDown(void) {}

// Equipo con firmware anterior y el control de humedad deshabilitado: el
// primer arranque con este firmware lo habilita y deja la marca.
void test_forces_on_existing_unit_with_it_off(void) {
  HumEnBoot r = hum_en_on_boot(false, true, 0);
  TEST_ASSERT_EQUAL_UINT8(1, r.value);
  TEST_ASSERT_TRUE(r.write);
}

void test_forces_on_existing_unit_without_key(void) {
  HumEnBoot r = hum_en_on_boot(false, false, 0);
  TEST_ASSERT_EQUAL_UINT8(1, r.value);
  TEST_ASSERT_TRUE(r.write);
}

// Ya forzado una vez: si alguien lo apago luego desde Ajustes, se respeta.
void test_respects_user_off_after_forcing(void) {
  HumEnBoot r = hum_en_on_boot(true, true, 0);
  TEST_ASSERT_EQUAL_UINT8(0, r.value);
  TEST_ASSERT_FALSE(r.write);
}

void test_keeps_on_after_forcing(void) {
  HumEnBoot r = hum_en_on_boot(true, true, 1);
  TEST_ASSERT_EQUAL_UINT8(1, r.value);
  TEST_ASSERT_FALSE(r.write);
}

// Marca puesta pero sin clave (NVS a medias): el defecto es habilitado.
void test_default_is_on_when_key_missing(void) {
  HumEnBoot r = hum_en_on_boot(true, false, 0);
  TEST_ASSERT_EQUAL_UINT8(1, r.value);
  TEST_ASSERT_FALSE(r.write);
}

void test_default_constant_is_on(void) {
  TEST_ASSERT_EQUAL_UINT8(1, HUM_EN_DEFAULT);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_forces_on_existing_unit_with_it_off);
  RUN_TEST(test_forces_on_existing_unit_without_key);
  RUN_TEST(test_respects_user_off_after_forcing);
  RUN_TEST(test_keeps_on_after_forcing);
  RUN_TEST(test_default_is_on_when_key_missing);
  RUN_TEST(test_default_constant_is_on);
  return UNITY_END();
}
