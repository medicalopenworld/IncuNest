#pragma once
// Ventanas de la tendencia y decimacion min/max. Logica pura.
//
// LVGL no puede pintar 8640 puntos utiles en ~670 px. Cada ventana (4 h, 12 h,
// 24 h) se reduce a TREND_BUCKETS cubos y cada cubo emite dos puntos: su
// minimo y su maximo, en el orden en que ocurrieron (conserva la forma de la
// traza y los picos no se promedian). El grafico tiene siempre
// TREND_CHART_POINTS puntos, asi que cambiar de ventana no realoja las series.

#include "modules/trend/trend_store.h"
#include "modules/trend/trend_types.h"

constexpr int TREND_WINDOW_COUNT = 3;                // 4 h, 12 h, 24 h
constexpr int TREND_BASE_BUCKETS = 720;              // cubos de la ventana exacta
constexpr int TREND_BUCKETS = TREND_BASE_BUCKETS + 1;  // +1: cubo parcial a la derecha
constexpr int TREND_CHART_POINTS = TREND_BUCKETS * 2;

// Ranuras de la ventana idx (0: 1440 = 4 h, 1: 4320 = 12 h, 2: 8640 = 24 h).
// Un idx fuera de rango se trata como 0.
int trend_window_slots(int idx);

struct TrendWindow {
  int64_t startSlot;     // primera ranura del primer cubo (multiplo de slotsPerBucket)
  int slotsPerBucket;    // 2 / 6 / 12
  int buckets;           // TREND_BUCKETS
  int64_t totalSlots;    // buckets * slotsPerBucket: ancho del eje X en ranuras
};

// Ventana que termina en `nowSlot`. Los limites de cubo se alinean a multiplos
// ABSOLUTOS de slotsPerBucket (no a "ahora"): si no, cada ranura nueva
// desplazaria los limites y el min/max de cada cubo parpadearia al repintar.
// El precio: un cubo parcial a la derecha y hasta slotsPerBucket-1 ranuras de
// desfase del borde derecho. Siempre se cumple startSlot + totalSlots > nowSlot.
TrendWindow trend_window_plan(int64_t nowSlot, int idx);

// Rellena out[0 .. 2*buckets) con (min, max) por cubo, en orden temporal
// dentro del cubo. Un cubo sin ningun dato emite TREND_NO_DATA en ambos.
// Una ranura fuera del anillo cuenta como sin dato.
void trend_decimate(const TrendStore *s, int series, const TrendWindow *w,
                    int16_t *out);
