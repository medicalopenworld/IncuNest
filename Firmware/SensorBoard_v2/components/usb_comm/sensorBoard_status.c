#include "sensorBoard_status.h"

esp_err_t sensorBoard_status_set_sensor(const char *name, bool available)
{
    (void)name;
    (void)available;
    return ESP_OK;
}

size_t sb_status_build_sensors_json(char *buf, size_t buf_size)
{
    (void)buf;
    (void)buf_size;
    return 0;
}

void sb_status_reset(void)
{
}
