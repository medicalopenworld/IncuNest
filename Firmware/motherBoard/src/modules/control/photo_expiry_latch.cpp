#include "modules/control/photo_expiry_latch.h"

void photo_expiry_latch_init(PhotoExpiryLatch *l) {
  if (l == nullptr) return;
  l->expired = false;
  l->persist_off_pending = false;
  l->log_pending = false;
  l->log_done = false;
}

void photo_expiry_latch_expire(PhotoExpiryLatch *l) {
  if (l == nullptr) return;
  l->expired = true;
}

bool photo_expiry_latch_filter(PhotoExpiryLatch *l, bool photo, int photo_min) {
  if (l == nullptr || !l->expired) {
    return photo;
  }
  // El display confirma que ya adopto el apagado: se cierra el cerrojo.
  if (!photo) {
    l->expired = false;
    return false;
  }
  // Una sesion nueva con temporizador es una orden inequivoca del operador.
  if (photo_min > 0) {
    l->expired = false;
    return true;
  }
  // photo=1 sin minutos tras la caducidad: es la trama rezagada, no una orden.
  return false;
}

// Esqueletos de la fase roja (TDD): sin logica, para que los tests compilen.
bool photo_expiry_latch_take_persist_off(PhotoExpiryLatch *l) {
  (void)l;
  return false;
}

bool photo_expiry_latch_take_filter_log(PhotoExpiryLatch *l) {
  (void)l;
  return false;
}

bool photo_hmi_fields_valid(int photo, int photo_min) {
  (void)photo;
  (void)photo_min;
  return true;
}
