#include "sb_door_state.h"

void sb_door_state_init(SbDoorState *s) {
  s->has_state = false;
  s->open = false;
  s->transition_count = 0;
  for (unsigned i = 0; i < SB_DOOR_FLAP_TRANSITIONS_FOR_FAULT; i++) {
    s->transitions_ms[i] = 0;
  }
}

void sb_door_state_note_event(SbDoorState *s, bool event_open,
                              uint32_t now_ms) {
  const bool is_transition = s->has_state && (event_open != s->open);
  s->has_state = true;
  s->open = event_open;
  if (!is_transition) return;  // primer estado o re-asercion periodica

  if (s->transition_count < SB_DOOR_FLAP_TRANSITIONS_FOR_FAULT) {
    s->transitions_ms[s->transition_count++] = now_ms;
    return;
  }
  for (unsigned i = 1; i < SB_DOOR_FLAP_TRANSITIONS_FOR_FAULT; i++) {
    s->transitions_ms[i - 1] = s->transitions_ms[i];
  }
  s->transitions_ms[SB_DOOR_FLAP_TRANSITIONS_FOR_FAULT - 1] = now_ms;
}

bool sb_door_state_get(const SbDoorState *s, bool *open_out) {
  if (!s->has_state) return false;
  *open_out = s->open;
  return true;
}

bool sb_door_state_is_faulty(const SbDoorState *s, uint32_t now_ms) {
  if (s->transition_count < SB_DOOR_FLAP_TRANSITIONS_FOR_FAULT) return false;
  return (uint32_t)(now_ms - s->transitions_ms[0]) <= SB_DOOR_FLAP_WINDOW_MS;
}
