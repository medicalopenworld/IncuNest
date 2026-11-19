#include "modules/util/vitals_out.h"

#include <math.h>

// See vitals_out.h. Every comparison is written so that NaN fails it.

bool vitals_has_value(float x) { return isfinite(x); }

uint16_t vitals_hr_bpm(float hr) {
  if (!(isfinite(hr) && hr > 0.0f && hr <= VITALS_HR_MAX_BPM)) {
    return 0;
  }
  return (uint16_t)(hr + 0.5f);
}

uint16_t vitals_spo2_pct(float spo2) {
  if (!(isfinite(spo2) && spo2 > 0.0f && spo2 <= VITALS_SPO2_MAX_PCT)) {
    return 0;
  }
  return (uint16_t)(spo2 + 0.5f);
}

float vitals_pi_pct(float pi) {
  if (!(isfinite(pi) && pi >= 0.0f && pi <= VITALS_PI_MAX_PCT)) {
    return 0.0f;
  }
  return pi;
}
