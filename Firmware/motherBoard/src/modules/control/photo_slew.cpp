#include "modules/control/photo_slew.h"

int photo_slew_next_pwm(int pwm_now, float current_a, float target_a,
                        float tol_a, int max_step_up, int max_step_down,
                        int min_pwm, int max_pwm) {
  if (current_a <= 0.0f) return pwm_now;
  const float error = current_a - target_a;
  if (error <= tol_a && error >= -tol_a) return pwm_now;

  const int model =
      (int)((float)pwm_now * target_a / current_a + 0.5f);
  int delta = model - pwm_now;
  if (delta > max_step_up) delta = max_step_up;
  if (delta < -max_step_down) delta = -max_step_down;

  int next = pwm_now + delta;
  if (next < min_pwm) next = min_pwm;
  if (next > max_pwm) next = max_pwm;
  return next;
}
