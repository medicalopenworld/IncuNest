// Politica de carga de la bateria segun VBAT (motherBoard V19,
// Hardware/Electronics/Motherboard/CHANGELOG.md, firmware 5): no cargar por
// encima de 15 V (V_SYS alimenta los TPS563231 de 17 V y el gate de Q8 tiene
// un zener de 15 V) y, por debajo de 10,5 V, cargar con corriente reducida en
// vez de no cargar (una AGM muy descargada tiene que poder recuperarse).

#include <unity.h>

#include "modules/control/charge_policy.h"

static ChargePolicy p;

void setUp(void) { charge_policy_init(&p); }
void tearDown(void) {}

static ChargePolicyMode upd(uint16_t mv) { return charge_policy_update(&p, true, mv); }

void test_starts_normal(void) { TEST_ASSERT_EQUAL(CHG_POLICY_NORMAL, p.mode); }

void test_normal_band_stays_normal(void) {
  TEST_ASSERT_EQUAL(CHG_POLICY_NORMAL, upd(12200));
  TEST_ASSERT_EQUAL(CHG_POLICY_NORMAL, upd(14400));
  TEST_ASSERT_EQUAL(CHG_POLICY_NORMAL, upd(10500));
  TEST_ASSERT_EQUAL(CHG_POLICY_NORMAL, upd(15000));
}

void test_above_15v_inhibits_with_hysteresis(void) {
  TEST_ASSERT_EQUAL(CHG_POLICY_INHIBIT, upd(15100));
  TEST_ASSERT_EQUAL(CHG_POLICY_INHIBIT, upd(14800));  // aun por encima de la liberacion
  TEST_ASSERT_EQUAL(CHG_POLICY_NORMAL, upd(14600));
}

void test_below_10v5_reduces_with_hysteresis(void) {
  TEST_ASSERT_EQUAL(CHG_POLICY_REDUCED, upd(10400));
  TEST_ASSERT_EQUAL(CHG_POLICY_REDUCED, upd(10900));  // aun por debajo de 11 V
  TEST_ASSERT_EQUAL(CHG_POLICY_NORMAL, upd(11100));
}

void test_deep_discharge_still_charges(void) {
  // Nunca inhibe por tension baja: reduce.
  TEST_ASSERT_EQUAL(CHG_POLICY_REDUCED, upd(6000));
}

void test_invalid_reading_keeps_mode(void) {
  upd(15200);
  TEST_ASSERT_EQUAL(CHG_POLICY_INHIBIT, charge_policy_update(&p, false, 12000));
  upd(14000);
  upd(9000);
  TEST_ASSERT_EQUAL(CHG_POLICY_REDUCED, charge_policy_update(&p, false, 14000));
}

void test_reduced_jumps_to_inhibit_above_15v(void) {
  upd(9000);
  TEST_ASSERT_EQUAL(CHG_POLICY_INHIBIT, upd(15200));
}

void test_reduced_current_is_a_valid_bq_step(void) {
  // ChargeCurrent del BQ25730 va en pasos de 128 mA con 5 mOhm.
  TEST_ASSERT_EQUAL_UINT16(0, CHG_POLICY_REDUCED_MA % 128);
  TEST_ASSERT_TRUE(CHG_POLICY_REDUCED_MA > 0);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_normal);
  RUN_TEST(test_normal_band_stays_normal);
  RUN_TEST(test_above_15v_inhibits_with_hysteresis);
  RUN_TEST(test_below_10v5_reduces_with_hysteresis);
  RUN_TEST(test_deep_discharge_still_charges);
  RUN_TEST(test_invalid_reading_keeps_mode);
  RUN_TEST(test_reduced_jumps_to_inhibit_above_15v);
  RUN_TEST(test_reduced_current_is_a_valid_bq_step);
  return UNITY_END();
}
