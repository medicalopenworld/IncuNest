// Tope del calefactor segun la fuente, motherBoard V19
// (Hardware/Electronics/Motherboard/CHANGELOG.md, firmware 6).

#include <unity.h>

#include "modules/control/heater_supply_limit.h"

void setUp(void) {}
void tearDown(void) {}

static void test_sin_chrg_ok_es_bateria_aunque_haya_vbus(void) {
  TEST_ASSERT_EQUAL(HEATER_SUPPLY_BATTERY,
                    heater_supply_classify(false, true, 24000));
  TEST_ASSERT_EQUAL(HEATER_SUPPLY_BATTERY,
                    heater_supply_classify(false, false, 0));
}

static void test_adaptador_de_24v(void) {
  TEST_ASSERT_EQUAL(HEATER_SUPPLY_ADAPTER,
                    heater_supply_classify(true, true, 24000));
}

static void test_coche_y_franja_dudosa(void) {
  TEST_ASSERT_EQUAL(HEATER_SUPPLY_CAR, heater_supply_classify(true, true, 13800));
  TEST_ASSERT_EQUAL(HEATER_SUPPLY_CAR, heater_supply_classify(true, true, 17000));
  // Justo en el umbral no cuenta como adaptador.
  TEST_ASSERT_EQUAL(HEATER_SUPPLY_CAR, heater_supply_classify(true, true, 18000));
}

static void test_vbus_desconocido_con_chrg_ok_es_coche(void) {
  TEST_ASSERT_EQUAL(HEATER_SUPPLY_CAR, heater_supply_classify(true, false, 24000));
}

static void test_topes(void) {
  TEST_ASSERT_EQUAL_UINT16(255, heater_supply_max_pwm(HEATER_SUPPLY_ADAPTER, 255));
  TEST_ASSERT_EQUAL_UINT16(127, heater_supply_max_pwm(HEATER_SUPPLY_CAR, 255));
  TEST_ASSERT_EQUAL_UINT16(102, heater_supply_max_pwm(HEATER_SUPPLY_BATTERY, 255));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_sin_chrg_ok_es_bateria_aunque_haya_vbus);
  RUN_TEST(test_adaptador_de_24v);
  RUN_TEST(test_coche_y_franja_dudosa);
  RUN_TEST(test_vbus_desconocido_con_chrg_ok_es_coche);
  RUN_TEST(test_topes);
  return UNITY_END();
}
