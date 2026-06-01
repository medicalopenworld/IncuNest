#pragma once
#include <stdbool.h>

typedef struct {
  float kp, ki, kd;
  float output_min, output_max;
  float integral;
  float prev_error;
} PidWrapper;

void  pid_init(PidWrapper *pid, float kp, float ki, float kd,
               float out_min, float out_max);
float pid_compute(PidWrapper *pid, float setpoint, float measured,
                  unsigned long dt_ms);
void  pid_reset(PidWrapper *pid);
