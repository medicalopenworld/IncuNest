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

// Clase visual de una marca de alarma. Una prioridad desconocida se trata
// como ALTA: ante la duda, la marca mas severa.
enum TrendMarkKind : int {
  TREND_MARK_HIGH = 0,
  TREND_MARK_MEDIUM = 1,
  TREND_MARK_LOW = 2,
  TREND_MARK_CLEARED = 3,
};
TrendMarkKind trend_mark_kind(uint8_t priority, bool raised);
