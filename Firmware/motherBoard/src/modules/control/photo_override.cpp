#include "photo_override.h"

// Ver photo_override.h para el porque de cada regla.

// Cierra la ventana de gracia si ya ha vencido. Resta sin signo y se compara
// el intervalo, no las marcas: asi sobrevive al desbordamiento de millis().
static void expire_grace(PhotoOverride *o, uint32_t now_ms) {
  if (o->releasing &&
      (uint32_t)(now_ms - o->release_since_ms) > PHOTO_OVERRIDE_RELEASE_GRACE_MS) {
    o->releasing = false;
  }
}

void photo_override_init(PhotoOverride *o) {
  o->active = false;
  o->real_before = false;
  o->releasing = false;
  o->release_since_ms = 0;
}

void photo_override_start(PhotoOverride *o, bool real_now, uint32_t now_ms) {
  expire_grace(o, now_ms);
  // Solo se captura el estado real si no hay una prueba en curso ni la cola de
  // una anterior: en esos dos casos real_now ya esta contaminado por la prueba.
  if (!o->active && !o->releasing) {
    o->real_before = real_now;
  }
  o->active = true;
  o->releasing = false;
}

void photo_override_release(PhotoOverride *o, uint32_t now_ms) {
  if (!o->active) {
    return;
  }
  o->active = false;
  o->releasing = true;
  o->release_since_ms = now_ms;
}

bool photo_override_active(const PhotoOverride *o) {
  return o->active;
}

bool photo_override_effective(PhotoOverride *o, bool display_value,
                              uint32_t now_ms) {
  expire_grace(o, now_ms);
  if (o->active) {
    return true;
  }
  if (o->releasing) {
    return o->real_before;
  }
  return display_value;
}

bool photo_override_may_persist(PhotoOverride *o, uint32_t now_ms) {
  expire_grace(o, now_ms);
  return !o->active && !o->releasing;
}
