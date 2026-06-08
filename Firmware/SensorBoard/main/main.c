#include "sensorBoard_comm.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "SensorBoard starting");

    esp_err_t ret = sensorBoard_comm_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "USB comm init failed: %d", ret);
        return;
    }

    ESP_LOGI(TAG, "USB comm ready");

    /* Heartbeat: send status event every 30 seconds */
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        sensorBoard_comm_send_json(
            "{\"type\":\"event\",\"cmd\":\"heartbeat\"}");
    }
}
