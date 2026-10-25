#include "modules/trend/trend_store.h"

size_t trend_store_bytes(int capacity) {
  return sizeof(int16_t) * (size_t)TREND_SERIES * (size_t)(capacity > 0 ? capacity : 0);
}

void trend_store_clear(TrendStore *s) {
  if (!s) return;
  s->writeIdx = 0;
  s->count = 0;
  s->lastSlot = 0;
}

void trend_store_init(TrendStore *s, int16_t *block, int capacity) {
  if (!s) return;
  s->data = block;
  s->capacity = (block && capacity > 0) ? capacity : 0;
  trend_store_clear(s);
}

namespace {

// Escribe una ranura en la posicion de escritura y avanza el anillo.
void push(TrendStore *s, const int16_t *vals) {
  for (int ser = 0; ser < TREND_SERIES; ser++)
    s->data[(size_t)ser * (size_t)s->capacity + (size_t)s->writeIdx] = vals[ser];
  s->writeIdx = (s->writeIdx + 1) % s->capacity;
  if (s->count < s->capacity) s->count++;
}

}  // namespace

bool trend_store_record(TrendStore *s, int64_t slot, const TrendSample *sample) {
  if (!s || !s->data || s->capacity <= 0 || !sample) return false;
  if (s->count > 0 && slot <= s->lastSlot) return false;

  if (s->count > 0) {
    int64_t missing = slot - s->lastSlot - 1;
    if (missing > s->capacity) missing = s->capacity;
    const TrendSample none = trend_empty_sample();
    for (int64_t i = 0; i < missing; i++) push(s, none.v);
  }
  push(s, sample->v);
  s->lastSlot = slot;
  return true;
}

namespace {

// Posicion del anillo de la ranura `slot`, o -1 si no esta guardada.
int indexOf(const TrendStore *s, int64_t slot) {
  if (!s || !s->data || s->count <= 0) return -1;
  const int64_t back = s->lastSlot - slot;  // 0 = la ultima escrita
  if (back < 0 || back >= s->count) return -1;
  // back < count <= capacity: aritmetica de int, sin el modulo de 64 bits (el
  // decimador llama a esto ~50 000 veces por repintado en un ESP32 de 32 bits).
  int idx = s->writeIdx - 1 - (int)back;
  if (idx < 0) idx += s->capacity;
  return idx;
}

}  // namespace

bool trend_store_get(const TrendStore *s, int64_t slot, TrendSample *out) {
  const int idx = indexOf(s, slot);
  if (idx < 0 || !out) return false;
  for (int ser = 0; ser < TREND_SERIES; ser++)
    out->v[ser] = s->data[(size_t)ser * (size_t)s->capacity + (size_t)idx];
  return true;
}

int16_t trend_store_value(const TrendStore *s, int series, int64_t slot) {
  if (series < 0 || series >= TREND_SERIES) return TREND_NO_DATA;
  const int idx = indexOf(s, slot);
  if (idx < 0) return TREND_NO_DATA;
  return s->data[(size_t)series * (size_t)s->capacity + (size_t)idx];
}

int64_t trend_store_first_slot(const TrendStore *s) {
  if (!s || s->count <= 0) return 0;
  return s->lastSlot - s->count + 1;
}

void trend_store_shift(TrendStore *s, int64_t delta) {
  if (!s) return;
  s->lastSlot += delta;
}

namespace {

// int16 little-endian de la posicion i de la serie ser de un bloque planar.
int16_t readLE(const uint8_t *planar, int ser, int i, int n) {
  const uint8_t *p = planar + ((size_t)ser * (size_t)n + (size_t)i) * 2u;
  return (int16_t)(uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

}  // namespace

int trend_store_prepend(TrendStore *s, int64_t blockLastSlot,
                        const uint8_t *planarLE, int n) {
  if (!s || !s->data || s->capacity <= 0 || !planarLE || n <= 0) return 0;

  const int64_t blockFirst = blockLastSlot - n + 1;
  // Primera ranura ya guardada. Con el anillo vacio, el bloque entero es lo
  // anterior: "la primera" es la siguiente a su ultima.
  const int64_t first =
      (s->count > 0) ? s->lastSlot - s->count + 1 : blockLastSlot + 1;
  const int64_t hi = first - 1;  // ultima ranura que puede entrar
  const int room = s->capacity - s->count;
  if (room <= 0 || hi < blockFirst) return 0;

  int64_t lo = hi - room + 1;  // lo que no cabe se pierde por el lado antiguo
  if (lo < blockFirst) lo = blockFirst;
  const int64_t newestFromBlock = (hi < blockLastSlot) ? hi : blockLastSlot;
  if (lo > newestFromBlock) return 0;  // el hueco se come todo el sitio

  const bool wasEmpty = (s->count == 0);
  if (wasEmpty) s->writeIdx = 0;
  const int oldestIdx = (s->writeIdx - s->count + s->capacity) % s->capacity;

  for (int64_t slot = lo; slot <= hi; slot++) {
    // first - slot esta en 1..room <= capacity: aritmetica de int.
    int idx = oldestIdx - (int)(first - slot);
    if (idx < 0) idx += s->capacity;
    const bool fromBlock = (slot <= blockLastSlot);
    const int bi = fromBlock ? (int)(slot - blockFirst) : 0;
    for (int ser = 0; ser < TREND_SERIES; ser++)
      s->data[(size_t)ser * (size_t)s->capacity + (size_t)idx] =
          fromBlock ? readLE(planarLE, ser, bi, n) : TREND_NO_DATA;
  }

  s->count += (int)(hi - lo + 1);
  if (wasEmpty) s->lastSlot = blockLastSlot;
  return (int)(newestFromBlock - lo + 1);
}
