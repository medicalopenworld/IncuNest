#include "sensorBoard_comm.h"
#include "sensorBoard_comm_protocol.h"
#include "sensorBoard_frame.h"
#include "sensorBoard_json.h"
#include "esp_heap_caps.h"
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
#include <stdlib.h>
#include <string.h>

static const char *TAG = "USB_COMM";

/* ── TX queues ─────────────────────────────────────────────── */
/* JSON (telemetría/resp/heartbeat): por valor, prioridad de drenado */
typedef struct {
    uint8_t frame[SB_PROTO_MAX_JSON_FRAME];
    size_t len;
} sb_tx_item_t;

/* Binarios (JPEG): cola separada de profundidad 1 — cota dura de un frame
 * en vuelo (presión de PSRAM acotada) y el JSON nunca espera detrás de más
 * de un binario. Ownership del puntero: usb_tx_task SIEMPRE libera. */
typedef struct {
    uint8_t *frame;
    size_t len;
} sb_tx_bin_item_t;

#define SB_TX_QUEUE_DEPTH 8
#define SB_TX_BIN_QUEUE_DEPTH 1

/* Sondeo de la cola binaria cuando la JSON está vacía (ms) */
#define SB_TX_IDLE_POLL_MS 20

/* 4096 B para ambas tareas: usb_rx ejecuta decoder + cJSON (la más profunda);
 * usb_tx solo copia a TinyUSB. Ajustar con uxTaskGetStackHighWaterMark si una
 * fase futura engorda el dispatcher. */
#define SB_COMM_TASK_STACK 4096

/* Prioridad 5: por encima de las tareas de sensores de las fases 2-4 (que
 * deben crearse con prioridad <5) para que la telemetría siempre se drene.
 * Presupuesto de prioridades del firmware: 5 = transporte, <5 = productores. */
#define SB_COMM_TASK_PRIO 5

static QueueHandle_t s_tx_queue = NULL;
static QueueHandle_t s_tx_bin_queue = NULL;
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

/* Escribe un frame grande por chunks; aborta ante stalls repetidos o host
 * desconectado (el frame se descarta — la motherboard reintenta capture).
 * LIMITACIÓN documentada: el framing exige frames contiguos en el cable, así
 * que un JSON urgente que llegue durante ESTE frame espera a que termine —
 * peor caso ~600 ms con host atascado (10 stalls × ~60 ms) antes de abortar.
 * La cola binaria de profundidad 1 garantiza que nunca hay más de un frame
 * grande por delante. */
static void tx_write_large(const uint8_t *data, size_t len)
{
    size_t off = 0;
    int stalls = 0;

    while (off < len && stalls < 10 && s_cdc_ready) {
        size_t queued = tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, data + off, len - off);
        off += queued;
        esp_err_t err = tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, pdMS_TO_TICKS(50));
        if (queued == 0 || err != ESP_OK) {
            stalls++;
            vTaskDelay(pdMS_TO_TICKS(10));
        } else {
            stalls = 0;
        }
    }
    if (off < len) {
        ESP_LOGE(TAG, "binary frame aborted at %u/%u B", (unsigned)off, (unsigned)len);
    }
}

/* ── TX task: único escritor del endpoint CDC ──────────────── */
static void usb_tx_task(void *arg)
{
    (void)arg;
    static sb_tx_item_t item;
    sb_tx_bin_item_t bin;

    for (;;) {
        /* El JSON (telemetría/heartbeat/resp) SIEMPRE drena primero */
        if (xQueueReceive(s_tx_queue, &item, 0) == pdTRUE) {
            if (s_cdc_ready) {
                tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, item.frame, item.len);
                tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, pdMS_TO_TICKS(50));
            }
            continue;
        }
        /* Sin JSON pendiente: un binario como mucho */
        if (xQueueReceive(s_tx_bin_queue, &bin, 0) == pdTRUE) {
            if (s_cdc_ready && bin.frame != NULL) {
                tx_write_large(bin.frame, bin.len);
            }
            free(bin.frame); /* ownership: SIEMPRE se libera aquí */
            continue;
        }
        /* Nada pendiente: bloquear en la JSON con sondeo corto de la binaria */
        if (xQueueReceive(s_tx_queue, &item, pdMS_TO_TICKS(SB_TX_IDLE_POLL_MS)) == pdTRUE) {
            if (s_cdc_ready) {
                tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, item.frame, item.len);
                tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, pdMS_TO_TICKS(50));
            }
        }
        /* Sin host conectado (DTR bajo): los frames se descartan. La señal de
         * vida para la motherboard es el heartbeat, no un contador aquí. */
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

    /* Las colas se publican ANTES de crear las tareas: usb_tx_task tiene
     * prio 5 (> main, prio 1) y puede ejecutar xQueueReceive de inmediato
     * — si una global fuera NULL en ese momento, configASSERT/hardfault. */
    s_tx_queue = xQueueCreate(SB_TX_QUEUE_DEPTH, sizeof(sb_tx_item_t));
    if (s_tx_queue == NULL) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    s_tx_bin_queue = xQueueCreate(SB_TX_BIN_QUEUE_DEPTH, sizeof(sb_tx_bin_item_t));
    if (s_tx_bin_queue == NULL) {
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
    if (s_tx_bin_queue != NULL) {
        vQueueDelete(s_tx_bin_queue);
        s_tx_bin_queue = NULL;
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
    if (buf == NULL || len == 0 || len > SB_PROTO_MAX_BINARY_PAYLOAD) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_tx_bin_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Frame completo SOLO en PSRAM: sin fallback a DRAM interna — un JPEG de
     * decenas de KB compitiendo con stacks/cJSON es peor que descartar la
     * captura (fail-safe). El caller recupera su buffer al retornar. */
    size_t frame_len = len + SB_PROTO_FRAME_OVERHEAD;
    uint8_t *frame = heap_caps_malloc(frame_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (frame == NULL) {
        return ESP_ERR_NO_MEM;
    }
    size_t encoded = sb_frame_encode(type, buf, len, frame, frame_len);
    if (encoded == 0) {
        free(frame);
        return ESP_ERR_INVALID_SIZE;
    }

    /* Cola de profundidad 1 y sin espera: si hay un binario en vuelo, este
     * se rechaza (cota dura de PSRAM y de latencia del JSON) */
    sb_tx_bin_item_t item = { .frame = frame, .len = encoded };
    if (xQueueSend(s_tx_bin_queue, &item, 0) != pdTRUE) {
        free(frame);
        return ESP_ERR_TIMEOUT; /* binario anterior aún sin drenar */
    }
    return ESP_OK;
}

esp_err_t sensorBoard_comm_send_json_noblock(const char *json_str)
{
    /* Para contextos que no deben bloquear jamás (dispatcher en usb_rx):
     * bajo saturación de la cola la respuesta se pierde y el emisor
     * reintenta — nunca al revés */
    return send_json_timeout(json_str, 0);
}
