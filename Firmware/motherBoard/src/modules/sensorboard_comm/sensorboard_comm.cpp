#include "sensorboard_comm.h"

#include <Arduino.h>
#include <string.h>

#include "config/task_config.h"
#include "config/telemetry_keys.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "modules/control/alarm_machine.h"
#include "sb_door_state.h"
#include "sb_frame_parser.h"
#include "sb_json_codec.h"
#include "sb_link_state.h"
#include "sb_protocol.h"
#include "usb/cdc_acm_host.h"
#include "usb/usb_host.h"

static const char *TAG = "SB_COMM";

// TinyUSB con VID de Espressif y PID por defecto: para un dispositivo con
// solo la clase CDC el descriptor sale 0x4000 | (1<<0) (usb_descriptors.c de
// esp_tinyusb). Si el SensorBoard fijara alguna vez su propio VID/PID, este
// es el unico sitio que hay que tocar.
#define SB_USB_VID 0x303A
#define SB_USB_PID 0x4001

// La captura JPEG se reserva del monton solo mientras esta en vuelo: esta
// placa NO tiene PSRAM y un buffer permanente de 128 KB no cabe en la RAM
// interna junto al resto del firmware.
#define SB_CAPTURE_MAX_BYTES SB_PROTO_MAX_BINARY_PAYLOAD

static cdc_acm_dev_hdl_t s_cdc = NULL;
static SemaphoreHandle_t s_mutex = NULL;

static SbFrameParser s_parser;
static uint8_t s_json_buf[SB_PROTO_MAX_JSON_PAYLOAD];

static SbLinkState s_link;
static SbDoorState s_door;
static SbSnapshot s_snapshot;

static uint32_t s_next_cmd_id = 1;
static bool s_capture_in_flight = false;
static uint8_t *s_capture_buf = NULL;   // captura completada
static size_t s_capture_len = 0;
static uint8_t *s_incoming_buf = NULL;  // captura anunciada, aun llegando

static void snapshot_lock(void) {
  if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY);
}

static void snapshot_unlock(void) {
  if (s_mutex) xSemaphoreGive(s_mutex);
}

// ── Recepcion ────────────────────────────────────────────────────

static void apply_triple(SbTriple *dst, const bool valid[3],
                         const float value[3]) {
  for (int i = 0; i < 3; i++) {
    dst->valid[i] = valid[i];
    dst->value[i] = value[i];
  }
}

// Prepara el buffer para el frame binario que viene detras de un
// resp:capture ok. Se ejecuta en la tarea del driver CDC, la misma que
// alimenta el parser, asi que cambiar aqui su buffer no compite con nadie.
static void arm_incoming_capture(uint32_t size) {
  if (size == 0 || size > SB_CAPTURE_MAX_BYTES) {
    ESP_LOGW(TAG, "capture size %u fuera de rango", (unsigned)size);
    s_capture_in_flight = false;
    return;
  }
  s_incoming_buf = (uint8_t *)malloc(size);
  if (!s_incoming_buf) {
    ESP_LOGE(TAG, "sin memoria para %u B de captura", (unsigned)size);
    s_capture_in_flight = false;
    return;
  }
  s_parser.payload_buf = s_incoming_buf;
  s_parser.payload_cap = size;
}

static void release_incoming_capture(void) {
  if (s_incoming_buf) {
    free(s_incoming_buf);
    s_incoming_buf = NULL;
  }
  s_parser.payload_buf = s_json_buf;
  s_parser.payload_cap = sizeof(s_json_buf);
}

static void handle_message(const SbMessage *m, uint32_t now_ms) {
  switch (m->kind) {
    case SB_MSG_HEARTBEAT:
      sb_link_state_note_heartbeat(&s_link, now_ms);
      break;

    case SB_MSG_SENSOR_DATA:
      snapshot_lock();
      apply_triple(&s_snapshot.temp, m->temp_valid, m->temp);
      apply_triple(&s_snapshot.hum, m->hum_valid, m->hum);
      s_snapshot.lux_valid = m->lux_valid;
      s_snapshot.lux = m->lux;
      snapshot_unlock();
      break;

    case SB_MSG_DOOR_OPEN:
    case SB_MSG_DOOR_CLOSED: {
      const bool open = (m->kind == SB_MSG_DOOR_OPEN);
      sb_door_state_note_event(&s_door, open, now_ms);
      snapshot_lock();
      s_snapshot.door_known = true;
      s_snapshot.door_open = open;
      s_snapshot.door_faulty = sb_door_state_is_faulty(&s_door, now_ms);
      snapshot_unlock();
      break;
    }

    case SB_MSG_SOUND_LEVEL:
      snapshot_lock();
      s_snapshot.dba_valid = m->dba_valid;
      s_snapshot.dba = m->dba;
      snapshot_unlock();
      break;

    case SB_MSG_CAPTURE_RESP:
      if (m->resp_ok) {
        arm_incoming_capture(m->capture_size);
      } else {
        ESP_LOGW(TAG, "capture rechazada: %s", m->msg);
        s_capture_in_flight = false;
      }
      break;

    case SB_MSG_LOG:
      ESP_LOGI(TAG, "[SB] %s", m->msg);
      break;

    case SB_MSG_STATUS_RESP:
    case SB_MSG_UNKNOWN:
    default:
      break;
  }
}

static void on_frame(uint8_t type, const uint8_t *payload, uint32_t len,
                     void *ctx) {
  (void)ctx;
  const uint32_t now = millis();

  if (type == SB_PROTO_TYPE_JPEG) {
    if (s_incoming_buf && payload == s_incoming_buf) {
      if (s_capture_buf) free(s_capture_buf);
      s_capture_buf = s_incoming_buf;
      s_capture_len = len;
      s_incoming_buf = NULL;  // la propiedad pasa a s_capture_buf
      s_parser.payload_buf = s_json_buf;
      s_parser.payload_cap = sizeof(s_json_buf);
      ESP_LOGI(TAG, "captura recibida: %u B", (unsigned)len);
    }
    s_capture_in_flight = false;
    return;
  }

  SbMessage msg;
  if (!sb_json_decode(payload, len, &msg)) {
    ESP_LOGW(TAG, "payload JSON invalido (%u B)", (unsigned)len);
    return;
  }
  handle_message(&msg, now);
}

static void on_frame_error(void *ctx) {
  (void)ctx;
  ESP_LOGW(TAG, "frame descartado (CRC o longitud invalida)");
}

static bool on_cdc_data(const uint8_t *data, size_t len, void *arg) {
  (void)arg;
  sb_frame_parser_feed(&s_parser, data, len);
  return true;  // datos consumidos: el driver puede vaciar su buffer
}

static void on_cdc_event(const cdc_acm_host_dev_event_data_t *event,
                         void *arg) {
  (void)arg;
  switch (event->type) {
    case CDC_ACM_HOST_DEVICE_DISCONNECTED:
      ESP_LOGW(TAG, "SensorBoard desconectado del USB");
      cdc_acm_host_close(event->data.cdc_hdl);
      s_cdc = NULL;
      release_incoming_capture();
      s_capture_in_flight = false;
      break;
    case CDC_ACM_HOST_ERROR:
      ESP_LOGW(TAG, "error del enlace CDC: %d", event->data.error);
      break;
    default:
      break;
  }
}

// ── Demonio del USB Host ─────────────────────────────────────────
// cdc_acm_host_install() crea su propia tarea de cliente, pero los eventos
// de la libreria USB Host siguen siendo responsabilidad de la aplicacion.
static void usb_host_daemon_task(void *pv) {
  (void)pv;
  for (;;) {
    uint32_t flags = 0;
    usb_host_lib_handle_events(portMAX_DELAY, &flags);
    if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
      usb_host_device_free_all();
    }
  }
}

// ── API publica ──────────────────────────────────────────────────

void sensorboard_comm_init(void) {
  s_mutex = xSemaphoreCreateMutex();
  memset(&s_snapshot, 0, sizeof(s_snapshot));
  sb_link_state_init(&s_link);
  sb_door_state_init(&s_door);
  sb_frame_parser_init(&s_parser, s_json_buf, sizeof(s_json_buf), on_frame,
                       on_frame_error, NULL);

  const usb_host_config_t host_config = {
      .skip_phy_setup = false,
      .intr_flags = ESP_INTR_FLAG_LEVEL1,
  };
  esp_err_t err = usb_host_install(&host_config);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "usb_host_install fallo: %d", err);
    return;
  }

  xTaskCreatePinnedToCore(usb_host_daemon_task, "USB_HOST_D", 4096, NULL,
                          SENSORBOARD_TASK_PRIORITY, NULL, CORE_ID_FREERTOS);

  // Stack propio mayor que el de por defecto: el callback de datos decodifica
  // el JSON en la tarea del driver y ArduinoJson necesita 1 KB de documento.
  const cdc_acm_host_driver_config_t driver_config = {
      .driver_task_stack_size = 6144,
      .driver_task_priority = SENSORBOARD_TASK_PRIORITY,
      .xCoreID = CORE_ID_FREERTOS,
      .new_dev_cb = NULL,
  };
  err = cdc_acm_host_install(&driver_config);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "cdc_acm_host_install fallo: %d", err);
  }
}

void sensorboard_comm_task(void *pv) {
  (void)pv;
  const cdc_acm_host_device_config_t dev_config = {
      .connection_timeout_ms = 1000,
      .out_buffer_size = SB_PROTO_MAX_JSON_PAYLOAD,
      .in_buffer_size = 512,
      .event_cb = on_cdc_event,
      .data_cb = on_cdc_data,
      .user_arg = NULL,
  };

  for (;;) {
    if (s_cdc == NULL) {
      cdc_acm_dev_hdl_t hdl = NULL;
      if (cdc_acm_host_open(SB_USB_VID, SB_USB_PID, 0, &dev_config, &hdl) ==
          ESP_OK) {
        s_cdc = hdl;
        ESP_LOGI(TAG, "SensorBoard conectado por USB");
      }
    }

    const uint32_t now = millis();
    const bool link_ok = sb_link_state_is_connected(&s_link, now);
    const bool door_faulty = sb_door_state_is_faulty(&s_door, now);

    snapshot_lock();
    s_snapshot.link_ok = link_ok;
    s_snapshot.door_faulty = door_faulty;
    snapshot_unlock();

    alarm_machine_condition(ALARM_SENSORBOARD_LINK_LOST, !link_ok, now);
    alarm_machine_condition(ALARM_SENSORBOARD_DOOR_FAULT, door_faulty, now);

    vTaskDelay(pdMS_TO_TICKS(SENSORBOARD_TASK_PERIOD_MS));
  }
}

bool sensorboard_comm_connected(void) {
  SbSnapshot s;
  sensorboard_get_snapshot(&s);
  return s.link_ok;
}

void sensorboard_get_snapshot(SbSnapshot *out) {
  snapshot_lock();
  *out = s_snapshot;
  snapshot_unlock();
}

bool sensorboard_capture_request(void) {
  if (s_cdc == NULL || s_capture_in_flight) return false;
  if (!sb_link_state_is_connected(&s_link, millis())) return false;

  uint8_t json[SB_PROTO_MAX_JSON_PAYLOAD];
  const size_t json_len =
      sb_json_encode_capture_cmd(s_next_cmd_id++, json, sizeof(json));
  if (json_len == 0) return false;

  uint8_t frame[SB_PROTO_FRAME_HEADER_SIZE + SB_PROTO_MAX_JSON_PAYLOAD +
                SB_PROTO_FRAME_CRC_SIZE];
  const size_t frame_len = sb_frame_encode(SB_PROTO_TYPE_JSON, json,
                                           (uint32_t)json_len, frame,
                                           sizeof(frame));
  if (frame_len == 0) return false;

  s_capture_in_flight = true;
  if (cdc_acm_host_data_tx_blocking(s_cdc, frame, frame_len, 200) != ESP_OK) {
    s_capture_in_flight = false;
    return false;
  }
  return true;
}

bool sensorboard_capture_result(const uint8_t **jpeg, size_t *len) {
  if (!s_capture_buf || s_capture_len == 0) return false;
  *jpeg = s_capture_buf;
  *len = s_capture_len;
  return true;
}

void sensorboard_add_telemetry(JsonObject &json) {
  SbSnapshot s;
  sensorboard_get_snapshot(&s);

  json[SB_LINK_OK_KEY] = s.link_ok;
  if (!s.link_ok) return;  // con el enlace caido los ultimos valores son viejos

  static const char *const kTempKeys[3] = {SB_TEMP0_KEY, SB_TEMP1_KEY,
                                           SB_TEMP2_KEY};
  static const char *const kHumKeys[3] = {SB_HUM0_KEY, SB_HUM1_KEY,
                                          SB_HUM2_KEY};
  for (int i = 0; i < 3; i++) {
    if (s.temp.valid[i]) json[kTempKeys[i]] = s.temp.value[i];
    if (s.hum.valid[i]) json[kHumKeys[i]] = s.hum.value[i];
  }
  if (s.lux_valid) json[SB_LUX_KEY] = s.lux;
  if (s.dba_valid) json[SB_DB_KEY] = s.dba;
  if (s.door_known) json[SB_DOOR_OPEN_KEY] = s.door_open;
  json[SB_DOOR_FAULT_KEY] = s.door_faulty;
}
