#include "modules/control/photo_fault.h"

// Esqueleto de la fase roja (TDD): las funciones existen para que los tests
// compilen y fallen por comportamiento, no por enlace. La logica real llega en
// el commit siguiente.

void photo_fault_init(PhotoFault *f) {
  if (f == nullptr) return;
  f->low_cycles = 0;
  f->present = false;
}

bool photo_fault_update(PhotoFault *f, bool evaluable, float current_a) {
  (void)f;
  (void)evaluable;
  (void)current_a;
  return false;
}

bool photo_fault_present(const PhotoFault *f) {
  return f != nullptr && f->present;
}
