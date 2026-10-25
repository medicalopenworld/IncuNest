#include "photo_override.h"

// Ver photo_override.h para el porque de cada regla.

void photo_override_init(PhotoOverride *o) {
  o->active = false;
  o->ordered = false;
  o->release_pending = false;
}

void photo_override_start(PhotoOverride *o, bool ordered_now) {
  // Solo se captura la orden si no hay una prueba en curso: en ese caso
  // `ordered` ya lleva las ordenes del operador y `ordered_now` esta
  // contaminado por la prueba.
  if (!o->active) {
    o->ordered = ordered_now;
  }
  o->active = true;
  // Volver a encender antes de aplicar lo pendiente cancela la liberacion.
  o->release_pending = false;
}

void photo_override_note_order(PhotoOverride *o, bool ordered) {
  if (o->active) {
    o->ordered = ordered;
  }
}

void photo_override_release(PhotoOverride *o) {
  if (!o->active) {
    return;
  }
  o->active = false;
  o->release_pending = true;
}

bool photo_override_active(const PhotoOverride *o) { return o->active; }

bool photo_override_effective(PhotoOverride *o, bool display_value) {
  if (o->active) {
    // La orden no se aplica (la lampara va forzada a ON) pero se REGISTRA: al
    // soltar manda la ultima del operador.
    o->ordered = display_value;
    return true;
  }
  return display_value;
}

bool photo_override_may_persist(const PhotoOverride *o) { return !o->active; }

bool photo_override_forced(const PhotoOverride *o, bool *value) {
  if (o->active) {
    *value = true;
    return true;
  }
  return false;
}

bool photo_override_take_release(PhotoOverride *o, bool *value) {
  if (!o->release_pending) {
    return false;
  }
  o->release_pending = false;
  *value = o->ordered;
  return true;
}

bool photo_override_state_field(const PhotoOverride *o, bool applied) {
  return o->active ? o->ordered : applied;
}
