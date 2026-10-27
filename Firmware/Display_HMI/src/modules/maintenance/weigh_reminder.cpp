#include "modules/maintenance/weigh_reminder.h"

uint32_t weigh_reference_epoch(const weigh_inputs_t *in) {
  if (in == nullptr || in->activeSeq == 0) return 0;
  // La pesada manda sobre el "visto", pero solo si es de ESTE bebe: un sello
  // de otro paciente no dice nada de este.
  if (in->weighSeq == in->activeSeq && in->weighEpoch != 0) {
    return in->weighEpoch;
  }
  if (in->seenSeq == in->activeSeq && in->seenEpoch != 0) {
    return in->seenEpoch;
  }
  return 0;
}

uint32_t weigh_elapsed_hours(const weigh_inputs_t *in) {
  if (in == nullptr || in->nowEpoch == 0) return 0;
  const uint32_t ref = weigh_reference_epoch(in);
  if (ref == 0 || ref > in->nowEpoch) return 0;
  return (in->nowEpoch - ref) / 3600u;
}

bool weigh_reminder_due(const weigh_inputs_t *in) {
  if (in == nullptr) return false;
  if (!in->liveSession || in->activeSeq == 0) return false;
  // Sin hora no se puede afirmar nada: no se inventa con millis().
  if (in->nowEpoch == 0) return false;
  const uint32_t ref = weigh_reference_epoch(in);
  if (ref == 0) return false;
  // Referencia en el futuro (reloj corregido hacia atras): no vencido. El
  // tracker la reancla a hoy; aqui solo se evita avisar mientras tanto.
  if (ref > in->nowEpoch) return false;
  return (in->nowEpoch - ref) >= WEIGH_REMINDER_PERIOD_SECS;
}

bool weigh_reminder_should_warn(const weigh_inputs_t *in) {
  if (!weigh_reminder_due(in)) return false;
  // `due` ya garantiza nowEpoch != 0.
  return in->snoozeUntil == 0 || in->nowEpoch >= in->snoozeUntil;
}

uint32_t weigh_snooze_until(uint32_t nowEpoch) {
  if (nowEpoch == 0) return 0;
  return nowEpoch + WEIGH_REMINDER_SNOOZE_SECS;
}
