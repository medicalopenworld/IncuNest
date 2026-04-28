/**
 * @file nvs_storage.h
 * @brief Public API for persistent configuration storage using ESP-IDF NVS.
 *
 * @details Replaces the Arduino Preferences/EEPROM layer from the original firmware.
 *          All values previously stored at EEPROM_defines.h addresses are now
 *          stored as NVS key-value pairs in the "incunest" namespace.
 *
 * Normativa aplicable:
 * - IEC 60601-2-19 §201.11.8 — CONTROL TEMPERATURE must persist ≥10 min after power loss
 *
 * @author IncuNest Team
 * @date   2026-04-28
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// TODO: Fase 2 — Define full NVS API (load/save config struct)
// Reference: 04_PLATFORMIO_DEPENDENCIES.md §EEPROM→NVS mapping table

/**
 * @brief Initialize NVS flash partition.
 * Must be called once from app_main before any other nvs_storage_* call.
 */
esp_err_t nvs_storage_init(void);

/** @brief Load air temperature setpoint from NVS (default 36.0°C if not set). */
float nvs_storage_get_air_temp(void);

/** @brief Save air temperature setpoint to NVS. */
esp_err_t nvs_storage_set_air_temp(float temp_c);

/** @brief Load skin temperature setpoint from NVS (default 36.5°C if not set). */
float nvs_storage_get_skin_temp(void);

/** @brief Save skin temperature setpoint to NVS. */
esp_err_t nvs_storage_set_skin_temp(float temp_c);

/** @brief Load UI language (0=ES, 1=EN, 2=FR). Default 0 (Spanish). */
uint8_t nvs_storage_get_language(void);

/** @brief Save UI language. */
esp_err_t nvs_storage_set_language(uint8_t lang);

#ifdef __cplusplus
}
#endif
