#include "sb_env_fusion.h"

static float median3(float a, float b, float c) {
  if ((a <= b && b <= c) || (c <= b && b <= a)) return b;
  if ((b <= a && a <= c) || (c <= a && a <= b)) return a;
  return c;
}

SbFusion sb_fuse(const bool valid[3], const float value[3], float lo,
                 float hi) {
  SbFusion out;
  out.valid = false;
  out.value = 0.0f;
  out.used = 0;

  // Gate de plausibilidad por posicion: un valor imposible no es una lectura,
  // aunque el CRC del enlace fuera bueno.
  float ok[3];
  uint8_t n = 0;
  for (int i = 0; i < 3; i++) {
    if (!valid[i]) continue;
    const float v = value[i];
    if (v < lo || v > hi) continue;
    ok[n++] = v;
  }
  if (n == 0) return out;

  out.valid = true;
  out.used = n;
  if (n == 3) {
    out.value = median3(ok[0], ok[1], ok[2]);
  } else if (n == 2) {
    out.value = (ok[0] + ok[1]) * 0.5f;
  } else {
    out.value = ok[0];
  }
  return out;
}
