#include "afe_watchdog.h"

// Ver afe_watchdog.h. Resta sin signo y se compara el intervalo, no las
// marcas: asi sobrevive al desbordamiento de millis().

void afe_wd_init(AfeWatchdog *wd, uint32_t now_ms) {
  wd->last_sample_ms = now_ms;
  wd->last_restart_ms = 0;
  wd->restarted_once = false;
  wd->restarts = 0;
}

void afe_wd_sample(AfeWatchdog *wd, uint32_t now_ms) {
  wd->last_sample_ms = now_ms;
}

bool afe_wd_should_restart(AfeWatchdog *wd, uint32_t now_ms) {
  const bool stalled =
      (uint32_t)(now_ms - wd->last_sample_ms) > AFE_WD_STALL_MS;
  if (!stalled) {
    return false;
  }
  const bool allowed =
      !wd->restarted_once ||
      (uint32_t)(now_ms - wd->last_restart_ms) >= AFE_WD_MIN_RESTART_INTERVAL_MS;
  if (!allowed) {
    return false;
  }
  wd->restarted_once = true;
  wd->last_restart_ms = now_ms;
  wd->last_sample_ms = now_ms;
  wd->restarts++;
  return true;
}

uint32_t afe_wd_restarts(const AfeWatchdog *wd) { return wd->restarts; }
