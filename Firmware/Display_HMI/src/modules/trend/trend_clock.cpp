#include "modules/trend/trend_clock.h"

void trend_clock_init(TrendClock *c) {
  if (!c) return;
  c->offsetSlots = 0;
  c->synced = false;
}

int64_t trend_clock_slot(const TrendClock *c, int64_t monoMs) {
  return monoMs / TREND_SLOT_MS + (c ? c->offsetSlots : 0);
}

int64_t trend_clock_update(TrendClock *c, int64_t monoMs, uint32_t epoch) {
  if (!c || epoch == 0) return 0;  // sin hora: ni se sincroniza ni se desincroniza

  // Offset que haria que la ranura de "ahora" fuese epoch/10.
  const int64_t target =
      (int64_t)(epoch / (uint32_t)TREND_SLOT_SECONDS) - monoMs / TREND_SLOT_MS;

  if (!c->synced) {
    const int64_t shift = target - c->offsetSlots;
    c->offsetSlots = target;
    c->synced = true;
    return shift;
  }

  const int64_t d = target - c->offsetSlots;
  if (d <= TREND_CLOCK_HYSTERESIS_SLOTS && d >= -TREND_CLOCK_HYSTERESIS_SLOTS)
    return 0;  // redondeo de millis() frente a esp_timer, no una correccion
  c->offsetSlots = target;
  return d;
}
