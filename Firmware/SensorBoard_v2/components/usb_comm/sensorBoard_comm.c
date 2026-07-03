#include "sensorBoard_comm.h"
#include "sensorBoard_comm_protocol.h"
#include "sensorBoard_frame.h"
#include "esp_check.h"
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
#define SB_COMM_TASK_STACK 4096
#define SB_COMM_TASK_PRIO 5

static QueueHandle_t s_tx_queue = NULL;
static SemaphoreHandle_t s_rx_sem = NULL;
static volatile bool s_cdc_ready = false;

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

    /* JSON con escapado manual de " y \ */
    char json_buf[SB_PROTO_MAX_JSON_PAYLOAD];
    int jpos = snprintf(json_buf, sizeof(json_buf), "{\"type\":\"log\",\"ts\":%lu,\"msg\":\"",
                        (unsigned long)ts_ms);
    if (jpos < 0 || jpos >= (int)sizeof(json_buf)) {
        return msg_len;
    }

    for (int i = 0; i < msg_len && jpos < (int)sizeof(json_buf) - 4; i++) {
        char c = msg_buf[i];
        if (c == '"' || c == '\\') {
            json_buf[jpos++] = '\\';
        }
        /* Los caracteres de control romperían el JSON: se sustituyen */
        json_buf[jpos++] = ((unsigned char)c < 0x20) ? ' ' : c;
    }

    json_buf[jpos++] = '"';
    json_buf[jpos++] = '}';
    json_buf[jpos] = '\0';
    sensorBoard_comm_send_json(json_buf);

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
            /* Sin host conectado (DTR bajo): el frame se descarta */
        }
    }
}

/* ── API pública ───────────────────────────────────────────── */
esp_err_t sensorBoard_comm_init(void)
{
    if (s_tx_queue != NULL) {
        return ESP_ERR_INVALID_STATE; /* ya inicializado */
    }

    s_rx_sem = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_rx_sem != NULL, ESP_ERR_NO_MEM, TAG, "rx sem alloc failed");

    const tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    ESP_RETURN_ON_ERROR(tinyusb_driver_install(&tusb_cfg), TAG, "TinyUSB install failed");

    const tinyusb_config_cdcacm_t acm_cfg = {
        .cdc_port = TINYUSB_CDC_ACM_0,
        .callback_rx = cdc_rx_callback,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = cdc_line_state_callback,
        .callback_line_coding_changed = NULL,
    };
    ESP_RETURN_ON_ERROR(tinyusb_cdcacm_init(&acm_cfg), TAG, "CDC ACM init failed");

    QueueHandle_t tx_queue = xQueueCreate(SB_TX_QUEUE_DEPTH, sizeof(sb_tx_item_t));
    ESP_RETURN_ON_FALSE(tx_queue != NULL, ESP_ERR_NO_MEM, TAG, "tx queue alloc failed");

    BaseType_t ok;
    ok = xTaskCreate(usb_tx_task, "usb_tx", SB_COMM_TASK_STACK, NULL, SB_COMM_TASK_PRIO, NULL);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "tx task create failed");
    ok = xTaskCreate(usb_rx_task, "usb_rx", SB_COMM_TASK_STACK, NULL, SB_COMM_TASK_PRIO, NULL);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "rx task create failed");

    /* La cola se publica al final: hasta aquí, el interceptor descarta logs.
     * (El interceptor solo emite cuando s_tx_queue != NULL.) */
    s_tx_queue = tx_queue;
    esp_log_set_vprintf(sb_log_vprintf);

    ESP_LOGI(TAG, "USB CDC ready");
    return ESP_OK;
}

esp_err_t sensorBoard_comm_send_json(const char *json_str)
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

    return (xQueueSend(s_tx_queue, &item, pdMS_TO_TICKS(10)) == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t sensorBoard_comm_send_binary(uint8_t type, uint8_t *buf, size_t len)
{
    /* Fase 5: transferencia de payloads grandes (JPEG) con ownership de buffer */
    (void)type;
    (void)buf;
    (void)len;
    return ESP_ERR_NOT_SUPPORTED;
}
