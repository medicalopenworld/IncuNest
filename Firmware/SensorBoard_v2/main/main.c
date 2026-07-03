#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sb_door_sensor.h"
#include "sb_env_sensors.h"
#include "sb_mic_sensor.h"
#include "sensorBoard_comm.h"
#include "sensorBoard_comm_protocol.h"
#include <stdio.h>

static const char *TAG = "MAIN";

#define SB_HEARTBEAT_PERIOD_MS 30000

void app_main(void)
{
    ESP_LOGI(TAG, "SensorBoard v%s booting", SB_PROTO_FW_VERSION);
    ESP_ERROR_CHECK(sensorBoard_comm_init());
    /* Sensores: fallo no fatal — cada componente reporta su disponibilidad */
    ESP_ERROR_CHECK_WITHOUT_ABORT(sb_env_sensors_init());
    ESP_ERROR_CHECK_WITHOUT_ABORT(sb_door_sensor_init());
    ESP_ERROR_CHECK_WITHOUT_ABORT(sb_mic_sensor_init());
    ESP_LOGI(TAG, "SensorBoard ready — USB CDC active");

    /* Heartbeat: señal de vida hacia la motherboard */
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(SB_HEARTBEAT_PERIOD_MS));
        char buf[96];
        uint32_t uptime = (uint32_t)(esp_timer_get_time() / 1000);
        snprintf(buf, sizeof(buf), "{\"type\":\"event\",\"cmd\":\"heartbeat\",\"uptime\":%lu}",
                 (unsigned long)uptime);
        sensorBoard_comm_send_json(buf);
    }
}
