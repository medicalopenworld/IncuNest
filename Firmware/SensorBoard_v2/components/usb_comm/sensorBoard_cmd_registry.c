#include "sensorBoard_cmd_registry.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

typedef struct {
    char name[SB_CMD_REG_NAME_MAX];
    sb_cmd_handler_t handler;
} sb_cmd_entry_t;

static sb_cmd_entry_t s_table[SB_CMD_REG_MAX];
static size_t s_count = 0;
/* Escritores: inits de componentes; lector: usb_rx (dispatcher) */
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

esp_err_t sensorBoard_cmd_register(const char *cmd, sb_cmd_handler_t handler)
{
    if (cmd == NULL || cmd[0] == '\0' || strlen(cmd) >= SB_CMD_REG_NAME_MAX ||
        handler == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_OK;
    taskENTER_CRITICAL(&s_lock);
    size_t i;
    for (i = 0; i < s_count; i++) {
        if (strcmp(s_table[i].name, cmd) == 0) {
            s_table[i].handler = handler;
            break;
        }
    }
    if (i == s_count) {
        if (s_count >= SB_CMD_REG_MAX) {
            err = ESP_ERR_NO_MEM;
        } else {
            strcpy(s_table[s_count].name, cmd);
            s_table[s_count].handler = handler;
            s_count++;
        }
    }
    taskEXIT_CRITICAL(&s_lock);
    return err;
}

sb_cmd_handler_t sb_cmd_registry_find(const char *cmd)
{
    if (cmd == NULL) {
        return NULL;
    }
    sb_cmd_handler_t h = NULL;
    taskENTER_CRITICAL(&s_lock);
    for (size_t i = 0; i < s_count; i++) {
        if (strcmp(s_table[i].name, cmd) == 0) {
            h = s_table[i].handler;
            break;
        }
    }
    taskEXIT_CRITICAL(&s_lock);
    return h;
}

void sb_cmd_registry_reset(void)
{
    taskENTER_CRITICAL(&s_lock);
    s_count = 0;
    taskEXIT_CRITICAL(&s_lock);
}
