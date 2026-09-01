#include "sb_link_state.h"

#include "sb_protocol.h"

void sb_link_state_init(SbLinkState *s) {
  s->has_seen_heartbeat = false;
  s->last_heartbeat_ms = 0;
}

void sb_link_state_note_heartbeat(SbLinkState *s, uint32_t now_ms) {
  s->has_seen_heartbeat = true;
  s->last_heartbeat_ms = now_ms;
}

bool sb_link_state_is_connected(const SbLinkState *s, uint32_t now_ms) {
  if (!s->has_seen_heartbeat) return false;
  // Resta sin signo: sobrevive al vuelco de millis() a los ~49.7 dias
  // mientras el hueco medido sea menor que ese periodo, y aqui son 90 s.
  return (uint32_t)(now_ms - s->last_heartbeat_ms) <= SB_LINK_TIMEOUT_MS;
}
