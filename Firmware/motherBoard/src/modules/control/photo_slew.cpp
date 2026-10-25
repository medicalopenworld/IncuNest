#include "modules/control/photo_slew.h"

#include <math.h>

void photo_slew_init(PhotoSlew *s) { s->high_count = 0; }

int photo_slew_step(PhotoSlew *s, int pwm_now, float current_a, float target_a,
                    float tol_a, int max_step_up, int max_step_down,
                    int min_pwm, int max_pwm) {
  if (!(current_a > 0.0f) || !isfinite(current_a)) {
    s->high_count = 0;
    return pwm_now;
  }
  const float error = current_a - target_a;
  if (error <= tol_a && error >= -tol_a) {
    s->high_count = 0;
    return pwm_now;
  }

  int down_limit = max_step_up < max_step_down ? max_step_up : max_step_down;
  if (error > 0.0f) {
    if (s->high_count < 2) s->high_count++;
    if (s->high_count >= 2) down_limit = max_step_down;
  } else {
    s->high_count = 0;
  }

  // Acotar en float ANTES del cast: una corriente diminuta desborda el int.
  float model_f = (float)pwm_now * target_a / current_a;
  if (!(model_f > 0.0f)) model_f = 0.0f;
  if (model_f > (float)max_pwm) model_f = (float)max_pwm;
  const int model = (int)(model_f + 0.5f);

  int delta = model - pwm_now;
  if (delta > max_step_up) delta = max_step_up;
  if (delta < -down_limit) delta = -down_limit;

  int next = pwm_now + delta;
  if (next < min_pwm) next = min_pwm;
  if (next > max_pwm) next = max_pwm;
  return next;
}
