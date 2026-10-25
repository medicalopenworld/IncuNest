#pragma once
// Anillo de ranuras de 10 s con seis series int16 (ver trend_types.h).
//
// El almacenamiento lo pone el llamador (un unico bloque, en PSRAM en el
// firmware): este modulo no reserva memoria. Disposicion planar: la serie s
// ocupa data[s*capacity .. (s+1)*capacity).
//
// Las ranuras son absolutas (int64): el llamador decide la base de tiempo
// (esp_timer o epoch, ver trend_clock.h). El anillo solo exige que crezcan.

#include "modules/trend/trend_types.h"

struct TrendStore {
  int16_t *data;     // TREND_SERIES * capacity valores
  int capacity;      // ranuras por serie
  int writeIdx;      // siguiente posicion a escribir
  int count;         // ranuras validas (incluidas las "sin dato")
  int64_t lastSlot;  // ranura de la ultima muestra escrita (si count > 0)
};

// Bytes del bloque que necesita un anillo de `capacity` ranuras.
size_t trend_store_bytes(int capacity);

// Vincula el bloque y lo deja vacio. `block` debe tener trend_store_bytes().
void trend_store_init(TrendStore *s, int16_t *block, int capacity);

// Vacia sin tocar el bloque.
void trend_store_clear(TrendStore *s);

// Escribe la muestra en `slot`. Rellena con "sin dato" las ranuras que falten
// entre la ultima y esta, en las seis series: el indice sigue siendo el
// tiempo y la traza no une dos medidas con una recta que implicaria una
// continuidad que no hubo. Un hueco mayor que la capacidad deja el anillo
// entero en "sin dato" salvo la muestra nueva.
// Devuelve false (sin modificar nada) si slot <= lastSlot.
bool trend_store_record(TrendStore *s, int64_t slot, const TrendSample *sample);

// Lee la ranura `slot`. false si esta fuera de [first, last].
bool trend_store_get(const TrendStore *s, int64_t slot, TrendSample *out);

// Valor de una serie en `slot`, o TREND_NO_DATA si la ranura no existe.
int16_t trend_store_value(const TrendStore *s, int series, int64_t slot);

// Primera ranura legible (lastSlot - count + 1), o 0 si esta vacio.
int64_t trend_store_first_slot(const TrendStore *s);

// Re-etiqueta todo lo guardado: lastSlot += delta. Los datos conservan su
// separacion relativa (transicion esp_timer -> epoch).
void trend_store_shift(TrendStore *s, int64_t delta);
