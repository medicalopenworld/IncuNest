#include "ppg_snapshot_plan.h"

uint16_t ppg_chunk_end(uint16_t next, uint16_t n, uint16_t chunk) {
  // En uint32_t: next + chunk puede pasar de 65535 y dar la vuelta.
  uint32_t const end = (uint32_t)next + chunk;
  return end < n ? (uint16_t)end : n;
}

uint64_t ppg_sample_ts_ms(uint64_t lastMs, uint16_t n, uint16_t i,
                          uint32_t stepMs) {
  return lastMs - (uint64_t)(n - 1 - i) * stepMs;
}

bool ppg_elapsed(uint32_t now, uint32_t since, uint32_t interval) {
  return (uint32_t)(now - since) >= interval;
}

bool ppg_autocapture_due(const PpgAutoCapture *st, uint32_t now,
                         uint32_t intervalMs, uint32_t retryMs) {
  if (!st->everTried)
    return true;
  if (st->everStarted && !ppg_elapsed(now, st->lastStartMs, intervalMs))
    return false;
  return ppg_elapsed(now, st->lastTryMs, retryMs);
}

void ppg_autocapture_record(PpgAutoCapture *st, uint32_t now, bool started) {
  st->everTried = true;
  st->lastTryMs = now;
  if (started) {
    st->everStarted = true;
    st->lastStartMs = now;
  }
}
