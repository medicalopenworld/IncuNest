#include "fw_image_tag.h"

#include <cstring>

// Este fichero NO menciona ningun identificador de placa a proposito: se
// compila en las dos placas, y una cadena de aqui acabaria dentro de los dos
// binarios, que es justo lo que romperia la comprobacion (ver fw_image_tag.h).

FwStreamMatcher::FwStreamMatcher(const char *pattern)
    : pat_(pattern), patLen_(0), tailLen_(0), found_(false) {
  if (pat_ == nullptr) {
    return;
  }
  const size_t len = strnlen(pat_, kMaxPattern + 1);
  if (len == 0 || len > kMaxPattern) {
    return; // patron invalido: el matcher queda desactivado
  }
  patLen_ = len;
}

void FwStreamMatcher::reset() {
  tailLen_ = 0;
  found_ = false;
}

bool FwStreamMatcher::contains(const uint8_t *buf, size_t len) const {
  if (len < patLen_) {
    return false;
  }
  const size_t last = len - patLen_;
  for (size_t i = 0; i <= last; i++) {
    if (buf[i] == (uint8_t)pat_[0] &&
        memcmp(buf + i, pat_, patLen_) == 0) {
      return true;
    }
  }
  return false;
}

void FwStreamMatcher::feed(const uint8_t *data, size_t len) {
  if (found_ || patLen_ == 0 || data == nullptr || len == 0) {
    return;
  }

  const size_t keep = patLen_ - 1; // bytes de solape que hay que arrastrar

  // 1) Coincidencias a caballo entre el trozo anterior y este. La ventana
  //    incluye trozos ya mirados (la cola) y por mirar (la cabeza); repetirlos
  //    no cuesta nada y evita tener que razonar sobre indices.
  if (tailLen_ > 0 && keep > 0) {
    uint8_t window[kMaxPattern * 2];
    const size_t head = (len < keep) ? len : keep;
    memcpy(window, tail_, tailLen_);
    memcpy(window + tailLen_, data, head);
    if (contains(window, tailLen_ + head)) {
      found_ = true;
      return;
    }
  }

  // 2) El trozo entero.
  if (contains(data, len)) {
    found_ = true;
    return;
  }

  // 3) Guardar la cola para la siguiente llamada.
  if (keep == 0) {
    tailLen_ = 0;
  } else if (len >= keep) {
    memcpy(tail_, data + len - keep, keep);
    tailLen_ = keep;
  } else {
    const size_t total = tailLen_ + len;
    if (total <= keep) {
      memcpy(tail_ + tailLen_, data, len);
      tailLen_ = total;
    } else {
      const size_t drop = total - keep;
      memmove(tail_, tail_ + drop, tailLen_ - drop);
      memcpy(tail_ + tailLen_ - drop, data, len);
      tailLen_ = keep;
    }
  }
}

bool fw_image_is_foreign(bool selfTagSeen, bool anyTagSeen) {
  return !selfTagSeen && anyTagSeen;
}
