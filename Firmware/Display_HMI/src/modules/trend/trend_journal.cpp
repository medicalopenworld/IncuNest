#include "modules/trend/trend_journal.h"

#include "modules/trend/trend_file.h"

#include <string.h>

namespace {

constexpr uint8_t KIND_SAMPLE = 1;
constexpr uint8_t KIND_EVENT = 2;
constexpr size_t CRC_OFF = 20;

void wr16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
}
void wr32(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}
uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t rd32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

// Ranura representable como epoch u32 (mismas reglas que el fichero).
bool epochOk(int64_t slot) {
  return slot >= 0 && slot * TREND_SLOT_SECONDS <= (int64_t)UINT32_MAX;
}

int64_t firstNewSlot(const TrendStore *s, int64_t afterSlot) {
  const int64_t first = trend_store_first_slot(s);
  return afterSlot + 1 > first ? afterSlot + 1 : first;
}

void seal(uint8_t *r) { wr32(r + CRC_OFF, trend_crc32(r, CRC_OFF)); }

// Registro valido: CRC correcto y tipo conocido.
bool valid(const uint8_t *r) {
  if (rd32(r + CRC_OFF) != trend_crc32(r, CRC_OFF)) return false;
  return r[0] == KIND_SAMPLE || r[0] == KIND_EVENT;
}

int64_t slotOf(const uint8_t *r) { return (int64_t)rd32(r + 4) / TREND_SLOT_SECONDS; }

}  // namespace

int trend_jnl_pending_records(const TrendStore *s, const TrendEvents *e,
                              int64_t afterSlot) {
  if (!s || s->count <= 0) return 0;
  int n = 0;
  const int64_t from = firstNewSlot(s, afterSlot);
  if (s->lastSlot >= from) n += (int)(s->lastSlot - from + 1);
  if (e) {
    for (int i = 0; i < e->count; i++) {
      TrendEvent ev;
      if (trend_events_get(e, i, &ev) && ev.slot > afterSlot) n++;
    }
  }
  return n;
}

size_t trend_jnl_build(const TrendStore *s, const TrendEvents *e, int64_t afterSlot,
                       uint8_t *out, size_t cap) {
  if (!s || !out) return 0;
  const int n = trend_jnl_pending_records(s, e, afterSlot);
  if (n <= 0 || (size_t)n * TREND_JNL_REC_BYTES > cap) return 0;
  const int64_t from = firstNewSlot(s, afterSlot);
  if (s->lastSlot >= from && (!epochOk(from) || !epochOk(s->lastSlot))) return 0;

  uint8_t *r = out;
  for (int64_t slot = from; slot <= s->lastSlot; slot++, r += TREND_JNL_REC_BYTES) {
    TrendSample x;
    if (!trend_store_get(s, slot, &x)) return 0;
    memset(r, 0, TREND_JNL_REC_BYTES);
    r[0] = KIND_SAMPLE;
    wr32(r + 4, (uint32_t)(slot * TREND_SLOT_SECONDS));
    for (int i = 0; i < TREND_SERIES; i++) wr16(r + 8 + 2 * i, (uint16_t)x.v[i]);
    seal(r);
  }
  if (e) {
    for (int i = 0; i < e->count; i++) {
      TrendEvent ev;
      if (!trend_events_get(e, i, &ev) || ev.slot <= afterSlot) continue;
      if (!epochOk(ev.slot)) return 0;
      memset(r, 0, TREND_JNL_REC_BYTES);
      r[0] = KIND_EVENT;
      r[1] = ev.id;
      r[2] = ev.priority;
      r[3] = ev.raised ? 1 : 0;
      wr32(r + 4, (uint32_t)(ev.slot * TREND_SLOT_SECONDS));
      seal(r);
      r += TREND_JNL_REC_BYTES;
    }
  }
  return (size_t)(r - out);
}

bool trend_jnl_last_sample_slot(const uint8_t *buf, size_t len, int64_t *slot) {
  if (!buf || !slot) return false;
  for (size_t k = len / TREND_JNL_REC_BYTES; k > 0; k--) {
    const uint8_t *r = buf + (k - 1) * TREND_JNL_REC_BYTES;
    if (valid(r) && r[0] == KIND_SAMPLE) {
      *slot = slotOf(r);
      return true;
    }
  }
  return false;
}

void trend_jnl_apply_begin(TrendJnlApply *a, TrendStore *s, TrendEvents *e,
                           const uint8_t *buf, size_t len, int64_t baseSlot) {
  a->s = s;
  a->e = e;
  a->buf = buf;
  a->len = len;
  a->off = 0;
  a->baseSlot = baseSlot;
  a->applied = 0;
  a->broken = false;
}

TrendJobStatus trend_jnl_apply_step(TrendJnlApply *a, int maxRecords) {
  if (!a->s || !a->buf) return TREND_JOB_DONE;
  for (int k = 0; k < maxRecords; k++) {
    if (a->off >= a->len) return TREND_JOB_DONE;
    if (a->len - a->off < TREND_JNL_REC_BYTES) {
      a->broken = true;  // cola a medias: un corte durante el anadido
      return TREND_JOB_DONE;
    }
    const uint8_t *r = a->buf + a->off;
    if (!valid(r)) {
      a->broken = true;
      return TREND_JOB_DONE;
    }
    const int64_t slot = slotOf(r);
    if (r[0] == KIND_SAMPLE) {
      TrendSample x;
      for (int i = 0; i < TREND_SERIES; i++) x.v[i] = (int16_t)rd16(r + 8 + 2 * i);
      // false con slot <= lastSlot: ya estaba en la instantanea.
      trend_store_record(a->s, slot, &x);
    } else if (a->e && slot > a->baseSlot) {
      trend_events_add(a->e, slot, r[1], r[2], r[3] != 0);
    }
    a->off += TREND_JNL_REC_BYTES;
    a->applied++;
  }
  return a->off >= a->len ? TREND_JOB_DONE : TREND_JOB_MORE;
}
