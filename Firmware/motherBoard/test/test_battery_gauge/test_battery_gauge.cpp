// Indicador de carga de la bateria interna (CTRL,BATT).
//
// Lo que se protege: que sin red el indicador refleje la tension corregida por
// la caida resistiva (un pico de consumo del calefactor no "vacia" la
// bateria) y que las rayas solo bajen durante una descarga; que con red el
// nivel suba con la carga que entra y no llegue a lleno hasta la flotacion; y
// que un fallo de I2C no cambie nada. Ver modules/control/battery_gauge.h.

#include <unity.h>

#include "modules/control/battery_gauge.h"

static BatteryGauge g;

void setUp(void) {
  const BatteryGauge init = BATTERY_GAUGE_INITIALIZER;
  g = init;
}
void tearDown(void) {}

static const uint32_t DT = 1000;  // lectura completa del cargador, 1 Hz

// Muestras sin red con la misma lectura, suficientes para asentar la media.
static void discharge(uint16_t vbat_mv, uint16_t idchg_ma, int n) {
  for (int i = 0; i < n; i++) {
    battery_gauge_sample(&g, true, false, false, vbat_mv, idchg_ma, 0, DT);
  }
}

// Muestras con red, cargando a `ichg_ma`.
static void charge(uint16_t ichg_ma, int seconds) {
  for (int i = 0; i < seconds; i++) {
    battery_gauge_sample(&g, true, true, false, 14000, 0, ichg_ma, DT);
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
  battery_gauge_sample(&g, false, false, false, 12000, 0, 0, DT);
  TEST_ASSERT_EQUAL(BATT_GAUGE_UNKNOWN, g.state);
}

void test_mains_charging_and_full(void) {
  battery_gauge_sample(&g, true, true, false, 14000, 0, 0, DT);
  TEST_ASSERT_EQUAL(BATT_GAUGE_CHARGING, g.state);
  battery_gauge_sample(&g, true, true, true, 13600, 0, 0, DT);
  TEST_ASSERT_EQUAL(BATT_GAUGE_FULL, g.state);
  TEST_ASSERT_EQUAL_UINT8(BATT_GAUGE_LEVEL_MAX, g.level);
  TEST_ASSERT_EQUAL_UINT8(100, g.percent);
}

void test_invalid_reading_changes_nothing(void) {
  discharge(12240, 0, 40);
  const BatteryGauge before = g;
  battery_gauge_sample(&g, false, true, true, 14000, 0, 0, DT);
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
  battery_gauge_sample(&g, true, false, false, 12200, 0, 0, DT);
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

void test_a_full_charge_allows_four_bars_again(void) {
  discharge(11700, 0, 60);  // ~22 %, 1 raya
  TEST_ASSERT_EQUAL_UINT8(1, g.level);
  battery_gauge_sample(&g, true, true, true, 13600, 0, 0, DT);  // flotacion
  discharge(12700, 0, 40);
  TEST_ASSERT_EQUAL_UINT8(4, g.level);
}

void test_discharge_starts_from_the_lower_of_voltage_and_charge(void) {
  // Se quita la red a media carga: la tension de flotacion reciente diria
  // "llena", pero solo entro hasta el 40 %.
  battery_gauge_restore(&g, 40);
  charge(0, 5);
  discharge(12700, 0, 10);
  TEST_ASSERT_EQUAL_UINT8(40, g.percent);
  TEST_ASSERT_EQUAL_UINT8(2, g.level);
}

void test_first_discharge_sample_seeds_the_average(void) {
  // Sin semilla, la media arrancaria en 0 V y la primera lectura diria vacia.
  battery_gauge_sample(&g, true, false, false, 12500, 0, 0, DT);
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

// ---- Carga ------------------------------------------------------------------

void test_charging_adds_the_charge_that_goes_in(void) {
  // 1,8 A durante 1 h con 85 % de rendimiento en 9 Ah: +17 puntos.
  battery_gauge_restore(&g, 50);
  charge(1800, 3600);
  TEST_ASSERT_EQUAL(BATT_GAUGE_CHARGING, g.state);
  TEST_ASSERT_UINT8_WITHIN(1, 67, g.percent);
  TEST_ASSERT_EQUAL_UINT8(3, g.level);
}

void test_charging_continues_from_the_discharge(void) {
  discharge(11700, 0, 60);  // ~22 %
  const uint8_t pct = g.percent;
  charge(3000, 60);
  TEST_ASSERT_TRUE(g.percent >= pct);
  TEST_ASSERT_UINT8_WITHIN(1, pct, g.percent);
}

void test_charging_never_reaches_full_before_float(void) {
  battery_gauge_restore(&g, 90);
  charge(5000, 7200);
  TEST_ASSERT_EQUAL_UINT8(BATT_GAUGE_CHARGING_MAX_PCT, g.percent);
  battery_gauge_sample(&g, true, true, true, 13600, 0, 0, DT);
  TEST_ASSERT_EQUAL_UINT8(100, g.percent);
}

void test_charging_without_history_starts_from_zero(void) {
  charge(1000, 1);
  TEST_ASSERT_EQUAL_UINT8(0, g.percent);
  TEST_ASSERT_EQUAL_UINT8(0, g.level);
}

// Caso de banco: primer arranque sin nada guardado, un rato cargando (el
// indicador integra desde 0) y se quita la red con la bateria casi llena.
// Ese "3 %" es una suposicion, no una medida: no puede mandar sobre la
// tension ni disparar BATERIA BAJA.
void test_discharge_after_charging_without_history_uses_the_voltage(void) {
  charge(2000, 600);  // ~3 % integrado desde la nada
  TEST_ASSERT_FALSE(g.known);  // no se guarda en NVS
  discharge(12500, 0, 10);
  TEST_ASSERT_UINT8_WITHIN(1, 80, g.percent);
  TEST_ASSERT_FALSE(battery_gauge_report_low(battery_gauge_pack(&g)));
  TEST_ASSERT_TRUE(g.known);
}

// Caso de banco: con red, el calefactor a plena potencia superaba lo que daba
// el convertidor y la bateria aportaba ~4 A (ICHG=0, IDCHG=4096). El nivel
// tiene que bajar con lo que sale, no quedarse quieto como si cargara.
void test_mains_with_the_battery_supplementing_lowers_the_level(void) {
  battery_gauge_restore(&g, 50);
  for (int i = 0; i < 600; i++) {  // 10 min a -4 A netos
    battery_gauge_sample(&g, true, true, false, 12400, 4000, 0, DT);
  }
  // 4 A x 10 min = 667 mAh sobre 9 Ah = -7,4 puntos.
  TEST_ASSERT_UINT8_WITHIN(1, 43, g.percent);
  // Y al volver a cargar, sube desde ahi.
  charge(2000, 600);
  TEST_ASSERT_TRUE(g.percent > 43);
}

void test_restore_clamps_out_of_range(void) {
  battery_gauge_restore(&g, 250);
  charge(0, 1);
  TEST_ASSERT_TRUE(g.percent <= BATT_GAUGE_CHARGING_MAX_PCT);
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
  charge(0, 1);
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
  RUN_TEST(test_a_full_charge_allows_four_bars_again);
  RUN_TEST(test_discharge_starts_from_the_lower_of_voltage_and_charge);
  RUN_TEST(test_first_discharge_sample_seeds_the_average);
  RUN_TEST(test_pack_layout);
  RUN_TEST(test_charging_adds_the_charge_that_goes_in);
  RUN_TEST(test_charging_continues_from_the_discharge);
  RUN_TEST(test_charging_never_reaches_full_before_float);
  RUN_TEST(test_charging_without_history_starts_from_zero);
  RUN_TEST(test_discharge_after_charging_without_history_uses_the_voltage);
  RUN_TEST(test_mains_with_the_battery_supplementing_lowers_the_level);
  RUN_TEST(test_restore_clamps_out_of_range);
  RUN_TEST(test_low_only_while_discharging_at_or_below_the_threshold);
  RUN_TEST(test_low_clears_as_soon_as_mains_is_back);
  RUN_TEST(test_low_never_without_a_charger);
  return UNITY_END();
}
