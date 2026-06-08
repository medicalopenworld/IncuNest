/*
 * sensorBoard_comm.c
 *
 * USB CDC communication layer for SensorBoard.
 *
 * Tasks 6-9:
 *   Task 6 – USB CDC init (TinyUSB install + CDC ACM init, queue/semaphore creation)
 *   Task 7 – TX task + sensorBoard_comm_send_json
 *   Task 8 – Log interceptor (sb_log_vprintf → JSON log frame)
 *   Task 9 – RX task (feed bytes into frame decoder)
 */

#include "sensorBoard_comm.h"
#include "sensorBoard_comm_protocol.h"
#include "sensorBoard_frame.h"
#include "tinyusb.h"
#include "tinyusb_cdc_acm.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* ─────────────────────────────────────────────────────────────────
   Static types
   ───────────────────────────────────────────────────────────────── */

typedef struct {
    uint8_t frame[SB_PROTO_MAX_JSON_FRAME]; /* pre-encoded complete frame */
    size_t  len;
} sb_tx_item_t;

#define TX_QUEUE_DEPTH   8
#define RX_BUF_SIZE      4096

/* ─────────────────────────────────────────────────────────────────
   Static variables
   ───────────────────────────────────────────────────────────────── */

static const char *TAG = "USB_COMM";

static QueueHandle_t     s_tx_queue   = NULL;
static SemaphoreHandle_t s_rx_sem     = NULL;
static volatile bool     s_cdc_ready  = false;

/* Saved original log output function so we can restore it if needed */
static vprintf_like_t    s_prev_vprintf = NULL;

/* ─────────────────────────────────────────────────────────────────
   Forward declarations
   ───────────────────────────────────────────────────────────────── */

static void usb_tx_task(void *arg);
static void usb_rx_task(void *arg);
static int  sb_log_vprintf(const char *fmt, va_list args);

/* ─────────────────────────────────────────────────────────────────
   CDC callbacks
   ───────────────────────────────────────────────────────────────── */

/**
 * Called by TinyUSB when RX data is available.
 * Signals the RX task via semaphore (non-blocking give).
 */
static void cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    (void)itf;
    (void)event;

    BaseType_t higher_prio_woken = pdFALSE;
    xSemaphoreGiveFromISR(s_rx_sem, &higher_prio_woken);
    portYIELD_FROM_ISR(higher_prio_woken);
}

/**
 * Called by TinyUSB when DTR/RTS change.
 * Tracks whether a terminal has opened the port (DTR high → ready).
 */
static void cdc_line_state_callback(int itf, cdcacm_event_t *event)
{
    (void)itf;
    s_cdc_ready = event->line_state_changed_data.dtr;
    ESP_LOGI(TAG, "CDC line state: DTR=%d RTS=%d",
             (int)event->line_state_changed_data.dtr,
             (int)event->line_state_changed_data.rts);
}

/* ─────────────────────────────────────────────────────────────────
   Log interceptor
   ───────────────────────────────────────────────────────────────── */

/**
 * sb_log_vprintf – replaces esp_log's vprintf.
 *
 * Formats the log message, wraps it in a minimal JSON log frame
 * {"type":"log","ts":NNN,"msg":"..."} and posts it to the TX queue.
 * Characters " and \ are escaped manually (no cJSON dependency here).
 *
 * Returns the number of characters that would have been written
 * (mirrors vprintf semantics so ESP_LOG works correctly).
 */
static int sb_log_vprintf(const char *fmt, va_list args)
{
    /* Guard: if not yet initialised, fall back to default */
    if (s_tx_queue == NULL) {
        return vprintf(fmt, args);
    }

    /* Format the raw log line into a temporary buffer */
    char msg_buf[192];
    int msg_len = vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    if (msg_len < 0) {
        msg_len = 0;
    }
    /* Ensure NUL termination in case of truncation */
    msg_buf[sizeof(msg_buf) - 1] = '\0';

    /* Strip trailing newline that ESP_LOG appends */
    int trim = msg_len < (int)sizeof(msg_buf) ? msg_len : (int)sizeof(msg_buf) - 1;
    while (trim > 0 && (msg_buf[trim - 1] == '\n' || msg_buf[trim - 1] == '\r')) {
        msg_buf[--trim] = '\0';
    }

    /* Build JSON with manual escaping of " and \ */
    char json_buf[SB_PROTO_MAX_JSON_PAYLOAD];
    int  pos = 0;
    int  cap = (int)sizeof(json_buf) - 1;

    /* Header */
    int64_t ts_us = esp_timer_get_time();
    int n = snprintf(json_buf + pos, (size_t)(cap - pos),
                     "{\"type\":\"log\",\"ts\":%lld,\"msg\":\"", (long long)ts_us);
    if (n > 0) { pos += n; }

    /* Escaped message body */
    for (int i = 0; msg_buf[i] != '\0' && pos < cap - 2; i++) {
        char c = msg_buf[i];
        if (c == '"' || c == '\\') {
            if (pos < cap - 1) { json_buf[pos++] = '\\'; }
        }
        json_buf[pos++] = c;
    }

    /* Footer */
    n = snprintf(json_buf + pos, (size_t)(cap - pos), "\"}");
    if (n > 0) { pos += n; }
    json_buf[pos] = '\0';

    /* Encode to frame and post to TX queue */
    sb_tx_item_t item;
    item.len = sb_frame_encode(SB_PROTO_TYPE_JSON,
                               (const uint8_t *)json_buf, (size_t)pos,
                               item.frame, sizeof(item.frame));
    if (item.len > 0) {
        xQueueSend(s_tx_queue, &item, 0); /* non-blocking from potential ISR context */
    }

    return msg_len;
}

/* ─────────────────────────────────────────────────────────────────
   Frame received callback (called by decoder inside usb_rx_task)
   ───────────────────────────────────────────────────────────────── */

static void on_frame_received(uint8_t type, const uint8_t *payload, size_t len, void *ctx)
{
    (void)ctx;

    if (type == SB_PROTO_TYPE_JSON) {
        sensorBoard_cmd_handle(payload, len);
    }
    /* Other frame types (e.g. JPEG) are ignored for now */
}

/* ─────────────────────────────────────────────────────────────────
   RX task
   ───────────────────────────────────────────────────────────────── */

static void usb_rx_task(void *arg)
{
    (void)arg;

    static uint8_t rx_buf[RX_BUF_SIZE];
    static uint8_t frame_payload_buf[SB_PROTO_MAX_JSON_PAYLOAD];

    /* Initialise frame decoder with the payload buffer */
    sb_frame_dec_t dec;
    sb_frame_dec_init(&dec, frame_payload_buf, sizeof(frame_payload_buf));

    while (1) {
        /* Wait for RX data signal from CDC callback */
        if (xSemaphoreTake(s_rx_sem, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        /* Drain all available bytes in a loop until none remain */
        size_t rx_size = 0;
        do {
            rx_size = 0;
            esp_err_t err = tinyusb_cdcacm_read(TINYUSB_CDC_ACM_0,
                                                 rx_buf, sizeof(rx_buf),
                                                 &rx_size);
            if (err != ESP_OK || rx_size == 0) {
                break;
            }
            for (size_t i = 0; i < rx_size; i++) {
                sb_frame_dec_feed(&dec, rx_buf[i], on_frame_received, NULL);
            }
        } while (rx_size == sizeof(rx_buf)); /* if buffer was full, there may be more */
    }
}

/* ─────────────────────────────────────────────────────────────────
   TX task
   ───────────────────────────────────────────────────────────────── */

static void usb_tx_task(void *arg)
{
    (void)arg;

    sb_tx_item_t item;

    while (1) {
        /* Block until a frame is queued */
        if (xQueueReceive(s_tx_queue, &item, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        /* Only write when a terminal is connected (DTR high) */
        if (!s_cdc_ready) {
            continue;
        }

        size_t queued = tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0,
                                                    item.frame, item.len);
        if (queued > 0) {
            /* Flush with a 50 ms timeout; note: do NOT call with timeout from CDC callbacks */
            esp_err_t err = tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0,
                                                        pdMS_TO_TICKS(50));
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "CDC flush err: %d", err);
            }
        }
    }
}

/* ─────────────────────────────────────────────────────────────────
   Public API
   ───────────────────────────────────────────────────────────────── */

esp_err_t sensorBoard_comm_init(void)
{
    esp_err_t ret;

    /* Create TX queue */
    s_tx_queue = xQueueCreate(TX_QUEUE_DEPTH, sizeof(sb_tx_item_t));
    if (s_tx_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create TX queue");
        return ESP_ERR_NO_MEM;
    }

    /* Create RX semaphore (binary) */
    s_rx_sem = xSemaphoreCreateBinary();
    if (s_rx_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create RX semaphore");
        vQueueDelete(s_tx_queue);
        s_tx_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    /* Install TinyUSB driver with default descriptors */
    const tinyusb_config_t tusb_cfg = {
        .port = TINYUSB_PORT_FULL_SPEED_0,
        .phy = {
            .skip_setup      = false,
            .self_powered    = false,
            .vbus_monitor_io = -1,
        },
        .task = {
            .size     = 4096,
            .priority = 5,
            .xCoreID  = 0,
        },
        .descriptor = {
            .device            = NULL,  /* use default */
            .qualifier         = NULL,
            .string            = NULL,
            .string_count      = 0,
            .full_speed_config = NULL,
            .high_speed_config = NULL,
        },
        .event_cb  = NULL,
        .event_arg = NULL,
    };

    ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TinyUSB install failed: %d", ret);
        vSemaphoreDelete(s_rx_sem);
        vQueueDelete(s_tx_queue);
        s_rx_sem   = NULL;
        s_tx_queue = NULL;
        return ret;
    }

    /* Initialise CDC ACM interface 0 */
    const tinyusb_config_cdcacm_t cdc_cfg = {
        .cdc_port                    = TINYUSB_CDC_ACM_0,
        .callback_rx                 = cdc_rx_callback,
        .callback_rx_wanted_char     = NULL,
        .callback_line_state_changed = cdc_line_state_callback,
        .callback_line_coding_changed = NULL,
    };

    ret = tinyusb_cdcacm_init(&cdc_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CDC ACM init failed: %d", ret);
        tinyusb_driver_uninstall();
        vSemaphoreDelete(s_rx_sem);
        vQueueDelete(s_tx_queue);
        s_rx_sem   = NULL;
        s_tx_queue = NULL;
        return ret;
    }

    /* Redirect ESP_LOG output through our JSON log interceptor */
    s_prev_vprintf = esp_log_set_vprintf(sb_log_vprintf);

    /* Start RX task (stack 4 kB, priority 5) */
    TaskHandle_t rx_task_handle = NULL;
    BaseType_t rc = xTaskCreate(usb_rx_task, "usb_rx", 4096, NULL, 5, &rx_task_handle);
    if (rc != pdPASS) {
        ESP_LOGE(TAG, "Failed to create RX task");
        esp_log_set_vprintf(s_prev_vprintf);
        tinyusb_driver_uninstall();
        vSemaphoreDelete(s_rx_sem);
        vQueueDelete(s_tx_queue);
        s_rx_sem   = NULL;
        s_tx_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    /* Start TX task (stack 4 kB, priority 5) */
    rc = xTaskCreate(usb_tx_task, "usb_tx", 4096, NULL, 5, NULL);
    if (rc != pdPASS) {
        ESP_LOGE(TAG, "Failed to create TX task");
        vTaskDelete(rx_task_handle);
        esp_log_set_vprintf(s_prev_vprintf);
        tinyusb_driver_uninstall();
        vSemaphoreDelete(s_rx_sem);
        vQueueDelete(s_tx_queue);
        s_rx_sem   = NULL;
        s_tx_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "USB CDC comm init complete");
    return ESP_OK;
}

esp_err_t sensorBoard_comm_send_json(const char *json_str)
{
    if (json_str == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t slen = strlen(json_str);
    if (slen >= SB_PROTO_MAX_JSON_PAYLOAD) {
        return ESP_ERR_INVALID_SIZE;
    }

    sb_tx_item_t item;
    item.len = sb_frame_encode(SB_PROTO_TYPE_JSON,
                               (const uint8_t *)json_str, slen,
                               item.frame, sizeof(item.frame));
    if (item.len == 0) {
        return ESP_FAIL;
    }

    if (xQueueSend(s_tx_queue, &item, pdMS_TO_TICKS(10)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t sensorBoard_comm_send_binary(uint8_t type, uint8_t *buf, size_t len)
{
    (void)type;
    (void)buf;
    (void)len;
    return ESP_ERR_NOT_SUPPORTED;
}
