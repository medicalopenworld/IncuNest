// Regresion del slew asimetrico del lazo de intensidad de fototerapia.
// Ver modules/control/photo_slew.h y known_issues.md, apartado 21.

#include <math.h>
#include <unity.h>

#include "modules/control/photo_slew.h"

#define UP 1
#define DOWN 20
#define TOL 0.02f
#define MINP 10
#define MAXP 255

static PhotoSlew s;

void setUp(void) { photo_slew_init(&s); }
void tearDown(void) {}

static int next(int pwm, float cur, float tgt) {
  return photo_slew_step(&s, pwm, cur, tgt, TOL, UP, DOWN, MINP, MAXP);
}

void test_in_band_does_not_move(void) {
  TEST_ASSERT_EQUAL_INT(100, next(100, 0.46f, 0.45f));
  TEST_ASSERT_EQUAL_INT(100, next(100, 0.44f, 0.45f));
}

void test_up_limited_to_one_step(void) {
  // corriente 0.225 con objetivo 0.45: el modelo pide 100 -> 200 (+100)
  TEST_ASSERT_EQUAL_INT(101, next(100, 0.225f, 0.45f));
  // modelo pide +50
  TEST_ASSERT_EQUAL_INT(101, next(100, 0.30f, 0.45f));
}

void test_down_limited_to_twenty_on_second_reading(void) {
  // modelo: 200 * 0.45 / 0.90 = 100 -> -100. La 1a lectura baja 1, la 2a, 20.
  TEST_ASSERT_EQUAL_INT(199, next(200, 0.90f, 0.45f));
  TEST_ASSERT_EQUAL_INT(179, next(199, 0.90f, 0.45f));
}

void test_down_does_not_cross_model(void) {
  // modelo: 100 * 0.45 / 0.4737 = 95 -> -5; la 1a lectura baja 1 y la 2a,
  // aun con limite 20, se queda en el modelo.
  TEST_ASSERT_EQUAL_INT(99, next(100, 0.4737f, 0.45f));
  TEST_ASSERT_EQUAL_INT(94, next(99, 0.4737f, 0.45f));
}

void test_down_exact_model_step_of_five(void) {
  // Con la cuenta armada, un modelo que pide -5 baja 5 y no mas.
  next(100, 0.4737f, 0.45f);  // 1a lectura: arma (baja 1)
  TEST_ASSERT_EQUAL_INT(95, photo_slew_step(&s, 100, 0.4737f, 0.45f, TOL, UP,
                                            DOWN, MINP, MAXP));
}

void test_clamp_min_applies_after_step_limit(void) {
  // pwm 40, 4.5 A: modelo 4; con la cuenta armada el limite -20 da 20.
  next(40, 4.5f, 0.45f);
  TEST_ASSERT_EQUAL_INT(20, photo_slew_step(&s, 40, 4.5f, 0.45f, TOL, UP, DOWN,
                                            MINP, MAXP));
  // pwm 15, 4.5 A: modelo 1, delta -14 (<20) cae bajo el minimo -> MINP.
  TEST_ASSERT_EQUAL_INT(MINP, photo_slew_step(&s, 15, 4.5f, 0.45f, TOL, UP,
                                              DOWN, MINP, MAXP));
}

void test_clamp_max(void) {
  TEST_ASSERT_EQUAL_INT(MAXP, next(MAXP, 0.10f, 0.45f));
  // y un pwm_now fuera de rango se recorta tambien
  TEST_ASSERT_EQUAL_INT(MAXP, next(300, 0.10f, 0.45f));
}

void test_non_positive_current_returns_now(void) {
  TEST_ASSERT_EQUAL_INT(100, next(100, 0.0f, 0.45f));
  TEST_ASSERT_EQUAL_INT(100, next(100, -1.0f, 0.45f));
}

void test_nan_and_inf_return_now(void) {
  TEST_ASSERT_EQUAL_INT(100, next(100, NAN, 0.45f));
  TEST_ASSERT_EQUAL_INT(100, next(100, INFINITY, 0.45f));
  TEST_ASSERT_EQUAL_INT(100, next(100, -INFINITY, 0.45f));
}

void test_tiny_current_does_not_overflow(void) {
  // el modelo se acota a max_pwm antes del cast: sube +1, sin UB
  TEST_ASSERT_EQUAL_INT(101, next(100, 1e-9f, 0.45f));
}

void test_pwm_below_min_jumps_to_min(void) {
  // excepcion documentada: la semilla < min sube a min de una vez
  TEST_ASSERT_EQUAL_INT(MINP, next(3, 0.10f, 0.45f));
}

void test_spurious_high_reading_drops_at_most_one(void) {
  TEST_ASSERT_EQUAL_INT(99, next(100, 0.90f, 0.45f));  // lectura espuria
  TEST_ASSERT_EQUAL_INT(99, next(99, 0.45f, 0.45f));   // en banda: reinicia
  TEST_ASSERT_EQUAL_INT(98, next(99, 0.90f, 0.45f));   // otra aislada: -1
  // quedar por debajo del objetivo tambien reinicia
  next(98, 0.30f, 0.45f);
  TEST_ASSERT_EQUAL_INT(97, next(98, 0.90f, 0.45f));
}

void test_invalid_reading_resets_count(void) {
  next(100, 0.90f, 0.45f);
  next(99, NAN, 0.45f);
  TEST_ASSERT_EQUAL_INT(98, next(99, 0.90f, 0.45f));
}

void test_issue_21_seed_converges_fast(void) {
  // Unidad de banco: PWM 175 da 1.06 A; corriente proporcional al PWM.
  const float k = 1.06f / 175.0f;
  int pwm = 175;
  int iterations = 0;
  while (iterations < 20) {
    int n = next(pwm, k * (float)pwm, 0.45f);
    if (n == pwm) break;
    pwm = n;
    iterations++;
  }
  TEST_ASSERT_INT_WITHIN(2, 75, pwm);
  TEST_ASSERT_TRUE(iterations <= 8);
}

// Corriente con zona muerta: I = k * (PWM - d). El modelo proporcional
// subestima el PWM necesario (undershoot) y la subida lenta lo recupera.
static float dead_zone_current(int pwm) {
  const float k = 1.06f / (175.0f - 20.0f);
  return k * (float)(pwm - 20);
}

void test_dead_zone_descent_has_no_large_undershoot(void) {
  int pwm = 175;
  int lowest = pwm;
  for (int i = 0; i < 20; i++) {
    pwm = next(pwm, dead_zone_current(pwm), 0.45f);
    if (pwm < lowest) lowest = pwm;
  }
  float err = dead_zone_current(pwm) - 0.45f;
  TEST_ASSERT_TRUE(fabsf(err) <= TOL);
  TEST_ASSERT_TRUE(lowest >= 70);
}

void test_dead_zone_undershoot_recovers(void) {
  // Arranque por debajo del punto de trabajo: sube a +1/s hasta entrar en banda.
  int pwm = 60;
  int iterations = 0;
  while (iterations < 40) {
    int n = next(pwm, dead_zone_current(pwm), 0.45f);
    if (n == pwm) break;
    TEST_ASSERT_EQUAL_INT(pwm + 1, n);
    pwm = n;
    iterations++;
  }
  TEST_ASSERT_TRUE(fabsf(dead_zone_current(pwm) - 0.45f) <= TOL);
  TEST_ASSERT_TRUE(iterations < 40);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_in_band_does_not_move);
  RUN_TEST(test_up_limited_to_one_step);
  RUN_TEST(test_down_limited_to_twenty_on_second_reading);
  RUN_TEST(test_down_does_not_cross_model);
  RUN_TEST(test_down_exact_model_step_of_five);
  RUN_TEST(test_clamp_min_applies_after_step_limit);
  RUN_TEST(test_clamp_max);
  RUN_TEST(test_non_positive_current_returns_now);
  RUN_TEST(test_nan_and_inf_return_now);
  RUN_TEST(test_tiny_current_does_not_overflow);
  RUN_TEST(test_pwm_below_min_jumps_to_min);
  RUN_TEST(test_spurious_high_reading_drops_at_most_one);
  RUN_TEST(test_invalid_reading_resets_count);
  RUN_TEST(test_issue_21_seed_converges_fast);
  RUN_TEST(test_dead_zone_descent_has_no_large_undershoot);
  RUN_TEST(test_dead_zone_undershoot_recovers);
  return UNITY_END();
}
