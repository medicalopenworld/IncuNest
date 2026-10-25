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

uint32_t trend_crc32(const uint8_t *data, size_t len) {
  // CRC-32/IEEE por nibbles: una tabla de 16 entradas (64 B) y dos pasos por
  // byte. ~4x mas rapido que bit a bit sin pagar 1 KB de tabla.
  static const uint32_t kTable[16] = {
      0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u,
      0x4DB26158u, 0x5005713Cu, 0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
      0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu};
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; i++) {
    crc = kTable[(crc ^ data[i]) & 0x0Fu] ^ (crc >> 4);
    crc = kTable[(crc ^ (data[i] >> 4)) & 0x0Fu] ^ (crc >> 4);
  }
  return ~crc;
}

size_t trend_file_serialize(const TrendStore *s, const TrendEvents *e,
                            uint8_t *out, size_t cap) {
  if (!s || !s->data || s->count <= 0 || !out) return 0;
  const int64_t last = s->lastSlot;
  const int64_t first = last - s->count + 1;
  // Solo epochs reales: con ranuras monotonas (sin hora) o fuera de u32 el
  // fichero se restauraria en el sitio equivocado.
  if (first < 0 || last > MAX_SLOT) return 0;

  int nEv = 0;
  if (e) {
    for (int i = 0; i < e->count; i++) {
      TrendEvent ev;
      if (trend_events_get(e, i, &ev) && ev.slot >= first && ev.slot <= last) nEv++;
    }
  }

  const size_t payloadSeries = sizeof(int16_t) * (size_t)TREND_SERIES * (size_t)s->count;
  const size_t total = TREND_FILE_HEADER_BYTES + payloadSeries +
                       TREND_FILE_EVENT_BYTES * (size_t)nEv;
  if (total > cap) return 0;

  uint8_t *p = out + TREND_FILE_HEADER_BYTES;
  for (int ser = 0; ser < TREND_SERIES; ser++) {
    for (int i = 0; i < s->count; i++) {
      put16(p, (uint16_t)trend_store_value(s, ser, first + i));
      p += 2;
    }
  }
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

  memcpy(out + OFF_MAGIC, MAGIC, sizeof(MAGIC));
  put16(out + OFF_VERSION, TREND_FILE_VERSION);
  put16(out + OFF_HEADER_SIZE, (uint16_t)TREND_FILE_HEADER_BYTES);
  put32(out + OFF_SLOT_SECONDS, (uint32_t)TREND_SLOT_SECONDS);
  put32(out + OFF_COUNT, (uint32_t)s->count);
  put32(out + OFF_LAST_EPOCH, (uint32_t)(last * TREND_SLOT_SECONDS));
  put16(out + OFF_SERIES, (uint16_t)TREND_SERIES);
  put16(out + OFF_EVENT_COUNT, (uint16_t)nEv);
  put32(out + OFF_PAYLOAD_CRC,
        trend_crc32(out + TREND_FILE_HEADER_BYTES, total - TREND_FILE_HEADER_BYTES));
  put32(out + OFF_HEADER_CRC, trend_crc32(out, OFF_HEADER_CRC));
  return total;
}

TrendFileStatus trend_file_validate(const uint8_t *buf, size_t len,
                                    TrendFileInfo *info) {
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
  if (get32(buf + OFF_PAYLOAD_CRC) !=
      trend_crc32(buf + TREND_FILE_HEADER_BYTES, len - TREND_FILE_HEADER_BYTES))
    return TREND_FILE_BAD_CRC;

  if (info) {
    info->count = (int)count;
    info->lastSlot = (int64_t)(get32(buf + OFF_LAST_EPOCH) / (uint32_t)TREND_SLOT_SECONDS);
    info->eventCount = (int)evCount;
  }
  return TREND_FILE_OK;
}

TrendRestoreResult trend_file_restore(const uint8_t *buf, size_t len,
                                      int64_t nowSlot, TrendStore *s,
                                      TrendEvents *e, int *restoredSlots) {
  if (restoredSlots) *restoredSlots = 0;
  if (!s || !s->data || s->capacity <= 0) return TREND_RESTORE_INVALID;

  TrendFileInfo info = {};
  if (trend_file_validate(buf, len, &info) != TREND_FILE_OK) return TREND_RESTORE_INVALID;

  // Un fichero de "despues de ahora" es de un reloj que estuvo mal: no se toca
  // (una ranura de tolerancia por el redondeo de millis() frente a esp_timer).
  if (info.lastSlot > nowSlot + 1) return TREND_RESTORE_FUTURE;
  // Tan viejo que ya no cae en el anillo.
  if (nowSlot - info.lastSlot >= s->capacity) return TREND_RESTORE_STALE;

  // Ranura mas antigua que va a quedar restaurada, para filtrar los eventos:
  // la mas reciente que entra es min(first-1, lastSlot) y entran `added`.
  const int64_t firstLive = (s->count > 0) ? s->lastSlot - s->count + 1 : info.lastSlot + 1;
  const int64_t newestIncluded = (firstLive - 1 < info.lastSlot) ? firstLive - 1 : info.lastSlot;

  const uint8_t *payload = buf + TREND_FILE_HEADER_BYTES;
  const int added = trend_store_prepend(s, info.lastSlot, payload, info.count);
  if (added <= 0) return TREND_RESTORE_STALE;
  const int64_t oldestIncluded = newestIncluded - added + 1;

  if (e && info.eventCount > 0) {
    TrendEvent evs[TREND_MAX_EVENTS];
    const uint8_t *ep = payload + sizeof(int16_t) * (size_t)TREND_SERIES * (size_t)info.count;
    int n = 0;
    for (int i = 0; i < info.eventCount && n < TREND_MAX_EVENTS; i++) {
      const uint8_t *r = ep + (size_t)i * TREND_FILE_EVENT_BYTES;
      evs[n].slot = (int64_t)(get32(r) / (uint32_t)TREND_SLOT_SECONDS);
      evs[n].id = r[4];
      evs[n].priority = r[5];
      evs[n].raised = (r[6] != 0);
      n++;
    }
    trend_events_prepend(e, evs, n, oldestIncluded);
  }

  if (restoredSlots) *restoredSlots = added;
  return TREND_RESTORE_OK;
}
