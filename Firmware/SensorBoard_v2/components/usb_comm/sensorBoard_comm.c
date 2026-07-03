#include "sensorBoard_comm.h"
#include "sensorBoard_comm_protocol.h"
#include "sensorBoard_frame.h"
#include "sensorBoard_json.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tinyusb_cdc_acm.h"
#include "tinyusb_default_config.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "USB_COMM";

/* ── TX queue ──────────────────────────────────────────────── */
typedef struct {
    uint8_t frame[SB_PROTO_MAX_JSON_FRAME];
    size_t len;
} sb_tx_item_t;

#define SB_TX_QUEUE_DEPTH 8

/* 4096 B para ambas tareas: usb_rx ejecuta decoder + cJSON (la más profunda);
 * usb_tx solo copia a TinyUSB. Ajustar con uxTaskGetStackHighWaterMark si una
 * fase futura engorda el dispatcher. */
#define SB_COMM_TASK_STACK 4096

/* Prioridad 5: por encima de las tareas de sensores de las fases 2-4 (que
 * deben crearse con prioridad <5) para que la telemetría siempre se drene.
 * Presupuesto de prioridades del firmware: 5 = transporte, <5 = productores. */
#define SB_COMM_TASK_PRIO 5

static QueueHandle_t s_tx_queue = NULL;
static SemaphoreHandle_t s_rx_sem = NULL;

/* Flag escrito por el callback de line-state (contexto de tarea TinyUSB) y
 * leído por usb_tx_task. volatile bool basta para un flag de un solo bit en
 * este idiom ESP-IDF (sin ordering C11 estricto — elección deliberada). */
static volatile bool s_cdc_ready = false;

/* ── envío interno (timeout parametrizado) ─────────────────── */
static esp_err_t send_json_timeout(const char *json_str, TickType_t wait_ticks)
{
    if (json_str == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_tx_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t payload_len = strlen(json_str);
    if (payload_len > SB_PROTO_MAX_JSON_PAYLOAD) {
        return ESP_ERR_INVALID_SIZE;
    }

    sb_tx_item_t item;
    item.len = sb_frame_encode(SB_PROTO_TYPE_JSON, (const uint8_t *)json_str, payload_len,
                               item.frame, sizeof(item.frame));
    if (item.len == 0) {
        return ESP_ERR_INVALID_SIZE;
    }

    return (xQueueSend(s_tx_queue, &item, wait_ticks) == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
}

/* ── CDC callbacks (contexto de la tarea TinyUSB, no ISR) ──── */
static void cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    (void)itf;
    (void)event;
    xSemaphoreGive(s_rx_sem);
}

static void cdc_line_state_callback(int itf, cdcacm_event_t *event)
{
    (void)itf;
    s_cdc_ready = event->line_state_changed_data.dtr;
}

/* ── Log interceptor ───────────────────────────────────────── */
/* Restricción de proyecto: NUNCA llamar ESP_LOG* desde ISR — este interceptor
 * encola (no bloqueante) y eso es ilegal en contexto de interrupción. */
static int sb_log_vprintf(const char *fmt, va_list args)
{
    if (s_tx_queue == NULL) {
        return 0; /* cola no lista: descartar sin bloquear */
    }

    char msg_buf[160];
    int msg_len = vsnprintf(msg_buf, sizeof(msg_buf) - 1, fmt, args);
    if (msg_len < 0) {
        return msg_len;
    }
    if (msg_len >= (int)sizeof(msg_buf) - 1) {
        msg_len = (int)sizeof(msg_buf) - 2;
    }
    msg_buf[sizeof(msg_buf) - 1] = '\0';

    /* Quitar \r\n finales */
    while (msg_len > 0 && (msg_buf[msg_len - 1] == '\n' || msg_buf[msg_len - 1] == '\r')) {
        msg_buf[--msg_len] = '\0';
    }

    uint32_t ts_ms = (uint32_t)(esp_timer_get_time() / 1000);

    char json_buf[SB_PROTO_MAX_JSON_PAYLOAD];
    int jpos = snprintf(json_buf, sizeof(json_buf), "{\"type\":\"log\",\"ts\":%lu,\"msg\":\"",
                        (unsigned long)ts_ms);
    if (jpos < 0 || jpos >= (int)sizeof(json_buf) - 3) {
        return msg_len;
    }

    /* Reservar 2 chars de cierre + NUL */
    jpos += (int)sb_json_escape(json_buf + jpos, sizeof(json_buf) - (size_t)jpos - 2, msg_buf,
                                (size_t)msg_len);
    json_buf[jpos++] = '"';
    json_buf[jpos++] = '}';
    json_buf[jpos] = '\0';

    /* Timeout 0: un log jamás bloquea a la tarea que loguea; si la cola está
     * llena, el log se pierde (sin reintento — evita recursión y jitter). */
    send_json_timeout(json_buf, 0);

    return msg_len;
}

/* ── RX: frame completo → dispatcher ───────────────────────── */
static void on_frame_received(uint8_t type, const uint8_t *payload, size_t len, void *ctx)
{
    (void)ctx;
    if (type == SB_PROTO_TYPE_JSON) {
        sensorBoard_cmd_handle(payload, len);
    }
    /* Otros tipos entrantes: ignorados (solo la motherboard envía JSON) */
}

static void usb_rx_task(void *arg)
{
    (void)arg;

    static uint8_t rx_buf[512];
    static uint8_t frame_payload_buf[SB_PROTO_MAX_JSON_PAYLOAD];
    sb_frame_dec_t decoder;
    sb_frame_dec_init(&decoder, frame_payload_buf, sizeof(frame_payload_buf));

    for (;;) {
        xSemaphoreTake(s_rx_sem, portMAX_DELAY);

        size_t rx_size = 0;
        do {
            rx_size = 0;
            esp_err_t ret = tinyusb_cdcacm_read(TINYUSB_CDC_ACM_0, rx_buf, sizeof(rx_buf), &rx_size);
            if (ret != ESP_OK) {
                break;
            }
            for (size_t i = 0; i < rx_size; i++) {
                sb_frame_dec_feed(&decoder, rx_buf[i], on_frame_received, NULL);
            }
        } while (rx_size > 0);
    }
}

/* ── TX task: único escritor del endpoint CDC ──────────────── */
static void usb_tx_task(void *arg)
{
    (void)arg;
    static sb_tx_item_t item;

    for (;;) {
        if (xQueueReceive(s_tx_queue, &item, portMAX_DELAY) == pdTRUE) {
            if (s_cdc_ready) {
                tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, item.frame, item.len);
                tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, pdMS_TO_TICKS(50));
            }
            /* Sin host conectado (DTR bajo): el frame se descarta. La señal de
             * vida para la motherboard es el heartbeat, no un contador aquí. */
        }
    }
}

/* ── API pública ───────────────────────────────────────────── */
/* Política de fallo: main.c envuelve init en ESP_ERROR_CHECK (fallo de
 * arranque ⇒ reboot; la motherboard detecta la ausencia de heartbeat como
 * "SensorBoard no disponible"). Aun así, esta función limpia lo creado en
 * caso de fallo parcial para ser segura ante futuros reintentos. */
esp_err_t sensorBoard_comm_init(void)
{
    if (s_tx_queue != NULL) {
        return ESP_ERR_INVALID_STATE; /* ya inicializado */
    }

    esp_err_t err = ESP_OK;
    TaskHandle_t tx_task_handle = NULL;
    bool driver_installed = false;
    bool cdc_initialized = false;

    s_rx_sem = xSemaphoreCreateBinary();
    if (s_rx_sem == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    err = tinyusb_driver_install(&tusb_cfg);
    if (err != ESP_OK) {
        goto fail;
    }
    driver_installed = true;

    const tinyusb_config_cdcacm_t acm_cfg = {
        .cdc_port = TINYUSB_CDC_ACM_0,
        .callback_rx = cdc_rx_callback,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = cdc_line_state_callback,
        .callback_line_coding_changed = NULL,
    };
    err = tinyusb_cdcacm_init(&acm_cfg);
    if (err != ESP_OK) {
        goto fail;
    }
    cdc_initialized = true;

    /* La cola se publica ANTES de crear las tareas: usb_tx_task tiene prio 5
     * (> main, prio 1) y puede ejecutar xQueueReceive(s_tx_queue) de inmediato
     * — si la global fuera NULL en ese momento, configASSERT/hardfault. */
    s_tx_queue = xQueueCreate(SB_TX_QUEUE_DEPTH, sizeof(sb_tx_item_t));
    if (s_tx_queue == NULL) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }

    if (xTaskCreate(usb_tx_task, "usb_tx", SB_COMM_TASK_STACK, NULL, SB_COMM_TASK_PRIO,
                    &tx_task_handle) != pdPASS) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    if (xTaskCreate(usb_rx_task, "usb_rx", SB_COMM_TASK_STACK, NULL, SB_COMM_TASK_PRIO, NULL) !=
        pdPASS) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }

    esp_log_set_vprintf(sb_log_vprintf);
    ESP_LOGI(TAG, "USB CDC ready");
    return ESP_OK;

fail:
    if (tx_task_handle != NULL) {
        vTaskDelete(tx_task_handle);
    }
    if (s_tx_queue != NULL) {
        vQueueDelete(s_tx_queue);
        s_tx_queue = NULL;
    }
    if (cdc_initialized) {
        tinyusb_cdcacm_deinit(TINYUSB_CDC_ACM_0);
    }
    if (driver_installed) {
        tinyusb_driver_uninstall();
    }
    vSemaphoreDelete(s_rx_sem);
    s_rx_sem = NULL;
    return err;
}

esp_err_t sensorBoard_comm_send_json(const char *json_str)
{
    return send_json_timeout(json_str, pdMS_TO_TICKS(10));
}

esp_err_t sensorBoard_comm_send_binary(uint8_t type, uint8_t *buf, size_t len)
{
    /* Fase 5: transferencia de payloads grandes (JPEG) con ownership de buffer */
    (void)type;
    (void)buf;
    (void)len;
    return ESP_ERR_NOT_SUPPORTED;
}
