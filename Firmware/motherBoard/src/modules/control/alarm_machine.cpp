#include "alarm_machine.h"

namespace {

struct Entry {
  bool present;       // la condicion fisica esta ocurriendo ahora
  AlarmState state;
  uint32_t announce_delay_ms;
  uint32_t present_since_ms;
  uint32_t silenced_until_ms;
};

Entry g_entries[ALARM_COUNT];

bool is_signalling(AlarmState s) { return s != ALARM_STATE_INACTIVE; }

bool valid(AlarmId id) { return id > ALARM_NONE && id < ALARM_COUNT; }

}  // namespace

void alarm_machine_init(void) {
  for (int i = 0; i < ALARM_COUNT; ++i) {
    g_entries[i].present = false;
    g_entries[i].state = ALARM_STATE_INACTIVE;
    g_entries[i].announce_delay_ms = 0;
    g_entries[i].present_since_ms = 0;
    g_entries[i].silenced_until_ms = 0;
  }
}

void alarm_machine_set_announce_delay(AlarmId id, uint32_t delay_ms) {
  if (valid(id)) {
    g_entries[id].announce_delay_ms = delay_ms;
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
      e.present_since_ms = now_ms;
      // Un corte termico nunca espera: la norma exige aviso inmediato.
      const bool may_wait =
          e.announce_delay_ms > 0 && !alarm_is_latching(id);
      e.state = may_wait ? ALARM_STATE_PENDING : ALARM_STATE_ACTIVE;
    }
  } else {
    // 201.15.4.2.1 aa)/bb): un corte termico mantiene la alarma hasta reset
    // manual aunque la temperatura ya haya vuelto a rango. El resto se limpia
    // solo (senal non-latching, 6.10).
    if (!alarm_is_latching(id)) {
      e.state = ALARM_STATE_INACTIVE;
    }
  }
}

void alarm_machine_tick(uint32_t now_ms) {
  for (int i = ALARM_NONE + 1; i < ALARM_COUNT; ++i) {
    Entry &e = g_entries[i];
    if (e.state == ALARM_STATE_PENDING && e.present &&
        (uint32_t)(now_ms - e.present_since_ms) >= e.announce_delay_ms) {
      e.state = ALARM_STATE_ACTIVE;
    }
    if (e.state == ALARM_STATE_SILENCED &&
        (int32_t)(now_ms - e.silenced_until_ms) >= 0) {
      e.state = ALARM_STATE_ACTIVE;
    }
  }
}

bool alarm_machine_audio_required(void) {
  for (int i = ALARM_NONE + 1; i < ALARM_COUNT; ++i) {
    if (g_entries[i].state == ALARM_STATE_ACTIVE) {
      return true;
    }
  }
  return false;
}

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

void alarm_machine_silence(AlarmId id, uint32_t duration_ms, uint32_t now_ms) {
  if (!valid(id)) {
    return;
  }
  Entry &e = g_entries[id];
  if (e.state != ALARM_STATE_ACTIVE) {
    return;  // solo se silencia lo que se esta anunciando
  }
  e.state = ALARM_STATE_SILENCED;
  e.silenced_until_ms = now_ms + duration_ms;
}

void alarm_machine_ack(AlarmId id, uint32_t now_ms) {
  (void)now_ms;
  if (!valid(id)) {
    return;
  }
  Entry &e = g_entries[id];
  if (e.state == ALARM_STATE_ACTIVE || e.state == ALARM_STATE_SILENCED) {
    e.state = ALARM_STATE_ACKED;
  }
}

bool alarm_machine_is_latched(AlarmId id) {
  if (!valid(id)) {
    return false;
  }
  const Entry &e = g_entries[id];
  return alarm_is_latching(id) && !e.present &&
         e.state != ALARM_STATE_INACTIVE;
}

bool alarm_machine_reset(AlarmId id, uint32_t now_ms) {
  (void)now_ms;
  if (!alarm_machine_is_latched(id)) {
    return false;
  }
  g_entries[id].state = ALARM_STATE_INACTIVE;
  return true;
}
