// Indicador de carga de la bateria interna (CTRL,BATT).
//
// Lo que se protege: que sin red el indicador refleje la tension corregida por
// la caida resistiva (un pico de consumo del calefactor no "vacia" la
// bateria), que las rayas solo bajen durante una descarga, que con red diga
// cargando o lleno sin inventarse un nivel, y que un fallo de I2C no cambie
// nada. Ver modules/control/battery_gauge.h.

#include <unity.h>

#include "modules/control/battery_gauge.h"

static BatteryGauge g;

void setUp(void) {
  const BatteryGauge init = BATTERY_GAUGE_INITIALIZER;
  g = init;
}
void tearDown(void) {}

// Muestras sin red con la misma lectura, suficientes para asentar la media.
static void discharge(uint16_t vbat_mv, uint16_t idchg_ma, int n) {
  for (int i = 0; i < n; i++) {
    battery_gauge_sample(&g, true, false, false, vbat_mv, idchg_ma);
  }
}

// ---- Curva ------------------------------------------------------------------

void test_curve_endpoints_and_clamps(void) {
  TEST_ASSERT_EQUAL_UINT8(100, battery_gauge_percent_from_mv(12730));
  TEST_ASSERT_EQUAL_UINT8(100, battery_gauge_percent_from_mv(13500));
  TEST_ASSERT_EQUAL_UINT8(0, battery_gauge_percent_from_mv(10500));
  TEST_ASSERT_EQUAL_UINT8(0, battery_gauge_percent_from_mv(9000));
}

void test_curve_interpolates_between_points(void) {
  TEST_ASSERT_EQUAL_UINT8(50, battery_gauge_percent_from_mv(12100));
  // A medio camino entre 12100 (50 %) y 12240 (60 %).
  TEST_ASSERT_EQUAL_UINT8(55, battery_gauge_percent_from_mv(12170));
}

void test_curve_is_monotonic(void) {
  uint8_t prev = 0;
  for (uint32_t mv = 10000; mv <= 13000; mv += 10) {
    const uint8_t p = battery_gauge_percent_from_mv(mv);
    TEST_ASSERT_TRUE(p >= prev);
    prev = p;
  }
}

void test_levels_from_percent(void) {
  TEST_ASSERT_EQUAL_UINT8(4, battery_gauge_level_from_percent(100));
  TEST_ASSERT_EQUAL_UINT8(4, battery_gauge_level_from_percent(75));
  TEST_ASSERT_EQUAL_UINT8(3, battery_gauge_level_from_percent(74));
  TEST_ASSERT_EQUAL_UINT8(3, battery_gauge_level_from_percent(50));
  TEST_ASSERT_EQUAL_UINT8(2, battery_gauge_level_from_percent(49));
  TEST_ASSERT_EQUAL_UINT8(2, battery_gauge_level_from_percent(25));
  TEST_ASSERT_EQUAL_UINT8(1, battery_gauge_level_from_percent(24));
  TEST_ASSERT_EQUAL_UINT8(1, battery_gauge_level_from_percent(10));
  TEST_ASSERT_EQUAL_UINT8(0, battery_gauge_level_from_percent(9));
  TEST_ASSERT_EQUAL_UINT8(0, battery_gauge_level_from_percent(0));
}

// ---- Estados ----------------------------------------------------------------

void test_unknown_until_a_valid_reading(void) {
  battery_gauge_sample(&g, false, false, false, 12000, 0);
  TEST_ASSERT_EQUAL(BATT_GAUGE_UNKNOWN, g.state);
}

void test_mains_charging_and_full(void) {
  battery_gauge_sample(&g, true, true, false, 14000, 0);
  TEST_ASSERT_EQUAL(BATT_GAUGE_CHARGING, g.state);
  battery_gauge_sample(&g, true, true, true, 13600, 0);
  TEST_ASSERT_EQUAL(BATT_GAUGE_FULL, g.state);
  TEST_ASSERT_EQUAL_UINT8(BATT_GAUGE_LEVEL_MAX, g.level);
  TEST_ASSERT_EQUAL_UINT8(100, g.percent);
}

void test_invalid_reading_changes_nothing(void) {
  discharge(12240, 0, 40);
  const BatteryGauge before = g;
  battery_gauge_sample(&g, false, true, true, 14000, 0);
  TEST_ASSERT_EQUAL(before.state, g.state);
  TEST_ASSERT_EQUAL_UINT8(before.level, g.level);
  TEST_ASSERT_EQUAL_UINT8(before.percent, g.percent);
}

// ---- Descarga ---------------------------------------------------------------

void test_discharge_at_rest_follows_the_curve(void) {
  discharge(12240, 0, 40);
  TEST_ASSERT_EQUAL(BATT_GAUGE_DISCHARGING, g.state);
  TEST_ASSERT_UINT8_WITHIN(1, 60, g.percent);
  TEST_ASSERT_EQUAL_UINT8(3, g.level);
}

void test_discharge_compensates_the_resistive_drop(void) {
  // 6 A por 50 mOhm = 300 mV de caida: 11940 bajo carga son ~12240 en reposo.
  discharge(11940, 6000, 40);
  TEST_ASSERT_UINT8_WITHIN(1, 60, g.percent);
}

void test_a_load_spike_does_not_empty_the_battery(void) {
  discharge(12500, 0, 40);  // 80 %, 4 rayas
  // Un segundo con 300 mV de caida SIN compensar (la corriente se lee con un
  // segundo de retraso respecto a la tension, o cae entre dos pasos del ADC):
  // la media lo amortigua y no cambia de raya.
  battery_gauge_sample(&g, true, false, false, 12200, 0);
  TEST_ASSERT_EQUAL_UINT8(4, g.level);
}

void test_levels_only_go_down_while_discharging(void) {
  discharge(12100, 0, 60);  // 50 %, 3 rayas
  TEST_ASSERT_EQUAL_UINT8(3, g.level);
  discharge(12000, 0, 60);  // ~43 %, 2 rayas
  TEST_ASSERT_EQUAL_UINT8(2, g.level);
  const uint8_t pct = g.percent;
  // Quitar la carga: la tension se recupera, el indicador no.
  discharge(12500, 0, 60);
  TEST_ASSERT_EQUAL_UINT8(2, g.level);
  TEST_ASSERT_EQUAL_UINT8(pct, g.percent);
}

void test_mains_back_forgets_the_discharge(void) {
  discharge(11700, 0, 60);  // ~22 %, 1 raya
  TEST_ASSERT_EQUAL_UINT8(1, g.level);
  battery_gauge_sample(&g, true, true, false, 14000, 0);
  TEST_ASSERT_EQUAL(BATT_GAUGE_CHARGING, g.state);
  // Nueva descarga con la bateria cargada: vuelve a poder mostrar 4.
  discharge(12700, 0, 40);
  TEST_ASSERT_EQUAL_UINT8(4, g.level);
}

void test_first_discharge_sample_seeds_the_average(void) {
  // Sin semilla, la media arrancaria en 0 V y la primera lectura diria vacia.
  battery_gauge_sample(&g, true, false, false, 12500, 0);
  TEST_ASSERT_EQUAL_UINT8(4, g.level);
  TEST_ASSERT_UINT8_WITHIN(1, 80, g.percent);
}

void test_pack_layout(void) {
  discharge(12100, 0, 60);
  const uint32_t p = battery_gauge_pack(&g);
  TEST_ASSERT_EQUAL_UINT32(BATT_GAUGE_DISCHARGING, (p >> 16) & 0xFF);
  TEST_ASSERT_EQUAL_UINT32(g.level, (p >> 8) & 0xFF);
  TEST_ASSERT_EQUAL_UINT32(g.percent, p & 0xFF);
}

// ---- Bateria baja (ALARM_BATTERY_LOW) ---------------------------------------

void test_low_only_while_discharging_at_or_below_the_threshold(void) {
  discharge(11660, 0, 60);  // 20 %
  TEST_ASSERT_FALSE(battery_gauge_report_low(battery_gauge_pack(&g)));
  discharge(11510, 0, 60);  // 10 %
  TEST_ASSERT_TRUE(battery_gauge_report_low(battery_gauge_pack(&g)));
}

void test_low_clears_as_soon_as_mains_is_back(void) {
  discharge(11400, 0, 60);
  TEST_ASSERT_TRUE(battery_gauge_report_low(battery_gauge_pack(&g)));
  battery_gauge_sample(&g, true, true, false, 14000, 0);
  TEST_ASSERT_FALSE(battery_gauge_report_low(battery_gauge_pack(&g)));
}

void test_low_never_without_a_charger(void) {
  TEST_ASSERT_FALSE(battery_gauge_report_low(battery_gauge_pack(&g)));
  TEST_ASSERT_FALSE(battery_gauge_report_low(0u));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_curve_endpoints_and_clamps);
  RUN_TEST(test_curve_interpolates_between_points);
  RUN_TEST(test_curve_is_monotonic);
  RUN_TEST(test_levels_from_percent);
  RUN_TEST(test_unknown_until_a_valid_reading);
  RUN_TEST(test_mains_charging_and_full);
  RUN_TEST(test_invalid_reading_changes_nothing);
  RUN_TEST(test_discharge_at_rest_follows_the_curve);
  RUN_TEST(test_discharge_compensates_the_resistive_drop);
  RUN_TEST(test_a_load_spike_does_not_empty_the_battery);
  RUN_TEST(test_levels_only_go_down_while_discharging);
  RUN_TEST(test_mains_back_forgets_the_discharge);
  RUN_TEST(test_first_discharge_sample_seeds_the_average);
  RUN_TEST(test_pack_layout);
  RUN_TEST(test_low_only_while_discharging_at_or_below_the_threshold);
  RUN_TEST(test_low_clears_as_soon_as_mains_is_back);
  RUN_TEST(test_low_never_without_a_charger);
  return UNITY_END();
}
