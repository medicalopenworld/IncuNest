#include "rtc_write_policy.h"

static const uint32_t kMinEpoch = 1609459200u; // 2021-01-01T00:00:00Z
static const uint32_t kMaxEpoch = 4102444800u; // 2100-01-01T00:00:00Z

bool rtc_should_write(uint32_t incomingEpoch, Proto_TimeSource incomingSrc,
                      uint32_t rtcEpoch, Proto_TimeSource storedSrc) {
  // epoch 0 es "la motherBoard aun no ha sincronizado", el caso normal de un
  // arranque sin red. La ventana cubre ademas cualquier valor absurdo.
  if (incomingEpoch < kMinEpoch || incomingEpoch >= kMaxEpoch) {
    return false;
  }
  // Una fuente desconocida no mejora nada, ni siquiera un chip vacio: viene de
  // una motherBoard anterior a este cambio, que no envia el campo `src`, y su
  // hora podria venir de un NITZ malo. Lo que el chip ya tenga es al menos
  // trazable.
  if (incomingSrc == PROTO_TIME_SOURCE_NONE) {
    return false;
  }
  // El rango MANUAL de la NVS no sobrevive a un reinicio de la motherBoard,
  // cuyo rango vive en RAM: una hora manual manda hasta que el equipo se
  // reinicia (spec mb-time-source-priority). Como la motherBoard nunca difunde
  // NTP en el mismo arranque en que se puso la hora a mano (la manual para
  // SNTP y gana el arbitraje), un NTP sobre un chip marcado MANUAL significa
  // que esa hora ya caduco: se compara como si el chip fuera de rango NTP.
  // Sin esto, la IncuNest 353 sembro durante semanas una hora de verano
  // tecleada a mano (~2 h de mas) que ningun NTP podia volver a corregir.
  if (storedSrc == PROTO_TIME_SOURCE_MANUAL &&
      incomingSrc == PROTO_TIME_SOURCE_NTP) {
    storedSrc = PROTO_TIME_SOURCE_NTP;
  }
  if (incomingSrc > storedSrc) {
    return true; // mejor fuente: se escribe aunque la diferencia sea de 1 s
  }
  if (incomingSrc < storedSrc) {
    return false; // peor fuente: no se toca el chip
  }

  // Mismo rango. Se escribe solo si el chip ha derivado lo suficiente. Un
  // rtcEpoch de 0 (chip sin hora valida, VL puesto) cuenta como deriva
  // infinita, que es lo que se quiere: hay que sembrarlo.
  if (rtcEpoch == 0) {
    return true;
  }
  const uint32_t diff =
      incomingEpoch > rtcEpoch ? incomingEpoch - rtcEpoch : rtcEpoch - incomingEpoch;
  return diff > RTC_WRITE_DRIFT_TOLERANCE_S;
}
