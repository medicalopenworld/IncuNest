#include "photo_override.h"

// Ver photo_override.h para el porque de cada regla.

// Tiempo desde que se solto, comparando intervalos (resta sin signo) y no
// marcas: asi sobrevive al desbordamiento de millis().
static uint32_t since_release(const PhotoOverride *o, uint32_t now_ms) {
  return (uint32_t)(now_ms - o->release_since_ms);
}

void photo_override_init(PhotoOverride *o) {
  o->active = false;
  o->real_before = false;
  o->releasing = false;
  o->overdue_reported = false;
  o->release_since_ms = 0;
}

void photo_override_start(PhotoOverride *o, bool real_now, uint32_t now_ms) {
  (void)now_ms;
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
  o->overdue_reported = false;
  o->release_since_ms = now_ms;
}

bool photo_override_active(const PhotoOverride *o) {
  return o->active;
}

bool photo_override_effective(PhotoOverride *o, bool display_value,
                              uint32_t now_ms) {
  if (o->active) {
    return true;
  }
  if (o->releasing) {
    // Confirmacion: el display ya devuelve lo que la placa emite. Antes del
    // minimo no cuenta (puede ser una trama anterior a que adoptase el ON).
    if (display_value == o->real_before &&
        since_release(o, now_ms) >= PHOTO_OVERRIDE_CONFIRM_MIN_MS) {
      o->releasing = false;
      return display_value;
    }
    // Sin confirmacion SIEMPRE se fuerza el valor real, venza o no el plazo
    // largo: adoptar un 1 sin confirmar es lo que esta maquina existe para
    // evitar.
    return o->real_before;
  }
  return display_value;
}

bool photo_override_may_persist(PhotoOverride *o, uint32_t now_ms) {
  (void)now_ms;
  return !o->active && !o->releasing;
}

bool photo_override_unconfirmed_overdue(PhotoOverride *o, uint32_t now_ms) {
  if (!o->releasing || o->overdue_reported) {
    return false;
  }
  if (since_release(o, now_ms) > PHOTO_OVERRIDE_RELEASE_BACKSTOP_MS) {
    o->overdue_reported = true;
    return true;
  }
  return false;
}

bool photo_override_forced(const PhotoOverride *o, bool *value) {
  if (o->active) {
    *value = true;
    return true;
  }
  if (o->releasing) {
    *value = o->real_before;
    return true;
  }
  return false;
}
