#include "modules/storage/hmi_fs.h"

#include <LittleFS.h>
#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

namespace {

const char *TAG = "HmiFs";

// Etiqueta de la particion (partitions/hmi_16mb_ota.csv: "spiffs"). El subtipo
// es spiffs pero LittleFS la monta por etiqueta.
const char *kPartitionLabel = "spiffs";
const char *kBasePath = "/littlefs";
// Cada fichero abierto cuesta buffers de cache en heap interno: pocos.
constexpr uint8_t kMaxOpenFiles = 4;

bool s_mounted = false;
bool s_gaveUp = false;

size_t internalFree() { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL); }

}  // namespace

bool HmiFs_Begin(void) {
  if (s_mounted) return true;
  if (s_gaveUp) return false;

  const size_t heapBefore = internalFree();
  const int64_t t0 = esp_timer_get_time();
  // formatOnFail = true: si la particion es virgen o esta corrupta, esp_littlefs
  // la formatea y monta UNA vez. Si eso tambien falla, no se reintenta.
  if (!LittleFS.begin(true, kBasePath, kMaxOpenFiles, kPartitionLabel)) {
    s_gaveUp = true;
    ESP_LOGE(TAG, "no se pudo montar ni formatear la particion '%s' — sin persistencia",
             kPartitionLabel);
    return false;
  }
  s_mounted = true;
  const size_t heapAfter = internalFree();
  // Cifra para el banco: heap interno que se come el montaje.
  ESP_LOGW(TAG, "LittleFS montado en %lld ms; heap interno libre %u -> %u B (delta %d B)",
           (long long)((esp_timer_get_time() - t0) / 1000), (unsigned)heapBefore,
           (unsigned)heapAfter, (int)heapAfter - (int)heapBefore);
  return true;
}

bool HmiFs_IsMounted(void) { return s_mounted; }

bool HmiFs_Info(size_t *total, size_t *used) {
  if (!s_mounted) return false;
  if (total) *total = LittleFS.totalBytes();
  if (used) *used = LittleFS.usedBytes();
  return true;
}

int32_t HmiFs_FileSize(const char *path) {
  if (!s_mounted || !path) return -1;
  fs::File f = LittleFS.open(path, "r");
  if (!f || f.isDirectory()) return -1;
  const int32_t sz = (int32_t)f.size();
  f.close();
  return sz;
}

bool HmiFs_ReadAll(const char *path, uint8_t *buf, size_t cap, size_t *outLen) {
  if (outLen) *outLen = 0;
  if (!s_mounted || !path || !buf) return false;
  fs::File f = LittleFS.open(path, "r");
  if (!f || f.isDirectory()) return false;
  const size_t sz = f.size();
  if (sz > cap) {
    f.close();
    return false;
  }
  const size_t got = f.read(buf, sz);
  f.close();
  if (got != sz) return false;
  if (outLen) *outLen = got;
  return true;
}

bool HmiFs_Remove(const char *path) {
  if (!s_mounted || !path) return false;
  return LittleFS.remove(path);
}

bool HmiFsAtomicWriter::begin(const char *path) {
  abort();
  if (!s_mounted || !path) return false;
  const size_t n = strlen(path);
  if (n == 0 || n >= kPathMax) return false;
  memcpy(path_, path, n + 1);
  snprintf(tmp_, sizeof(tmp_), "%s.tmp", path_);
  file_ = LittleFS.open(tmp_, "w");  // "w" trunca un .tmp huerfano de un corte
  if (!file_) {
    ESP_LOGE(TAG, "no se pudo abrir %s", tmp_);
    return false;
  }
  open_ = true;
  return true;
}

size_t HmiFsAtomicWriter::write(const uint8_t *data, size_t n) {
  if (!open_ || !data) return 0;
  return file_.write(data, n);
}

bool HmiFsAtomicWriter::commit(void) {
  if (!open_) return false;
  file_.close();  // vuelca y cierra
  open_ = false;
  // lfs_rename sobre un destino existente lo sustituye de forma atomica.
  if (LittleFS.rename(tmp_, path_)) return true;
  // Algunas versiones del VFS no sobrescriben: se borra el destino y se repite.
  // Hay una ventana minima sin fichero; el temporal sigue integro y se
  // renombrara en el siguiente intento si un corte cae justo aqui.
  ESP_LOGW(TAG, "rename %s -> %s fallo; reintento tras borrar el destino", tmp_, path_);
  LittleFS.remove(path_);
  if (LittleFS.rename(tmp_, path_)) return true;
  ESP_LOGE(TAG, "rename %s -> %s fallo", tmp_, path_);
  return false;
}

void HmiFsAtomicWriter::abort(void) {
  if (open_) {
    file_.close();
    open_ = false;
    if (s_mounted) LittleFS.remove(tmp_);
  }
}

bool HmiFs_WriteAtomic(const char *path, const uint8_t *data, size_t len) {
  HmiFsAtomicWriter w;
  if (!w.begin(path)) return false;
  if (len > 0 && w.write(data, len) != len) {
    w.abort();
    return false;
  }
  return w.commit();
}
