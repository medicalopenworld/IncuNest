#include "sb_door_sensor.h"
#include "sb_door_logic.h"
#include "sensorBoard_comm.h"
#include "sensorBoard_comm_protocol.h"
#include "sensorBoard_status.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "DOOR";

/* Pinout: docs/hardware.md */
#define SB_PIN_HALL GPIO_NUM_47

/* Prio 4: productor, por debajo del transporte (5) — presupuesto en
 * sensorBoard_comm.c */
#define SB_DOOR_TASK_PRIO 4
#define SB_DOOR_TASK_STACK 3072

#if CONFIG_SB_DOOR_ACTIVE_LOW
#define SB_DOOR_ACTIVE_LOW true
#else
#define SB_DOOR_ACTIVE_LOW false
#endif

static TaskHandle_t s_door_task = NULL;

/* ISR: solo hand-off (rules/embedded.md) — nada de lógica ni logging */
static void IRAM_ATTR hall_isr(void *arg)
{
    (void)arg;
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(s_door_task, &woken);
    portYIELD_FROM_ISR(woken);
}

static void publish(sb_door_event_t evt)
{
    char buf[64];
    uint32_t ts = (uint32_t)(esp_timer_get_time() / 1000);
    if (sb_door_build_event(buf, sizeof(buf), evt, ts) > 0) {
        sensorBoard_comm_send_json(buf);
    }
}

static void door_task(void *arg)
{
    (void)arg;
    sb_door_fsm_t fsm;
    sb_door_fsm_init(&fsm);

    /* Estado inicial: la motherboard no arranca a ciegas */
    publish(sb_door_fsm_update(&fsm, gpio_get_level(SB_PIN_HALL), SB_DOOR_ACTIVE_LOW));

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        /* Ventana de debounce: el DRV5032FB muestrea a ~5 Hz; esperamos a
         * nivel asentado y colapsamos ráfagas de flancos en una lectura */
        vTaskDelay(pdMS_TO_TICKS(CONFIG_SB_DOOR_DEBOUNCE_MS));
        xTaskNotifyStateClear(NULL); /* flancos acumulados durante la ventana */

        sb_door_event_t evt =
            sb_door_fsm_update(&fsm, gpio_get_level(SB_PIN_HALL), SB_DOOR_ACTIVE_LOW);
        if (evt != SB_DOOR_EVT_NONE) {
            publish(evt);
        }
    }
}

esp_err_t sb_door_sensor_init(void)
{
    if (s_door_task != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << SB_PIN_HALL,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, /* cubre salida open-drain del hall */
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        return err;
    }

    /* La tarea existe ANTES de habilitar la ISR (lección Fase 1: publicar
     * recursos antes de que alguien pueda usarlos) */
    if (xTaskCreate(door_task, "door", SB_DOOR_TASK_STACK, NULL, SB_DOOR_TASK_PRIO,
                    &s_door_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) { /* INVALID_STATE = ya instalado */
        vTaskDelete(s_door_task);
        s_door_task = NULL;
        return err;
    }
    err = gpio_isr_handler_add(SB_PIN_HALL, hall_isr, NULL);
    if (err != ESP_OK) {
        vTaskDelete(s_door_task);
        s_door_task = NULL;
        return err;
    }

    sensorBoard_status_set_sensor("door", true);
    ESP_LOGI(TAG, "door sensor up (debounce %dms)", CONFIG_SB_DOOR_DEBOUNCE_MS);
    return ESP_OK;
}
