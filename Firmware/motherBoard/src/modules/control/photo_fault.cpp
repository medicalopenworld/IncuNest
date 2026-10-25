#include "modules/control/photo_fault.h"

// Esqueleto de la fase roja (TDD): comportamiento ANTERIOR (sin la rama de
// ceguera), para que los tests nuevos fallen por comportamiento y no por
// enlace. La logica real llega en el commit siguiente.

void photo_fault_init(PhotoFault *f) {
  if (f == nullptr) return;
  f->low_cycles = 0;
  f->blind_cycles = 0;
  f->present = false;
}

bool photo_fault_update(PhotoFault *f, PhotoFaultInput in, float current_a) {
  if (f == nullptr) return false;
  if (in != PHOTO_FAULT_INPUT_SAMPLE) {
    f->low_cycles = 0;
    f->present = false;
    return false;
  }
  if (f->present) {
    if (current_a >= PHOTO_FAULT_CLEAR_A) {
      f->present = false;
      f->low_cycles = 0;
    }
    return f->present;
  }
  if (current_a < PHOTO_FAULT_MIN_A) {
    if (f->low_cycles < PHOTO_FAULT_CYCLES) f->low_cycles++;
    if (f->low_cycles >= PHOTO_FAULT_CYCLES) f->present = true;
  } else {
    f->low_cycles = 0;
  }
  return f->present;
}

bool photo_fault_present(const PhotoFault *f) {
  return f != nullptr && f->present;
}
