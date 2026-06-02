#include "alarm_machine.h"

static uint32_t g_bitmask = 0;

void alarm_machine_init(void) { g_bitmask = 0; }

void alarm_machine_set(AlarmId id, bool active) {
  if ((int)id >= (int)NUM_ALARMS) return;
  if (active) g_bitmask |=  (1u << (int)id);
  else        g_bitmask &= ~(1u << (int)id);
}

bool alarm_machine_get(AlarmId id) {
  return (int)id < (int)NUM_ALARMS && (g_bitmask & (1u << (int)id)) != 0;
}
uint32_t alarm_machine_bitmask(void)      { return g_bitmask; }
bool     alarm_machine_any_active(void)   { return g_bitmask != 0; }
bool     alarm_machine_any_critical(void) {
  const uint32_t CRITICAL =
    (1u << (int)TEMPERATURE_ALARM)        |
    (1u << (int)AIR_THERMAL_CUTOUT_ALARM) |
    (1u << (int)SKIN_THERMAL_CUTOUT_ALARM)|
    (1u << (int)FAN_ISSUE_ALARM);
  return (g_bitmask & CRITICAL) != 0;
}
