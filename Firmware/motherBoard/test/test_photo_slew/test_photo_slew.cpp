// Regresion del slew asimetrico del lazo de intensidad de fototerapia.
// Ver modules/control/photo_slew.h y known_issues.md, apartado 21.

#include <unity.h>

#include "modules/control/photo_slew.h"

#define UP 1
#define DOWN 20
#define TOL 0.02f
#define MINP 10
#define MAXP 255

void setUp(void) {}
void tearDown(void) {}

static int next(int pwm, float cur, float tgt) {
  return photo_slew_next_pwm(pwm, cur, tgt, TOL, UP, DOWN, MINP, MAXP);
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

void test_down_limited_to_twenty(void) {
  // modelo: 200 * 0.45 / 0.90 = 100 -> -100
  TEST_ASSERT_EQUAL_INT(180, next(200, 0.90f, 0.45f));
}

void test_down_does_not_cross_model(void) {
  // modelo: 100 * 0.45 / 0.4737 = 95 -> -5
  TEST_ASSERT_EQUAL_INT(95, next(100, 0.4737f, 0.45f));
}

void test_clamp_min(void) {
  // el modelo pide 1; el delta de -20 caeria por debajo de MINP
  TEST_ASSERT_EQUAL_INT(MINP, next(15, 4.5f, 0.45f));
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
  TEST_ASSERT_TRUE(iterations <= 6);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_in_band_does_not_move);
  RUN_TEST(test_up_limited_to_one_step);
  RUN_TEST(test_down_limited_to_twenty);
  RUN_TEST(test_down_does_not_cross_model);
  RUN_TEST(test_clamp_min);
  RUN_TEST(test_clamp_max);
  RUN_TEST(test_non_positive_current_returns_now);
  RUN_TEST(test_issue_21_seed_converges_fast);
  return UNITY_END();
}
