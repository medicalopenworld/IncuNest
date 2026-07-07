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
        if (sensorBoard_comm_send_json(buf) != ESP_OK) {
            /* Evento de seguridad descartado (cola llena / sin host): la
             * re-aserción periódica lo autocorrige, pero debe quedar señal */
            ESP_LOGE(TAG, "door event dropped");
        }
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
        uint32_t notified =
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(CONFIG_SB_DOOR_REASSERT_S * 1000));

        if (notified == 0) {
            /* Timeout sin flancos: re-aserción idempotente del estado actual
             * — un evento perdido por backpressure deja de ser permanente */
            fsm.last_reported = -1;
            publish(sb_door_fsm_update(&fsm, gpio_get_level(SB_PIN_HALL), SB_DOOR_ACTIVE_LOW));
            continue;
        }

        /* Anti-tormenta: sin ISR mientras dura la ventana (un hall oscilante
         * preempta a todo el sistema si se deja la interrupción armada) */
        gpio_intr_disable(SB_PIN_HALL);
        /* Ventana de debounce: el DRV5032FB muestrea a ~5 Hz; esperamos a
         * nivel asentado */
        vTaskDelay(pdMS_TO_TICKS(CONFIG_SB_DOOR_DEBOUNCE_MS));
        /* Vacía el CONTADOR de notificaciones acumulado durante la ventana
         * (xTaskNotifyStateClear solo limpia el flag, no ulNotifiedValue,
         * y dejaría N-1 ciclos fantasma de debounce) */
        ulTaskNotifyValueClear(NULL, 0xFFFFFFFF);
        gpio_intr_enable(SB_PIN_HALL);

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

    /* Interrupción deshabilitada hasta tener el handler registrado: si otro
     * componente instaló ya el servicio ISR compartido, un flanco entre
     * gpio_config y handler_add se despacharía sin handler y se perdería */
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << SB_PIN_HALL,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, /* cubre salida open-drain del hall */
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        goto fail;
    }

    /* La tarea existe ANTES de habilitar la ISR (lección Fase 1: publicar
     * recursos antes de que alguien pueda usarlos) */
    if (xTaskCreate(door_task, "door", SB_DOOR_TASK_STACK, NULL, SB_DOOR_TASK_PRIO,
                    &s_door_task) != pdPASS) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }

    /* IRAM: la ISR sigue viva durante escrituras de flash (NVS/OTA) — sin el
     * flag, un flanco durante una escritura se serviría con retraso. OJO:
     * servicio singleton — el primer install fija los flags para TODAS las
     * fases; si otra fase lo instaló antes con otros flags, INVALID_STATE
     * se acepta y se heredan los suyos. */
    err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        goto fail_task;
    }
    err = gpio_isr_handler_add(SB_PIN_HALL, hall_isr, NULL);
    if (err != ESP_OK) {
        goto fail_task;
    }
    err = gpio_set_intr_type(SB_PIN_HALL, GPIO_INTR_ANYEDGE);
    if (err == ESP_OK) {
        err = gpio_intr_enable(SB_PIN_HALL);
    }
    if (err != ESP_OK) {
        gpio_isr_handler_remove(SB_PIN_HALL);
        goto fail_task;
    }

    sensorBoard_status_set_sensor("door", true);
    ESP_LOGI(TAG, "door sensor up (debounce %dms, reassert %ds)", CONFIG_SB_DOOR_DEBOUNCE_MS,
             CONFIG_SB_DOOR_REASSERT_S);
    return ESP_OK;

fail_task:
    vTaskDelete(s_door_task);
    s_door_task = NULL;
fail:
    /* El componente es dueño de su nombre en status: main no lo conoce */
    sensorBoard_status_set_sensor("door", false);
    return err;
}
