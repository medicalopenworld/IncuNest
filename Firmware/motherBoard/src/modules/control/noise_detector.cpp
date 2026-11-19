#include "modules/control/noise_detector.h"

static void reset_runs(NoiseDetector *d) {
  d->above_run = false;
  d->below_run = false;
}

void noise_detector_sample(NoiseDetector *d, bool valid, float dba,
                           bool suppressed, uint32_t now_ms) {
  if (d == 0 || !valid) return;

  // Tras un hueco sin lecturas se empieza de cero: lo de antes ya no dice nada
  // del ruido de ahora.
  if (d->have && (uint32_t)(now_ms - d->last_ms) > NOISE_STALE_MS) {
    d->raised = false;
    reset_runs(d);
  }
  d->have = true;
  d->last_ms = now_ms;

  // Suena otra alarma: la lectura no es ruido ambiental. Ni avanza ni rompe.
  if (suppressed) return;

  if (dba >= NOISE_ON_DBA) {
    d->below_run = false;
    if (!d->above_run) {
      d->above_run = true;
      d->above_since = now_ms;
    }
    if ((uint32_t)(now_ms - d->above_since) >= NOISE_ON_HOLD_MS) {
      d->raised = true;
    }
  } else if (dba < NOISE_OFF_DBA) {
    d->above_run = false;
    if (!d->below_run) {
      d->below_run = true;
      d->below_since = now_ms;
    }
    if ((uint32_t)(now_ms - d->below_since) >= NOISE_OFF_HOLD_MS) {
      d->raised = false;
    }
  } else {
    // Banda de histeresis: rompe las dos rachas y no cambia la condicion.
    reset_runs(d);
  }
}

bool noise_detector_condition(const NoiseDetector *d, uint32_t now_ms) {
  if (d == 0 || !d->have || !d->raised) return false;
  return (uint32_t)(now_ms - d->last_ms) <= NOISE_STALE_MS;
}
