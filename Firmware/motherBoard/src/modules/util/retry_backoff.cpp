#include "modules/util/retry_backoff.h"

void backoff_init(Backoff *b) {
  b->delay_ms = 0;
  b->next_ms = 0;
}

bool backoff_due(const Backoff *b, uint32_t now_ms) {
  if (b->delay_ms == 0) {
    return true;
  }
  // Resta sin signo: correcta aunque millis() haya dado la vuelta.
  return (uint32_t)(now_ms - b->next_ms) < 0x80000000u;
}

void backoff_failed(Backoff *b, uint32_t now_ms) {
  if (b->delay_ms == 0) {
    b->delay_ms = BACKOFF_MIN_MS;
  } else if (b->delay_ms < BACKOFF_MAX_MS / 2u) {
    b->delay_ms *= 2u;
  } else {
    b->delay_ms = BACKOFF_MAX_MS;
  }
  b->next_ms = now_ms + b->delay_ms;
}

void backoff_ok(Backoff *b) { backoff_init(b); }

void log_limiter_init(LogLimiter *l) {
  l->last_ms = 0;
  l->logged = false;
}

bool log_limiter_allow(LogLimiter *l, uint32_t now_ms) {
  if (l->logged && (uint32_t)(now_ms - l->last_ms) < LOG_LIMITER_INTERVAL_MS) {
    return false;
  }
  l->logged = true;
  l->last_ms = now_ms;
  return true;
}

void log_limiter_reset(LogLimiter *l) { log_limiter_init(l); }
