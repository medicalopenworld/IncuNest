#pragma once

// Filtro de flancos del tacometro del ventilador, sin nada de hardware.
//
// Incidencia de campo, unidad 363 (2026-11-26, motherBoard 18.67.0): con el
// calefactor conmutando (PWM a 400 Hz), fan_rpm saltaba entre 0 y 7041 mientras
// el ventilador iba MAS LENTO que en reposo (Fan_current 0,04 A frente a 0,07).
// La ISR aceptaba cualquier flanco en GPIO38 -que solo tiene el pull-up interno
// del ESP32- y cada pulso espurio era un "medio periodo" de microsegundos, o
// sea decenas de miles de rpm. El PID creia que el ventilador iba sobrado y lo
// bajaba a duty 0. Con 18.52 eso ademas disparaba ALARM_FAN_FAILURE en ciclo.
//
// Dos barreras, las dos baratas:
//   1. Nivel asentado: la ISR espera unos microsegundos y LEE el pin. Un pulso
//      espurio mas corto que esa espera ya ha terminado y el nivel leido es el
//      mismo que el ultimo aceptado: no hay transicion, se descarta. Los dos
//      flancos del pulso caen ahi.
//   2. Periodo completo, de subida a subida, con un minimo fisico: una subida
//      que llega antes de lo que permite el ventilador mas rapido posible no es
//      real. Medir el periodo completo, ademas, no depende de que el pulso del
//      tacometro sea simetrico.
//
// Todo es static inline a proposito: lo llama fanEncoderISR() (IRAM_ATTR), y
// asi el codigo queda dentro de la propia ISR en IRAM en vez de en flash.
// Los tests de host estan en motherBoard/test/test_fan_tach.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  bool have_level;        // hay un nivel aceptado
  bool level;             // ultimo nivel aceptado
  bool have_rise;         // hay una subida de referencia
  uint32_t last_rise_us;  // micros() de esa subida
  uint32_t glitches;      // flancos descartados (diagnostico)
} FanTach;

static inline void fan_tach_init(FanTach *t) {
  if (t == NULL) {
    return;
  }
  t->have_level = false;
  t->level = false;
  t->have_rise = false;
  t->last_rise_us = 0;
  t->glitches = 0;
}

// Un flanco de la interrupcion. `settled_level` es el nivel del pin leido
// DESPUES de la espera de asentamiento; `now_us`, micros() al entrar en la ISR.
// Devuelve el periodo completo en us (subida a subida) cuando se cierra uno
// valido, 0 en cualquier otro caso.
static inline uint32_t fan_tach_edge(FanTach *t, bool settled_level,
                                     uint32_t now_us, uint32_t min_period_us) {
  if (t == NULL) {
    return 0;
  }
  if (t->have_level && settled_level == t->level) {
    t->glitches++; // el pin no ha cambiado: pulso ya terminado o rebote
    return 0;
  }
  t->have_level = true;
  t->level = settled_level;
  if (!settled_level) {
    return 0; // las bajadas solo siguen el nivel
  }
  if (!t->have_rise) {
    t->have_rise = true;
    t->last_rise_us = now_us;
    return 0;
  }
  const uint32_t period = (uint32_t)(now_us - t->last_rise_us);
  if (period < min_period_us) {
    t->glitches++; // mas rapido que cualquier ventilador real
    return 0;
  }
  t->last_rise_us = now_us;
  return period;
}

// rpm a partir de un periodo completo. `half_period_conversion` es
// FAN_RPM_CONVERSION, que esta calibrado para el medio periodo (el intervalo
// entre flancos CHANGE que media la ISR antigua).
static inline double fan_tach_rpm(double full_period_us,
                                  double half_period_conversion) {
  if (full_period_us <= 0) {
    return 0;
  }
  return 2.0 * half_period_conversion / full_period_us;
}
