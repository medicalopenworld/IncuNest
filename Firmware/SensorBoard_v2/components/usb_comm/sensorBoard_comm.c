#include "sensorBoard_comm.h"
#include "esp_log.h"

static const char *TAG = "USB_COMM";

esp_err_t sensorBoard_comm_init(void)
{
    ESP_LOGI(TAG, "comm init stub");
    return ESP_OK;
}

esp_err_t sensorBoard_comm_send_json(const char *json_str)
{
    (void)json_str;
    return ESP_OK;
}

esp_err_t sensorBoard_comm_send_binary(uint8_t type, uint8_t *buf, size_t len)
{
    (void)type;
    (void)buf;
    (void)len;
    return ESP_ERR_NOT_SUPPORTED;
}
