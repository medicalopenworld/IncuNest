#include "sensorboard_comm.h"

#include <Arduino.h>
#include <Wire.h>
#include <string.h>

#include "config/task_config.h"
#include "config/telemetry_keys.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "main.h"
#include "modules/control/alarm_machine.h"
#include "sb_door_state.h"
#include "sb_env_fusion.h"
#include "sb_frame_parser.h"
#include "sb_json_codec.h"
#include "sb_link_state.h"
#include "sb_protocol.h"
#include "usb/cdc_acm_host.h"
#include "usb/usb_host.h"

static const char *TAG = "SB_COMM";

extern IncuNest_parameters in3;
extern long lastSuccesfullSensorUpdate[SENSOR_TEMP_QTY];

// TinyUSB con VID de Espressif y PID por defecto: para un dispositivo con
// solo la clase CDC el descriptor sale 0x4000 | (1<<0). Verificado contra los
// bytes del descriptor del binario del SensorBoard, no solo contra su
// Kconfig. Es un contrato entre placas: si el SensorBoard habilitara otra
// clase USB (MSC, HID) su PID cambiaria y este open dejaria de encontrarlo.
#define SB_USB_VID 0x303A
#define SB_USB_PID 0x4001

// Tope realista para un QVGA con calidad 12 (8-25 KB tipicos), no los 128 KB
// que permite el protocolo: esta placa no tiene PSRAM y el buffer sale de la
// DRAM interna, compartida con WiFi, GPRS y el TFT.
#define SB_CAPTURE_MAX_BYTES (48u * 1024u)

// Si tras la resp de capture el frame binario no llega, hay que desarmar: el
// SensorBoard puede quedarse sin enviarlo (cola binaria ocupada, PSRAM sin
// hueco, frame abortado tras 10 stalls) y su comentario habla de un reintento
// de la motherboard que antes no existia.
#define SB_CAPTURE_TIMEOUT_MS 10000u

// Margen de arranque antes de declarar el enlace perdido, y SOLO mientras no
// se haya visto ningun heartbeat: el primer heartbeat del SensorBoard llega a
// los 30 s (su vTaskDelay precede al primer envio) y antes hay que enumerar.
// Sin este margen sonaba una alarma MEDIA audible en cada encendido. Mismo
// criterio que checkHmiLink() en security.cpp.
#define SB_LINK_BOOT_GRACE_MS 60000u

// Cada cuantos intentos fallidos de apertura se deja una traza: sin esto un
// fallo de VID/PID era indistinguible de un cable suelto, pero un log por
// intento ensucia el diagnostico para siempre.
#define SB_OPEN_LOG_EVERY 30u

// s_mutex protege TODO lo de abajo. Lo tocan tres contextos: la tarea del
// driver CDC (callbacks de datos y de evento), esta tarea del modulo, y
// cualquier tarea que pida una captura. La tarea del modulo es la UNICA que
// abre, cierra y transmite, y nunca hace esas llamadas con el mutex tomado:
// cdc_acm_host_open()/tx_blocking() esperan a que progrese la tarea del
// driver, asi que bloquearla seria un abrazo mortal.
static SemaphoreHandle_t s_mutex = NULL;

static cdc_acm_dev_hdl_t s_cdc = NULL;
static cdc_acm_dev_hdl_t s_cdc_to_close = NULL;

static SbFrameParser s_parser;
static uint8_t s_json_buf[SB_PROTO_MAX_JSON_PAYLOAD];

static SbLinkState s_link;
static SbDoorState s_door;
static SbSnapshot s_snapshot;

// Posiciones que sostienen la ultima medida de aire aceptada. Sale por
// telemetria: una perdida progresiva de redundancia debe verse en remoto
// antes de quedarse sin medida.
static uint8_t s_env_used = 0;

static uint32_t s_task_start_ms = 0;
static bool s_have_uptime = false;
static uint32_t s_last_uptime = 0;

static uint32_t s_next_cmd_id = 1;
static bool s_capture_wanted = false;
static bool s_capture_in_flight = false;
static uint32_t s_capture_armed_ms = 0;

static uint8_t *s_incoming_buf = NULL;  // captura anunciada, aun llegando
static size_t s_incoming_cap = 0;
static uint8_t *s_capture_buf = NULL;   // captura completada
static size_t s_capture_len = 0;

static void lock(void) {
  if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY);
}

static void unlock(void) {
  if (s_mutex) xSemaphoreGive(s_mutex);
}

// ── Recepcion (todo esto corre con el mutex tomado) ──────────────

static void apply_triple(SbTriple *dst, const bool valid[3],
                         const float value[3]) {
  for (int i = 0; i < 3; i++) {
    dst->valid[i] = valid[i];
    dst->value[i] = value[i];
  }
}

static void release_incoming_capture(void) {
  if (s_incoming_buf) {
    free(s_incoming_buf);
    s_incoming_buf = NULL;
  }
  s_incoming_cap = 0;
  sb_frame_parser_set_buffer(&s_parser, s_json_buf, sizeof(s_json_buf));
}

// Prepara el buffer del frame binario que viene detras de un resp:capture ok.
static void arm_incoming_capture(uint32_t size) {
  if (s_incoming_buf) {
    // Dos resp seguidas sin binario entre medias: sin esto se perdia la
    // reserva anterior y la fuga era acumulativa.
    ESP_LOGW(TAG, "captura anterior sin completar, se descarta");
    release_incoming_capture();
  }
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
  s_incoming_cap = size;
  sb_frame_parser_set_buffer(&s_parser, s_incoming_buf, size);
}

// Un reinicio del SensorBoard con el USB enumerado no genera ningun evento
// USB: el uptime que retrocede es la unica pista. Las lecturas anteriores
// dejan de valer, incluido el estado de puerta.
static void invalidate_after_reboot(void) {
  ESP_LOGW(TAG, "el SensorBoard se ha reiniciado: se invalida el estado");
  s_snapshot.env_seen = false;
  s_snapshot.sound_seen = false;
  s_snapshot.door_known = false;
  s_snapshot.last_env_ms = 0;
  s_snapshot.last_door_ms = 0;
  s_snapshot.last_sound_ms = 0;
  sb_door_state_init(&s_door);
  release_incoming_capture();
  s_capture_in_flight = false;
}

static void handle_message(const SbMessage *m, uint32_t now_ms) {
  switch (m->kind) {
    case SB_MSG_HEARTBEAT:
      if (m->uptime_valid) {
        if (s_have_uptime && m->uptime < s_last_uptime) {
          invalidate_after_reboot();
        }
        s_have_uptime = true;
        s_last_uptime = m->uptime;
      }
      sb_link_state_note_heartbeat(&s_link, now_ms);
      break;

    case SB_MSG_SENSOR_DATA:
      apply_triple(&s_snapshot.temp, m->temp_valid, m->temp);
      apply_triple(&s_snapshot.hum, m->hum_valid, m->hum);
      s_snapshot.lux_valid = m->lux_valid;
      s_snapshot.lux = m->lux;
      s_snapshot.last_env_ms = now_ms;
      s_snapshot.env_seen = true;
      break;

    case SB_MSG_DOOR_OPEN:
    case SB_MSG_DOOR_CLOSED: {
      const bool open = (m->kind == SB_MSG_DOOR_OPEN);
      sb_door_state_note_event(&s_door, open, now_ms);
      s_snapshot.door_known = true;
      s_snapshot.door_open = open;
      s_snapshot.door_faulty = sb_door_state_evaluate(&s_door, now_ms);
      s_snapshot.last_door_ms = now_ms;
      break;
    }

    case SB_MSG_SOUND_LEVEL:
      s_snapshot.dba_valid = m->dba_valid;
      s_snapshot.dba = m->dba;
      s_snapshot.last_sound_ms = now_ms;
      s_snapshot.sound_seen = true;
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
      break;

    case SB_MSG_UNKNOWN:
    default:
      // No se ignora en silencio: un cmd que no reconocemos suele ser un
      // SensorBoard mas nuevo que este firmware.
      ESP_LOGD(TAG, "mensaje no reconocido del SensorBoard");
      break;
  }
}

static void on_frame(uint8_t type, const uint8_t *payload, uint32_t len,
                     void *ctx) {
  (void)ctx;
  const uint32_t now = millis();

  if (type == SB_PROTO_TYPE_JPEG) {
    // Se identifica por buffer Y tamano: el SensorBoard drena SIEMPRE el JSON
    // antes que los binarios, asi que "el siguiente frame" no es garantia de
    // nada. Un binario que no esperabamos NO desarma la captura en vuelo.
    if (s_incoming_buf && payload == s_incoming_buf && len == s_incoming_cap) {
      if (s_capture_buf) free(s_capture_buf);
      s_capture_buf = s_incoming_buf;
      s_capture_len = len;
      s_incoming_buf = NULL;
      s_incoming_cap = 0;
      sb_frame_parser_set_buffer(&s_parser, s_json_buf, sizeof(s_json_buf));
      s_capture_in_flight = false;
      ESP_LOGI(TAG, "captura recibida: %u B", (unsigned)len);
    } else {
      ESP_LOGW(TAG, "frame binario inesperado (%u B), descartado",
               (unsigned)len);
    }
    return;
  }

  if (type != SB_PROTO_TYPE_JSON) {
    ESP_LOGW(TAG, "tipo de frame desconocido: 0x%02X", type);
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
  ESP_LOGW(TAG, "frame descartado (CRC, longitud o tipo invalidos)");
}

static bool on_cdc_data(const uint8_t *data, size_t len, void *arg) {
  (void)arg;
  lock();
  sb_frame_parser_feed(&s_parser, data, len);
  unlock();
  return true;  // datos consumidos: el driver puede vaciar su buffer
}

static void on_cdc_event(const cdc_acm_host_dev_event_data_t *event,
                         void *arg) {
  (void)arg;
  switch (event->type) {
    case CDC_ACM_HOST_DEVICE_DISCONNECTED:
      ESP_LOGW(TAG, "SensorBoard desconectado del USB");
      lock();
      // El cierre lo hace la tarea del modulo: es la unica duena del handle,
      // y cerrar aqui abria una ventana en la que un tx en curso escribia
      // sobre un objeto ya liberado.
      s_cdc_to_close = event->data.cdc_hdl;
      s_cdc = NULL;
      release_incoming_capture();
      s_capture_in_flight = false;
      s_capture_wanted = false;
      // Hay evidencia DEFINITIVA de que la placa se ha ido: no tiene sentido
      // esperar los 90 s del timeout inferencial para decirlo.
      sb_link_state_mark_down(&s_link);
      s_snapshot.env_seen = false;
      s_snapshot.sound_seen = false;
      s_snapshot.door_known = false;
      s_have_uptime = false;
      unlock();
      break;

    case CDC_ACM_HOST_ERROR:
      ESP_LOGW(TAG, "error del enlace CDC: %d", event->data.error);
      break;

    default:
      break;
  }
}

// ── Demonio del USB Host ─────────────────────────────────────────
// cdc_acm_host_install() crea su propia tarea de cliente, pero los eventos de
// la libreria USB Host siguen siendo responsabilidad de la aplicacion.
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

// Cada dispositivo que el host llega a enumerar, con VID/PID/velocidad.
// Diagnostico de campo del enlace: distingue "no enumera nada" (cable,
// alimentacion, orientacion D+/D- del SensorBoard HW4) de "enumera pero no es
// el SensorBoard" (VID/PID distinto) sin necesidad de un analizador USB. La
// pila host de Arduino (ESP-IDF 4.4.6) solo loguea sus propios errores.
static void on_new_usb_device(usb_device_handle_t usb_dev) {
  const usb_device_desc_t *desc = NULL;
  usb_device_info_t info = {};
  usb_host_get_device_descriptor(usb_dev, &desc);
  usb_host_device_info(usb_dev, &info);
  ESP_LOGI(TAG, "dispositivo USB enumerado: VID 0x%04X PID 0x%04X speed=%d addr=%u",
           desc ? desc->idVendor : 0, desc ? desc->idProduct : 0, (int)info.speed,
           (unsigned)info.dev_addr);
}

// ── Apertura y transmision (solo desde la tarea del modulo) ──────

static void try_open(void) {
  static uint32_t attempts = 0;

  const cdc_acm_host_device_config_t dev_config = {
      .connection_timeout_ms = 1000,
      // Un frame de payload maximo son 256 + 9 bytes de envoltura: con 256 el
      // driver rechazaba por tamano cualquier comando al limite.
      .out_buffer_size = SB_PROTO_FRAME_HEADER_SIZE +
                         SB_PROTO_MAX_JSON_PAYLOAD + SB_PROTO_FRAME_CRC_SIZE,
      .in_buffer_size = 512,
      .event_cb = on_cdc_event,
      .data_cb = on_cdc_data,
      .user_arg = NULL,
  };

  cdc_acm_dev_hdl_t hdl = NULL;
  if (cdc_acm_host_open(SB_USB_VID, SB_USB_PID, 0, &dev_config, &hdl) !=
      ESP_OK) {
    if (++attempts % SB_OPEN_LOG_EVERY == 0) {
      ESP_LOGW(TAG, "sin SensorBoard en VID 0x%04X PID 0x%04X (%u intentos)",
               SB_USB_VID, SB_USB_PID, (unsigned)attempts);
    }
    return;
  }

  // DTR es obligatorio, no cortesia: el SensorBoard guarda TODOS sus caminos
  // de transmision con la senal de line state (s_cdc_ready en su
  // sensorBoard_comm.c). Sin asertarlo el enlace enumera, abre y no llega ni
  // un byte -- y el unico sintoma seria la alarma de enlace perdido.
  if (cdc_acm_host_set_control_line_state(hdl, true, true) != ESP_OK) {
    ESP_LOGE(TAG, "no se pudo asertar DTR/RTS: se cierra y se reintenta");
    cdc_acm_host_close(hdl);
    return;
  }

  attempts = 0;
  lock();
  s_cdc = hdl;
  // Un stream nuevo nunca continua el frame del anterior.
  sb_frame_parser_set_buffer(&s_parser, s_json_buf, sizeof(s_json_buf));
  unlock();
  ESP_LOGI(TAG, "SensorBoard conectado por USB");
}

static void send_capture_cmd(cdc_acm_dev_hdl_t hdl, uint32_t id) {
  uint8_t json[SB_PROTO_MAX_JSON_PAYLOAD];
  const size_t json_len = sb_json_encode_capture_cmd(id, json, sizeof(json));
  if (json_len == 0) return;

  uint8_t frame[SB_PROTO_FRAME_HEADER_SIZE + SB_PROTO_MAX_JSON_PAYLOAD +
                SB_PROTO_FRAME_CRC_SIZE];
  const size_t frame_len = sb_frame_encode(SB_PROTO_TYPE_JSON, json,
                                           (uint32_t)json_len, frame,
                                           sizeof(frame));
  if (frame_len == 0) return;

  if (cdc_acm_host_data_tx_blocking(hdl, frame, frame_len, 200) != ESP_OK) {
    ESP_LOGW(TAG, "no se pudo enviar el comando capture");
    lock();
    s_capture_in_flight = false;
    unlock();
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

  // Los pines 19/20 venian de hacer de bus I2C2 en el sondeo de arranque: hay
  // que soltarlos antes de que el PHY los reclame como D-/D+.
  Wire1.end();
  gpio_reset_pin(GPIO_NUM_19);
  gpio_reset_pin(GPIO_NUM_20);

  // El driver CDC-ACM emite un ESP_LOGE por CADA intento de apertura fallido
  // ("USB device with VID:... not found"), es decir ~30 lineas de ERROR por
  // minuto, para siempre, en cuanto el SensorBoard no este presente o su
  // conector falle. Verificado en banco (2026-09-02). Se silencia su tag: la
  // misma informacion la da try_open() cada SB_OPEN_LOG_EVERY intentos, y los
  // errores del enlace ya abierto llegan por on_cdc_event().
  esp_log_level_set("cdc_acm", ESP_LOG_NONE);

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
      .new_dev_cb = on_new_usb_device,
  };
  err = cdc_acm_host_install(&driver_config);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "cdc_acm_host_install fallo: %d", err);
  }
}

void sensorboard_comm_task(void *pv) {
  (void)pv;
  s_task_start_ms = millis();

  for (;;) {
    const uint32_t now = millis();

    cdc_acm_dev_hdl_t to_close = NULL;
    cdc_acm_dev_hdl_t hdl = NULL;
    bool need_open = false;
    bool send_capture = false;
    bool capture_expired = false;
    uint32_t capture_id = 0;
    bool link_ok = false;
    bool door_faulty = false;
    bool ever_seen_heartbeat = false;

    lock();
    if (s_cdc_to_close) {
      to_close = s_cdc_to_close;
      s_cdc_to_close = NULL;
    }

    if (s_capture_in_flight &&
        (uint32_t)(now - s_capture_armed_ms) > SB_CAPTURE_TIMEOUT_MS) {
      release_incoming_capture();
      s_capture_in_flight = false;
      s_capture_wanted = false;
      capture_expired = true;
    }

    hdl = s_cdc;
    need_open = (hdl == NULL);
    if (!need_open && s_capture_wanted && !s_capture_in_flight) {
      s_capture_wanted = false;
      s_capture_in_flight = true;
      s_capture_armed_ms = now;
      capture_id = s_next_cmd_id++;
      send_capture = true;
    }

    // evaluate() y no is_connected(): al caducar deja el estado caido de
    // forma permanente, para que el vuelco de millis() no resucite un enlace
    // muerto hace semanas. Se lee has_seen_heartbeat ANTES de evaluar, que es
    // lo que distingue "nunca ha hablado" (margen de arranque) de "hablaba y
    // se ha callado" (alarma inmediata).
    ever_seen_heartbeat = s_link.has_seen_heartbeat;
    link_ok = sb_link_state_evaluate(&s_link, now);
    door_faulty = sb_door_state_evaluate(&s_door, now);
    s_snapshot.link_ok = link_ok;
    s_snapshot.door_faulty = door_faulty;
    unlock();

    if (capture_expired) {
      ESP_LOGW(TAG, "captura caducada sin recibir el JPEG: se desarma");
    }
    // Fuera del mutex a proposito: estas llamadas esperan a que progrese la
    // tarea del driver CDC, que a su vez puede estar esperando el mutex.
    if (to_close) cdc_acm_host_close(to_close);
    if (need_open) {
      try_open();
    } else if (send_capture) {
      send_capture_cmd(hdl, capture_id);
    }

    const bool in_boot_grace =
        !ever_seen_heartbeat &&
        (uint32_t)(now - s_task_start_ms) < SB_LINK_BOOT_GRACE_MS;
    alarm_machine_condition(ALARM_SENSORBOARD_LINK_LOST,
                            !link_ok && !in_boot_grace, now);
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
  lock();
  *out = s_snapshot;
  unlock();
}

bool sensorboard_apply_room_sensor(void) {
  SbSnapshot s;
  sensorboard_get_snapshot(&s);

  if (!s.link_ok || !s.env_seen) return false;
  const uint32_t age = (uint32_t)(millis() - s.last_env_ms);
  if (age > SB_ENV_STALE_MS) return false;

  // Sin cribado: las tres lecturas viajan crudas a la nube y el cribado lo
  // hara la motherboard mas adelante. Aqui solo se elige CUAL gobierna el
  // lazo, y se elige la mediana porque de esta variable comen el PID, el
  // corte termico y la alarma de desviacion -- ver sb_env_fusion.h.
  const SbFusion temp =
      sb_fuse(s.temp.valid, s.temp.value, SB_ENV_TEMP_MIN_C, SB_ENV_TEMP_MAX_C);
  if (!temp.valid) {
    ESP_LOGW(TAG, "ninguna temperatura de aire plausible");
    return false;
  }
  const SbFusion hum = sb_fuse(s.hum.valid, s.hum.value, 0.0f, 100.0f);

  // Escrituras hechas desde la tarea de sensores, la misma que las hace en el
  // camino I2C: el enlace USB no introduce escrituras concurrentes sobre
  // variables clinicas.
  in3.temperature[ROOM_DIGITAL_TEMP_SENSOR] = temp.value;
  if (hum.valid) {
    in3.humidity[ROOM_DIGITAL_HUM_SENSOR] = hum.value;
  }
  // Cuantas posiciones sostienen la medida: perder redundancia en silencio es
  // lo que hace que una averia progresiva no se vea venir en remoto.
  s_env_used = temp.used;
  lastSuccesfullSensorUpdate[ROOM_DIGITAL_TEMP_SENSOR] = millis();
  return true;
}

bool sensorboard_capture_request(void) {
  bool queued = false;
  lock();
  if (s_snapshot.link_ok && !s_capture_in_flight && !s_capture_wanted) {
    s_capture_wanted = true;
    queued = true;
  }
  unlock();
  return queued;
}

bool sensorboard_capture_take(uint8_t **jpeg, size_t *len) {
  bool taken = false;
  lock();
  if (s_capture_buf && s_capture_len > 0) {
    *jpeg = s_capture_buf;
    *len = s_capture_len;
    s_capture_buf = NULL;
    s_capture_len = 0;
    taken = true;
  }
  unlock();
  return taken;
}

void sensorboard_capture_free(uint8_t *jpeg) {
  if (jpeg) free(jpeg);
}

void sensorboard_add_telemetry(JsonObject &json) {
  SbSnapshot s;
  sensorboard_get_snapshot(&s);
  const uint32_t now = millis();

  json[SB_LINK_OK_KEY] = s.link_ok;
  if (!s.link_ok) return;  // con el enlace caido los ultimos valores son viejos

  // Las TRES posiciones crudas, tal como llegan. El valor que gobierna el lazo
  // sale por Air_temp como en cualquier equipo; estas son la materia prima con
  // la que disenar el cribado que hara la motherboard mas adelante, y la unica
  // forma de ver en remoto que un sensor se esta desviando de sus companeros.
  if (s.env_seen && (uint32_t)(now - s.last_env_ms) <= SB_ENV_STALE_MS) {
    static const char *const kTempKeys[3] = {SB_TEMP0_KEY, SB_TEMP1_KEY,
                                             SB_TEMP2_KEY};
    static const char *const kHumKeys[3] = {SB_HUM0_KEY, SB_HUM1_KEY,
                                            SB_HUM2_KEY};
    for (int i = 0; i < 3; i++) {
      if (s.temp.valid[i]) {
        json[kTempKeys[i]] =
            roundSignificantDigits(s.temp.value[i], TELEMETRIES_DECIMALS);
      }
      if (s.hum.valid[i]) {
        json[kHumKeys[i]] =
            roundSignificantDigits(s.hum.value[i], TELEMETRIES_DECIMALS);
      }
    }
  }

  if (s.env_seen && (uint32_t)(now - s.last_env_ms) <= SB_ENV_STALE_MS &&
      s.lux_valid) {
    json[SB_LUX_KEY] = roundSignificantDigits(s.lux, TELEMETRIES_DECIMALS);
  }
  if (s.sound_seen && (uint32_t)(now - s.last_sound_ms) <= SB_SOUND_STALE_MS &&
      s.dba_valid) {
    json[SB_DB_KEY] = roundSignificantDigits(s.dba, TELEMETRIES_DECIMALS);
  }
  if (s.door_known && (uint32_t)(now - s.last_door_ms) <= SB_DOOR_STALE_MS) {
    json[SB_DOOR_OPEN_KEY] = s.door_open;
  }
  json[SB_ENV_USED_KEY] = s_env_used;
  json[SB_DOOR_FAULT_KEY] = s.door_faulty;
}
