#include "esp_log.h"
#include "sensorBoard_comm.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "SensorBoard booting...");
    ESP_ERROR_CHECK(sensorBoard_comm_init());
    ESP_LOGI(TAG, "SensorBoard ready");
}
