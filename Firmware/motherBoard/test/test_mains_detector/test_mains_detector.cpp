// Detector de interrupcion de red (ALARM_MAINS_INTERRUPTION).
//
// Lo que se protege: que un corte de red real se declare en ~10 s y NO se
// retire hasta cumplirse el minimo de 10 min de 60601-2-19 201.12.3.103, y que
// un parpadeo o un fallo de I2C del cargador no levante ni retire una alarma
// ALTA que el operador no puede silenciar. Ver modules/control/mains_detector.h
// y openspec/changes/shared-mains-detector/design.md.

#include <unity.h>

#include "modules/control/mains_detector.h"

static MainsDetector d;

// Periodo de lectura del BQ en main.cpp.
static const uint32_t PERIOD_MS = 5000u;
static const uint16_t VBAT_OK = 12600;

void setUp(void) { mains_detector_init(&d); }
void tearDown(void) {}

static void feed(bool valid, bool ac, uint32_t t, uint16_t vbat = VBAT_OK) {
  mains_detector_sample(&d, valid, ac, vbat, t);
}
static bool cond(uint32_t t) { return mains_detector_condition(&d, t); }

// Alimenta `n` lecturas validas con el mismo valor, una cada PERIOD_MS desde
// `t`. Devuelve el instante de la ULTIMA.
static uint32_t feedN(int n, bool ac, uint32_t t) {
  uint32_t last = t;
  for (int i = 0; i < n; ++i) {
    last = t + (uint32_t)i * PERIOD_MS;
    feed(true, ac, last);
  }
  return last;
}

// --- arranque ---

void test_starts_without_condition(void) {
  TEST_ASSERT_FALSE(cond(0));
  TEST_ASSERT_FALSE(cond(1000000));
}

void test_mains_present_never_declares(void) {
  feedN(50, true, 100000);
  TEST_ASSERT_FALSE(cond(100000 + 50 * PERIOD_MS));
}

// --- antirrebote de entrada ---

void test_single_absent_reading_does_not_declare(void) {
  feed(true, false, 100000);
  TEST_ASSERT_FALSE(cond(100000));
  TEST_ASSERT_FALSE(cond(105000));
}

void test_two_consecutive_absent_readings_declare(void) {
  feed(true, false, 100000);
  feed(true, false, 105000);
  TEST_ASSERT_TRUE(cond(105000));
}

void test_blip_between_absent_readings_resets_the_streak(void) {
  feed(true, false, 100000);
  feed(true, true, 105000);   // un parpadeo: vuelve a haber red
  feed(true, false, 110000);  // la racha empieza de nuevo
  TEST_ASSERT_FALSE(cond(110000));
  feed(true, false, 115000);
  TEST_ASSERT_TRUE(cond(115000));
}

// --- lecturas no validas: no cambian el estado ---

void test_invalid_reading_does_not_declare(void) {
  for (int i = 0; i < 100; ++i) {
    feed(false, false, 100000 + (uint32_t)i * PERIOD_MS);
  }
  TEST_ASSERT_FALSE(cond(100000 + 100 * PERIOD_MS));
}

void test_invalid_reading_does_not_break_the_streak(void) {
  feed(true, false, 100000);
  feed(false, false, 105000);  // fallo de I2C entre medias
  TEST_ASSERT_FALSE(cond(105000));
  feed(true, false, 110000);
  TEST_ASSERT_TRUE(cond(110000));
}

void test_invalid_reading_does_not_count_as_absent(void) {
  feed(true, false, 100000);
  feed(false, false, 105000);
  TEST_ASSERT_FALSE(cond(105000));  // una valida + una invalida: no son dos
}

void test_invalid_reading_does_not_release(void) {
  const uint32_t t0 = feedN(2, false, 100000);  // declarada en t0
  // Pasado el minimo, solo hay lecturas invalidas: no es un restablecimiento.
  const uint32_t t = t0 + MAINS_ALARM_MIN_HOLD_MS + 1000u;
  for (int i = 0; i < 10; ++i) {
    feed(false, true, t + (uint32_t)i * PERIOD_MS);
  }
  TEST_ASSERT_TRUE(cond(t + 10 * PERIOD_MS));
}

void test_zero_battery_voltage_makes_the_reading_invalid(void) {
  // vbat 0 = el ADC del cargador aun no ha convertido: el resto de la trama no
  // es de fiar, aunque el flag de lectura diga que si.
  feed(true, false, 100000, 0);
  feed(true, false, 105000, 0);
  TEST_ASSERT_FALSE(cond(105000));
}

void test_zero_battery_voltage_does_not_release(void) {
  const uint32_t t0 = feedN(2, false, 100000);
  const uint32_t t = t0 + MAINS_ALARM_MIN_HOLD_MS + 1000u;
  feed(true, true, t, 0);
  feed(true, true, t + PERIOD_MS, 0);
  TEST_ASSERT_TRUE(cond(t + PERIOD_MS));
}

// --- minimo de 10 min (201.12.3.103) ---

void test_hold_counts_from_the_declaration(void) {
  feed(true, false, 100000);
  feed(true, false, 105000);  // se declara AQUI
  // La red vuelve enseguida y se confirma.
  feed(true, true, 110000);
  feed(true, true, 115000);
  TEST_ASSERT_TRUE(cond(115000));
  TEST_ASSERT_TRUE(cond(105000 + MAINS_ALARM_MIN_HOLD_MS - 1u));
  TEST_ASSERT_FALSE(cond(105000 + MAINS_ALARM_MIN_HOLD_MS));
}

void test_hold_is_ten_minutes(void) {
  TEST_ASSERT_EQUAL_UINT32(600000u, MAINS_ALARM_MIN_HOLD_MS);
}

void test_mains_back_after_30s_keeps_the_alarm_to_the_minimum(void) {
  const uint32_t t0 = feedN(2, false, 100000);
  feedN(2, true, t0 + 30000u);
  TEST_ASSERT_TRUE(cond(t0 + 40000u));
  TEST_ASSERT_TRUE(cond(t0 + 300000u));
  TEST_ASSERT_TRUE(cond(t0 + MAINS_ALARM_MIN_HOLD_MS - 1u));
  TEST_ASSERT_FALSE(cond(t0 + MAINS_ALARM_MIN_HOLD_MS));
}

void test_long_interruption_stays_present_the_whole_time(void) {
  const uint32_t t0 = feedN(2, false, 100000);
  for (uint32_t t = t0 + PERIOD_MS; t < t0 + 25u * 60u * 1000u; t += PERIOD_MS) {
    feed(true, false, t);
    TEST_ASSERT_TRUE(cond(t));
  }
}

void test_release_after_the_minimum_needs_two_present_readings(void) {
  const uint32_t t0 = feedN(2, false, 100000);
  uint32_t t = t0 + MAINS_ALARM_MIN_HOLD_MS + 20000u;
  feed(true, false, t - PERIOD_MS);  // sigue el corte pasados los 10 min
  TEST_ASSERT_TRUE(cond(t));
  feed(true, true, t);               // una sola lectura de red: aun no
  TEST_ASSERT_TRUE(cond(t));
  feed(true, true, t + PERIOD_MS);   // la segunda confirma
  TEST_ASSERT_FALSE(cond(t + PERIOD_MS));
}

void test_mains_flapping_after_the_minimum_does_not_release(void) {
  const uint32_t t0 = feedN(2, false, 100000);
  uint32_t t = t0 + MAINS_ALARM_MIN_HOLD_MS + 20000u;
  for (int i = 0; i < 10; ++i) {  // red presente / ausente alternando
    feed(true, (i % 2) == 0, t + (uint32_t)i * PERIOD_MS);
  }
  TEST_ASSERT_TRUE(cond(t + 10 * PERIOD_MS));
}

// --- nueva interrupcion mientras el aviso aun se mantiene ---

void test_reinterruption_during_hold_restarts_the_minimum(void) {
  const uint32_t t0 = feedN(2, false, 100000);   // declaracion 1 en t0
  feedN(2, true, t0 + 60000u);                    // red de vuelta (~t0+65 s)
  const uint32_t t2 = feedN(2, false, t0 + 180000u);  // corte 2 en t2
  feedN(2, true, t2 + 20000u);                    // vuelve otra vez
  // Pasado el minimo del PRIMER corte, el segundo aun lo mantiene.
  TEST_ASSERT_TRUE(cond(t0 + MAINS_ALARM_MIN_HOLD_MS + 1000u));
  TEST_ASSERT_TRUE(cond(t2 + MAINS_ALARM_MIN_HOLD_MS - 1u));
  TEST_ASSERT_FALSE(cond(t2 + MAINS_ALARM_MIN_HOLD_MS));
}

void test_new_interruption_after_release_is_declared_again(void) {
  const uint32_t t0 = feedN(2, false, 100000);
  const uint32_t tr = feedN(2, true, t0 + MAINS_ALARM_MIN_HOLD_MS + 5000u);
  TEST_ASSERT_FALSE(cond(tr));
  const uint32_t t2 = feedN(2, false, tr + 60000u);
  TEST_ASSERT_TRUE(cond(t2));
}

// --- desbordamiento de millis() ---

void test_hold_survives_millis_wraparound(void) {
  const uint32_t start = 0xFFFFFFFFu - 6000u;  // la declaracion cae antes del giro
  const uint32_t t0 = feedN(2, false, start);
  feedN(2, true, t0 + 20000u);
  TEST_ASSERT_TRUE(cond(t0 + MAINS_ALARM_MIN_HOLD_MS - 1u));
  TEST_ASSERT_FALSE(cond(t0 + MAINS_ALARM_MIN_HOLD_MS));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_without_condition);
  RUN_TEST(test_mains_present_never_declares);
  RUN_TEST(test_single_absent_reading_does_not_declare);
  RUN_TEST(test_two_consecutive_absent_readings_declare);
  RUN_TEST(test_blip_between_absent_readings_resets_the_streak);
  RUN_TEST(test_invalid_reading_does_not_declare);
  RUN_TEST(test_invalid_reading_does_not_break_the_streak);
  RUN_TEST(test_invalid_reading_does_not_count_as_absent);
  RUN_TEST(test_invalid_reading_does_not_release);
  RUN_TEST(test_zero_battery_voltage_makes_the_reading_invalid);
  RUN_TEST(test_zero_battery_voltage_does_not_release);
  RUN_TEST(test_hold_counts_from_the_declaration);
  RUN_TEST(test_hold_is_ten_minutes);
  RUN_TEST(test_mains_back_after_30s_keeps_the_alarm_to_the_minimum);
  RUN_TEST(test_long_interruption_stays_present_the_whole_time);
  RUN_TEST(test_release_after_the_minimum_needs_two_present_readings);
  RUN_TEST(test_mains_flapping_after_the_minimum_does_not_release);
  RUN_TEST(test_reinterruption_during_hold_restarts_the_minimum);
  RUN_TEST(test_new_interruption_after_release_is_declared_again);
  RUN_TEST(test_hold_survives_millis_wraparound);
  return UNITY_END();
}
