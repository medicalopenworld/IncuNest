#include "sensorBoard_cmd_registry.h"

esp_err_t sensorBoard_cmd_register(const char *cmd, sb_cmd_handler_t handler)
{
    (void)cmd;
    (void)handler;
    return ESP_OK;
}

sb_cmd_handler_t sb_cmd_registry_find(const char *cmd)
{
    (void)cmd;
    return (sb_cmd_handler_t)0;
}

void sb_cmd_registry_reset(void)
{
}
