#pragma once
// Modelo completo de la tendencia: anillo + eventos + reloj. Logica pura; el
// firmware (TelemetryHistory.cpp) solo pone el bloque de PSRAM y el hilo.
//
// Hilo: TODO el modelo pertenece a UI_Task. CommTask no lo toca: sus flancos de
// alarma llegan por una cola (TelemetryHistory_RecordAlarmEdge) y se aplican
// aqui con el instante en que ocurrieron.

#include "modules/trend/trend_clock.h"
#include "modules/trend/trend_events.h"
#include "modules/trend/trend_store.h"
#include "modules/trend/trend_types.h"

struct TrendModel {
  TrendStore store;
  TrendEvents events;
  TrendClock clock;
};

// Vincula el bloque (trend_store_bytes(capacity)) y deja todo vacio y sin hora.
void trend_model_init(TrendModel *m, int16_t *block, int capacity);

// Incorpora la hora y devuelve la ranura de "ahora". Si la hora obliga a
// re-etiquetar (primera sincronizacion o correccion mayor que la histeresis),
// desplaza el anillo y los eventos el mismo numero de ranuras. epoch = 0 (sin
// hora) no cambia nada. Llamar al menos una vez por pasada de UI: asi la
// transicion a epoch ocurre en cuanto hay hora, haya o no muestras.
int64_t trend_model_tick(TrendModel *m, int64_t monoMs, uint32_t epoch);

// tick() + registrar la muestra en la ranura de "ahora". false si esa ranura ya
// estaba escrita (una muestra por ranura: la primera de cada 10 s manda).
bool trend_model_record(TrendModel *m, int64_t monoMs, uint32_t epoch,
                        const TrendSample *sample);

// Flanco de alarma ocurrido en `monoMs` (no hace tick: usa el offset vigente).
void trend_model_edge(TrendModel *m, int64_t monoMs, uint8_t id,
                      uint8_t priority, bool raised);

// Ranura de la ultima muestra, o 0 si no hay ninguna.
int64_t trend_model_newest_slot(const TrendModel *m);
