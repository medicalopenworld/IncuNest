#include "PpgSnapshot.h"

namespace {

int32_t  s_buf[PPG_SNAPSHOT_SAMPLES];
uint16_t s_count      = 0;     // muestras (ya diezmadas) recogidas
uint16_t s_decimCount = 0;     // muestras crudas a 500 Hz desde la última guardada
bool     s_capturing  = false;
bool     s_ready      = false;
uint32_t s_startMs    = 0;

bool signalGateOk(bool probeApplied, uint8_t rsqi) {
  return probeApplied && rsqi == 1;
}

} // namespace

void ppgSnapshotFeed(const AFE4490Data &data, uint32_t now_ms) {
  if (!s_capturing)
    return;

  if (now_ms - s_startMs > PPG_SNAPSHOT_TIMEOUT_MS) {
    // Captura estancada (p. ej. el AFE dejó de entregar muestras): abortar y
    // liberar el slot para que el siguiente intento (auto o RPC) pueda
    // reintentar en vez de quedar bloqueado para siempre.
    s_capturing = false;
    return;
  }

  if (++s_decimCount < PPG_SNAPSHOT_DECIM)
    return;
  s_decimCount = 0;

  s_buf[s_count++] = data.ppg_disp;
  if (s_count >= PPG_SNAPSHOT_SAMPLES) {
    s_capturing = false;
    s_ready     = true;
  }
}

PpgSnapshotStatus ppgSnapshotRequestCapture(bool probeApplied, uint8_t rsqi,
                                            uint32_t now_ms) {
  if (s_capturing) {
    if (now_ms - s_startMs <= PPG_SNAPSHOT_TIMEOUT_MS)
      return PpgSnapshotStatus::BUSY;
    s_capturing = false; // captura obsoleta/estancada: reintentar abajo
  }

  if (!signalGateOk(probeApplied, rsqi))
    return PpgSnapshotStatus::SIGNAL_NOT_READY;

  s_count      = 0;
  s_decimCount = 0;
  s_ready      = false;
  s_startMs    = now_ms;
  s_capturing  = true;
  return PpgSnapshotStatus::STARTED;
}

bool ppgSnapshotIsReady() { return s_ready; }

uint16_t ppgSnapshotSampleCount() { return s_count; }

const int32_t *ppgSnapshotSamples() { return s_buf; }

void ppgSnapshotClear() {
  s_ready = false;
  s_count = 0;
}
