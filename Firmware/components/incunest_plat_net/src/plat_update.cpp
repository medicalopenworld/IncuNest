#include "platform/plat_update.h"

#include "esp_log.h"

static const char *TAG = "plat_update";

UpdateClass Update;

bool UpdateClass::begin(size_t size) {
  if (running_) {
    abort();
  }
  error_ = 0;
  finished_ = false;
  written_ = 0;
  part_ = esp_ota_get_next_update_partition(nullptr);
  if (part_ == nullptr) {
    error_ = ESP_ERR_NOT_FOUND;
    ESP_LOGE(TAG, "no hay particion OTA libre");
    return false;
  }
  const size_t ota_size = (size == UPDATE_SIZE_UNKNOWN) ? OTA_WITH_SEQUENTIAL_WRITES : size;
  const esp_err_t err = esp_ota_begin(part_, ota_size, &handle_);
  if (err != ESP_OK) {
    error_ = err;
    ESP_LOGE(TAG, "esp_ota_begin -> %s", esp_err_to_name(err));
    return false;
  }
  running_ = true;
  return true;
}

size_t UpdateClass::write(const uint8_t *data, size_t len) {
  if (!running_ || data == nullptr) {
    return 0;
  }
  const esp_err_t err = esp_ota_write(handle_, data, len);
  if (err != ESP_OK) {
    error_ = err;
    ESP_LOGE(TAG, "esp_ota_write -> %s", esp_err_to_name(err));
    return 0;
  }
  written_ += len;
  return len;
}

bool UpdateClass::end(bool evenIfRemaining) {
  (void)evenIfRemaining;
  if (!running_) {
    return false;
  }
  running_ = false;
  esp_err_t err = esp_ota_end(handle_);
  if (err != ESP_OK) {
    error_ = err;
    ESP_LOGE(TAG, "esp_ota_end -> %s", esp_err_to_name(err));
    return false;
  }
  err = esp_ota_set_boot_partition(part_);
  if (err != ESP_OK) {
    error_ = err;
    ESP_LOGE(TAG, "esp_ota_set_boot_partition -> %s", esp_err_to_name(err));
    return false;
  }
  finished_ = true;
  return true;
}

void UpdateClass::abort() {
  if (running_) {
    esp_ota_abort(handle_);
    running_ = false;
  }
  finished_ = false;
}

const char *UpdateClass::errorString() const {
  return error_ == 0 ? "No Error" : esp_err_to_name(error_);
}

void UpdateClass::printError(Print &out) { out.println(errorString()); }
