#include "pid_wrapper.h"
#include <string.h>

void pid_init(PidWrapper *pid, float kp, float ki, float kd,
              float out_min, float out_max) {
  memset(pid, 0, sizeof(*pid));
  pid->kp = kp; pid->ki = ki; pid->kd = kd;
  pid->output_min = out_min; pid->output_max = out_max;
}

float pid_compute(PidWrapper *pid, float setpoint, float measured,
                  unsigned long dt_ms) {
  float dt  = dt_ms / 1000.0f;
  float err = setpoint - measured;
  pid->integral  += err * dt;
  float deriv     = dt > 0.0f ? (err - pid->prev_error) / dt : 0.0f;
  pid->prev_error = err;
  float out = pid->kp * err + pid->ki * pid->integral + pid->kd * deriv;
  if (out < pid->output_min) { out = pid->output_min; pid->integral -= err * dt; }
  if (out > pid->output_max) { out = pid->output_max; pid->integral -= err * dt; }
  return out;
}

void pid_reset(PidWrapper *pid) {
  pid->integral   = 0.0f;
  pid->prev_error = 0.0f;
}
