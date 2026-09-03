#include "sensorBoard_comm.h"
#include "sensorBoard_comm_protocol.h"
#include "sensorBoard_frame.h"
#include "sensorBoard_host_watch.h"
#include "sensorBoard_json.h"
#include "sensorBoard_status.h"
#include "sensorBoard_usb_orient.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "hal/usb_wrap_ll.h"
#include "sdkconfig.h"
#include "soc/usb_wrap_struct.h"
#include "tinyusb.h"
#include "tinyusb_cdc_acm.h"
#include "tinyusb_default_config.h"
#include "tusb.h"
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

/* ── Orientación del conector (autoswap D+/D-, ADR-0003) ───── */
/* Política pura (sb_usb_orient) + ejecución aquí. Solo usb_tx_task la toca:
 * sin host no hay tráfico y la tarea itera cada SB_TX_IDLE_POLL_MS, así que
 * el plazo se evalúa con resolución ~20 ms sin tarea ni timer adicionales. */
#if CONFIG_SB_USB_AUTOSWAP
#define SB_USB_AUTOSWAP_TIMEOUT_MS CONFIG_SB_USB_AUTOSWAP_TIMEOUT_MS
#else
#define SB_USB_AUTOSWAP_TIMEOUT_MS 0 /* 0 = política desactivada */
#endif

/* Detach visible para el host antes de re-attach con la otra orientación.
 * Los controladores host latchean el cambio de conexión (>2.5 µs de SE0
 * basta), pero un hub intermedio o una pila host que sondea el estado del
 * puerto puede muestrear más despacio: 250 ms garantiza que el detach se ve
 * y el host reinicia la enumeración. Sin host no hay tráfico, así que
 * bloquear usb_tx_task este tiempo no retrasa nada. */
#define SB_USB_SWAP_DETACH_MS 250

/* Clave en la resp de `status` (sensors{}): true = D+/D- intercambiados.
 * Diagnóstico determinista para la motherboard y el flasher (el ROM no aplica
 * el intercambio: con usb_swap=true hay que girar el cable para flashear). */
#define SB_USB_SWAP_STATUS_KEY "usb_swap"

static sb_usb_orient_t s_orient;

static uint32_t orient_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* "Host activo" = orientación demostrada correcta. tud_connected() se pone
 * al recibir el PRIMER paquete SETUP (usbd.c: "Mark as connected after
 * receiving 1st setup packet") y se borra con bus reset/unplug; un SETUP
 * válido (NRZI + CRC) es imposible con D+/D- cruzados. Así un host lento en
 * llegar a SET_CONFIGURATION (reintentos, pila arrancando, hub) NUNCA
 * provoca un intercambio: solo se alterna cuando el host no nos habla. */
static bool usb_host_active(void)
{
    return tud_mounted() || tud_connected();
}

/* Contexto de tarea (usb_tx_task), nunca ISR.
 * - Solo se ejecuta sin host activo (invariante de la política): la tarea
 *   TinyUSB no tiene actividad de endpoints, así que los read-modify-write
 *   de USB_WRAP.otg_conf / dctl aquí y en dcd_connect/dcd_disconnect no
 *   compiten con nadie (dcd_connect/disconnect solo los llama además
 *   dcd_init/tud_deinit). dcd_* preservan exchg_pins/exchg_pins_override.
 * - El ESP_LOGW encola en s_tx_queue con timeout 0 (interceptor): loguear
 *   desde la propia usb_tx_task no puede bloquearla. */
static void orient_service(void)
{
    if (sb_usb_orient_tick(&s_orient, usb_host_active(), orient_now_ms()) != SB_ORIENT_SWAP) {
        return;
    }
    if (usb_host_active()) {
        return; /* el host habló entre el tick y ahora: no interrumpirle */
    }
    bool swapped = sb_usb_orient_is_swapped(&s_orient);
    /* DTR es pegajoso (TinyUSB no invoca line_state_cb en reset/unplug y no
     * hay VBUS sensing): sin host no hay CDC escribible, re-armar la
     * retención de arranque para que el log de abajo no muera en el FIFO. */
    s_cdc_ready = false;
    tud_disconnect();
    usb_wrap_ll_phy_enable_pin_exchg(&USB_WRAP, swapped);
    vTaskDelay(pdMS_TO_TICKS(SB_USB_SWAP_DETACH_MS));
    tud_connect();
    sensorBoard_status_set_sensor(SB_USB_SWAP_STATUS_KEY, swapped);
    /* Queda en la retención de arranque y sale al conectar: la motherboard
     * ve que el conector está invertido (o que no hubo host). */
    ESP_LOGW(TAG, "USB not enumerated in %u ms: D+/D- %s (swap #%lu)",
             (unsigned)SB_USB_AUTOSWAP_TIMEOUT_MS, swapped ? "swapped" : "normal",
             (unsigned long)sb_usb_orient_swap_count(&s_orient));
}

/* CDC escribible = DTR visto (s_cdc_ready) Y enlace listo ahora mismo
 * (tud_cdc_n_connected = montado && !suspendido && DTR). Solo el flag de DTR
 * es pegajoso ante un host que desaparece sin cerrar el puerto; con esta
 * conjunción los frames vuelven a la retención en vez de morir en el FIFO. */
static bool cdc_writable(void)
{
    return s_cdc_ready && tud_cdc_n_connected(TINYUSB_CDC_ACM_0);
}

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

/* Semáforo binario: se da en cada transición ausente→presente del host.
 * Lo consume sensorBoard_comm_wait_host_ready() (heartbeat inmediato). */
static SemaphoreHandle_t s_host_ready_sem = NULL;

/* Único punto que escribe s_cdc_ready. Contexto: tarea TinyUSB. */
static void set_host_ready(bool ready)
{
    const bool was_ready = s_cdc_ready;
    s_cdc_ready = ready;
    if (ready && !was_ready && s_host_ready_sem != NULL) {
        xSemaphoreGive(s_host_ready_sem);
    }
}

static void cdc_line_state_callback(int itf, cdcacm_event_t *event)
{
    (void)itf;
    set_host_ready(event->line_state_changed_data.dtr);
}

/* Eventos de bus de TinyUSB. El DTR sólo cambia cuando el host envía
 * SET_CONTROL_LINE_STATE: al desenchufar el cable o reiniciarse la
 * motherboard NO llega nada por ese camino — sin sensado de VBUS el DCD solo
 * reporta SUSPEND (cesan los SOF). Sin este callback, s_cdc_ready se quedaba
 * en true con el host ausente, los frames iban a un FIFO que nunca drena y
 * la retención de arranque no se usaba. */
static void tinyusb_event_callback(tinyusb_event_t *event, void *arg)
{
    (void)arg;
    switch (event->id) {
    case TINYUSB_EVENT_DETACHED:
#ifdef CONFIG_TINYUSB_SUSPEND_CALLBACK
    case TINYUSB_EVENT_SUSPENDED:
#endif
        set_host_ready(false);
        break;
#ifdef CONFIG_TINYUSB_RESUME_CALLBACK
    case TINYUSB_EVENT_RESUMED:
        /* Un host que suspende y reanuda sin re-abrir el puerto (PC en
         * reposo) conserva el DTR: se recupera del estado de TinyUSB. */
        set_host_ready((tud_cdc_n_get_line_state(TINYUSB_CDC_ACM_0) & 0x01) != 0);
        break;
#endif
    case TINYUSB_EVENT_ATTACHED: /* el DTR llegará después, por line-state */
    default:
        break;
    }
}

/* Reinicio tras perder el host: sin esto, la incubadora se queda sin sensor
 * de aire hasta que alguien desenchufa un cable.
 *
 * Comprobado en banco (2026-09-03): cuando la motherboard se reinicia con el
 * SensorBoard ya conectado, el SensorBoard NO vuelve a enumerar — la
 * motherboard no controla el VBUS, así que el dispositivo nunca ve una
 * desconexión y se queda con la configuración anterior mientras el host
 * arranca de cero creyendo el puerto vacío. Hacía falta ciclar la
 * alimentación a mano. En campo eso significa que un watchdog, una OTA o un
 * brown-out de la motherboard dejan la incubadora sin la variable de control
 * del PID y con el calefactor cortado hasta que alguien vaya físicamente.
 *
 * Reiniciarse es lo que fuerza la re-enumeración desde este lado, que es el
 * único que puede actuar sin cambiar el hardware.
 *
 * La señal de "host presente" es dtr && tud_ready() (montado y no
 * suspendido), no sólo el DTR: ver tinyusb_event_callback. La política (sólo
 * si antes hubo host, 15 s, rearme al recuperar) vive en sb_host_watch, que
 * es función pura y tiene tests Unity. Tras el reinicio el estado vuelve a
 * cero, así que no puede encadenar reinicios: como mucho uno por pérdida. */
static void host_watchdog_task(void *arg)
{
    (void)arg;
    sb_host_watch_t watch;
    sb_host_watch_init(&watch);
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        const uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        if (sb_host_watch_update(&watch, s_cdc_ready, tud_ready(), now)) {
            /* El log no llegará a ninguna parte (no hay host), pero queda en
             * el buffer de retención por si alguien conecta después. */
            ESP_LOGW(TAG, "host perdido %u ms: reinicio para re-enumerar",
                     (unsigned)SB_HOST_LOST_RESTART_MS);
            vTaskDelay(pdMS_TO_TICKS(50));
            esp_restart();
        }
    }
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
            esp_err_t ret =
                tinyusb_cdcacm_read(TINYUSB_CDC_ACM_0, rx_buf, sizeof(rx_buf), &rx_size);
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

    while (off < len && stalls < 10 && cdc_writable()) {
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

/* ── Retención de arranque ─────────────────────────────────── */
/* Sin host (DTR bajo) los frames JSON se guardan en un buffer estático que
 * CONSERVA LOS PRIMEROS (se congela al llenarse — para diagnosticar un
 * arranque importa el principio, no lo último) y se vuelca al conectar.
 * Tras el volcado vuelve a armarse para el siguiente periodo sin host. */
#define SB_BOOT_RING_FRAMES 32

static sb_tx_item_t s_boot_ring[SB_BOOT_RING_FRAMES];
static size_t s_boot_count = 0;

static void boot_ring_push(const sb_tx_item_t *item)
{
    if (s_boot_count >= SB_BOOT_RING_FRAMES) {
        return; /* lleno: se conservan los primeros, el resto se descarta */
    }
    s_boot_ring[s_boot_count++] = *item;
}

static void boot_ring_flush(void)
{
    for (size_t i = 0; i < s_boot_count && cdc_writable(); i++) {
        tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, s_boot_ring[i].frame, s_boot_ring[i].len);
        tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, pdMS_TO_TICKS(50));
    }
    s_boot_count = 0;
}

/* ── TX task: único escritor del endpoint CDC ──────────────── */
static void tx_send_or_retain(const sb_tx_item_t *item)
{
    if (cdc_writable()) {
        if (s_boot_count > 0) {
            boot_ring_flush(); /* lo retenido sale primero, en orden */
        }
        tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, item->frame, item->len);
        tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, pdMS_TO_TICKS(50));
    } else {
        boot_ring_push(item);
    }
}

static void usb_tx_task(void *arg)
{
    (void)arg;
    static sb_tx_item_t item;
    sb_tx_bin_item_t bin;

    for (;;) {
        /* Sin enumeración en plazo: alternar D+/D- (conector invertido) */
        orient_service();
        /* Host recién conectado sin tráfico nuevo: volcar lo retenido */
        if (cdc_writable() && s_boot_count > 0) {
            boot_ring_flush();
        }
        /* El JSON (telemetría/heartbeat/resp) SIEMPRE drena primero */
        if (xQueueReceive(s_tx_queue, &item, 0) == pdTRUE) {
            tx_send_or_retain(&item);
            continue;
        }
        /* Sin JSON pendiente: un binario como mucho */
        if (xQueueReceive(s_tx_bin_queue, &bin, 0) == pdTRUE) {
            if (cdc_writable() && bin.frame != NULL) {
                tx_write_large(bin.frame, bin.len);
            }
            free(bin.frame); /* ownership: SIEMPRE se libera aquí */
            continue;
        }
        /* Nada pendiente: bloquear en la JSON con sondeo corto de la binaria */
        if (xQueueReceive(s_tx_queue, &item, pdMS_TO_TICKS(SB_TX_IDLE_POLL_MS)) == pdTRUE) {
            tx_send_or_retain(&item);
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
    s_host_ready_sem = xSemaphoreCreateBinary();
    if (s_host_ready_sem == NULL) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }

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

    /* Interceptor ANTES de TinyUSB: los logs del propio arranque USB entran
     * en la cola (y de ahí a la retención) en vez de perderse. Hasta que
     * usb_tx_task exista, la cola (depth 8) conserva los primeros frames. */
    esp_log_set_vprintf(sb_log_vprintf);

    const tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG(tinyusb_event_callback);
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

    /* El plazo de enumeración se arma justo antes de que exista quien lo
     * evalúa (usb_tx_task): así no depende del tiempo de init del CDC. */
    sb_usb_orient_init(&s_orient, SB_USB_AUTOSWAP_TIMEOUT_MS, orient_now_ms());
    sensorBoard_status_set_sensor(SB_USB_SWAP_STATUS_KEY, false);

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
    /* Pila mínima: sólo compara dos enteros una vez por segundo. */
    if (xTaskCreate(host_watchdog_task, "host_wd", 2560, NULL, SB_COMM_TASK_PRIO, NULL) !=
        pdPASS) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }

    ESP_LOGI(TAG, "USB CDC ready");
    return ESP_OK;

fail:
    esp_log_set_vprintf(vprintf); /* el interceptor apuntaría a colas muertas */
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
    if (s_host_ready_sem != NULL) {
        vSemaphoreDelete(s_host_ready_sem);
        s_host_ready_sem = NULL;
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
    sb_tx_bin_item_t item = {.frame = frame, .len = encoded};
    if (xQueueSend(s_tx_bin_queue, &item, 0) != pdTRUE) {
        free(frame);
        return ESP_ERR_TIMEOUT; /* binario anterior aún sin drenar */
    }
    return ESP_OK;
}

bool sensorBoard_comm_wait_host_ready(uint32_t timeout_ms)
{
    if (s_host_ready_sem == NULL) {
        return false; /* sin init no hay host que esperar */
    }
    return xSemaphoreTake(s_host_ready_sem, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

esp_err_t sensorBoard_comm_send_json_noblock(const char *json_str)
{
    /* Para contextos que no deben bloquear jamás (dispatcher en usb_rx):
     * bajo saturación de la cola la respuesta se pierde y el emisor
     * reintenta — nunca al revés */
    return send_json_timeout(json_str, 0);
}
