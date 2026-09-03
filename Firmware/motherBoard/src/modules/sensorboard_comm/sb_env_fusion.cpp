#include "sb_env_fusion.h"

static float median3(float a, float b, float c) {
  if ((a <= b && b <= c) || (c <= b && b <= a)) return b;
  if ((b <= a && a <= c) || (c <= a && a <= b)) return a;
  return c;
}

static float absf(float x) { return x < 0.0f ? -x : x; }

SbFusion sb_fuse(const bool valid[3], const float value[3], float lo, float hi,
                 float max_spread) {
  SbFusion out;
  out.valid = false;
  out.value = 0.0f;
  out.has_redundant = false;
  out.redundant = 0.0f;
  out.used = 0;
  out.discarded = 0;

  // 1. Gate de plausibilidad por posicion. Una lectura fuera de rango no es
  //    una lectura, aunque el CRC del enlace fuera bueno.
  float ok[3];
  uint8_t n = 0;
  for (int i = 0; i < 3; i++) {
    if (!valid[i]) continue;
    const float v = value[i];
    if (v < lo || v > hi) continue;
    ok[n++] = v;
  }
  if (n == 0) return out;

  if (n == 1) {
    // Sin redundancia, pero con medida. Cortar el control termico por haber
    // perdido la redundancia seria peor que seguir con un solo sensor.
    out.valid = true;
    out.value = ok[0];
    out.used = 1;
    return out;
  }

  if (n == 2) {
    // Dos en desacuerdo no se pueden arbitrar: no hay tercero que vote. Es
    // preferible declarar que no hay medida y dejar que salte la alarma de
    // sensor de aire a promediar una mentira con una verdad.
    if (absf(ok[0] - ok[1]) > max_spread) {
      out.discarded = 2;
      return out;
    }
    out.valid = true;
    out.value = (ok[0] + ok[1]) * 0.5f;
    out.has_redundant = true;
    out.redundant = ok[1];
    out.used = 2;
    return out;
  }

  // 3 posiciones: la mediana es inmune a un sensor mentiroso, por arriba o
  // por abajo. Se descarta lo que se aparte de ella y se promedia el resto.
  const float med = median3(ok[0], ok[1], ok[2]);
  float sum = 0.0f;
  uint8_t used = 0;
  float first_other = 0.0f;
  bool have_other = false;
  for (int i = 0; i < 3; i++) {
    if (absf(ok[i] - med) > max_spread) {
      out.discarded++;
      continue;
    }
    sum += ok[i];
    used++;
    if (used == 2) {
      first_other = ok[i];
      have_other = true;
    }
  }
  // used siempre >= 1: la propia mediana dista 0 de si misma.
  out.valid = true;
  out.value = sum / used;
  out.used = used;
  out.has_redundant = have_other;
  out.redundant = first_other;
  return out;
}
