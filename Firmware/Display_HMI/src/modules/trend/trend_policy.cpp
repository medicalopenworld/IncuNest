#include "modules/trend/trend_policy.h"

TrendSetpointValidity trend_setpoint_validity(int actuation, bool airMode) {
  TrendSetpointValidity v = {false, false, false};
  // ActuationMode (shared/control_types.h): 0 off, 1 temp, 2 hum, 3 ambas. Se
  // usan los valores y no el enum para que el modulo compile sin shared/.
  if (actuation < 0 || actuation > 3) return v;
  const bool tempOn = (actuation == 1 || actuation == 3);
  const bool humOn = (actuation == 2 || actuation == 3);
  v.air = tempOn && airMode;
  v.skin = tempOn && !airMode;
  v.hum = humOn;
  return v;
}

int trend_slot_to_px(int64_t slot, int64_t startSlot, int64_t totalSlots,
                     int widthPx) {
  if (totalSlots <= 0 || widthPx <= 0) return -1;
  const int64_t off = slot - startSlot;
  if (off < 0 || off >= totalSlots) return -1;
  return (int)((off * (int64_t)widthPx) / totalSlots);
}

void trend_dump_init(TrendDumpState *d, uint32_t nowMs) {
  if (!d) return;
  d->lastDumpMs = nowMs;
  d->lastDumpedSlot = 0;
  d->haveDumped = false;
}

bool trend_dump_due(const TrendDumpState *d, bool clockSynced, bool haveSamples,
                    int64_t newestSlot, uint32_t nowMs, bool lockEntered) {
  if (!d || !clockSynced || !haveSamples) return false;
  if (d->haveDumped && newestSlot == d->lastDumpedSlot) return false;
  // Resta sin signo: correcta aunque millis() haya dado la vuelta (49 dias).
  const uint32_t elapsed = nowMs - d->lastDumpMs;
  return elapsed >= TREND_DUMP_PERIOD_MS ||
         (lockEntered && elapsed >= TREND_DUMP_LOCK_MIN_MS);
}

void trend_dump_done(TrendDumpState *d, uint32_t nowMs, int64_t newestSlot) {
  if (!d) return;
  d->lastDumpMs = nowMs;
  d->lastDumpedSlot = newestSlot;
  d->haveDumped = true;
}

void trend_dump_failed(TrendDumpState *d, uint32_t nowMs) {
  if (!d) return;
  d->lastDumpMs = nowMs;  // reintento tras un periodo completo; la ranura NO se da por guardada
}

bool trend_chunk_due(uint32_t nowMs, uint32_t lastChunkMs) {
  return (uint32_t)(nowMs - lastChunkMs) >= TREND_CHUNK_GAP_MS;
}

bool trend_io_allowed(bool anyAlarmActive, bool boardLinkLost) {
  return !anyAlarmActive && !boardLinkLost;
}

bool trend_dump_blocked_by_rejected_file(bool rejectedPending, uint32_t rejectedAtMs,
                                         uint32_t nowMs) {
  if (!rejectedPending) return false;
  // Resta sin signo: segura ante la vuelta de millis().
  return (uint32_t)(nowMs - rejectedAtMs) < TREND_REJECT_GRACE_MS;
}

TrendMarkKind trend_mark_kind(uint8_t priority, bool raised) {
  if (!raised) return TREND_MARK_CLEARED;
  switch (priority) {
    case 0:
      return TREND_MARK_LOW;
    case 1:
      return TREND_MARK_MEDIUM;
    default:
      return TREND_MARK_HIGH;  // 2 y cualquier valor desconocido
  }
}
