#pragma once
// Anillo de eventos de alarma (flancos alta/baja) para pintar las marcas
// verticales de la tendencia. Logica pura.

#include "modules/trend/trend_types.h"

constexpr int TREND_MAX_EVENTS = 64;

struct TrendEvent {
  int64_t slot;      // ranura del flanco (misma base que TrendStore)
  uint8_t id;        // ALARMS_ID
  uint8_t priority;  // 0 baja, 1 media, 2 alta (PROTOCOL.md, CTRL,ALM)
  bool raised;       // true = alta (la condicion aparece), false = baja
};

struct TrendEvents {
  TrendEvent ev[TREND_MAX_EVENTS];
  int head;   // posicion del mas antiguo
  int count;  // eventos guardados
};

void trend_events_init(TrendEvents *e);

// Anade al final (el mas reciente). Con el anillo lleno descarta el mas
// antiguo. No reordena: el llamador anade en orden cronologico.
void trend_events_add(TrendEvents *e, int64_t slot, uint8_t id, uint8_t priority,
                      bool raised);

// Evento i-esimo en orden cronologico (0 = el mas antiguo). false si i fuera
// de rango.
bool trend_events_get(const TrendEvents *e, int i, TrendEvent *out);

// Copia a `out` (hasta `max`) los eventos con loSlot <= slot <= hiSlot, en
// orden cronologico. Devuelve cuantos copio.
int trend_events_in_window(const TrendEvents *e, int64_t loSlot, int64_t hiSlot,
                           TrendEvent *out, int max);

// Re-etiqueta todos los eventos: slot += delta.
void trend_events_shift(TrendEvents *e, int64_t delta);

// Antepone `n` eventos anteriores (cronologicos) a los ya guardados
// (restauracion de un fichero). Solo entran los de slot >= minSlot y los de
// slot estrictamente anterior al primer evento ya guardado (los vivos mandan:
// el mismo flanco no se duplica). Si no caben todos, se descartan los mas
// antiguos de `older`: los guardados nunca se pierden. Devuelve cuantos entran.
int trend_events_prepend(TrendEvents *e, const TrendEvent *older, int n,
                         int64_t minSlot);
