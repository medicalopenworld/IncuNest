#include "modules/trend/trend_persist.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "modules/storage/hmi_fs.h"
#include "modules/trend/trend_file.h"
#include "modules/trend/trend_journal.h"
#include "modules/trend/trend_policy.h"

// Maquina de estados. TODO el trabajo se reparte en pasadas acotadas (una
// llamada a TrendPersist_Service hace UN tramo) y nada empieza ni continua si
// hay una alarma activa o el enlace con la placa esta caido (ioAllowed):
//
//   restauracion:  Idle -> RestoreRead (abre + reserva) -> RestoreRead*
//                  (4 KB por pasada) -> [JnlRead* (4 KB por pasada) ->
//                  Merge* (instantanea + diario en un anillo temporal y de
//                  vuelta a imagen de fichero, por tramos)] -> RestoreRun*
//                  (CRC y escritura por tramos)
//   anadido:       Idle -> Idle (una sola escritura de < 4 KB en /trend.jnl)
//   volcado:       Idle -> DumpSerialize* (series y CRC por tramos) ->
//                  DumpWrite* (4 KB por pasada, separados >= 50 ms) -> Idle
//
// Cada 5 min se ANADE al diario si lo que hay en disco sigue en sintonia con el
// modelo; la instantanea completa (13 bloques: ~1 s de temblor del panel, ver
// trend_journal.h) solo se reescribe al compactar o cuando no hay sintonia.
//
// Con ioAllowed = false: una restauracion se PAUSA donde esta; un volcado se
// ABORTA limpio (el .tmp se descarta) y se reintenta cuando toque.
namespace {

const char *TAG = "TrendPersist";
const char *kPath = "/trend.bin";
const char *kJnlPath = "/trend.jnl";
// Un diario mas grande que esto no lo ha escrito este firmware: se ignora y el
// primer volcado lo sustituye.
constexpr size_t kJnlMaxBytes =
    (size_t)(TREND_JNL_MAX_RECORDS + TREND_JNL_APPEND_MAX_RECORDS) * TREND_JNL_REC_BYTES;
constexpr int kJnlApplyPerStep = 512;

enum class Phase : uint8_t {
  Idle,
  RestoreRead,
  JnlRead,
  Merge,
  RestoreRun,
  DumpSerialize,
  DumpWrite,
};
Phase s_phase = Phase::Idle;

// --- restauracion ---
bool s_restoreDone = false;
HmiFsReader s_reader;
uint8_t *s_rbuf = nullptr;
size_t s_rlen = 0;
size_t s_roff = 0;
TrendRestoreJob s_rjob;
uint32_t s_restoreStartMs = 0;
int s_restoreSteps = 0;
// Un fichero rechazado por FUTURE o STALE puede ser de un reloj que estaba mal
// (A3): se reintenta UNA vez cuando el reloj se corrija, y hasta entonces (o
// hasta que pase la gracia) el volcado no lo sobrescribe.
bool s_rejected = false;
bool s_retried = false;
uint32_t s_rejectedAtMs = 0;
uint32_t s_shiftsAtReject = 0;

// --- volcado ---
TrendDumpState s_dump;
bool s_dumpInit = false;
volatile bool s_lockRequest = false;
// El buffer de staging es una INSTANTANEA reconstruida por tramos; vive en
// PSRAM (~104 KB) solo mientras dura el volcado: nada de SRAM interna.
uint8_t *s_stage = nullptr;
size_t s_stageCap = 0;
size_t s_stageLen = 0;
size_t s_stageOff = 0;
int64_t s_stageNewest = 0;
uint32_t s_lastChunkMs = 0;
uint32_t s_startMs = 0;
int s_serSteps = 0;
int s_serRestarts = 0;
TrendSerializer s_ser;
HmiFsAtomicWriter s_writer;
uint32_t s_stageShifts = 0;

// --- diario ---
// true si lo que hay en disco (instantanea + diario) es el modelo hasta
// s_dump.lastDumpedSlot CON LAS MISMAS ETIQUETAS de ranura: solo entonces se
// puede anadir al diario. Una re-etiqueta por la hora (clockShifts), un diario
// roto o no tener instantanea obligan a una instantanea completa.
bool s_diskInSync = false;
uint32_t s_shiftsAtSync = 0;
int s_jnlRecords = 0;  // registros en el diario: al llegar al tope se compacta
// restauracion: lectura del diario y fusion con la instantanea
uint8_t *s_jbuf = nullptr;
size_t s_jlen = 0;
size_t s_joff = 0;
bool s_jnlBroken = false;
int s_mergeStage = 0;
int16_t *s_mblock = nullptr;
TrendEvents *s_mevents = nullptr;
TrendStore s_mstore;
TrendRestoreJob s_mjob;
TrendJnlApply s_mapply;
TrendSerializer s_mser;
uint8_t *s_mimg = nullptr;

constexpr int MAX_SER_RESTARTS = 5;

void freeStage() {
  if (s_stage) heap_caps_free(s_stage);
  s_stage = nullptr;
  s_stageLen = 0;
  s_stageOff = 0;
}

void freeMerge() {
  if (s_jbuf) heap_caps_free(s_jbuf);
  if (s_mblock) heap_caps_free(s_mblock);
  if (s_mevents) heap_caps_free(s_mevents);
  if (s_mimg) heap_caps_free(s_mimg);
  s_jbuf = nullptr;
  s_mblock = nullptr;
  s_mevents = nullptr;
  s_mimg = nullptr;
  s_jlen = 0;
  s_joff = 0;
}

void freeRestore() {
  s_reader.close();
  if (s_rbuf) heap_caps_free(s_rbuf);
  s_rbuf = nullptr;
  s_rlen = 0;
  s_roff = 0;
  freeMerge();
}

void failDump(uint32_t nowMs, const char *why) {
  s_writer.abort();
  freeStage();
  s_phase = Phase::Idle;
  trend_dump_failed(&s_dump, nowMs);
  ESP_LOGE(TAG, "volcado fallido (%s); reintento en %u s", why,
           (unsigned)(TREND_DUMP_PERIOD_MS / 1000));
}

// ---------------------------------------------------------------- restauracion

// Una pasada para decidir si hay algo que restaurar; las siguientes leen.
void startRestore() {
  if (!HmiFs_Begin()) {
    // Sin imagen valida (formateo diferido pendiente o fallido) no hay fichero
    // que restaurar: se da por hecho y, cuando el FS exista, el primer volcado
    // lo crea.
    s_restoreDone = true;
    return;
  }
  const int32_t sz = s_reader.open(kPath);
  if (sz <= 0) {
    ESP_LOGI(TAG, "sin fichero de tendencia previo");
    s_reader.close();
    s_restoreDone = true;
    return;
  }
  s_rlen = (size_t)sz;
  s_roff = 0;
  s_phase = Phase::RestoreRead;
}

void restoreFinished(TrendModel *m, uint32_t nowMs, bool *changed) {
  const TrendRestoreResult r = s_rjob.result;
  static const char *const kNames[] = {"OK", "INVALIDO", "DEL FUTURO", "OBSOLETO"};
  const char *name = kNames[(int)r < 4 ? (int)r : 1];
  if (r == TREND_RESTORE_OK)
    ESP_LOGI(TAG, "restauracion %s: %d ranuras, %d eventos (fichero %u B, %d pasadas, %u ms)",
             name, s_rjob.restored, m->events.count, (unsigned)s_rlen, s_restoreSteps,
             (unsigned)(nowMs - s_restoreStartMs));
  else
    ESP_LOGW(TAG, "restauracion %s (fichero %u B, %d pasadas)", name, (unsigned)s_rlen,
             s_restoreSteps);
  // Lo restaurado ES lo que hay en disco: se sigue anadiendo al diario desde
  // su ultima ranura (el hueco hasta las muestras vivas entra como "sin dato").
  // Con el diario roto, el primer volcado es una instantanea que lo sustituye.
  s_diskInSync = (r == TREND_RESTORE_OK) && !s_jnlBroken;
  if (s_diskInSync) {
    trend_dump_done(&s_dump, nowMs, s_rjob.info.lastSlot);
    s_shiftsAtSync = m->clockShifts;
    s_jnlRecords = (int)(s_jlen / TREND_JNL_REC_BYTES);
  }
  freeRestore();
  s_phase = Phase::Idle;
  s_restoreDone = true;
  *changed = (r == TREND_RESTORE_OK);
  if ((r == TREND_RESTORE_FUTURE || r == TREND_RESTORE_STALE) && !s_retried) {
    s_rejected = true;
    s_rejectedAtMs = nowMs;
    s_shiftsAtReject = m->clockShifts;
  } else {
    s_rejected = false;
  }
}

// Restauracion al modelo vivo de la imagen que haya en s_rbuf (la instantanea
// tal cual, o la fusion con el diario).
void beginLiveRestore(TrendModel *m, int64_t monoMs) {
  trend_restore_begin(&s_rjob, s_rbuf, s_rlen, trend_clock_slot(&m->clock, monoMs),
                      &m->store, &m->events);
  s_phase = Phase::RestoreRun;
}

// Sin diario utilizable se restaura la instantanea sola.
void dropJournal(TrendModel *m, int64_t monoMs, const char *why) {
  ESP_LOGW(TAG, "diario descartado (%s): se restaura solo la instantanea", why);
  s_jnlBroken = true;
  freeMerge();
  beginLiveRestore(m, monoMs);
}

void startJnlRead(TrendModel *m, int64_t monoMs) {
  s_jnlBroken = false;
  const int32_t sz = HmiFs_FileSize(kJnlPath);
  if (sz <= 0) {
    beginLiveRestore(m, monoMs);
    return;
  }
  if ((size_t)sz > kJnlMaxBytes) {
    dropJournal(m, monoMs, "demasiado grande");
    return;
  }
  if (s_reader.open(kJnlPath) != sz) {
    s_reader.close();
    dropJournal(m, monoMs, "no se pudo abrir");
    return;
  }
  s_jlen = (size_t)sz;
  s_joff = 0;
  s_phase = Phase::JnlRead;
}

void stepJnlRead(TrendModel *m, int64_t monoMs) {
  if (!s_jbuf) {  // pasada propia para la reserva
    s_jbuf = (uint8_t *)heap_caps_malloc(s_jlen, MALLOC_CAP_SPIRAM);
    if (!s_jbuf) {
      s_reader.close();
      dropJournal(m, monoMs, "sin PSRAM");
    }
    return;
  }
  size_t n = s_jlen - s_joff;
  if (n > TREND_CHUNK_BYTES) n = TREND_CHUNK_BYTES;
  if (s_reader.read(s_jbuf + s_joff, n) != n) {
    s_reader.close();
    dropJournal(m, monoMs, "lectura corta");
    return;
  }
  s_joff += n;
  if (s_joff < s_jlen) return;
  s_reader.close();
  s_mergeStage = 0;
  s_phase = Phase::Merge;
}

// Fusion en un anillo TEMPORAL (el vivo no se toca hasta RestoreRun):
//   0 reserva y planifica la instantanea, 1 la restaura por tramos,
//   2 aplica el diario por tramos, 3 lo serializa de vuelta a imagen de fichero.
// Asi la restauracion al modelo vivo (alineado por epoch, vivas mandan...) es
// la de siempre, sobre una imagen que ya incluye el diario.
void stepMerge(TrendModel *m, int64_t monoMs) {
  switch (s_mergeStage) {
    case 0: {
      int64_t jnlLast = 0;
      if (!trend_jnl_last_sample_slot(s_jbuf, s_jlen, &jnlLast)) {
        dropJournal(m, monoMs, "sin muestras validas");
        return;
      }
      const int cap = m->store.capacity;
      s_mblock = (int16_t *)heap_caps_malloc(trend_store_bytes(cap), MALLOC_CAP_SPIRAM);
      s_mevents = (TrendEvents *)heap_caps_malloc(sizeof(TrendEvents), MALLOC_CAP_SPIRAM);
      if (!s_mblock || !s_mevents) {
        dropJournal(m, monoMs, "sin PSRAM para la fusion");
        return;
      }
      trend_store_init(&s_mstore, s_mblock, cap);
      trend_events_init(s_mevents);
      // "Ahora" para la instantanea es el final del diario: una instantanea
      // de hace mas de 24 h con un diario reciente no es obsoleta.
      trend_restore_begin(&s_mjob, s_rbuf, s_rlen, jnlLast, &s_mstore, s_mevents);
      s_mergeStage = 1;
      return;
    }
    case 1:
      if (trend_restore_step(&s_mjob) == TREND_JOB_MORE) return;
      if (s_mjob.result != TREND_RESTORE_OK) {
        // La instantanea no vale: que la restauracion normal lo registre.
        dropJournal(m, monoMs, "instantanea no valida");
        return;
      }
      trend_jnl_apply_begin(&s_mapply, &s_mstore, s_mevents, s_jbuf, s_jlen,
                            s_mjob.info.lastSlot);
      s_mergeStage = 2;
      return;
    case 2: {
      if (trend_jnl_apply_step(&s_mapply, kJnlApplyPerStep) == TREND_JOB_MORE) return;
      if (s_mapply.broken)
        ESP_LOGW(TAG, "diario cortado en el registro %d: lo posterior se ignora",
                 s_mapply.applied);
      s_jnlBroken = s_mapply.broken;
      const size_t cap = trend_file_max_bytes(s_mstore.capacity);
      s_mimg = (uint8_t *)heap_caps_malloc(cap, MALLOC_CAP_SPIRAM);
      if (!s_mimg || !trend_ser_begin(&s_mser, &s_mstore, s_mevents, s_mimg, cap)) {
        dropJournal(m, monoMs, "no se pudo serializar la fusion");
        return;
      }
      s_mergeStage = 3;
      return;
    }
    default: {
      const TrendJobStatus st = trend_ser_step(&s_mser);
      if (st == TREND_JOB_MORE) return;
      if (st != TREND_JOB_DONE) {
        dropJournal(m, monoMs, "serializador");
        return;
      }
      ESP_LOGI(TAG, "instantanea %u B + diario %d registros fusionados",
               (unsigned)s_rlen, s_mapply.applied);
      // La imagen fusionada sustituye a la instantanea leida. s_jlen se
      // conserva: cuenta los registros del diario para la compactacion.
      heap_caps_free(s_rbuf);
      s_rbuf = s_mimg;
      s_rlen = trend_ser_size(&s_mser);
      s_mimg = nullptr;
      heap_caps_free(s_jbuf);
      heap_caps_free(s_mblock);
      heap_caps_free(s_mevents);
      s_jbuf = nullptr;
      s_mblock = nullptr;
      s_mevents = nullptr;
      beginLiveRestore(m, monoMs);
      return;
    }
  }
}

void stepRestore(TrendModel *m, int64_t monoMs, uint32_t nowMs, bool *changed) {
  if (s_phase == Phase::RestoreRead) {
    if (!s_rbuf) {  // pasada propia para la reserva
      s_rbuf = (uint8_t *)heap_caps_malloc(s_rlen, MALLOC_CAP_SPIRAM);
      if (!s_rbuf) {
        ESP_LOGW(TAG, "sin PSRAM para leer %u B: no se restaura", (unsigned)s_rlen);
        freeRestore();
        s_phase = Phase::Idle;
        s_restoreDone = true;
        return;
      }
      s_restoreStartMs = nowMs;
      s_restoreSteps = 0;
      return;
    }
    size_t n = s_rlen - s_roff;
    if (n > TREND_CHUNK_BYTES) n = TREND_CHUNK_BYTES;
    if (s_reader.read(s_rbuf + s_roff, n) != n) {
      ESP_LOGW(TAG, "no se pudo leer %s", kPath);
      freeRestore();
      s_phase = Phase::Idle;
      s_restoreDone = true;
      return;
    }
    s_roff += n;
    if (s_roff >= s_rlen) {
      s_reader.close();
      startJnlRead(m, monoMs);
    }
    return;
  }

  if (s_phase == Phase::JnlRead) {
    stepJnlRead(m, monoMs);
    return;
  }
  if (s_phase == Phase::Merge) {
    s_restoreSteps++;
    stepMerge(m, monoMs);
    return;
  }

  // RestoreRun: un tramo de CRC o de escritura al anillo.
  s_restoreSteps++;
  if (trend_restore_step(&s_rjob) != TREND_JOB_MORE) restoreFinished(m, nowMs, changed);
}

// ---------------------------------------------------------------------- volcado

void startDump(TrendModel *m, uint32_t nowMs) {
  // Sin FS (formateo diferido pendiente) no es un fallo: se reintenta en la
  // pasada siguiente sin penalizar el periodo.
  if (!HmiFs_Begin()) return;
  const size_t cap = trend_file_max_bytes(m->store.capacity);
  if (!s_stage) {
    s_stage = (uint8_t *)heap_caps_malloc(cap, MALLOC_CAP_SPIRAM);
    if (!s_stage) {
      failDump(nowMs, "sin PSRAM para el volcado");
      return;
    }
    s_stageCap = cap;
  }
  if (!trend_ser_begin(&s_ser, &m->store, &m->events, s_stage, s_stageCap)) {
    failDump(nowMs, "nada que serializar");
    return;
  }
  s_serSteps = 0;
  s_serRestarts = 0;
  s_startMs = nowMs;
  s_lockRequest = false;
  s_phase = Phase::DumpSerialize;
}

void stepSerialize(TrendModel *m, uint32_t nowMs) {
  s_serSteps++;
  const TrendJobStatus st = trend_ser_step(&s_ser);
  if (st == TREND_JOB_MORE) return;
  if (st == TREND_JOB_RESTART) {
    // Entro una muestra (o la hora corrigio el eje) entre tramos.
    if (++s_serRestarts > MAX_SER_RESTARTS ||
        !trend_ser_begin(&s_ser, &m->store, &m->events, s_stage, s_stageCap))
      failDump(nowMs, "el anillo cambia demasiado deprisa");
    return;
  }
  if (st != TREND_JOB_DONE) {
    failDump(nowMs, "serializador");
    return;
  }
  s_stageLen = trend_ser_size(&s_ser);
  s_stageNewest = s_ser.last;
  s_stageShifts = m->clockShifts;  // etiquetas de ranura de esta instantanea
  if (!s_writer.begin(kPath)) {
    failDump(nowMs, "no se pudo abrir el temporal");
    return;
  }
  s_stageOff = 0;
  s_lastChunkMs = nowMs - TREND_CHUNK_GAP_MS;
  s_phase = Phase::DumpWrite;  // la escritura empieza en la pasada siguiente
}

void stepWrite(uint32_t nowMs) {
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
  s_phase = Phase::Idle;
  trend_dump_done(&s_dump, nowMs, s_stageNewest);
  // La instantanea nueva ya lo contiene todo: el diario sobra. Si el borrado
  // falla (o un corte cae justo antes), sus registros son anteriores a la
  // instantanea y la restauracion los ignora (trend_jnl_apply_*).
  if (HmiFs_FileSize(kJnlPath) >= 0 && !HmiFs_Remove(kJnlPath))
    ESP_LOGW(TAG, "no se pudo borrar %s", kJnlPath);
  s_diskInSync = true;
  s_shiftsAtSync = s_stageShifts;
  s_jnlRecords = 0;
  ESP_LOGI(TAG, "volcado %u B en %u ms (%d tramos de serializacion, %u de escritura)",
           (unsigned)total, (unsigned)(nowMs - s_startMs), s_serSteps,
           (unsigned)((total + TREND_CHUNK_BYTES - 1) / TREND_CHUNK_BYTES));
}

// Anadido al diario en una sola escritura de < 4 KB: lo normal cada 5 min. Si
// no se puede (sin sintonia con el disco, hora re-etiquetada, diario lleno o
// demasiadas ranuras pendientes) devuelve false y toca instantanea completa.
bool tryAppendJournal(TrendModel *m, uint32_t nowMs) {
  if (!s_diskInSync || !s_dump.haveDumped || m->clockShifts != s_shiftsAtSync) return false;
  const int n = trend_jnl_pending_records(&m->store, &m->events, s_dump.lastDumpedSlot);
  if (n <= 0 || n > TREND_JNL_APPEND_MAX_RECORDS ||
      s_jnlRecords + n > TREND_JNL_MAX_RECORDS)
    return false;
  if (!HmiFs_Begin()) return true;  // sin FS: se reintenta en la pasada siguiente
  const size_t cap = (size_t)n * TREND_JNL_REC_BYTES;
  uint8_t *buf = (uint8_t *)heap_caps_malloc(cap, MALLOC_CAP_SPIRAM);
  if (!buf) return false;
  const size_t len =
      trend_jnl_build(&m->store, &m->events, s_dump.lastDumpedSlot, buf, cap);
  const int64_t newest = m->store.lastSlot;
  const int64_t t0 = esp_timer_get_time();
  const bool ok = len == cap && HmiFs_Append(kJnlPath, buf, len);
  heap_caps_free(buf);
  s_lockRequest = false;
  if (!ok) {
    // El diario puede haber quedado con una cola a medias: que lo sustituya
    // una instantanea en el siguiente intento.
    s_diskInSync = false;
    failDump(nowMs, "anadido al diario");
    return true;
  }
  trend_dump_done(&s_dump, nowMs, newest);
  s_jnlRecords += n;
  ESP_LOGI(TAG, "diario +%d registros (%u B, %d en total) en %u ms", n, (unsigned)len,
           s_jnlRecords, (unsigned)((esp_timer_get_time() - t0) / 1000));
  return true;
}

void abortDump() {
  s_writer.abort();  // descarta el .tmp
  freeStage();
  s_phase = Phase::Idle;
  ESP_LOGI(TAG, "volcado abortado: alarma activa o enlace caido; se reintenta despues");
}

}  // namespace

bool TrendPersist_Service(TrendModel *m, int64_t monoMs, uint32_t nowMs, bool ioAllowed) {
  if (!m || !m->store.data) return false;
  if (!s_dumpInit) {
    trend_dump_init(&s_dump, nowMs);
    s_dumpInit = true;
  }

  // El formateo diferido de la particion lo reparte hmi_fs con la misma regla.
  HmiFs_Service(ioAllowed, nowMs);

  // Sin hora las ranuras son monotonas: ni se restaura ni se vuelca.
  if (!m->clock.synced) return false;

  // A3: reintento unico de un fichero rechazado, cuando el reloj se corrige.
  if (s_phase == Phase::Idle && s_rejected && !s_retried &&
      m->clockShifts != s_shiftsAtReject) {
    s_retried = true;
    s_restoreDone = false;
  }

  if (!ioAllowed) {
    // Un volcado a medias se aborta limpio; una restauracion se pausa donde esta.
    if (s_phase == Phase::DumpSerialize || s_phase == Phase::DumpWrite) abortDump();
    return false;
  }

  bool changed = false;
  switch (s_phase) {
    case Phase::Idle:
      if (!s_restoreDone) {
        startRestore();
      } else if (!trend_dump_blocked_by_rejected_file(s_rejected, s_rejectedAtMs, nowMs) &&
                 trend_dump_due(&s_dump, true, m->store.count > 0,
                                trend_model_newest_slot(m), nowMs, s_lockRequest)) {
        if (!tryAppendJournal(m, nowMs)) startDump(m, nowMs);
      }
      break;
    case Phase::RestoreRead:
    case Phase::JnlRead:
    case Phase::Merge:
    case Phase::RestoreRun:
      stepRestore(m, monoMs, nowMs, &changed);
      break;
    case Phase::DumpSerialize:
      stepSerialize(m, nowMs);
      break;
    case Phase::DumpWrite:
      stepWrite(nowMs);
      break;
  }
  return changed;
}

void TrendPersist_RequestDumpOnLock(void) { s_lockRequest = true; }
