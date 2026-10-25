#include "modules/control/mains_detector.h"

void mains_detector_init(MainsDetector *d) {
  d->absent_streak = 0;
  d->present_streak = 0;
  d->absent_confirmed = false;  // se asume red hasta que se demuestre lo contrario
  d->declared = false;
  d->since_ms = 0;
}

void mains_detector_sample(MainsDetector *d, bool reading_valid, bool ac_present,
                           uint16_t vbat_mv, uint32_t now_ms) {
  // Un plazo ya vencido se da por cerrado aqui (el unico escritor), para que
  // since_ms no resucite al dar millis() la vuelta a los ~49,7 dias. Es
  // mantenimiento del plazo, no una lectura: va antes del filtro de validez.
  if (d->declared && !d->absent_confirmed &&
      (uint32_t)(now_ms - d->since_ms) >= MAINS_ALARM_MIN_HOLD_MS) {
    d->declared = false;
  }

  // Lectura no valida: no cambia NADA, tampoco la racha.
  if (!reading_valid || vbat_mv == 0) {
    return;
  }

  if (ac_present) {
    d->absent_streak = 0;
    if (d->present_streak < MAINS_DETECTOR_CONFIRM_SAMPLES) {
      ++d->present_streak;
    }
    if (d->absent_confirmed &&
        d->present_streak >= MAINS_DETECTOR_CONFIRM_SAMPLES) {
      d->absent_confirmed = false;
    }
    return;
  }

  d->present_streak = 0;
  if (d->absent_streak < MAINS_DETECTOR_CONFIRM_SAMPLES) {
    ++d->absent_streak;
  }
  if (!d->absent_confirmed &&
      d->absent_streak >= MAINS_DETECTOR_CONFIRM_SAMPLES) {
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
  return (uint32_t)(now_ms - d->since_ms) < MAINS_ALARM_MIN_HOLD_MS;
}
