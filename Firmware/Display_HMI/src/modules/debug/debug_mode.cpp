#include "modules/debug/debug_mode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "main.h"
#include "tasks/CommTask.h"

static const char *TAG = "hmi_debug";

// Lecturas del GT911 que el bus no contesto (components/incunest_gt911). Un
// numero que crece con la interfaz moviendose sola senala al I2C del tactil,
// no al panel.
extern "C" uint32_t gt911_read_failures;
extern "C" uint32_t gt911_press_events;
extern "C" uint16_t gt911_last_x;
extern "C" uint16_t gt911_last_y;
extern "C" uint8_t gt911_touched_now;

// Ver la regla 1 en la cabecera: no se persiste.
static volatile bool s_enabled = false;
static volatile bool s_link_muted = false;

// --------------------------------------------------------------------------
// Cola de inyeccion
//
// La drena Comm_Task (ver Comm_DebugDrainInjected en CommTask.cpp), no el
// manejador HTTP. El porque esta en la cabecera.
// --------------------------------------------------------------------------
#define DEBUG_INJECT_LINE_MAX 192
#define DEBUG_INJECT_DEPTH 8

typedef struct {
  char text[DEBUG_INJECT_LINE_MAX];
} InjectedLine;

static QueueHandle_t s_inject_q = NULL;

bool debug_mode_enabled(void) { return s_enabled; }

void debug_mode_set(bool on) {
  if (on == s_enabled) {
    return;
  }
  if (!on) {
    // Apagarlo retira TODO lo simulado. El enlace falseado es lo primero: es
    // lo unico que puede dejar al display enseñando un aviso que no existe.
    s_link_muted = false;
    if (s_inject_q != NULL) {
      xQueueReset(s_inject_q);
    }
  }
  s_enabled = on;
  ESP_LOGW(TAG, "%s", on ? "MODO DEPURACION ENCENDIDO - el estado mostrado "
                           "puede estar simulado"
                         : "modo depuracion apagado, simulaciones retiradas");
}

bool debug_inject_line(const char *line) {
  if (!s_enabled || line == NULL) {
    return false;
  }
  const size_t len = strlen(line);
  if (len == 0 || len >= DEBUG_INJECT_LINE_MAX) {
    return false;
  }
  // Mismo filtro que el camino real: una linea que no lleva el prefijo no la
  // habria mirado nadie, y aceptarla aqui seria probar algo que no existe.
  if (strncmp(line, EXPECTED_PREFIX, strlen(EXPECTED_PREFIX)) != 0) {
    return false;
  }
  if (s_inject_q == NULL) {
    s_inject_q = xQueueCreate(DEBUG_INJECT_DEPTH, sizeof(InjectedLine));
    if (s_inject_q == NULL) {
      return false;
    }
  }
  InjectedLine item;
  memset(&item, 0, sizeof(item));
  memcpy(item.text, line, len);
  return xQueueSend(s_inject_q, &item, 0) == pdTRUE;
}

// La llama Comm_Task. Devuelve true y deja la linea en `out` si habia una.
extern "C" bool debug_inject_take(char *out, size_t out_len) {
  if (s_inject_q == NULL || out == NULL || out_len == 0) {
    return false;
  }
  InjectedLine item;
  if (xQueueReceive(s_inject_q, &item, 0) != pdTRUE) {
    return false;
  }
  snprintf(out, out_len, "%s", item.text);
  return true;
}

void debug_link_mute_set(bool muted) {
  // Solo se puede FALSEAR el enlace con el modo encendido; retirarlo, siempre.
  if (muted && !s_enabled) {
    return;
  }
  s_link_muted = muted;
  ESP_LOGW(TAG, "enlace con la placa %s",
           muted ? "SIMULADO COMO PERDIDO" : "de vuelta a la realidad");
}

bool debug_link_mute_get(void) { return s_link_muted; }

// --------------------------------------------------------------------------
// Red por partes (ver la cabecera). No se persisten a proposito.
// --------------------------------------------------------------------------
static volatile bool s_tb_enabled = true;
static volatile bool s_radio_enabled = true;

void debug_net_set_tb(bool enabled) {
  s_tb_enabled = enabled;
  ESP_LOGW(TAG, "publicacion a ThingsBoard %s",
           enabled ? "ENCENDIDA" : "APAGADA");
}
bool debug_net_tb_enabled(void) { return s_tb_enabled; }

void debug_net_set_radio(bool enabled) {
  s_radio_enabled = enabled;
  ESP_LOGW(TAG, "radio WiFi %s%s", enabled ? "ENCENDIDA" : "APAGADA",
           enabled ? "" : " - sin webserver ni OTA hasta reiniciar");
}
bool debug_net_radio_enabled(void) { return s_radio_enabled; }

// --------------------------------------------------------------------------
// Volcado de estado
// --------------------------------------------------------------------------

// Memoria de usar y tirar para el volcado de estado: PSRAM si la hay, y solo
// si no la hay se cae a la interna. Ver la nota del heap mas abajo: el
// display no puede permitirse gastar DRAM interna por peticion HTTP.
static void *debug_scratch_alloc(size_t bytes) {
  if (bytes == 0) {
    return NULL;
  }
  void *p = heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM);
  if (p == NULL) {
    p = calloc(1, bytes);
  }
  return p;
}

static void debug_scratch_free(void *p) { free(p); }

#define J(...)                                                                 \
  do {                                                                         \
    if (n < out_len) {                                                         \
      int w = snprintf(out + n, out_len - n, __VA_ARGS__);                     \
      if (w > 0) {                                                             \
        n += (size_t)w;                                                        \
        if (n > out_len) {                                                     \
          n = out_len;                                                         \
        }                                                                      \
      }                                                                        \
    }                                                                          \
  } while (0)

size_t debug_state_json(char *out, size_t out_len) {
  return debug_state_json_ex(out, out_len, false);
}

size_t debug_state_json_ex(char *out, size_t out_len, bool with_tasks) {
  if (out == NULL || out_len == 0) {
    return 0;
  }
  size_t n = 0;

  J("{\"debug\":%d", s_enabled ? 1 : 0);
  J(",\"uptime_ms\":%lu", (unsigned long)millis());

  J(",\"touch\":{\"read_fail\":%lu,\"presses\":%lu,\"now\":%d"
    ",\"x\":%u,\"y\":%u}",
    (unsigned long)gt911_read_failures, (unsigned long)gt911_press_events,
    (int)gt911_touched_now, (unsigned)gt911_last_x, (unsigned)gt911_last_y);

  J(",\"link\":{\"lost\":%d,\"muted\":%d,\"ever_seen\":%d}",
    Display_IsBoardLinkLost() ? 1 : 0, s_link_muted ? 1 : 0,
    Display_BoardEverSeen() ? 1 : 0);

  J(",\"tel\":{\"air\":%.2f,\"skin\":%.2f,\"hum\":%.2f,\"serial\":%d"
    ",\"srv\":%d}",
    ctrl_tel_msg.detectedAirTemperature, ctrl_tel_msg.detectedSkinTemperature,
    ctrl_tel_msg.detectedHumidity, ctrl_tel_msg.serialNumber,
    ctrl_tel_msg.serverCommStatus);

  J(",\"state\":{\"actuation\":%d,\"mode\":%d,\"set_air\":%.2f,\"set_skin\":%.2f"
    ",\"set_hum\":%.1f,\"photo\":%d,\"mute\":%d,\"skin_mode\":%d"
    ",\"alarm_mask\":%lu,\"silenced_mask\":%lu,\"test_prio\":%d"
    ",\"bars\":%d,\"probe\":%d,\"fw\":\"%s\"}",
    ctrl_state_msg.actuation, ctrl_state_msg.controlMode,
    ctrl_state_msg.desiredAirTemperature, ctrl_state_msg.desiredSkinTemperature,
    ctrl_state_msg.desiredHumidity, ctrl_state_msg.phototherapyMode,
    ctrl_state_msg.muteAlarm, ctrl_state_msg.skinModeEnabled,
    (unsigned long)ctrl_state_msg.alarmBitmask,
    (unsigned long)ctrl_state_msg.silencedBitmask,
    ctrl_state_msg.alarmTestPriority, ctrl_state_msg.linkBars,
    ctrl_state_msg.skinProbeState, ctrl_state_msg.fwVer);

  // Solo las alarmas que el display tiene por activas: es lo que se contrasta
  // contra el bitmask de la placa para detectar una desincronizacion.
  J(",\"alarms\":[");
  bool first = true;
  for (int i = 0; i < MAX_ALARMS; i++) {
    if (!alarmList[i].state) {
      continue;
    }
    J("%s{\"id\":%d,\"prio\":%u,\"type\":\"%s\"}", first ? "" : ",",
      alarmList[i].id, (unsigned)alarmList[i].priority, alarmList[i].type);
    first = false;
  }
  J("]");

  // El heap se informa SEPARANDO interna y PSRAM, y nunca como un total.
  //
  // Antes salia `esp_get_free_heap_size()`, que SUMA las dos. En el display eso
  // es un numero inservible: hay 7 MB de PSRAM tapando que la DRAM interna —la
  // que necesitan los descriptores DMA del WiFi y los buffers de dibujo de
  // LVGL— se puede haber quedado en nada. Paso en banco el 2026-09-11: la
  // interna bajo a 5,7 KB con el total intacto, y lo que se vio fue
  // `wifi:mem fail`, glitches en el panel y un HMI LINK LOST fantasma; el total
  // no se habia movido. Y el numero que de verdad manda no es el libre sino el
  // MAYOR BLOQUE CONTIGUO: 5 KB libres en trozos de 500 B no sirven para un
  // buffer de 2 KB.
  J(",\"heap\":{\"int_free\":%u,\"int_min\":%u,\"int_largest\":%u"
    ",\"psram_free\":%u,\"psram_largest\":%u}",
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
    (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));

  // La tabla de tareas va BAJO PETICION (debug_state_json_ex(.., true)).
  //
  // Es la mitad del volcado: ~30 entradas que se construyen con snprintf sobre
  // un bufer en PSRAM, y encima uxTaskGetSystemState() recorre todas las TCB
  // con el planificador suspendido. En el display eso compite por el ancho de
  // banda de PSRAM con el propio panel, que lee de ahi su framebuffer, y en
  // banco se vio como flicker mientras un script consultaba el estado en bucle
  // (2026-09-11). El volcado corriente —medidas, alarmas, memoria— no lo
  // necesita; quien diagnostica un desbordamiento de pila lo pide expresamente.
  if (!with_tasks) {
    J("}");
    return n;
  }

  J(",\"tasks\":[");
  {
    const UBaseType_t count = uxTaskGetNumberOfTasks();
    // A la PSRAM: son ~40 B por tarea y unas 30 tareas, y esta funcion la
    // llama un manejador HTTP que puede dispararse en bucle desde un script
    // de pruebas. Un kilo y medio de DRAM interna por peticion es justo lo que
    // no hay que gastar en el display.
    TaskStatus_t *st = (TaskStatus_t *)debug_scratch_alloc(
        (size_t)count * sizeof(TaskStatus_t));
    if (st != NULL) {
      const UBaseType_t got = uxTaskGetSystemState(st, count, NULL);
      for (UBaseType_t i = 0; i < got; i++) {
        J("%s{\"n\":\"%s\",\"prio\":%u,\"stack_min\":%u}", i ? "," : "",
          st[i].pcTaskName, (unsigned)st[i].uxCurrentPriority,
          (unsigned)st[i].usStackHighWaterMark);
      }
      debug_scratch_free(st);
    }
  }
  J("]}");

  return n;
}

#undef J

// --------------------------------------------------------------------------
// Provocacion de fallo
// --------------------------------------------------------------------------

debug_crash_kind_t debug_crash_from_name(const char *name) {
  if (name == NULL) {
    return DEBUG_CRASH_NONE;
  }
  if (strcmp(name, "abort") == 0) {
    return DEBUG_CRASH_ABORT;
  }
  if (strcmp(name, "null") == 0) {
    return DEBUG_CRASH_NULL_DEREF;
  }
  if (strcmp(name, "stack") == 0) {
    return DEBUG_CRASH_STACK;
  }
  if (strcmp(name, "wdt") == 0) {
    return DEBUG_CRASH_TASK_WDT;
  }
  if (strcmp(name, "assert") == 0) {
    return DEBUG_CRASH_ASSERT;
  }
  return DEBUG_CRASH_NONE;
}

static volatile debug_crash_kind_t s_crash_kind = DEBUG_CRASH_NONE;
static volatile uint32_t s_crash_delay_ms = 0;

static void debug_burn_stack(int depth) {
  volatile uint8_t pad[512];
  for (size_t i = 0; i < sizeof(pad); i++) {
    pad[i] = (uint8_t)(depth + i);
  }
  if (pad[0] != 0xFF) {
    debug_burn_stack(depth + 1);
  }
}

static void debug_crash_task(void *pv) {
  (void)pv;
  vTaskDelay(pdMS_TO_TICKS(s_crash_delay_ms));
  ESP_LOGE(TAG, "provocando fallo tipo %d para la prueba de coredump",
           (int)s_crash_kind);
  switch (s_crash_kind) {
  case DEBUG_CRASH_NULL_DEREF: {
    volatile int *p = (volatile int *)0;
    *p = 1;
    break;
  }
  case DEBUG_CRASH_STACK:
    debug_burn_stack(0);
    break;
  case DEBUG_CRASH_TASK_WDT:
    portDISABLE_INTERRUPTS();
    for (;;) {
    }
  case DEBUG_CRASH_ASSERT:
    configASSERT(s_crash_kind == DEBUG_CRASH_NONE);
    break;
  case DEBUG_CRASH_ABORT:
  default:
    abort();
  }
  abort();
}

bool debug_crash_request(debug_crash_kind_t kind, uint32_t delay_ms) {
  if (!s_enabled || kind == DEBUG_CRASH_NONE) {
    return false;
  }
  s_crash_kind = kind;
  s_crash_delay_ms = delay_ms;
  return xTaskCreate(debug_crash_task, "DBG_CRASH", 4096, NULL, 1, NULL) ==
         pdPASS;
}
