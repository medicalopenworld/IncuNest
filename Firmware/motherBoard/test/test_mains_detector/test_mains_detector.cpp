// Detector de interrupcion de red (ALARM_MAINS_INTERRUPTION).
//
// Lo que se protege: que un corte de red real se declare en ~10 s y NO se
// retire hasta que la red vuelva (confirmada), y que un parpadeo o un fallo de
// I2C del cargador no levante ni retire una alarma ALTA que el operador no
// puede silenciar. Y que un cargador que deja de responder no deje al detector
// ciego sin que nadie se entere (mains_detector_blind). Ver
// modules/control/mains_detector.h y openspec/changes/archive/*mains-detector*.
//
// El plazo minimo de aviso es 0 por decision de producto (design de
// shared-mains-detector-fixes): los tests del mecanismo con plazo usan su
// propia constante, TEST_HOLD_MS, para que revertir la decision no deje el
// mecanismo sin probar.

#include <unity.h>

#include "modules/control/mains_detector.h"

static MainsDetector d;

// Periodo de lectura del BQ en main.cpp.
static const uint32_t PERIOD_MS = MAINS_DETECTOR_SAMPLE_PERIOD_MS;
// Plazo de prueba para ejercitar el mecanismo de minimo de aviso.
static const uint32_t TEST_HOLD_MS = 600000u;

// Casi todos los tests son de un equipo que ya tenia red: el detector solo se
// arma tras haber visto red (ver los tests de arranque en bateria, abajo).
void setUp(void) {
  mains_detector_init(&d);
  d.mains_seen = true;
}
void tearDown(void) {}

static void feed(bool valid, bool ac, uint32_t t) {
  mains_detector_sample(&d, valid, ac, t);
}
static bool cond(uint32_t t) { return mains_detector_condition(&d, t); }
static bool blind(uint32_t t, bool detected = true) {
  return mains_detector_blind(&d, detected, t);
}

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
  const uint32_t t0 = feedN(2, false, 100000);  // declarada
  // Solo lecturas invalidas con "red presente": no es un restablecimiento.
  for (int i = 0; i < 10; ++i) {
    feed(false, true, t0 + 60000u + (uint32_t)i * PERIOD_MS);
  }
  TEST_ASSERT_TRUE(cond(t0 + 60000u + 10 * PERIOD_MS));
}

// --- plazo minimo por defecto: 0 (decision de producto) ---

void test_default_hold_is_zero(void) {
  TEST_ASSERT_EQUAL_UINT32(0u, MAINS_ALARM_MIN_HOLD_MS);
}

// H1: la instancia de PRODUCCION (main.cpp) se declara con este inicializador
// estatico. Con `= {}` el plazo valia 0 porque la estructura estaba a cero, no
// porque lo dijera la constante: poner MAINS_ALARM_MIN_HOLD_MS a 600000u no
// habria cambiado nada. Este test fija que el inicializador coincide con
// mains_detector_init() y con la constante.
void test_production_initializer_uses_the_constants(void) {
  MainsDetector p = MAINS_DETECTOR_INITIALIZER;
  MainsDetector q;
  mains_detector_init(&q);
  TEST_ASSERT_EQUAL_UINT32(MAINS_ALARM_MIN_HOLD_MS, p.hold_ms);
  TEST_ASSERT_EQUAL_UINT32(q.hold_ms, p.hold_ms);
  TEST_ASSERT_EQUAL_UINT8(q.absent_streak, p.absent_streak);
  TEST_ASSERT_EQUAL_UINT8(q.present_streak, p.present_streak);
  TEST_ASSERT_EQUAL(q.absent_confirmed, p.absent_confirmed);
  TEST_ASSERT_EQUAL(q.declared, p.declared);
  TEST_ASSERT_EQUAL_UINT32(q.since_ms, p.since_ms);
  TEST_ASSERT_EQUAL_UINT32(q.last_valid_ms, p.last_valid_ms);
  TEST_ASSERT_EQUAL(q.mains_seen, p.mains_seen);
  // Y se comporta igual: se arma con red y declara con la racha de ausencia.
  mains_detector_sample(&p, true, true, 90000);
  mains_detector_sample(&p, true, true, 95000);
  mains_detector_sample(&p, true, false, 100000);
  mains_detector_sample(&p, true, false, 105000);
  TEST_ASSERT_TRUE(mains_detector_condition(&p, 105000));
}

// H6: histeresis asimetrica. Declarar rapido (2 lecturas, ~0,4 s), retirar
// mas despacio (10 seguidas con red, ~2 s), leyendo cada 200 ms.
void test_confirmation_is_asymmetric(void) {
  TEST_ASSERT_EQUAL_UINT32(2u, MAINS_DETECTOR_CONFIRM_ABSENT_SAMPLES);
  TEST_ASSERT_EQUAL_UINT32(10u, MAINS_DETECTOR_CONFIRM_PRESENT_SAMPLES);
  TEST_ASSERT_EQUAL_UINT32(200u, MAINS_DETECTOR_SAMPLE_PERIOD_MS);
}

// Una fuente intermitente (enchufe flojo, generador inestable) no puede hacer
// oscilar la alarma: mientras no haya SEIS lecturas seguidas con red, se
// mantiene continua. Aqui: dos ausencias y cinco con red, repetido.
void test_intermittent_mains_keeps_the_alarm_continuous(void) {
  uint32_t t = 100000;
  feedN(2, false, t);
  t += 2 * PERIOD_MS;
  for (int cycle = 0; cycle < 20; ++cycle) {
    t = feedN(MAINS_DETECTOR_CONFIRM_PRESENT_SAMPLES - 1u, true, t) + PERIOD_MS;
    TEST_ASSERT_TRUE(cond(t));
    t = feedN(MAINS_DETECTOR_CONFIRM_ABSENT_SAMPLES, false, t) + PERIOD_MS;
    TEST_ASSERT_TRUE(cond(t));
  }
}

void test_each_new_absence_restarts_the_present_streak(void) {
  const uint32_t t0 = feedN(2, false, 100000);
  uint32_t t = feedN(MAINS_DETECTOR_CONFIRM_PRESENT_SAMPLES - 1u, true, t0 + PERIOD_MS);
  feed(true, false, t + PERIOD_MS);  // UNA ausencia basta para reiniciar la racha
  // Cinco con red otra vez: sigue sin bastar.
  t = feedN(MAINS_DETECTOR_CONFIRM_PRESENT_SAMPLES - 1u, true, t + 2 * PERIOD_MS);
  TEST_ASSERT_TRUE(cond(t));
  feed(true, true, t + PERIOD_MS);
  TEST_ASSERT_FALSE(cond(t + PERIOD_MS));
}

void test_default_alarm_clears_when_mains_is_confirmed_back(void) {
  const uint32_t t0 = feedN(2, false, 100000);
  TEST_ASSERT_TRUE(cond(t0));
  // Cinco lecturas seguidas con red: aun no (se necesitan seis, ~30 s).
  const uint32_t t5 = feedN(MAINS_DETECTOR_CONFIRM_PRESENT_SAMPLES - 1u, true, t0 + 30000u);
  TEST_ASSERT_TRUE(cond(t5));
  feed(true, true, t5 + PERIOD_MS);  // la sexta confirma: se retira
  TEST_ASSERT_FALSE(cond(t5 + PERIOD_MS));
}


void test_default_long_interruption_stays_present_the_whole_time(void) {
  const uint32_t t0 = feedN(2, false, 100000);
  for (uint32_t t = t0 + PERIOD_MS; t < t0 + 25u * 60u * 1000u; t += PERIOD_MS) {
    feed(true, false, t);
    TEST_ASSERT_TRUE(cond(t));
  }
}

void test_default_mains_flapping_does_not_release(void) {
  const uint32_t t0 = feedN(2, false, 100000);
  for (int i = 0; i < 10; ++i) {  // red presente / ausente alternando
    feed(true, (i % 2) == 0, t0 + PERIOD_MS * (uint32_t)(i + 1));
  }
  TEST_ASSERT_TRUE(cond(t0 + 11 * PERIOD_MS));
}

void test_default_new_interruption_is_declared_again(void) {
  const uint32_t t0 = feedN(2, false, 100000);
  const uint32_t tr = feedN(MAINS_DETECTOR_CONFIRM_PRESENT_SAMPLES, true, t0 + 30000u);
  TEST_ASSERT_FALSE(cond(tr));
  const uint32_t t2 = feedN(2, false, tr + 60000u);
  TEST_ASSERT_TRUE(cond(t2));
}

// --- mecanismo de minimo de aviso (plazo de prueba) ---
//
// Con MAINS_ALARM_MIN_HOLD_MS a 0 esto no corre en produccion, pero es lo que
// hay que tener probado para revertir la decision con una linea.

void test_hold_counts_from_the_declaration(void) {
  mains_detector_init_hold(&d, TEST_HOLD_MS);
  d.mains_seen = true;
  feed(true, false, 100000);
  feed(true, false, 105000);  // se declara AQUI
  const uint32_t tb =
      feedN(MAINS_DETECTOR_CONFIRM_PRESENT_SAMPLES, true, 110000);  // red confirmada
  TEST_ASSERT_TRUE(cond(tb));
  TEST_ASSERT_TRUE(cond(105000 + TEST_HOLD_MS - 1u));
  TEST_ASSERT_FALSE(cond(105000 + TEST_HOLD_MS));
}

void test_hold_long_interruption_outlasts_the_minimum(void) {
  mains_detector_init_hold(&d, TEST_HOLD_MS);
  d.mains_seen = true;
  const uint32_t t0 = feedN(2, false, 100000);
  for (uint32_t t = t0 + PERIOD_MS; t < t0 + 25u * 60u * 1000u; t += PERIOD_MS) {
    feed(true, false, t);
    TEST_ASSERT_TRUE(cond(t));
  }
}

void test_hold_release_after_the_minimum_needs_the_present_streak(void) {
  mains_detector_init_hold(&d, TEST_HOLD_MS);
  d.mains_seen = true;
  const uint32_t t0 = feedN(2, false, 100000);
  const uint32_t t = t0 + TEST_HOLD_MS + 20000u;
  feed(true, false, t - PERIOD_MS);  // sigue el corte pasados los 10 min
  TEST_ASSERT_TRUE(cond(t));
  const uint32_t t5 =
      feedN(MAINS_DETECTOR_CONFIRM_PRESENT_SAMPLES - 1u, true, t);
  TEST_ASSERT_TRUE(cond(t5));
  feed(true, true, t5 + PERIOD_MS);
  TEST_ASSERT_FALSE(cond(t5 + PERIOD_MS));
}

void test_hold_reinterruption_restarts_the_minimum(void) {
  mains_detector_init_hold(&d, TEST_HOLD_MS);
  d.mains_seen = true;
  const uint32_t t0 = feedN(2, false, 100000);
  feedN(MAINS_DETECTOR_CONFIRM_PRESENT_SAMPLES, true, t0 + 60000u);
  const uint32_t t2 = feedN(2, false, t0 + 180000u);
  feedN(MAINS_DETECTOR_CONFIRM_PRESENT_SAMPLES, true, t2 + 20000u);
  TEST_ASSERT_TRUE(cond(t0 + TEST_HOLD_MS + 1000u));
  TEST_ASSERT_TRUE(cond(t2 + TEST_HOLD_MS - 1u));
  TEST_ASSERT_FALSE(cond(t2 + TEST_HOLD_MS));
}

void test_hold_survives_millis_wraparound(void) {
  mains_detector_init_hold(&d, TEST_HOLD_MS);
  d.mains_seen = true;
  const uint32_t t0 = feedN(2, false, 0xFFFFFFFFu - 6000u);
  feedN(MAINS_DETECTOR_CONFIRM_PRESENT_SAMPLES, true, t0 + 20000u);
  TEST_ASSERT_TRUE(cond(t0 + TEST_HOLD_MS - 1u));
  TEST_ASSERT_FALSE(cond(t0 + TEST_HOLD_MS));
}

// Un plazo ya cerrado no puede resucitar cuando millis() da la vuelta (~49,7
// dias despues): since_ms quedaria "reciente" otra vez.
void test_hold_expired_does_not_resurrect_after_a_full_clock_cycle(void) {
  mains_detector_init_hold(&d, TEST_HOLD_MS);
  d.mains_seen = true;
  const uint32_t t0 = feedN(2, false, 100000);
  const uint32_t tr = feedN(MAINS_DETECTOR_CONFIRM_PRESENT_SAMPLES, true, t0 + TEST_HOLD_MS + 5000u);
  TEST_ASSERT_FALSE(cond(tr));
  feed(true, true, tr + 3600000u);
  TEST_ASSERT_FALSE(cond(t0 + 1000u));
}

// --- ceguera: el cargador deja de dar lecturas validas (A2) ---

void test_not_blind_with_a_recent_valid_reading(void) {
  feed(true, true, 100000);
  TEST_ASSERT_FALSE(blind(100000));
  TEST_ASSERT_FALSE(blind(100000 + MAINS_DETECTOR_BLIND_MS));
}

void test_blind_after_the_limit_without_valid_readings(void) {
  feed(true, true, 100000);
  TEST_ASSERT_TRUE(blind(100000 + MAINS_DETECTOR_BLIND_MS + 1u));
}

void test_blind_limit_is_thirty_seconds(void) {
  TEST_ASSERT_EQUAL_UINT32(30000u, MAINS_DETECTOR_BLIND_MS);
}

void test_invalid_readings_do_not_refresh_the_blindness_clock(void) {
  feed(true, true, 100000);
  const int n = (int)(MAINS_DETECTOR_BLIND_MS / PERIOD_MS) + 2;
  for (int i = 1; i <= n; ++i) {
    feed(false, false, 100000 + (uint32_t)i * PERIOD_MS);
  }
  TEST_ASSERT_TRUE(blind(100000 + (uint32_t)n * PERIOD_MS));
}

void test_a_valid_reading_ends_the_blindness(void) {
  feed(true, true, 100000);
  TEST_ASSERT_TRUE(blind(200000));
  feed(true, true, 200000);
  TEST_ASSERT_FALSE(blind(200000));
}

void test_never_blind_without_a_detected_charger(void) {
  TEST_ASSERT_FALSE(blind(10000000, /*detected=*/false));
}

void test_blind_from_boot_when_no_reading_ever_arrives(void) {
  // Cargador detectado, pero ninguna lectura valida desde el arranque.
  TEST_ASSERT_FALSE(blind(MAINS_DETECTOR_BLIND_MS));
  TEST_ASSERT_TRUE(blind(MAINS_DETECTOR_BLIND_MS + 1u));
}

void test_declared_alarm_is_kept_while_blind(void) {
  const uint32_t t0 = feedN(2, false, 100000);
  // El cargador deja de responder durante el corte: la alarma NO se retira
  // (lado seguro) y la ceguera se ve.
  const uint32_t t = t0 + MAINS_DETECTOR_BLIND_MS + 5000u;
  TEST_ASSERT_TRUE(cond(t));
  TEST_ASSERT_TRUE(blind(t));
}

void test_blindness_survives_millis_wraparound(void) {
  feed(true, true, 0xFFFFFFFFu - 1000u);
  TEST_ASSERT_FALSE(blind(MAINS_DETECTOR_BLIND_MS - 2000u));  // ya dio la vuelta
  TEST_ASSERT_TRUE(blind(MAINS_DETECTOR_BLIND_MS + 2000u));
}


// Arrancar en bateria NO es una interrupcion de red: la red no se ha cortado,
// no estaba. Visto en el banco (2026-10-26): unidad alimentada por el
// conector de bateria con una fuente, ALTA no silenciable desde el segundo 15.
void test_boot_on_battery_never_declares(void) {
  MainsDetector f;
  mains_detector_init(&f);
  uint32_t t = 10000;
  for (int i = 0; i < 40; i++, t += PERIOD_MS) {
    mains_detector_sample(&f, true, false, t);
  }
  TEST_ASSERT_FALSE(mains_detector_condition(&f, t));
}

// Una sola lectura con red (un parpadeo del comparador) no arma el detector.
void test_single_present_reading_does_not_arm(void) {
  MainsDetector f;
  mains_detector_init(&f);
  mains_detector_sample(&f, true, false, 10000);
  mains_detector_sample(&f, true, true, 15000);
  mains_detector_sample(&f, true, false, 20000);
  mains_detector_sample(&f, true, false, 25000);
  TEST_ASSERT_FALSE(mains_detector_condition(&f, 25000));
}

// En bateria desde el arranque, se enchufa la red y luego se corta: eso SI es
// una interrupcion y se declara como siempre.
void test_mains_seen_then_lost_declares(void) {
  MainsDetector f;
  mains_detector_init(&f);
  mains_detector_sample(&f, true, false, 10000);
  mains_detector_sample(&f, true, false, 15000);
  mains_detector_sample(&f, true, true, 20000);
  mains_detector_sample(&f, true, true, 25000);  // armado
  mains_detector_sample(&f, true, false, 30000);
  mains_detector_sample(&f, true, false, 35000);
  TEST_ASSERT_TRUE(mains_detector_condition(&f, 35000));
}

// --- combinacion de fuentes: AC_STAT (I2C) y pin CHRG_OK (V19) ---
// Red presente solo si TODAS las fuentes que responden lo dicen; lectura
// valida si responde el registro O hay pin. Banco V19.A (19/11/2026): al
// desenchufar, AC_STAT seguia a 1 varios segundos con VBUS ya en 3,4 V y el pin
// CHRG_OK ya a 0.

void test_combine_pin_absent_wins_over_stale_register(void) {
  MainsReading r = mains_reading_combine(true, true, true, false);
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_FALSE(r.present);
}

void test_combine_register_absent_wins_over_pin(void) {
  MainsReading r = mains_reading_combine(true, false, true, true);
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_FALSE(r.present);
}

void test_combine_both_present(void) {
  MainsReading r = mains_reading_combine(true, true, true, true);
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.present);
}

void test_combine_pin_keeps_detector_alive_with_i2c_down(void) {
  MainsReading r = mains_reading_combine(false, false, true, false);
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_FALSE(r.present);
  r = mains_reading_combine(false, true, true, true);
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.present);
}

void test_combine_without_pin_is_the_register(void) {
  MainsReading r = mains_reading_combine(true, true, false, false);
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.present);
  r = mains_reading_combine(true, false, false, true);
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_FALSE(r.present);
}

void test_combine_nothing_responds_is_invalid(void) {
  MainsReading r = mains_reading_combine(false, true, false, true);
  TEST_ASSERT_FALSE(r.valid);
  TEST_ASSERT_FALSE(r.present);
}

// De punta a punta: con el pin, el corte se declara en dos lecturas aunque el
// registro siga diciendo que hay red.
void test_pin_declares_cut_while_register_is_stale(void) {
  uint32_t t = feedN(5, true, 1000);
  for (int i = 1; i <= 2; ++i) {
    MainsReading r = mains_reading_combine(true, true, true, false);
    feed(r.valid, r.present, t + (uint32_t)i * PERIOD_MS);
  }
  TEST_ASSERT_TRUE(cond(t + 2 * PERIOD_MS));
}

// --- el pin CHRG_OK no cuenta justo despues de escribir en el cargador ---
// Banco V19.A (19/11/2026): al escribir la tension de flotacion, CHRG_OK bajo
// 750 ms con los 24 V presentes (AC_STAT no se movio) y daba un falso corte.

void test_pin_ignored_right_after_charger_write(void) {
  TEST_ASSERT_FALSE(mains_pin_usable(true, 10000, 10000));
  TEST_ASSERT_FALSE(mains_pin_usable(true, 10000 + MAINS_PIN_BLANK_AFTER_CHARGER_WRITE_MS - 1, 10000));
}

void test_pin_usable_after_blank_window(void) {
  TEST_ASSERT_TRUE(mains_pin_usable(true, 10000 + MAINS_PIN_BLANK_AFTER_CHARGER_WRITE_MS, 10000));
}

void test_pin_never_usable_without_pin(void) {
  TEST_ASSERT_FALSE(mains_pin_usable(false, 50000, 0));
}

void test_blank_window_survives_millis_wraparound(void) {
  const uint32_t write = 0xFFFFFF00u;
  TEST_ASSERT_FALSE(mains_pin_usable(true, write + 200u, write));
  TEST_ASSERT_TRUE(mains_pin_usable(true, write + MAINS_PIN_BLANK_AFTER_CHARGER_WRITE_MS + 1u, write));
}

void test_blank_window_covers_measured_glitch(void) {
  TEST_ASSERT_TRUE(MAINS_PIN_BLANK_AFTER_CHARGER_WRITE_MS >= 2u * 750u);
}

// De punta a punta: pulso de 750 ms tras una escritura, con AC_STAT a 1: no
// declara.
void test_write_glitch_does_not_declare(void) {
  uint32_t t = feedN(5, true, 1000);
  const uint32_t write = t + 10;
  for (int i = 1; i <= 4; ++i) {  // 4 lecturas = 800 ms de pin en bajo
    const uint32_t now = t + (uint32_t)i * PERIOD_MS;
    const bool usable = mains_pin_usable(true, now, write);
    MainsReading r = mains_reading_combine(true, true, usable, false);
    feed(r.valid, r.present, now);
    TEST_ASSERT_FALSE(cond(now));
  }
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
  RUN_TEST(test_default_hold_is_zero);
  RUN_TEST(test_production_initializer_uses_the_constants);
  RUN_TEST(test_confirmation_is_asymmetric);
  RUN_TEST(test_intermittent_mains_keeps_the_alarm_continuous);
  RUN_TEST(test_each_new_absence_restarts_the_present_streak);
  RUN_TEST(test_default_alarm_clears_when_mains_is_confirmed_back);
  RUN_TEST(test_default_long_interruption_stays_present_the_whole_time);
  RUN_TEST(test_default_mains_flapping_does_not_release);
  RUN_TEST(test_default_new_interruption_is_declared_again);
  RUN_TEST(test_hold_counts_from_the_declaration);
  RUN_TEST(test_hold_long_interruption_outlasts_the_minimum);
  RUN_TEST(test_hold_release_after_the_minimum_needs_the_present_streak);
  RUN_TEST(test_hold_reinterruption_restarts_the_minimum);
  RUN_TEST(test_hold_survives_millis_wraparound);
  RUN_TEST(test_hold_expired_does_not_resurrect_after_a_full_clock_cycle);
  RUN_TEST(test_not_blind_with_a_recent_valid_reading);
  RUN_TEST(test_blind_after_the_limit_without_valid_readings);
  RUN_TEST(test_blind_limit_is_thirty_seconds);
  RUN_TEST(test_invalid_readings_do_not_refresh_the_blindness_clock);
  RUN_TEST(test_a_valid_reading_ends_the_blindness);
  RUN_TEST(test_never_blind_without_a_detected_charger);
  RUN_TEST(test_blind_from_boot_when_no_reading_ever_arrives);
  RUN_TEST(test_declared_alarm_is_kept_while_blind);
  RUN_TEST(test_blindness_survives_millis_wraparound);
  RUN_TEST(test_boot_on_battery_never_declares);
  RUN_TEST(test_single_present_reading_does_not_arm);
  RUN_TEST(test_mains_seen_then_lost_declares);
  RUN_TEST(test_combine_pin_absent_wins_over_stale_register);
  RUN_TEST(test_combine_register_absent_wins_over_pin);
  RUN_TEST(test_combine_both_present);
  RUN_TEST(test_combine_pin_keeps_detector_alive_with_i2c_down);
  RUN_TEST(test_combine_without_pin_is_the_register);
  RUN_TEST(test_combine_nothing_responds_is_invalid);
  RUN_TEST(test_pin_declares_cut_while_register_is_stale);
  RUN_TEST(test_pin_ignored_right_after_charger_write);
  RUN_TEST(test_pin_usable_after_blank_window);
  RUN_TEST(test_pin_never_usable_without_pin);
  RUN_TEST(test_blank_window_survives_millis_wraparound);
  RUN_TEST(test_blank_window_covers_measured_glitch);
  RUN_TEST(test_write_glitch_does_not_declare);
  return UNITY_END();
}
