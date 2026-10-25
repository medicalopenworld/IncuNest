#include "photo_override.h"

// Esqueleto de la fase roja (TDD): las funciones existen para que los tests
// compilen y fallen por comportamiento. La logica llega en el commit siguiente.

void photo_override_init(PhotoOverride *o) {
  o->active = false;
  o->ordered = false;
  o->release_pending = false;
}

void photo_override_start(PhotoOverride *o, bool ordered_now) {
  (void)o;
  (void)ordered_now;
}

void photo_override_note_order(PhotoOverride *o, bool ordered) {
  (void)o;
  (void)ordered;
}

void photo_override_release(PhotoOverride *o) { (void)o; }

bool photo_override_active(const PhotoOverride *o) {
  (void)o;
  return false;
}

bool photo_override_effective(PhotoOverride *o, bool display_value) {
  (void)o;
  return display_value;
}

bool photo_override_may_persist(const PhotoOverride *o) {
  (void)o;
  return true;
}

bool photo_override_forced(const PhotoOverride *o, bool *value) {
  (void)o;
  (void)value;
  return false;
}

bool photo_override_take_release(PhotoOverride *o, bool *value) {
  (void)o;
  (void)value;
  return false;
}

bool photo_override_state_field(const PhotoOverride *o, bool applied) {
  (void)o;
  return applied;
}
