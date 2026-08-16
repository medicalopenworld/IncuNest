#include "PpgSnapshotPublish.h"

#include "PpgSnapshot.h"
#include "config/telemetry_keys.h"

#include <time.h>

bool ppgSnapshotPublish(ThingsBoard &client, const char *tag) {
  if (!ppgSnapshotTryAcquire())
    return false;

  // A partir de aquí el snapshot es nuestro: hay que soltarlo por todas las
  // salidas, o el slot queda bloqueado y no se vuelve a capturar nunca.
  bool published = false;

  time_t nowSec = 0;
  time(&nowSec);
  if (nowSec < 1609459200L) { // antes de 2021-01-01: reloj sin sincronizar
    logI(String("[") + tag + "] -> PPG snapshot descartado: reloj sin hora");
    ppgSnapshotRelease();
    return false;
  }

  uint16_t const n = ppgSnapshotSampleCount();
  int32_t const *samples = ppgSnapshotSamples();
  if (!n || samples == nullptr) {
    ppgSnapshotRelease();
    return false;
  }

  uint32_t const stepMs = 1000UL / PPG_SNAPSHOT_FS_HZ;
  // La última muestra del array es "ahora"; el resto retrocede en pasos de
  // stepMs — el orden temporal real de la captura.
  uint64_t const lastMs = (uint64_t)nowSec * 1000ULL;

  {
    DynamicJsonDocument seriesDoc(
        JSON_ARRAY_SIZE(n) + n * (JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(1)));
    // ~22 KB para 400 muestras. Si el heap está justo, la reserva falla y el
    // documento queda nulo: publicarlo mandaría basura a la nube.
    if (seriesDoc.capacity() == 0) {
      logI(String("[") + tag + "] -> PPG snapshot FAIL: sin heap para el JSON");
      ppgSnapshotRelease();
      return false;
    }

    JsonArray series = seriesDoc.to<JsonArray>();
    for (uint16_t i = 0; i < n; i++) {
      JsonObject point = series.createNestedObject();
      point["ts"] = lastMs - (uint64_t)(n - 1 - i) * stepMs;
      point.createNestedObject("values")[PPG_SNAPSHOT_KEY] = samples[i];
    }

    bool const seriesOk =
        client.sendTelemetryJson(series, JSON_STRING_SIZE(measureJson(series)));

    StaticJsonDocument<JSON_OBJECT_SIZE(2)> metaDoc;
    JsonObject metaObj = metaDoc.to<JsonObject>();
    metaObj[PPG_SNAPSHOT_FS_KEY] = PPG_SNAPSHOT_FS_HZ;
    metaObj[PPG_SNAPSHOT_N_KEY] = n;
    bool const metaOk = client.sendTelemetryJson(
        metaObj, JSON_STRING_SIZE(measureJson(metaObj)));

    published = seriesOk && metaOk;
    logI(String("[") + tag + "] -> PPG snapshot " +
         (published ? "PUBLISH SUCCESS" : "PUBLISH FAIL") + " (" + n +
         " muestras, " + measureJson(series) + " B)");
  }

  ppgSnapshotRelease();
  return published;
}
