#include "PpgSnapshotPublish.h"

#include "PpgSnapshot.h"
#include "config/telemetry_keys.h"
#include "modules/util/ppg_snapshot_plan.h"

#include <atomic>
#include <time.h>

namespace {

// Transporte que tiene el snapshot reclamado, o nullptr. Lo lee la otra
// tarea para saber que no le toca.
std::atomic<ThingsBoard *> s_client{nullptr};
// true mientras el dueño está montando o mandando un trozo: la otra tarea no
// puede soltar el snapshot por timeout justo entonces, o una captura nueva
// podría escribir en el buffer que se está serializando.
std::atomic<bool> s_inChunk{false};

// Solo los toca el dueño.
uint16_t s_next = 0;         // primera muestra aún sin mandar
uint64_t s_lastMs = 0;       // ts de la última muestra, fijado al empezar
uint32_t s_startMs = 0;      // millis() al reclamar
uint32_t s_lastChunkMs = 0;  // millis() del último trozo mandado
uint32_t s_bytes = 0;        // JSON mandado, para el log
uint16_t s_failures = 0;     // trozos fallidos (reintentados)

// Suelta el snapshot. Solo lo hace quien gane el compare_exchange, así que no
// hay doble release aunque las dos tareas lleguen a la vez.
bool finish(ThingsBoard *owner, const char *tag, const char *outcome) {
  if (!s_client.compare_exchange_strong(owner, nullptr))
    return false;
  logI(String("[") + tag + "] -> PPG snapshot " + outcome + " (" + s_next +
       "/" + ppgSnapshotSampleCount() + " muestras, " + s_bytes + " B, " +
       (millis() - s_startMs) + " ms, " + s_failures + " trozos reintentados)");
  ppgSnapshotRelease();
  return true;
}

bool begin(ThingsBoard &client, const char *tag) {
  if (!ppgSnapshotTryAcquire())
    return false;

  // A partir de aquí el snapshot es nuestro: hay que soltarlo por todas las
  // salidas, o el slot queda bloqueado y no se vuelve a capturar nunca.
  time_t nowSec = 0;
  time(&nowSec);
  if (nowSec < 1609459200L) { // antes de 2021-01-01: reloj sin sincronizar
    logI(String("[") + tag + "] -> PPG snapshot descartado: reloj sin hora");
    ppgSnapshotRelease();
    return false;
  }
  if (!ppgSnapshotSampleCount() || ppgSnapshotSamples() == nullptr) {
    ppgSnapshotRelease();
    return false;
  }

  s_next = 0;
  // La última muestra del array es "ahora": se fija una vez, no al mandar
  // cada trozo, para que la onda quede continua aunque los trozos salgan en
  // momentos distintos.
  s_lastMs = (uint64_t)nowSec * 1000ULL;
  s_startMs = millis();
  s_lastChunkMs = 0;
  s_bytes = 0;
  s_failures = 0;
  s_client.store(&client);
  return true;
}

// Si el trozo no cabe en el buffer del cliente MQTT, la librería vuelve al
// streaming de 64 B en 64 B, que es justo lo que se atascaba.
static_assert(PPG_SNAPSHOT_CHUNK_JSON_BUDGET + 64 <= MAX_MESSAGE_SIZE,
              "un trozo PPG tiene que caber en el buffer MQTT");

bool sendChunk(ThingsBoard &client) {
  DynamicJsonDocument doc(PPG_SNAPSHOT_CHUNK_DOC_CAPACITY);
  // Si ni esto cabe en el heap, el documento queda nulo: publicarlo mandaría
  // basura a la nube. Se reintenta en la siguiente llamada.
  if (doc.capacity() == 0)
    return false;

  JsonArray series = doc.to<JsonArray>();
  uint16_t const end =
      ppg_chunk_to_json(series, ppgSnapshotSamples(), s_next,
                        ppgSnapshotSampleCount(), s_lastMs,
                        1000UL / PPG_SNAPSHOT_FS_HZ);

  size_t const bytes = measureJson(series);
  if (!client.sendTelemetryJson(series, JSON_STRING_SIZE(bytes)))
    return false;
  s_next = end;
  s_bytes += bytes;
  return true;
}

bool sendMeta(ThingsBoard &client) {
  StaticJsonDocument<JSON_OBJECT_SIZE(2)> metaDoc;
  JsonObject metaObj = metaDoc.to<JsonObject>();
  metaObj[PPG_SNAPSHOT_FS_KEY] = PPG_SNAPSHOT_FS_HZ;
  metaObj[PPG_SNAPSHOT_N_KEY] = ppgSnapshotSampleCount();
  return client.sendTelemetryJson(metaObj,
                                  JSON_STRING_SIZE(measureJson(metaObj)));
}

} // namespace

bool ppgSnapshotPublish(ThingsBoard &client, const char *tag, bool burst) {
  ThingsBoard *owner = s_client.load();

  if (owner != nullptr && !s_inChunk.load() &&
      ppg_elapsed(millis(), s_startMs, PPG_SNAPSHOT_PUBLISH_TIMEOUT_MS)) {
    finish(owner, tag, "ABANDONADO por timeout");
    owner = nullptr;
  }

  if (owner == nullptr) {
    if (!begin(client, tag))
      return false;
  } else if (owner != &client) {
    return false; // lo está mandando el otro transporte
  }

  uint16_t const n = ppgSnapshotSampleCount();
  while (s_next < n) {
    if (!burst && s_lastChunkMs != 0 &&
        !ppg_elapsed(millis(), s_lastChunkMs, PPG_SNAPSHOT_CHUNK_GAP_MS))
      return false;

    s_inChunk.store(true);
    bool const ok = sendChunk(client);
    s_inChunk.store(false);
    s_lastChunkMs = millis();
    if (s_lastChunkMs == 0)
      s_lastChunkMs = 1; // 0 significa "aún no se ha mandado ninguno"

    if (!ok) {
      s_failures++;
      return false; // se reintenta el mismo trozo en la siguiente llamada
    }
    if (!burst)
      return false; // un trozo por vuelta; el siguiente, en la próxima
  }

  if (!sendMeta(client)) {
    s_failures++;
    return false;
  }
  return finish(&client, tag, "PUBLISH SUCCESS");
}
