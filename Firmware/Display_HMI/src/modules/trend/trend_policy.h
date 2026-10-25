#pragma once
// Decisiones puras de la tendencia: que consignas gobiernan algo, a que pixel
// cae una ranura y como se clasifica una marca de alarma. Sin LVGL.

#include "modules/trend/trend_types.h"

// Que consignas tiene sentido registrar, segun el ECO de la placa
// (CTRL,STATE: actuation y controlMode). Una consigna que no gobierna nada se
// guarda como "sin dato": pintar una linea de consigna con el control apagado
// sugeriria un objetivo que el equipo no persigue.
struct TrendSetpointValidity {
  bool air;
  bool skin;
  bool hum;
};

// actuation: ActuationMode (0 apagado, 1 temperatura, 2 humedad, 3 ambas).
// airMode:   true = control por aire, false = control por piel (CONTROL_AIR).
// Un valor de actuation fuera de rango no valida ninguna consigna.
TrendSetpointValidity trend_setpoint_validity(int actuation, bool airMode);

// Columna (0..widthPx-1) del area de datos de un grafico cuyo eje X cubre
// `totalSlots` ranuras desde `startSlot`. -1 si la ranura cae fuera de la
// ventana o los parametros no son validos.
int trend_slot_to_px(int64_t slot, int64_t startSlot, int64_t totalSlots,
                     int widthPx);

// ---- Politica de volcado (hmi-trend, persistencia) ------------------------
// Cada 5 min si han entrado muestras nuevas, y al entrar en bloqueo con una
// separacion minima de 2 min respecto al volcado anterior: el auto-bloqueo se
// dispara tras cada interaccion (20 s) y sin tope cada toque seria un volcado.
// Un volcado cuesta ~27 borrados de bloque de LittleFS.
constexpr uint32_t TREND_DUMP_PERIOD_MS = 5u * 60u * 1000u;
constexpr uint32_t TREND_DUMP_LOCK_MIN_MS = 2u * 60u * 1000u;
// El volcado se escribe en trozos: uno por pasada del bucle y no mas de uno
// cada TREND_CHUNK_GAP_MS. 4 KB = un bloque de LittleFS = un borrado (~45 ms
// con la cache de flash parada): troceado, el refresco respira entre trozos.
constexpr size_t TREND_CHUNK_BYTES = 4096;
constexpr uint32_t TREND_CHUNK_GAP_MS = 50;

struct TrendDumpState {
  uint32_t lastDumpMs;      // ultimo volcado (o el arranque, o el ultimo fallo)
  int64_t lastDumpedSlot;   // ultima ranura incluida en el ultimo volcado bueno
  bool haveDumped;          // false hasta el primer volcado bueno
};

// `nowMs` es el instante de arranque del servicio: el primer volcado llega a
// los 5 min, no en el primer instante.
void trend_dump_init(TrendDumpState *d, uint32_t nowMs);

// true si toca iniciar un volcado. Nunca sin hora sincronizada (las ranuras
// serian monotonas, no epoch) ni sin muestras, ni si la ultima ranura es la que
// ya se guardo. La aritmetica de milisegundos es segura ante la vuelta de
// millis() (49 dias).
bool trend_dump_due(const TrendDumpState *d, bool clockSynced, bool haveSamples,
                    int64_t newestSlot, uint32_t nowMs, bool lockEntered);

// Volcado terminado bien: anota ranura y hora.
void trend_dump_done(TrendDumpState *d, uint32_t nowMs, int64_t newestSlot);

// Volcado fallido: se reintenta tras un periodo COMPLETO, nunca en bucle, y no
// se da por guardada ninguna ranura.
void trend_dump_failed(TrendDumpState *d, uint32_t nowMs);

// true si ya puede escribirse el siguiente trozo.
bool trend_chunk_due(uint32_t nowMs, uint32_t lastChunkMs);

// Clase visual de una marca de alarma. Una prioridad desconocida se trata
// como ALTA: ante la duda, la marca mas severa.
enum TrendMarkKind : int {
  TREND_MARK_HIGH = 0,
  TREND_MARK_MEDIUM = 1,
  TREND_MARK_LOW = 2,
  TREND_MARK_CLEARED = 3,
};
TrendMarkKind trend_mark_kind(uint8_t priority, bool raised);
