/**
 * @file nvs_storage.c
 * @brief Persistent key-value storage for IncuNest configuration via ESP-IDF NVS.
 *
 * @details Replaces Arduino Preferences + EEPROM from the original firmware.
 *          All keys use the "incunest" namespace in the NVS partition.
 *
 * Normativa aplicable:
 * - IEC 60601-2-19 §201.11.8 — temperature persistence after power loss
 *
 * @author IncuNest Team
 * @date   2026-04-28
 */

#include "nvs_storage.h"

// TODO: Fase 2 — Implement full NVS read/write for all config values
// Reference: 04_PLATFORMIO_DEPENDENCIES.md, EEPROM_defines.h key map

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "NVS";
static const char *NVS_NAMESPACE = "incunest";

esp_err_t nvs_storage_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition invalid — erasing and reinitializing");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "NVS initialized (namespace: %s)", NVS_NAMESPACE);
    }
    return ret;
}

float nvs_storage_get_air_temp(void)
{
    // TODO: Fase 2 — read from NVS key "air_temp"
    return 36.0f;
}

esp_err_t nvs_storage_set_air_temp(float temp_c)
{
    // TODO: Fase 2 — write to NVS key "air_temp"
    (void)temp_c;
    return ESP_OK;
}

float nvs_storage_get_skin_temp(void)
{
    // TODO: Fase 2 — read from NVS key "skin_temp"
    return 36.5f;
}

esp_err_t nvs_storage_set_skin_temp(float temp_c)
{
    // TODO: Fase 2 — write to NVS key "skin_temp"
    (void)temp_c;
    return ESP_OK;
}

uint8_t nvs_storage_get_language(void)
{
    // TODO: Fase 2 — read from NVS key "language"
    return 0;
}

esp_err_t nvs_storage_set_language(uint8_t lang)
{
    // TODO: Fase 2 — write to NVS key "language"
    (void)lang;
    return ESP_OK;
}
