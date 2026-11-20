#pragma once
// Reloj de ranuras de la tendencia: de esp_timer (reinicia en 0 en cada
// arranque) a epoch UTC cuando la motherBoard ya ha dado la hora. Logica pura.
//
//   slot = monoMs / TREND_SLOT_MS + offsetSlots
//
// Sin hora offsetSlots = 0 (la base "monotona" de siempre). Con hora,
// offsetSlots = epoch/10 - monoSlot, de modo que slot == epoch/10: el mismo
// numero con el que se alinea un fichero tras un reinicio.
//
// La transicion no pierde datos: update() devuelve cuantas ranuras hay que
// desplazar lo ya guardado (TrendStore/TrendEvents), que conserva asi su
// separacion relativa y solo se re-etiqueta con su hora real.

#include "modules/trend/trend_types.h"

// Una diferencia de una ranura o menos entre el offset vigente y el que daria
// la hora actual es redondeo (millis() frente a esp_timer, truncados a 10 s):
// no es una correccion de hora y no mueve el eje.
constexpr int64_t TREND_CLOCK_HYSTERESIS_SLOTS = 1;

struct TrendClock {
  int64_t offsetSlots;
  bool synced;
};

void trend_clock_init(TrendClock *c);

// Ranura correspondiente a `monoMs` (ms desde el arranque) con el offset
// vigente. No modifica el reloj.
int64_t trend_clock_slot(const TrendClock *c, int64_t monoMs);

// Incorpora la hora. epoch = segundos UTC (0 = sin hora, no hace nada).
// Devuelve el desplazamiento a aplicar a los datos ya guardados: el offset
// completo en la primera sincronizacion, la correccion si cambia mas que la
// histeresis, 0 en cualquier otro caso. Puede ser negativo.
int64_t trend_clock_update(TrendClock *c, int64_t monoMs, uint32_t epoch);
