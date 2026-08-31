#include "alarm_audio_pattern.h"

uint32_t alarm_audio_burst_period_ms(int priority) {
  switch (priority) {
  case ALARM_PRIORITY_LOW:
    return ALARM_BURST_PERIOD_MS_LOW;
  case ALARM_PRIORITY_MEDIUM:
    return ALARM_BURST_PERIOD_MS_MEDIUM;
  default:
    // Prioridad no reconocida -> ALTA. Misma eleccion que alarm_priority():
    // sobreestimar la urgencia de algo que no se entiende es lo seguro.
    return ALARM_BURST_PERIOD_MS_HIGH;
  }
}

bool alarm_audio_pulse_on(uint32_t elapsed_ms, int priority) {
  uint32_t pulses;
  uint32_t spacing;
  bool splitBurst; // el hueco de 2x + y tras el quinto pulso, solo en ALTA

  switch (priority) {
  case ALARM_PRIORITY_LOW:
    pulses = ALARM_BURST_PULSES_LOW;
    spacing = 0u; // irrelevante: un unico pulso
    splitBurst = false;
    break;
  case ALARM_PRIORITY_MEDIUM:
    pulses = ALARM_BURST_PULSES_MEDIUM;
    spacing = ALARM_PULSE_SPACING_Y_MS;
    splitBurst = false;
    break;
  default:
    pulses = ALARM_BURST_PULSES_HIGH;
    spacing = ALARM_PULSE_SPACING_X_MS;
    splitBurst = true;
    break;
  }

  // Se recorre la rafaga pulso a pulso en vez de resolverla con una formula
  // cerrada: la rafaga de ALTA no es periodica —el hueco entre el quinto y el
  // sexto pulso vale 2x + y y no x—, asi que cualquier formula tendria que
  // llevar ese caso especial dentro de todos modos. Diez iteraciones como
  // maximo, sin division.
  uint32_t cursor = 0u;
  for (uint32_t i = 0u; i < pulses; ++i) {
    if (elapsed_ms < cursor) {
      return false; // en el hueco anterior a este pulso
    }
    if (elapsed_ms < cursor + ALARM_PULSE_MS) {
      return true;
    }
    cursor += ALARM_PULSE_MS;
    cursor += (splitBurst && i == 4u) ? ALARM_GROUP_GAP_MS : spacing;
  }

  // Pasada la rafaga: silencio hasta que el llamante reinicie el conteo. Este
  // silencio es parte del patron, no una ausencia de el — es lo que separa
  // las rafagas segun la Tabla 3, y en el display es ademas lo que impide que
  // su zumbador tape una alarma ALTA de la placa (design.md D2).
  return false;
}
