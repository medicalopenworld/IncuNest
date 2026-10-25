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
  // Contador de generacion: sube con CUALQUIER cambio del contenido visible
  // (muestra nueva, shift, prepend, vaciado). Los trabajos que se reparten en
  // varias pasadas (serializador, restauracion) lo comparan para detectar que
  // el anillo cambio por debajo y reiniciarse en vez de leer/escribir sobre un
  // anillo que ya no es el que vieron al empezar.
  uint32_t gen;
};

// Estado de un trabajo repartido en pasadas.
enum TrendJobStatus : int {
  TREND_JOB_MORE = 0,  // quedan pasadas
  TREND_JOB_DONE,      // terminado
  TREND_JOB_RESTART,   // el anillo cambio por debajo: volver a empezar
  TREND_JOB_FAIL,      // no se puede continuar
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

// Antepone un bloque de `n` ranuras ANTERIORES a lo ya guardado (restauracion
// de un fichero). `planarLE`: series planares, n valores int16 little-endian
// por serie, de la mas antigua a la mas reciente; la mas reciente es la ranura
// `blockLastSlot`. Se lee byte a byte, sin suponer alineacion ni endianness.
//  - Con el anillo vacio, el bloque pasa a ser todo el contenido.
//  - Si no, solo entran las ranuras del bloque anteriores a la primera ya
//    guardada (las vivas mandan en el solape), y el hueco entre el final del
//    bloque y esa primera se rellena con "sin dato".
//  - Lo que no cabe se descarta por el lado ANTIGUO del bloque.
// Devuelve cuantas ranuras del bloque se incorporaron (0 si ninguna: el bloque
// es posterior, o el hueco no deja sitio para nada).
int trend_store_prepend(TrendStore *s, int64_t blockLastSlot,
                        const uint8_t *planarLE, int n);

// ---- prepend REPARTIDO en pasadas (C2) ----------------------------------
// Mismo resultado que trend_store_prepend, pero escribiendo como mucho
// `maxSlots` ranuras por llamada. Las ranuras se escriben en posiciones LIBRES
// del anillo (anteriores a la primera, fuera de `count`), asi que mientras el
// trabajo esta a medias el anillo visible NO cambia: count/lastSlot/gen solo se
// tocan en la pasada final. Si entre pasadas el anillo cambia (otra muestra
// viva), step() devuelve TREND_JOB_RESTART sin haber modificado nada visible.
struct TrendPrependJob {
  TrendStore *s;
  const uint8_t *planar;
  int n;
  int64_t blockLast, blockFirst;
  int64_t first, hi, lo, newestFromBlock;
  int64_t next;       // siguiente ranura a escribir
  int oldestIdx;
  uint32_t gen;
  bool wasEmpty;
  int added;          // ranuras del bloque que entraran
};

// Planifica. false si no hay nada que anadir (argumentos invalidos, bloque
// posterior, sin sitio): entonces added = 0 y no hay nada que hacer.
bool trend_prepend_begin(TrendPrependJob *j, TrendStore *s, int64_t blockLastSlot,
                         const uint8_t *planarLE, int n);
TrendJobStatus trend_prepend_step(TrendPrependJob *j, int maxSlots);
