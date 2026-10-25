#include "modules/trend/trend_persist.h"

#include "esp_heap_caps.h"
#include "esp_log.h"

#include "modules/storage/hmi_fs.h"
#include "modules/trend/trend_file.h"
#include "modules/trend/trend_policy.h"

// Maquina de estados. TODO el trabajo se reparte en pasadas acotadas (una
// llamada a TrendPersist_Service hace UN tramo) y nada empieza ni continua si
// hay una alarma activa o el enlace con la placa esta caido (ioAllowed):
//
//   restauracion:  Idle -> RestoreRead (abre + reserva) -> RestoreRead*
//                  (4 KB por pasada) -> RestoreRun* (CRC y escritura por tramos)
//   volcado:       Idle -> DumpSerialize* (series y CRC por tramos) ->
//                  DumpWrite* (4 KB por pasada, separados >= 50 ms) -> Idle
//
// Con ioAllowed = false: una restauracion se PAUSA donde esta; un volcado se
// ABORTA limpio (el .tmp se descarta) y se reintenta cuando toque.
namespace {

const char *TAG = "TrendPersist";
const char *kPath = "/trend.bin";

enum class Phase : uint8_t {
  Idle,
  RestoreRead,
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

constexpr int MAX_SER_RESTARTS = 5;

void freeStage() {
  if (s_stage) heap_caps_free(s_stage);
  s_stage = nullptr;
  s_stageLen = 0;
  s_stageOff = 0;
}

void freeRestore() {
  s_reader.close();
  if (s_rbuf) heap_caps_free(s_rbuf);
  s_rbuf = nullptr;
  s_rlen = 0;
  s_roff = 0;
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
      trend_restore_begin(&s_rjob, s_rbuf, s_rlen,
                          trend_clock_slot(&m->clock, monoMs), &m->store, &m->events);
      s_phase = Phase::RestoreRun;
    }
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
  ESP_LOGI(TAG, "volcado %u B en %u ms (%d tramos de serializacion, %u de escritura)",
           (unsigned)total, (unsigned)(nowMs - s_startMs), s_serSteps,
           (unsigned)((total + TREND_CHUNK_BYTES - 1) / TREND_CHUNK_BYTES));
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
        startDump(m, nowMs);
      }
      break;
    case Phase::RestoreRead:
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
