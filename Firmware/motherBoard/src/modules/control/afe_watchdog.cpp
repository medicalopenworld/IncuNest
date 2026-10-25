#include "afe_watchdog.h"

// Ver afe_watchdog.h. Resta sin signo y se compara el intervalo, no las
// marcas: asi sobrevive al desbordamiento de millis().

void afe_wd_init(AfeWatchdog *wd, uint32_t now_ms) {
  wd->last_sample_ms = now_ms;
  wd->episode_open = false;
  wd->stalls = 0;
}

void afe_wd_sample(AfeWatchdog *wd, uint32_t now_ms) {
  wd->last_sample_ms = now_ms;
  wd->episode_open = false;
}

bool afe_wd_data_fresh(const AfeWatchdog *wd, uint32_t now_ms) {
  return (uint32_t)(now_ms - wd->last_sample_ms) <= AFE_WD_DATA_STALE_MS;
}

bool afe_wd_stall_detected(AfeWatchdog *wd, uint32_t now_ms) {
  if (wd->episode_open) {
    return false;
  }
  if ((uint32_t)(now_ms - wd->last_sample_ms) <= AFE_WD_STALL_MS) {
    return false;
  }
  wd->episode_open = true;
  wd->stalls++;
  return true;
}

uint32_t afe_wd_stalls(const AfeWatchdog *wd) { return wd->stalls; }
