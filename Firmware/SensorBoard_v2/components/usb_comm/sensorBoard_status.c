#include "sensorBoard_status.h"
#include "freertos/FreeRTOS.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    char name[SB_STATUS_NAME_MAX];
    bool available;
} sb_status_entry_t;

static sb_status_entry_t s_table[SB_STATUS_MAX_SENSORS];
static size_t s_count = 0;
/* Escritores: tareas de sensores (fases 2-5); lector: usb_rx (build_status).
 * Sección crítica corta (copias de tabla), formateo fuera del lock. */
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

esp_err_t sensorBoard_status_set_sensor(const char *name, bool available)
{
    if (name == NULL || name[0] == '\0' || strlen(name) >= SB_STATUS_NAME_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_OK;
    taskENTER_CRITICAL(&s_lock);
    size_t i;
    for (i = 0; i < s_count; i++) {
        if (strcmp(s_table[i].name, name) == 0) {
            s_table[i].available = available;
            break;
        }
    }
    if (i == s_count) {
        if (s_count >= SB_STATUS_MAX_SENSORS) {
            err = ESP_ERR_NO_MEM;
        } else {
            strcpy(s_table[s_count].name, name);
            s_table[s_count].available = available;
            s_count++;
        }
    }
    taskEXIT_CRITICAL(&s_lock);
    return err;
}

size_t sb_status_build_sensors_json(char *buf, size_t buf_size)
{
    sb_status_entry_t snap[SB_STATUS_MAX_SENSORS];
    size_t count;

    taskENTER_CRITICAL(&s_lock);
    count = s_count;
    memcpy(snap, s_table, count * sizeof(sb_status_entry_t));
    taskEXIT_CRITICAL(&s_lock);

    if (count == 0 || buf == NULL || buf_size == 0) {
        return 0;
    }

    int pos = snprintf(buf, buf_size, "\"sensors\":{");
    if (pos < 0 || pos >= (int)buf_size) {
        return 0;
    }
    for (size_t i = 0; i < count; i++) {
        int n = snprintf(buf + pos, buf_size - (size_t)pos, "%s\"%s\":%s", (i > 0) ? "," : "",
                         snap[i].name, snap[i].available ? "true" : "false");
        if (n < 0 || n >= (int)(buf_size - (size_t)pos)) {
            return 0;
        }
        pos += n;
    }
    if ((size_t)pos + 1 >= buf_size) {
        return 0;
    }
    buf[pos++] = '}';
    buf[pos] = '\0';
    return (size_t)pos;
}

void sb_status_reset(void)
{
    taskENTER_CRITICAL(&s_lock);
    s_count = 0;
    taskEXIT_CRITICAL(&s_lock);
}
