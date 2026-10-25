#include "modules/control/photo_expiry_latch.h"

// Esqueleto de la fase roja (TDD): sin logica, para que los tests compilen.

void photo_expiry_latch_init(PhotoExpiryLatch *l) {
  if (l == nullptr) return;
  l->expired = false;
}

void photo_expiry_latch_expire(PhotoExpiryLatch *l) { (void)l; }

bool photo_expiry_latch_filter(PhotoExpiryLatch *l, bool photo, int photo_min) {
  (void)l;
  (void)photo_min;
  return photo;
}
