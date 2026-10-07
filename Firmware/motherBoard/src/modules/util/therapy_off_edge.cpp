#include "therapy_off_edge.h"

#include <string.h>

void therapy_off_edge_init(TherapyOffEdge *st) {
  if (!st) return;
  memset(st, 0, sizeof(*st));
}

void therapy_off_edge_observe(TherapyOffEdge *st, bool photoOn,
                              bool controlOn) {
  if (!st) return;
  if (st->primed) {
    if ((st->photoWasOn && !photoOn) || (st->controlWasOn && !controlOn)) {
      st->pending = true;
    }
  }
  st->primed = true;
  st->photoWasOn = photoOn;
  st->controlWasOn = controlOn;
}

bool therapy_off_edge_due(const TherapyOffEdge *st, uint32_t nowMs) {
  if (!st || !st->pending) return false;
  if (!st->anyPublished) return true;
  // Resta sin signo: sigue midiendo bien cuando millis() da la vuelta.
  return (uint32_t)(nowMs - st->lastPublishMs) >= THERAPY_OFF_EDGE_MIN_GAP_MS;
}

void therapy_off_edge_published(TherapyOffEdge *st, uint32_t nowMs) {
  if (!st) return;
  st->pending = false;
  st->anyPublished = true;
  st->lastPublishMs = nowMs;
}
