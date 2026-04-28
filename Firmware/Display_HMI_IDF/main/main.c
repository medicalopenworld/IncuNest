/**
 * @file main.c
 * @brief Application entry point for IncuNest Display HMI.
 *
 * @details This file contains ONLY app_main() which initializes
 *          components in the correct dependency order and creates FreeRTOS tasks.
 *          No business logic belongs here — only initialization sequence.
 *
 *          Phase 0: Display init + backlight. Solid blue on screen.
 *          Phase 1: + LVGL port + touch driver
 *          Phase 2: + UI manager + home screen
 *          Phase 4: + alarm manager
 *          Phase 5: + motherboard communication
 *          Phase 6: + splash screen + system info
 *          Phase 7: + WiFi + OTA
 *
 * Normativa aplicable:
 * - IEC 62304 §5.5 — software unit implementation
 * - RNF-003 — max 5s from power-on to functional screen (cold boot)
 *
 * @author IncuNest Team
 * @date   2026-04-28
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "display_driver.h"
#include "nvs_storage.h"
#include "version.h"
#include "app_config.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "=== %s ===", FW_BUILD_ID);
    ESP_LOGI(TAG, "Starting initialization sequence...");

    /* ------------------------------------------------------------------
     * Step 1 — NVS: must be first (config persistence for all modules)
     * ------------------------------------------------------------------ */
    ESP_ERROR_CHECK(nvs_storage_init());

    /* ------------------------------------------------------------------
     * Step 2 — Display: init RGB panel + I2C bus + backlight controller
     * ------------------------------------------------------------------ */
    ESP_ERROR_CHECK(display_driver_init());

    /* Step 3 — Turn on backlight (80% default)
     * Phase 6 will restore persisted brightness from NVS. */
    display_driver_set_backlight(80);

    /* ------------------------------------------------------------------
     * TODO: Phase 1 — lvgl_port_init() + touch_driver_init()
     * TODO: Phase 2 — ui_manager_init() + create UITask
     * TODO: Phase 4 — alarm_manager_init()
     * TODO: Phase 5 — motherboard_comm_init() + create CommTask
     * TODO: Phase 6 — screen_splash_show() during boot
     * TODO: Phase 7 — wifi_manager_init() + create OTATask
     * ------------------------------------------------------------------ */

    ESP_LOGI(TAG, "Phase 0 complete — display active, backlight on");
    ESP_LOGI(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());

    /* Main loop — replaced by FreeRTOS tasks in later phases */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "Heartbeat — free heap: %lu bytes",
                 (unsigned long)esp_get_free_heap_size());
    }
}
