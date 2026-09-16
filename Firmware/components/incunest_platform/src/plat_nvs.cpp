#include "platform/plat_nvs.h"

#include <vector>

#include "esp_log.h"

static const char *TAG = "plat_nvs";

void plat_nvs_init(void) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TAG, "NVS ilegible (%s): se borra y se reinicia",
             esp_err_to_name(err));
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
}

NvsPrefs::~NvsPrefs() { end(); }

bool NvsPrefs::begin(const char *name, bool readOnly) {
  if (started_) {
    return false;
  }
  read_only_ = readOnly;
  esp_err_t err =
      nvs_open(name, readOnly ? NVS_READONLY : NVS_READWRITE, &handle_);
  if (err != ESP_OK) {
    // A nivel DEBUG a proposito: abrir en solo lectura un namespace que aun no
    // existe es normal en el primer arranque, y con ESP_LOGE llenaria el log
    // de ruido en cada placa nueva.
    ESP_LOGD(TAG, "nvs_open('%s') -> %s", name, esp_err_to_name(err));
    return false;
  }
  started_ = true;
  return true;
}

void NvsPrefs::end() {
  if (!started_) {
    return;
  }
  nvs_close(handle_);
  handle_ = 0;
  started_ = false;
}

bool NvsPrefs::clear() {
  if (!started_ || read_only_) {
    return false;
  }
  return nvs_erase_all(handle_) == ESP_OK && nvs_commit(handle_) == ESP_OK;
}

bool NvsPrefs::remove(const char *key) {
  if (!started_ || key == nullptr || read_only_) {
    return false;
  }
  if (nvs_erase_key(handle_, key) != ESP_OK) {
    return false;
  }
  return nvs_commit(handle_) == ESP_OK;
}

bool NvsPrefs::isKey(const char *key) {
  if (!started_ || key == nullptr) {
    return false;
  }
  // Arduino resolvia esto con getType(), que prueba tipo por tipo hasta que
  // uno acierta. Se hace igual: NVS no ofrece un "existe" independiente del
  // tipo, y probar en este orden reproduce su resultado.
  size_t len = 0;
  int8_t i8;
  uint8_t u8;
  int16_t i16;
  uint16_t u16;
  int32_t i32;
  uint32_t u32;
  int64_t i64;
  uint64_t u64;
  return nvs_get_i8(handle_, key, &i8) == ESP_OK ||
         nvs_get_u8(handle_, key, &u8) == ESP_OK ||
         nvs_get_i16(handle_, key, &i16) == ESP_OK ||
         nvs_get_u16(handle_, key, &u16) == ESP_OK ||
         nvs_get_i32(handle_, key, &i32) == ESP_OK ||
         nvs_get_u32(handle_, key, &u32) == ESP_OK ||
         nvs_get_i64(handle_, key, &i64) == ESP_OK ||
         nvs_get_u64(handle_, key, &u64) == ESP_OK ||
         nvs_get_str(handle_, key, nullptr, &len) == ESP_OK ||
         nvs_get_blob(handle_, key, nullptr, &len) == ESP_OK;
}

// Todas las escrituras siguen el mismo molde que Arduino: set, comprobar,
// commit, comprobar, devolver 1 si todo fue bien y 0 si no.
#define PLAT_NVS_PUT(setter, value_expr)                                       \
  do {                                                                         \
    if (!started_ || key == nullptr || read_only_) {                           \
      return 0;                                                                \
    }                                                                          \
    if (setter(handle_, key, value_expr) != ESP_OK) {                          \
      ESP_LOGE(TAG, #setter " fallo en '%s'", key);                            \
      return 0;                                                                \
    }                                                                          \
    if (nvs_commit(handle_) != ESP_OK) {                                       \
      ESP_LOGE(TAG, "nvs_commit fallo en '%s'", key);                          \
      return 0;                                                                \
    }                                                                          \
    return 1;                                                                  \
  } while (0)

size_t NvsPrefs::putChar(const char *key, int8_t v) {
  PLAT_NVS_PUT(nvs_set_i8, v);
}
size_t NvsPrefs::putUChar(const char *key, uint8_t v) {
  PLAT_NVS_PUT(nvs_set_u8, v);
}
size_t NvsPrefs::putShort(const char *key, int16_t v) {
  PLAT_NVS_PUT(nvs_set_i16, v);
}
size_t NvsPrefs::putUShort(const char *key, uint16_t v) {
  PLAT_NVS_PUT(nvs_set_u16, v);
}
size_t NvsPrefs::putInt(const char *key, int32_t v) {
  PLAT_NVS_PUT(nvs_set_i32, v);
}
size_t NvsPrefs::putUInt(const char *key, uint32_t v) {
  PLAT_NVS_PUT(nvs_set_u32, v);
}
size_t NvsPrefs::putLong64(const char *key, int64_t v) {
  PLAT_NVS_PUT(nvs_set_i64, v);
}
size_t NvsPrefs::putULong64(const char *key, uint64_t v) {
  PLAT_NVS_PUT(nvs_set_u64, v);
}
size_t NvsPrefs::putString(const char *key, const char *v) {
  if (v == nullptr) {
    return 0;
  }
  PLAT_NVS_PUT(nvs_set_str, v);
}

// Alias exactos de Arduino: Long era i32 y ULong era u32.
size_t NvsPrefs::putLong(const char *key, int32_t v) { return putInt(key, v); }
size_t NvsPrefs::putULong(const char *key, uint32_t v) {
  return putUInt(key, v);
}
size_t NvsPrefs::putBool(const char *key, bool v) {
  return putUChar(key, v ? 1 : 0);
}
size_t NvsPrefs::putString(const char *key, const String &v) {
  return putString(key, v.c_str());
}

size_t NvsPrefs::putString(const char *key, const std::string &v) {
  return putString(key, v.c_str());
}

// float y double van como BLOB. Ver la advertencia de la cabecera.
size_t NvsPrefs::putFloat(const char *key, float v) {
  return putBytes(key, &v, sizeof(float));
}
size_t NvsPrefs::putDouble(const char *key, double v) {
  return putBytes(key, &v, sizeof(double));
}

size_t NvsPrefs::putBytes(const char *key, const void *value, size_t len) {
  if (!started_ || key == nullptr || value == nullptr || read_only_) {
    return 0;
  }
  if (nvs_set_blob(handle_, key, value, len) != ESP_OK) {
    ESP_LOGE(TAG, "nvs_set_blob fallo en '%s'", key);
    return 0;
  }
  if (nvs_commit(handle_) != ESP_OK) {
    ESP_LOGE(TAG, "nvs_commit fallo en '%s'", key);
    return 0;
  }
  return len;
}

#define PLAT_NVS_GET(getter, type)                                             \
  do {                                                                         \
    type value = defaultValue;                                                 \
    if (!started_ || key == nullptr) {                                         \
      return value;                                                            \
    }                                                                          \
    getter(handle_, key, &value);                                              \
    return value;                                                              \
  } while (0)

int8_t NvsPrefs::getChar(const char *key, int8_t defaultValue) {
  PLAT_NVS_GET(nvs_get_i8, int8_t);
}
uint8_t NvsPrefs::getUChar(const char *key, uint8_t defaultValue) {
  PLAT_NVS_GET(nvs_get_u8, uint8_t);
}
int16_t NvsPrefs::getShort(const char *key, int16_t defaultValue) {
  PLAT_NVS_GET(nvs_get_i16, int16_t);
}
uint16_t NvsPrefs::getUShort(const char *key, uint16_t defaultValue) {
  PLAT_NVS_GET(nvs_get_u16, uint16_t);
}
int32_t NvsPrefs::getInt(const char *key, int32_t defaultValue) {
  PLAT_NVS_GET(nvs_get_i32, int32_t);
}
uint32_t NvsPrefs::getUInt(const char *key, uint32_t defaultValue) {
  PLAT_NVS_GET(nvs_get_u32, uint32_t);
}
int64_t NvsPrefs::getLong64(const char *key, int64_t defaultValue) {
  PLAT_NVS_GET(nvs_get_i64, int64_t);
}
uint64_t NvsPrefs::getULong64(const char *key, uint64_t defaultValue) {
  PLAT_NVS_GET(nvs_get_u64, uint64_t);
}

int32_t NvsPrefs::getLong(const char *key, int32_t defaultValue) {
  return getInt(key, defaultValue);
}
uint32_t NvsPrefs::getULong(const char *key, uint32_t defaultValue) {
  return getUInt(key, defaultValue);
}
bool NvsPrefs::getBool(const char *key, bool defaultValue) {
  return getUChar(key, defaultValue ? 1 : 0) == 1;
}

float NvsPrefs::getFloat(const char *key, float defaultValue) {
  float value = defaultValue;
  getBytes(key, &value, sizeof(float));
  return value;
}

double NvsPrefs::getDouble(const char *key, double defaultValue) {
  double value = defaultValue;
  getBytes(key, &value, sizeof(double));
  return value;
}

size_t NvsPrefs::getBytesLength(const char *key) {
  size_t len = 0;
  if (!started_ || key == nullptr) {
    return 0;
  }
  if (nvs_get_blob(handle_, key, nullptr, &len) != ESP_OK) {
    return 0;
  }
  return len;
}

size_t NvsPrefs::getBytes(const char *key, void *buf, size_t maxLen) {
  size_t len = getBytesLength(key);
  if (len == 0 || buf == nullptr || maxLen == 0) {
    return len;
  }
  if (len > maxLen) {
    ESP_LOGE(TAG, "'%s' no cabe: %u < %u", key, (unsigned)maxLen, (unsigned)len);
    return 0;
  }
  if (nvs_get_blob(handle_, key, buf, &len) != ESP_OK) {
    return 0;
  }
  return len;
}

size_t NvsPrefs::getString(const char *key, char *value, size_t maxLen) {
  size_t len = 0;
  if (!started_ || key == nullptr || value == nullptr || maxLen == 0) {
    return 0;
  }
  if (nvs_get_str(handle_, key, nullptr, &len) != ESP_OK) {
    return 0;
  }
  if (len > maxLen) {
    ESP_LOGE(TAG, "'%s' no cabe: %u < %u", key, (unsigned)maxLen, (unsigned)len);
    return 0;
  }
  if (nvs_get_str(handle_, key, value, &len) != ESP_OK) {
    return 0;
  }
  return len;
}

std::string NvsPrefs::getString(const char *key, const char *defaultValue) {
  size_t len = 0;
  if (!started_ || key == nullptr) {
    return std::string(defaultValue);
  }
  if (nvs_get_str(handle_, key, nullptr, &len) != ESP_OK || len == 0) {
    return std::string(defaultValue);
  }
  // Arduino reservaba aqui un VLA en la pila (char buf[len]) y ademas leia
  // sobre un puntero que acababa de apuntar a esa pila; con una cadena larga
  // eso es un desbordamiento de pila esperando su turno. Aqui va al heap con
  // un tamano conocido.
  std::vector<char> buf(len);
  if (nvs_get_str(handle_, key, buf.data(), &len) != ESP_OK) {
    return std::string(defaultValue);
  }
  return std::string(buf.data());
}
