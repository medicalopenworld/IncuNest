#include "modules/trend/trend_persist.h"

#include "esp_heap_caps.h"
#include "esp_log.h"

#include "modules/storage/hmi_fs.h"
#include "modules/trend/trend_file.h"
#include "modules/trend/trend_policy.h"

namespace {

const char *TAG = "TrendPersist";
const char *kPath = "/trend.bin";

bool s_restoreDone = false;
TrendDumpState s_dump;
bool s_dumpInit = false;
volatile bool s_lockRequest = false;

// Volcado en curso. El buffer de staging es una INSTANTANEA: el modelo sigue
// avanzando mientras se escribe, y el fichero sale consistente de todos modos.
// Vive en PSRAM (~104 KB) solo mientras dura el volcado: nada de SRAM interna.
uint8_t *s_stage = nullptr;
size_t s_stageLen = 0;
size_t s_stageOff = 0;
int64_t s_stageNewest = 0;
uint32_t s_lastChunkMs = 0;
uint32_t s_startMs = 0;
HmiFsAtomicWriter s_writer;

void freeStage() {
  if (s_stage) heap_caps_free(s_stage);
  s_stage = nullptr;
  s_stageLen = 0;
  s_stageOff = 0;
}

void failDump(uint32_t nowMs, const char *why) {
  s_writer.abort();
  freeStage();
  trend_dump_failed(&s_dump, nowMs);
  ESP_LOGE(TAG, "volcado fallido (%s); reintento en %u s", why,
           (unsigned)(TREND_DUMP_PERIOD_MS / 1000));
}

// Restaura una sola vez por arranque, en cuanto el reloj tiene hora: antes las
// ranuras serian monotonas y el fichero (epoch) no se podria alinear.
bool restoreOnce(TrendModel *m, int64_t monoMs) {
  if (!HmiFs_Begin()) return false;
  const int32_t sz = HmiFs_FileSize(kPath);
  if (sz <= 0) {
    ESP_LOGI(TAG, "sin fichero de tendencia previo");
    return false;
  }
  if ((size_t)sz > trend_file_max_bytes(m->store.capacity)) {
    ESP_LOGW(TAG, "%s ocupa %d B, mas que el maximo de %u B: se ignora", kPath,
             (int)sz, (unsigned)trend_file_max_bytes(m->store.capacity));
    return false;
  }
  uint8_t *buf = (uint8_t *)heap_caps_malloc((size_t)sz, MALLOC_CAP_SPIRAM);
  if (!buf) {
    ESP_LOGW(TAG, "sin PSRAM para leer %d B: no se restaura", (int)sz);
    return false;
  }
  size_t len = 0;
  bool changed = false;
  if (HmiFs_ReadAll(kPath, buf, (size_t)sz, &len)) {
    int restored = 0;
    const int64_t now = trend_clock_slot(&m->clock, monoMs);
    const TrendRestoreResult r =
        trend_file_restore(buf, len, now, &m->store, &m->events, &restored);
    static const char *const kNames[] = {"OK", "INVALIDO", "DEL FUTURO", "OBSOLETO"};
    ESP_LOGW(TAG, "restauracion %s: %d ranuras, %d eventos (fichero %u B)",
             kNames[(int)r < 4 ? (int)r : 1], restored, m->events.count, (unsigned)len);
    changed = (r == TREND_RESTORE_OK);
  } else {
    ESP_LOGW(TAG, "no se pudo leer %s", kPath);
  }
  heap_caps_free(buf);
  return changed;
}

void startDump(TrendModel *m, uint32_t nowMs) {
  if (!HmiFs_Begin()) {
    trend_dump_failed(&s_dump, nowMs);
    return;
  }
  const size_t cap = trend_file_max_bytes(m->store.capacity);
  s_stage = (uint8_t *)heap_caps_malloc(cap, MALLOC_CAP_SPIRAM);
  if (!s_stage) {
    failDump(nowMs, "sin PSRAM para el volcado");
    return;
  }
  s_stageLen = trend_file_serialize(&m->store, &m->events, s_stage, cap);
  if (s_stageLen == 0) {
    failDump(nowMs, "nada que serializar");
    return;
  }
  s_stageNewest = m->store.lastSlot;
  if (!s_writer.begin(kPath)) {
    failDump(nowMs, "no se pudo abrir el temporal");
    return;
  }
  s_stageOff = 0;
  s_startMs = nowMs;
  s_lastChunkMs = nowMs - TREND_CHUNK_GAP_MS;  // el primer trozo, ya
  s_lockRequest = false;
}

void continueDump(uint32_t nowMs) {
  if (!trend_chunk_due(nowMs, s_lastChunkMs)) return;
  const size_t left = s_stageLen - s_stageOff;
  const size_t n = left < TREND_CHUNK_BYTES ? left : TREND_CHUNK_BYTES;
  s_lastChunkMs = nowMs;
  if (s_writer.write(s_stage + s_stageOff, n) != n) {
    failDump(nowMs, "escritura corta");
    return;
  }
  s_stageOff += n;
  if (s_stageOff < s_stageLen) return;

  const size_t total = s_stageLen;
  if (!s_writer.commit()) {
    failDump(nowMs, "renombrado");
    return;
  }
  freeStage();
  trend_dump_done(&s_dump, nowMs, s_stageNewest);
  ESP_LOGI(TAG, "volcado %u B en %u ms (%u trozos)", (unsigned)total,
           (unsigned)(nowMs - s_startMs),
           (unsigned)((total + TREND_CHUNK_BYTES - 1) / TREND_CHUNK_BYTES));
}

}  // namespace

bool TrendPersist_Service(TrendModel *m, int64_t monoMs, uint32_t nowMs) {
  if (!m || !m->store.data) return false;
  if (!s_dumpInit) {
    trend_dump_init(&s_dump, nowMs);
    s_dumpInit = true;
  }
  // Sin hora las ranuras son monotonas: ni se restaura ni se vuelca.
  if (!m->clock.synced) return false;

  bool changed = false;
  if (!s_restoreDone) {
    s_restoreDone = true;
    changed = restoreOnce(m, monoMs);
  }

  if (s_stage) {
    continueDump(nowMs);
  } else if (trend_dump_due(&s_dump, true, m->store.count > 0,
                            trend_model_newest_slot(m), nowMs, s_lockRequest)) {
    startDump(m, nowMs);
  }
  return changed;
}

void TrendPersist_RequestDumpOnLock(void) { s_lockRequest = true; }
