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
  int64_t idx = ((int64_t)s->writeIdx - 1 - back) % s->capacity;
  if (idx < 0) idx += s->capacity;
  return (int)idx;
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
