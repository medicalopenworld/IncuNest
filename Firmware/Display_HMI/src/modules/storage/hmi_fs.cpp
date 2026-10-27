#include "modules/storage/hmi_fs.h"

#include <LittleFS.h>
#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_timer.h"

#include "modules/storage/hmi_fs_format.h"
#include "drivers/lcd_recover.h"

namespace {

const char *TAG = "HmiFs";

// Etiqueta de la particion (partitions/hmi_16mb_ota.csv: "spiffs"). El subtipo
// es spiffs pero LittleFS la monta por etiqueta.
const char *kPartitionLabel = "spiffs";
const char *kBasePath = "/littlefs";
// Cada fichero abierto cuesta buffers de cache en heap interno: pocos.
constexpr uint8_t kMaxOpenFiles = 4;

bool s_mounted = false;
HmiFsState s_state = HmiFsState::Unmounted;

// Formateo diferido en curso.
const esp_partition_t *s_part = nullptr;
HmiFsErasePlan s_plan;
bool s_planStarted = false;
uint32_t s_lastStepMs = 0;
int64_t s_formatT0Us = 0;
int64_t s_eraseUs = 0;  // suma de lo que han tardado los borrados

size_t internalFree() { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL); }

// Monta SIN formatear. true si ya hay una imagen valida.
bool tryMount() {
  const size_t heapBefore = internalFree();
  const int64_t t0 = esp_timer_get_time();
  if (!LittleFS.begin(false, kBasePath, kMaxOpenFiles, kPartitionLabel)) return false;
  s_mounted = true;
  s_state = HmiFsState::Mounted;
  const size_t heapAfter = internalFree();
  // Cifra para el banco: heap interno que se come el montaje.
  ESP_LOGI(TAG, "LittleFS montado en %lld ms; heap interno libre %u -> %u B (delta %d B)",
           (long long)((esp_timer_get_time() - t0) / 1000), (unsigned)heapBefore,
           (unsigned)heapAfter, (int)heapAfter - (int)heapBefore);
  return true;
}

void giveUp(const char *why) {
  s_state = HmiFsState::Failed;
  ESP_LOGE(TAG, "formateo diferido fallido (%s): sin persistencia", why);
}

}  // namespace

bool HmiFs_Begin(void) {
  if (s_mounted) return true;
  // NeedsFormat / Formatting / Failed: no se reintenta el montaje (cada intento
  // fallido escribe errores en el log del VFS) ni se formatea aqui.
  if (s_state != HmiFsState::Unmounted) return false;

  if (tryMount()) return true;
  s_state = HmiFsState::NeedsFormat;
  ESP_LOGW(TAG,
           "la particion '%s' no tiene una imagen LittleFS valida: formateo "
           "diferido (sin alarma activa y con el enlace vivo); mientras tanto "
           "sin persistencia",
           kPartitionLabel);
  return false;
}

HmiFsState HmiFs_State(void) { return s_state; }

void HmiFs_Service(bool ioAllowed, uint32_t nowMs) {
  if (s_state != HmiFsState::NeedsFormat && s_state != HmiFsState::Formatting) return;
  if (!ioAllowed) return;  // en pausa: ni empieza ni continua

  if (s_state == HmiFsState::NeedsFormat) {
    s_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY,
                                      kPartitionLabel);
    if (!s_part) {
      giveUp("no existe la particion");
      return;
    }
    hmifs_erase_plan_init(&s_plan, s_part->size);
    s_planStarted = true;
    s_lastStepMs = nowMs - HMIFS_ERASE_GAP_MS;  // el primer rango, ya
    s_formatT0Us = esp_timer_get_time();
    s_eraseUs = 0;
    s_state = HmiFsState::Formatting;
    ESP_LOGI(TAG, "formateo diferido: %u rangos de %u KB", (unsigned)hmifs_erase_plan_ranges(s_part->size),
             (unsigned)(HMIFS_ERASE_RANGE_BYTES / 1024u));
    return;
  }

  // Formatting: un rango por pasada, con separacion.
  if (!s_planStarted || !hmifs_erase_step_due(nowMs, s_lastStepMs)) return;
  uint32_t off = 0, len = 0;
  if (hmifs_erase_plan_next(&s_plan, &off, &len)) {
    s_lastStepMs = nowMs;
    const int64_t t0 = esp_timer_get_time();
    // Empieza por el offset 0 (donde viven los superblocks): un corte a mitad
    // deja la particion "virgen" y el siguiente arranque reinicia el formateo.
    if (esp_partition_erase_range(s_part, off, len) != ESP_OK) {
      giveUp("borrado de rango");
      return;
    }
    s_eraseUs += esp_timer_get_time() - t0;
    LCD_RequestRestart();
    return;
  }

  // Particion borrada: format() solo escribe los superblocks, y un montaje.
  const int64_t tf = esp_timer_get_time();
  if (!LittleFS.format()) {
    giveUp("format()");
    return;
  }
  const int64_t formatUs = esp_timer_get_time() - tf;
  LCD_RequestRestart();
  if (!tryMount()) {
    giveUp("montaje tras el formateo");
    return;
  }
  ESP_LOGI(TAG, "formateo diferido terminado en %lld ms (borrado %lld ms, format() %lld ms)",
           (long long)((esp_timer_get_time() - s_formatT0Us) / 1000),
           (long long)(s_eraseUs / 1000), (long long)(formatUs / 1000));
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
  // exists() antes de open(): abrir un fichero inexistente en modo "r" escribe
  // un error en el log del VFS, y la primera vez es lo normal.
  if (!LittleFS.exists(path)) return -1;
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
  const bool ok = LittleFS.remove(path);
  LCD_RequestRestart();
  return ok;
}

int32_t HmiFsReader::open(const char *path) {
  close();
  if (!s_mounted || !path || !LittleFS.exists(path)) return -1;
  file_ = LittleFS.open(path, "r");
  if (!file_ || file_.isDirectory()) {
    file_.close();
    return -1;
  }
  open_ = true;
  return (int32_t)file_.size();
}

size_t HmiFsReader::read(uint8_t *buf, size_t n) {
  if (!open_ || !buf) return 0;
  return file_.read(buf, n);
}

void HmiFsReader::close(void) {
  if (open_) {
    file_.close();
    open_ = false;
  }
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
  const size_t w = file_.write(data, n);
  LCD_RequestRestart();
  return w;
}

bool HmiFsAtomicWriter::commit(void) {
  if (!open_) return false;
  file_.close();  // vuelca y cierra
  LCD_RequestRestart();  // el cierre y el renombrado escriben flash
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

bool HmiFs_Append(const char *path, const uint8_t *data, size_t len) {
  if (!s_mounted || !path || !data) return false;
  fs::File f = LittleFS.open(path, "a");
  if (!f) {
    ESP_LOGE(TAG, "no se pudo abrir %s para anadir", path);
    return false;
  }
  const size_t w = f.write(data, len);
  f.close();  // aqui se hace visible (y se escribe la metadata)
  LCD_RequestRestart();
  return w == len;
}
