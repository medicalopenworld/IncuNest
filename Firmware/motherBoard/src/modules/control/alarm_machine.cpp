#include "alarm_machine.h"
#include "alarm_policy.h"

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
  // "Critico" = cualquier alarma activa cuya prioridad, segun la Tabla 1 de
  // IEC 60601-1-8 (ver alarm_policy.h), sea ALTA. Sustituye a la lista de 4
  // ids codeada a mano, que quedo obsoleta al renombrarse el enum AlarmId.
  for (int id = ALARM_NONE + 1; id < (int)NUM_ALARMS; ++id) {
    if ((g_bitmask & (1u << id)) && alarm_priority((AlarmId)id) == ALARM_PRIORITY_HIGH) {
      return true;
    }
  }
  return false;
}
