#include "family_state.h"

#include <Arduino.h>
#include <string.h>
#include <time.h>

#include "family_notice.h"
#include "modules/util/system_clock.h"

// Un solo spinlock para el estado. La inicializacion del nucleo es perezosa y
// va dentro de la seccion critica (no hay un family_state_init() en setup():
// asi el modulo no obliga a tocar main.cpp y no depende del orden de arranque).
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static FamilyNotice s_notice;
static bool s_inited = false;

// Llamar con s_mux tomado.
static inline FamilyNotice *notice_locked() {
  if (!s_inited) {
    family_notice_init(&s_notice);
    s_inited = true;
  }
  return &s_notice;
}

void family_state_apply_json(JsonObjectConst const &data, bool fromRequest) {
  // Todo lo que lee JSON o el reloj, FUERA de la seccion critica.
  JsonVariantConst holdV = data["hand_hold_until"];
  const bool hasHold = holdV.is<double>();
  const double until = hasHold ? holdV.as<double>() : 0.0;

  // En la respuesta a la peticion de reconexion, una clave ausente = borrada en
  // ThingsBoard: se olvida la URL (ver family_notice_url_action).
  JsonVariantConst urlV = data["family_qr_url"];
  const char *urlS =
      urlV.is<const char *>() ? urlV.as<const char *>() : nullptr;
  const FamilyUrlAction urlAction = family_notice_url_action(
      data.containsKey("family_qr_url"), urlS != nullptr, fromRequest);
  char url[FAMILY_URL_MAX + 1];
  url[0] = '\0';
  // Mas larga que el tope: queda vacia y el nucleo no pinta QR; no se acepta
  // un prefijo truncado.
  if (urlAction == FAMILY_URL_SET && strlen(urlS) <= FAMILY_URL_MAX) {
    strncpy(url, urlS, FAMILY_URL_MAX);
    url[FAMILY_URL_MAX] = '\0';
  }

  const time_t t = time(nullptr);
  const uint32_t nowEpochS = (t < 1609459200) ? 0u : (uint32_t)t;  // 2021-01-01
  const Proto_TimeSource src = systemClockSource();
  const bool fromNet =
      (src == PROTO_TIME_SOURCE_NTP || src == PROTO_TIME_SOURCE_NITZ);
  const uint32_t nowMs = millis();

  portENTER_CRITICAL(&s_mux);
  FamilyNotice *f = notice_locked();
  if (hasHold) {
    family_notice_on_hand_hold(f, until, fromRequest, nowEpochS, fromNet, nowMs);
  }
  if (urlAction != FAMILY_URL_KEEP) {
    family_notice_set_url(f, urlAction == FAMILY_URL_SET ? url : nullptr);
  }
  portEXIT_CRITICAL(&s_mux);
}

void family_state_on_discharge(void) {
  portENTER_CRITICAL(&s_mux);
  family_notice_on_discharge(notice_locked());
  portEXIT_CRITICAL(&s_mux);
}

bool family_state_take_msg(uint32_t nowMs, bool force, FamilyMsg *out) {
  portENTER_CRITICAL(&s_mux);
  const bool r = family_notice_take_msg(notice_locked(), nowMs, force, out);
  portEXIT_CRITICAL(&s_mux);
  return r;
}
