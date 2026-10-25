#include "modules/trend/trend_model.h"

void trend_model_init(TrendModel *m, int16_t *block, int capacity) {
  if (!m) return;
  trend_store_init(&m->store, block, capacity);
  trend_events_init(&m->events);
  trend_clock_init(&m->clock);
  m->clockShifts = 0;
}

int64_t trend_model_tick(TrendModel *m, int64_t monoMs, uint32_t epoch) {
  if (!m) return 0;
  const int64_t shift = trend_clock_update(&m->clock, monoMs, epoch);
  if (shift != 0) {
    // La hora re-etiqueta lo guardado: misma cantidad para el anillo y los
    // eventos, para que sigan alineados entre si.
    trend_store_shift(&m->store, shift);
    trend_events_shift(&m->events, shift);
    m->clockShifts++;
  }
  return trend_clock_slot(&m->clock, monoMs);
}

bool trend_model_record(TrendModel *m, int64_t monoMs, uint32_t epoch,
                        const TrendSample *sample) {
  if (!m) return false;
  const int64_t slot = trend_model_tick(m, monoMs, epoch);
  return trend_store_record(&m->store, slot, sample);
}

void trend_model_edge(TrendModel *m, int64_t monoMs, uint8_t id,
                      uint8_t priority, bool raised) {
  if (!m) return;
  trend_events_add(&m->events, trend_clock_slot(&m->clock, monoMs), id, priority,
                   raised);
}

int64_t trend_model_newest_slot(const TrendModel *m) {
  return (m && m->store.count > 0) ? m->store.lastSlot : 0;
}
