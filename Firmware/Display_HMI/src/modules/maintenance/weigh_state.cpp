#include "modules/maintenance/weigh_state.h"

// Esqueleto del ciclo rojo: compila y enlaza, pero no decide nada todavia.

void weigh_state_init(weigh_state_t *s) {
  if (s == nullptr) return;
  *s = weigh_state_t{};
}

void weigh_state_tick(weigh_state_t *, const weigh_env_t *) {}

bool weigh_state_note_weighed(weigh_state_t *, const weigh_env_t *, uint32_t,
                              uint16_t, bool) {
  return false;
}

bool weigh_state_snooze(weigh_state_t *, uint32_t) { return false; }

void weigh_state_set_enabled(weigh_state_t *, bool) {}

weigh_inputs_t weigh_state_inputs(const weigh_state_t *, const weigh_env_t *,
                                  bool) {
  return weigh_inputs_t{};
}

bool weigh_state_should_warn(const weigh_state_t *, const weigh_env_t *,
                             bool) {
  return false;
}
