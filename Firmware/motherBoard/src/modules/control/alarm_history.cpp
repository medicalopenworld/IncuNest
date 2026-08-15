#include "alarm_history.h"

#include <string.h>

namespace {

// Sube si cambia el layout de AlarmHistoryEntry. Un blob con otra version se
// descarta entero: leer registros clinicos con el paso equivocado es peor que
// no tenerlos.
const uint16_t kBlobVersion = 1;
const uint16_t kBlobMagic = 0xA1A5;

struct BlobHeader {
  uint16_t magic;
  uint16_t version;
  uint16_t entrySize;
  uint16_t count;
};

AlarmHistoryEntry g_entries[ALARM_HISTORY_CAPACITY];
size_t g_count = 0;  // cuantas posiciones validas hay, tope CAPACITY

// Las entradas se guardan de mas reciente a mas antigua: insertar por delante
// hace que alarm_history_get(0) sea siempre la ultima sin tener que llevar un
// indice de cabeza, y el coste de mover diez estructuras es irrelevante frente
// a la escritura en NVS que viene detras.
void push_front(const AlarmHistoryEntry &e) {
  const size_t keep = (g_count < ALARM_HISTORY_CAPACITY)
                          ? g_count
                          : (ALARM_HISTORY_CAPACITY - 1);
  for (size_t i = keep; i > 0; --i) {
    g_entries[i] = g_entries[i - 1];
  }
  g_entries[0] = e;
  if (g_count < ALARM_HISTORY_CAPACITY) {
    ++g_count;
  }
}

}  // namespace

void alarm_history_init(void) {
  memset(g_entries, 0, sizeof(g_entries));
  g_count = 0;
}

void alarm_history_record_raise(AlarmId id, uint8_t priority,
                                uint32_t nowEpoch, int16_t limitCenti,
                                int16_t valueCenti) {
  if (id <= ALARM_NONE || id >= ALARM_COUNT) {
    return;
  }
  AlarmHistoryEntry e;
  e.id = (uint8_t)id;
  e.priority = priority;
  e.raisedEpoch = nowEpoch;
  e.clearedEpoch = 0;
  e.limitCenti = limitCenti;
  e.valueCenti = valueCenti;
  push_front(e);
}

bool alarm_history_record_clear(AlarmId id, uint32_t nowEpoch) {
  for (size_t i = 0; i < g_count; ++i) {
    // La mas reciente abierta de esa condicion. Recorrer de reciente a antigua
    // importa cuando la misma alarma ha saltado varias veces: la que se cierra
    // es la que sigue viva, no la primera que aparezca.
    if (g_entries[i].id == (uint8_t)id && g_entries[i].clearedEpoch == 0) {
      g_entries[i].clearedEpoch = nowEpoch;
      return true;
    }
  }
  return false;
}

size_t alarm_history_count(void) { return g_count; }

const AlarmHistoryEntry *alarm_history_get(size_t index) {
  return (index < g_count) ? &g_entries[index] : NULL;
}

size_t alarm_history_blob_size(void) {
  return sizeof(BlobHeader) + sizeof(g_entries);
}

size_t alarm_history_serialize(void *buf, size_t bufLen) {
  const size_t need = alarm_history_blob_size();
  if (!buf || bufLen < need) {
    return 0;
  }
  BlobHeader h;
  h.magic = kBlobMagic;
  h.version = kBlobVersion;
  h.entrySize = (uint16_t)sizeof(AlarmHistoryEntry);
  h.count = (uint16_t)g_count;

  uint8_t *p = (uint8_t *)buf;
  memcpy(p, &h, sizeof(h));
  memcpy(p + sizeof(h), g_entries, sizeof(g_entries));
  return need;
}

bool alarm_history_deserialize(const void *buf, size_t bufLen) {
  alarm_history_init();
  if (!buf || bufLen < alarm_history_blob_size()) {
    return false;
  }
  BlobHeader h;
  memcpy(&h, buf, sizeof(h));
  // entrySize entra en la comprobacion aposta: un cambio de layout que se
  // olvide de subir la version se caza igual, que es exactamente el fallo que
  // el historial de bebes tuvo que resolver a posteriori con un tamano legado.
  if (h.magic != kBlobMagic || h.version != kBlobVersion ||
      h.entrySize != sizeof(AlarmHistoryEntry) ||
      h.count > ALARM_HISTORY_CAPACITY) {
    return false;
  }
  memcpy(g_entries, (const uint8_t *)buf + sizeof(h), sizeof(g_entries));
  g_count = h.count;
  return true;
}
