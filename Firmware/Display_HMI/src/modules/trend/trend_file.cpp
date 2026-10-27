#include "modules/trend/trend_file.h"

#include <string.h>

namespace {

// ---- Little-endian explicito -------------------------------------------------
void put16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
}
void put32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
  p[3] = (uint8_t)(v >> 24);
}
uint16_t get16(const uint8_t *p) {
  return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
uint32_t get32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

// Offsets de la cabecera.
constexpr size_t OFF_MAGIC = 0;
constexpr size_t OFF_VERSION = 4;
constexpr size_t OFF_HEADER_SIZE = 6;
constexpr size_t OFF_SLOT_SECONDS = 8;
constexpr size_t OFF_COUNT = 12;
constexpr size_t OFF_LAST_EPOCH = 16;
constexpr size_t OFF_SERIES = 20;
constexpr size_t OFF_EVENT_COUNT = 22;
constexpr size_t OFF_PAYLOAD_CRC = 24;
constexpr size_t OFF_HEADER_CRC = 28;

constexpr uint8_t MAGIC[4] = {'I', 'T', 'R', 'N'};
// Cota de cordura del numero de ranuras: lo que de verdad limita es la longitud
// exacta, pero asi la aritmetica de tamanos nunca desborda.
constexpr uint32_t MAX_COUNT = 1u << 20;

// El epoch del fichero es u32 de segundos: ranuras representables.
constexpr int64_t MAX_SLOT = (int64_t)(0xFFFFFFFFu / (uint32_t)TREND_SLOT_SECONDS);

}  // namespace

size_t trend_file_max_bytes(int capacity) {
  if (capacity <= 0) return 0;
  return TREND_FILE_HEADER_BYTES +
         sizeof(int16_t) * (size_t)TREND_SERIES * (size_t)capacity +
         TREND_FILE_EVENT_BYTES * (size_t)TREND_MAX_EVENTS;
}

uint32_t trend_crc32_update(uint32_t crc, const uint8_t *data, size_t len) {
  // CRC-32/IEEE por nibbles: una tabla de 16 entradas (64 B) y dos pasos por
  // byte. ~4x mas rapido que bit a bit sin pagar 1 KB de tabla.
  static const uint32_t kTable[16] = {
      0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u,
      0x4DB26158u, 0x5005713Cu, 0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
      0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu};
  for (size_t i = 0; i < len; i++) {
    crc = kTable[(crc ^ data[i]) & 0x0Fu] ^ (crc >> 4);
    crc = kTable[(crc ^ (data[i] >> 4)) & 0x0Fu] ^ (crc >> 4);
  }
  return crc;
}

uint32_t trend_crc32_final(uint32_t state) { return ~state; }

uint32_t trend_crc32(const uint8_t *data, size_t len) {
  return trend_crc32_final(trend_crc32_update(TREND_CRC_INIT, data, len));
}

// ---- Serializador por tramos -------------------------------------------------

bool trend_ser_begin(TrendSerializer *z, const TrendStore *s, const TrendEvents *e,
                     uint8_t *out, size_t cap) {
  if (!z || !s || !s->data || s->count <= 0 || !out) return false;
  const int64_t last = s->lastSlot;
  const int64_t first = last - s->count + 1;
  // Solo epochs reales: con ranuras monotonas (sin hora) o fuera de u32 el
  // fichero se restauraria en el sitio equivocado.
  if (first < 0 || last > MAX_SLOT) return false;

  int nEv = 0;
  if (e) {
    for (int i = 0; i < e->count; i++) {
      TrendEvent ev;
      if (trend_events_get(e, i, &ev) && ev.slot >= first && ev.slot <= last) nEv++;
    }
  }

  const size_t seriesBytes = sizeof(int16_t) * (size_t)TREND_SERIES * (size_t)s->count;
  const size_t total = TREND_FILE_HEADER_BYTES + seriesBytes +
                       TREND_FILE_EVENT_BYTES * (size_t)nEv;
  if (total > cap) return false;

  z->s = s;
  z->out = out;
  z->total = total;
  z->gen = s->gen;
  z->first = first;
  z->last = last;
  z->count = s->count;
  z->stage = 0;
  z->ser = 0;
  z->idx = 0;
  z->crcOff = 0;
  z->crc = TREND_CRC_INIT;

  // Los eventos son <= 512 B: se copian ya, en su sitio final. Quedan fijados
  // al instante de begin(); los que lleguen despues entran en el volcado
  // siguiente.
  uint8_t *p = out + TREND_FILE_HEADER_BYTES + seriesBytes;
  if (e) {
    for (int i = 0; i < e->count; i++) {
      TrendEvent ev;
      if (!trend_events_get(e, i, &ev) || ev.slot < first || ev.slot > last) continue;
      put32(p, (uint32_t)(ev.slot * TREND_SLOT_SECONDS));
      p[4] = ev.id;
      p[5] = ev.priority;
      p[6] = ev.raised ? 1 : 0;
      p[7] = 0;
      p += TREND_FILE_EVENT_BYTES;
    }
  }
  return true;
}

TrendJobStatus trend_ser_step(TrendSerializer *z) {
  if (!z || !z->s) return TREND_JOB_FAIL;
  if (z->stage >= 2) return TREND_JOB_DONE;

  if (z->stage == 0) {
    // El anillo cambio desde begin() (muestra nueva, shift): lo ya copiado ya
    // no es del mismo instante. Hay que reiniciar.
    if (z->s->gen != z->gen) return TREND_JOB_RESTART;
    int budget = TREND_SER_VALUES_PER_STEP;
    uint8_t *base = z->out + TREND_FILE_HEADER_BYTES;
    while (budget > 0 && z->ser < TREND_SERIES) {
      const int16_t v = trend_store_value(z->s, z->ser, z->first + z->idx);
      put16(base + ((size_t)z->ser * (size_t)z->count + (size_t)z->idx) * 2u,
            (uint16_t)v);
      if (++z->idx >= z->count) {
        z->idx = 0;
        z->ser++;
      }
      budget--;
    }
    if (z->ser >= TREND_SERIES) {
      z->stage = 1;
      z->crcOff = TREND_FILE_HEADER_BYTES;
      z->crc = TREND_CRC_INIT;
    }
    return TREND_JOB_MORE;
  }

  // stage 1: CRC de la carga por tramos, y al final la cabecera.
  size_t n = z->total - z->crcOff;
  if (n > TREND_CRC_BYTES_PER_STEP) n = TREND_CRC_BYTES_PER_STEP;
  z->crc = trend_crc32_update(z->crc, z->out + z->crcOff, n);
  z->crcOff += n;
  if (z->crcOff < z->total) return TREND_JOB_MORE;

  uint8_t *h = z->out;
  const size_t evBytes = z->total - TREND_FILE_HEADER_BYTES -
                         sizeof(int16_t) * (size_t)TREND_SERIES * (size_t)z->count;
  memcpy(h + OFF_MAGIC, MAGIC, sizeof(MAGIC));
  put16(h + OFF_VERSION, TREND_FILE_VERSION);
  put16(h + OFF_HEADER_SIZE, (uint16_t)TREND_FILE_HEADER_BYTES);
  put32(h + OFF_SLOT_SECONDS, (uint32_t)TREND_SLOT_SECONDS);
  put32(h + OFF_COUNT, (uint32_t)z->count);
  put32(h + OFF_LAST_EPOCH, (uint32_t)(z->last * TREND_SLOT_SECONDS));
  put16(h + OFF_SERIES, (uint16_t)TREND_SERIES);
  put16(h + OFF_EVENT_COUNT, (uint16_t)(evBytes / TREND_FILE_EVENT_BYTES));
  put32(h + OFF_PAYLOAD_CRC, trend_crc32_final(z->crc));
  put32(h + OFF_HEADER_CRC, trend_crc32(h, OFF_HEADER_CRC));
  z->stage = 2;
  return TREND_JOB_DONE;
}

size_t trend_ser_size(const TrendSerializer *z) { return z ? z->total : 0; }

size_t trend_file_serialize(const TrendStore *s, const TrendEvents *e,
                            uint8_t *out, size_t cap) {
  TrendSerializer z;
  if (!trend_ser_begin(&z, s, e, out, cap)) return 0;
  while (trend_ser_step(&z) == TREND_JOB_MORE) {
  }
  return trend_ser_size(&z);
}

// ---- Validacion --------------------------------------------------------------

namespace {

// Todo menos el CRC de la carga (que es el trabajo caro y va por tramos).
TrendFileStatus validateHeader(const uint8_t *buf, size_t len, TrendFileInfo *info) {
  if (!buf || len < TREND_FILE_HEADER_BYTES) return TREND_FILE_BAD_LENGTH;
  if (memcmp(buf + OFF_MAGIC, MAGIC, sizeof(MAGIC)) != 0) return TREND_FILE_BAD_MAGIC;
  if (get32(buf + OFF_HEADER_CRC) != trend_crc32(buf, OFF_HEADER_CRC))
    return TREND_FILE_BAD_CRC;
  if (get16(buf + OFF_VERSION) != TREND_FILE_VERSION) return TREND_FILE_BAD_VERSION;

  const uint32_t count = get32(buf + OFF_COUNT);
  const uint32_t evCount = get16(buf + OFF_EVENT_COUNT);
  if (get16(buf + OFF_HEADER_SIZE) != TREND_FILE_HEADER_BYTES ||
      get32(buf + OFF_SLOT_SECONDS) != (uint32_t)TREND_SLOT_SECONDS ||
      get16(buf + OFF_SERIES) != (uint16_t)TREND_SERIES || count == 0 ||
      count > MAX_COUNT || evCount > (uint32_t)TREND_MAX_EVENTS)
    return TREND_FILE_BAD_HEADER;

  const uint64_t expected = (uint64_t)TREND_FILE_HEADER_BYTES +
                            (uint64_t)sizeof(int16_t) * TREND_SERIES * count +
                            (uint64_t)TREND_FILE_EVENT_BYTES * evCount;
  if ((uint64_t)len != expected) return TREND_FILE_BAD_LENGTH;

  if (info) {
    info->count = (int)count;
    info->lastSlot = (int64_t)(get32(buf + OFF_LAST_EPOCH) / (uint32_t)TREND_SLOT_SECONDS);
    info->eventCount = (int)evCount;
  }
  return TREND_FILE_OK;
}

}  // namespace

TrendFileStatus trend_file_validate(const uint8_t *buf, size_t len,
                                    TrendFileInfo *info) {
  const TrendFileStatus st = validateHeader(buf, len, info);
  if (st != TREND_FILE_OK) return st;
  if (get32(buf + OFF_PAYLOAD_CRC) !=
      trend_crc32(buf + TREND_FILE_HEADER_BYTES, len - TREND_FILE_HEADER_BYTES))
    return TREND_FILE_BAD_CRC;
  return TREND_FILE_OK;
}

// ---- Restauracion por tramos -------------------------------------------------

namespace {

constexpr int RESTORE_MAX_RETRIES = 3;
constexpr int ST_HEADER = 0, ST_CRC = 1, ST_PLAN = 2, ST_APPLY = 3, ST_EVENTS = 4,
              ST_DONE = 5;

TrendJobStatus finish(TrendRestoreJob *r, TrendRestoreResult res) {
  r->result = res;
  r->stage = ST_DONE;
  return TREND_JOB_DONE;
}

}  // namespace

void trend_restore_begin(TrendRestoreJob *r, const uint8_t *buf, size_t len,
                         int64_t nowSlot, TrendStore *s, TrendEvents *e) {
  if (!r) return;
  memset(r, 0, sizeof(*r));
  r->buf = buf;
  r->len = len;
  r->nowSlot = nowSlot;
  r->s = s;
  r->e = e;
  r->stage = ST_HEADER;
  r->result = TREND_RESTORE_INVALID;
  if (!s || !s->data || s->capacity <= 0) r->stage = ST_DONE;
}

TrendJobStatus trend_restore_step(TrendRestoreJob *r) {
  if (!r) return TREND_JOB_FAIL;
  switch (r->stage) {
    case ST_HEADER:
      if (validateHeader(r->buf, r->len, &r->info) != TREND_FILE_OK)
        return finish(r, TREND_RESTORE_INVALID);
      r->stage = ST_CRC;
      r->crcOff = TREND_FILE_HEADER_BYTES;
      r->crc = TREND_CRC_INIT;
      return TREND_JOB_MORE;

    case ST_CRC: {
      size_t n = r->len - r->crcOff;
      if (n > TREND_CRC_BYTES_PER_STEP) n = TREND_CRC_BYTES_PER_STEP;
      r->crc = trend_crc32_update(r->crc, r->buf + r->crcOff, n);
      r->crcOff += n;
      if (r->crcOff < r->len) return TREND_JOB_MORE;
      if (trend_crc32_final(r->crc) != get32(r->buf + OFF_PAYLOAD_CRC))
        return finish(r, TREND_RESTORE_INVALID);
      r->stage = ST_PLAN;
      return TREND_JOB_MORE;
    }

    case ST_PLAN: {
      // Un fichero de "despues de ahora" es de un reloj que estuvo mal: no se
      // toca (una ranura de tolerancia por el redondeo de millis() frente a
      // esp_timer). Y uno tan viejo que ya no cae en el anillo, tampoco.
      if (r->info.lastSlot > r->nowSlot + 1) return finish(r, TREND_RESTORE_FUTURE);
      if (r->nowSlot - r->info.lastSlot >= r->s->capacity)
        return finish(r, TREND_RESTORE_STALE);
      const uint8_t *payload = r->buf + TREND_FILE_HEADER_BYTES;
      if (!trend_prepend_begin(&r->pj, r->s, r->info.lastSlot, payload, r->info.count))
        return finish(r, TREND_RESTORE_STALE);
      r->stage = ST_APPLY;
      return TREND_JOB_MORE;
    }

    case ST_APPLY: {
      const TrendJobStatus st = trend_prepend_step(&r->pj, TREND_APPLY_SLOTS_PER_STEP);
      if (st == TREND_JOB_MORE) return TREND_JOB_MORE;
      if (st == TREND_JOB_DONE) {
        r->stage = ST_EVENTS;
        return TREND_JOB_MORE;
      }
      // RESTART: entro una muestra viva entre pasadas. No se toco nada visible:
      // se replanifica contra el anillo de ahora (unas pocas veces; luego se
      // renuncia, y el reintento ya lo decidira quien llama).
      if (st == TREND_JOB_RESTART && ++r->retries <= RESTORE_MAX_RETRIES) {
        r->stage = ST_PLAN;
        return TREND_JOB_MORE;
      }
      return finish(r, TREND_RESTORE_STALE);
    }

    case ST_EVENTS: {
      r->restored = r->pj.added;
      r->oldestIncluded = r->pj.lo;
      if (r->e && r->info.eventCount > 0) {
        TrendEvent evs[TREND_MAX_EVENTS];
        const uint8_t *ep = r->buf + TREND_FILE_HEADER_BYTES +
                            sizeof(int16_t) * (size_t)TREND_SERIES * (size_t)r->info.count;
        int n = 0;
        for (int i = 0; i < r->info.eventCount && n < TREND_MAX_EVENTS; i++) {
          const uint8_t *q = ep + (size_t)i * TREND_FILE_EVENT_BYTES;
          evs[n].slot = (int64_t)(get32(q) / (uint32_t)TREND_SLOT_SECONDS);
          evs[n].id = q[4];
          evs[n].priority = q[5];
          evs[n].raised = (q[6] != 0);
          n++;
        }
        trend_events_prepend(r->e, evs, n, r->oldestIncluded);
      }
      return finish(r, TREND_RESTORE_OK);
    }

    default:
      return TREND_JOB_DONE;
  }
}

TrendRestoreResult trend_file_restore(const uint8_t *buf, size_t len,
                                      int64_t nowSlot, TrendStore *s,
                                      TrendEvents *e, int *restoredSlots) {
  TrendRestoreJob r;
  trend_restore_begin(&r, buf, len, nowSlot, s, e);
  while (trend_restore_step(&r) == TREND_JOB_MORE) {
  }
  if (restoredSlots) *restoredSlots = (r.result == TREND_RESTORE_OK) ? r.restored : 0;
  return r.result;
}
