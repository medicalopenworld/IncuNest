#include "modules/trend/trend_types.h"

#include <cmath>

TrendSample trend_empty_sample(void) {
  TrendSample s;
  for (int i = 0; i < TREND_SERIES; i++) s.v[i] = TREND_NO_DATA;
  return s;
}

int16_t trend_tenths(float value, bool ok) {
  if (!ok || !std::isfinite(value)) return TREND_NO_DATA;
  const float t = value * 10.0f;
  // Saturar antes de convertir: lroundf de un float enorme es indefinido, y
  // -32768 es TREND_NO_DATA, asi que el piso legible es -32767.
  if (t >= 32767.0f) return 32767;
  if (t <= -32767.0f) return -32767;
  return (int16_t)lroundf(t);
}
