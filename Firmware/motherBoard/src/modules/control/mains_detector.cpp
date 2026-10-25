#include "modules/control/mains_detector.h"

void mains_detector_init_hold(MainsDetector *d, uint32_t hold_ms) {
  d->absent_streak = 0;
  d->present_streak = 0;
  d->absent_confirmed = false;  // se asume red hasta que se demuestre lo contrario
  d->declared = false;
  d->since_ms = 0;
  d->hold_ms = hold_ms;
  d->last_valid_ms = 0;
  d->mains_seen = false;
}

void mains_detector_init(MainsDetector *d) {
  mains_detector_init_hold(d, MAINS_ALARM_MIN_HOLD_MS);
}

void mains_detector_sample(MainsDetector *d, bool reading_valid, bool ac_present,
                           uint32_t now_ms) {
  // Un plazo ya vencido se da por cerrado aqui (el unico escritor), para que
  // since_ms no resucite al dar millis() la vuelta a los ~49,7 dias. Es
  // mantenimiento del plazo, no una lectura: va antes del filtro de validez.
  if (d->declared && !d->absent_confirmed &&
      (uint32_t)(now_ms - d->since_ms) >= d->hold_ms) {
    d->declared = false;
  }

  // Lectura no valida: no cambia NADA, tampoco la racha.
  if (!reading_valid) {
    return;
  }
  d->last_valid_ms = now_ms;

  if (ac_present) {
    d->absent_streak = 0;
    if (d->present_streak < MAINS_DETECTOR_CONFIRM_PRESENT_SAMPLES) {
      ++d->present_streak;
    }
    if (d->present_streak >= MAINS_DETECTOR_ARM_PRESENT_SAMPLES) {
      d->mains_seen = true;
    }
    if (d->absent_confirmed &&
        d->present_streak >= MAINS_DETECTOR_CONFIRM_PRESENT_SAMPLES) {
      d->absent_confirmed = false;
    }
    return;
  }

  d->present_streak = 0;
  // Sin red vista desde el arranque no hay nada interrumpido: ver
  // MAINS_DETECTOR_ARM_PRESENT_SAMPLES.
  if (!d->mains_seen) {
    return;
  }
  if (d->absent_streak < MAINS_DETECTOR_CONFIRM_ABSENT_SAMPLES) {
    ++d->absent_streak;
  }
  if (!d->absent_confirmed &&
      d->absent_streak >= MAINS_DETECTOR_CONFIRM_ABSENT_SAMPLES) {
    // Flanco de declaracion: arranca (o reinicia) el plazo minimo.
    d->absent_confirmed = true;
    d->declared = true;
    d->since_ms = now_ms;
  }
}

bool mains_detector_condition(const MainsDetector *d, uint32_t now_ms) {
  if (d->absent_confirmed) {
    return true;
  }
  if (!d->declared) {
    return false;
  }
  // Resta sin signo: correcta aunque millis() haya dado la vuelta.
  return (uint32_t)(now_ms - d->since_ms) < d->hold_ms;
}

bool mains_detector_blind(const MainsDetector *d, bool charger_expected,
                          uint32_t now_ms) {
  if (!charger_expected) {
    return false;
  }
  return (uint32_t)(now_ms - d->last_valid_ms) > MAINS_DETECTOR_BLIND_MS;
}
