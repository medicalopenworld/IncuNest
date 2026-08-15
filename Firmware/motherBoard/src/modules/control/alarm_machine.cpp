#include "alarm_machine.h"

namespace {

struct Entry {
  bool present;       // la condicion fisica esta ocurriendo ahora
  AlarmState state;
};

Entry g_entries[ALARM_COUNT];

bool is_signalling(AlarmState s) { return s != ALARM_STATE_INACTIVE; }

bool valid(AlarmId id) { return id > ALARM_NONE && id < ALARM_COUNT; }

}  // namespace

void alarm_machine_init(void) {
  for (int i = 0; i < ALARM_COUNT; ++i) {
    g_entries[i].present = false;
    g_entries[i].state = ALARM_STATE_INACTIVE;
  }
}

void alarm_machine_condition(AlarmId id, bool present, uint32_t now_ms) {
  (void)now_ms;
  if (!valid(id)) {
    return;
  }
  Entry &e = g_entries[id];
  e.present = present;

  if (present) {
    if (e.state == ALARM_STATE_INACTIVE) {
      e.state = ALARM_STATE_ACTIVE;
    }
  } else {
    // El latching se aplica en la Task 6; de momento toda condicion que
    // desaparece limpia su alarma.
    e.state = ALARM_STATE_INACTIVE;
  }
}

void alarm_machine_tick(uint32_t now_ms) { (void)now_ms; }

AlarmState alarm_machine_state(AlarmId id) {
  return valid(id) ? g_entries[id].state : ALARM_STATE_INACTIVE;
}

uint32_t alarm_machine_bitmask(void) {
  uint32_t mask = 0;
  for (int i = ALARM_NONE + 1; i < ALARM_COUNT; ++i) {
    if (is_signalling(g_entries[i].state)) {
      mask |= (1u << i);
    }
  }
  return mask;
}

bool alarm_machine_heater_must_cut(void) {
  for (int i = ALARM_NONE + 1; i < ALARM_COUNT; ++i) {
    if (g_entries[i].present && alarm_cuts_heater((AlarmId)i)) {
      return true;
    }
  }
  return false;
}
