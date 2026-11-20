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
  // Persistir OFF y el aviso de log son UNO por episodio. getRemainingPhotoTime()
  // la puede detectar a la vez desde dos tareas: la segunda llamada, dentro del
  // mismo episodio, no vuelve a armar nada.
  if (!l->expired) {
    l->persist_off_pending = true;
    l->log_done = false;
    l->log_pending = false;
  }
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
  // (Tambien la "continua" del modo formacion, que va con photoMin=5.)
  if (photo_min > 0) {
    l->expired = false;
    return true;
  }
  // photo=1 sin minutos tras la caducidad: es la trama rezagada, no una orden.
  // Un solo aviso de log por episodio: la trama retenida llega muchas veces.
  if (!l->log_done) {
    l->log_done = true;
    l->log_pending = true;
  }
  return false;
}

bool photo_expiry_latch_take_persist_off(PhotoExpiryLatch *l) {
  if (l == nullptr || !l->persist_off_pending) return false;
  l->persist_off_pending = false;
  return true;
}

bool photo_expiry_latch_take_filter_log(PhotoExpiryLatch *l) {
  if (l == nullptr || !l->log_pending) return false;
  l->log_pending = false;
  return true;
}

bool photo_hmi_fields_valid(int photo, int photo_min) {
  return (photo == 0 || photo == 1) && photo_min >= 0 &&
         photo_min <= PHOTO_TIMER_MAX_MINUTES;
}
