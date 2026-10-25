#include "modules/trend/trend_events.h"

void trend_events_init(TrendEvents *e) {
  if (!e) return;
  e->head = 0;
  e->count = 0;
}

void trend_events_add(TrendEvents *e, int64_t slot, uint8_t id, uint8_t priority,
                      bool raised) {
  if (!e) return;
  if (e->count >= TREND_MAX_EVENTS) {
    // Lleno: se pierde el mas antiguo, que es el primero en salir de cualquier
    // ventana de todos modos.
    e->head = (e->head + 1) % TREND_MAX_EVENTS;
    e->count--;
  }
  TrendEvent &slotRef = e->ev[(e->head + e->count) % TREND_MAX_EVENTS];
  slotRef.slot = slot;
  slotRef.id = id;
  slotRef.priority = priority;
  slotRef.raised = raised;
  e->count++;
}

bool trend_events_get(const TrendEvents *e, int i, TrendEvent *out) {
  if (!e || !out || i < 0 || i >= e->count) return false;
  *out = e->ev[(e->head + i) % TREND_MAX_EVENTS];
  return true;
}

int trend_events_in_window(const TrendEvents *e, int64_t loSlot, int64_t hiSlot,
                           TrendEvent *out, int max) {
  if (!e || !out || max <= 0) return 0;
  int n = 0;
  for (int i = 0; i < e->count && n < max; i++) {
    const TrendEvent &ev = e->ev[(e->head + i) % TREND_MAX_EVENTS];
    if (ev.slot < loSlot || ev.slot > hiSlot) continue;
    out[n++] = ev;
  }
  return n;
}

void trend_events_shift(TrendEvents *e, int64_t delta) {
  if (!e) return;
  for (int i = 0; i < e->count; i++)
    e->ev[(e->head + i) % TREND_MAX_EVENTS].slot += delta;
}
