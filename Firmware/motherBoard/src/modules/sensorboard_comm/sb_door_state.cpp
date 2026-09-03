#include "sb_door_state.h"

void sb_door_state_init(SbDoorState *s) {
  s->has_state = false;
  s->open = false;
  s->transition_count = 0;
  for (unsigned i = 0; i < SB_DOOR_FLAP_TRANSITIONS_FOR_FAULT; i++) {
    s->transitions_ms[i] = 0;
  }
}

// Descarta por delante las transiciones que ya han salido de la ventana. La
// resta sin signo hace que una marca de antes de un vuelco de millis()
// aparezca como antiquisima y se purgue, que es exactamente lo que se quiere.
static void purge_expired(SbDoorState *s, uint32_t now_ms) {
  while (s->transition_count > 0 &&
         (uint32_t)(now_ms - s->transitions_ms[0]) > SB_DOOR_FLAP_WINDOW_MS) {
    for (unsigned i = 1; i < s->transition_count; i++) {
      s->transitions_ms[i - 1] = s->transitions_ms[i];
    }
    s->transition_count--;
  }
}

void sb_door_state_note_event(SbDoorState *s, bool event_open,
                              uint32_t now_ms) {
  const bool is_transition = s->has_state && (event_open != s->open);
  s->has_state = true;
  s->open = event_open;

  purge_expired(s, now_ms);
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

bool sb_door_state_evaluate(SbDoorState *s, uint32_t now_ms) {
  purge_expired(s, now_ms);
  return s->transition_count >= SB_DOOR_FLAP_TRANSITIONS_FOR_FAULT;
}
