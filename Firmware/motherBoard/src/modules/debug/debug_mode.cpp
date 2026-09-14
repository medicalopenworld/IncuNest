#include "modules/debug/debug_mode.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
// Por LittleFS.totalBytes()/usedBytes() en el bloque "fs" del volcado.
#include "platform/plat_fs.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "main.h"
#include "config/board.h"
#include "modules/control/alarm_machine.h"
#include "modules/util/tz_source.h"
#include "modules/util/system_clock.h"
#include "modules/baby_profile/baby_profile_store.h"
#include "platform/plat_time.h"

extern IncuNest_parameters in3;
extern double fanControlPIDOutput;
extern double HeaterPIDOutput;

// El modo NO se persiste: ver la regla 1 de debug_mode.h.
static volatile bool s_enabled = false;

struct Override {
  bool active;
  double value;
};
static Override s_ovr[DEBUG_CH_COUNT];

// Bitmask de alarmas forzadas. NUM_ALARMS es 20, asi que cabe de sobra.
static uint32_t s_forced_present;

// Cola de tramas inyectadas; la crea debug_inject_line() en la primera y la
// vacia debug_mode_set(false). Declarada aqui arriba porque debug_mode_set()
// la toca y esta antes que la implementacion de la inyeccion.
static QueueHandle_t s_inject_q = NULL;

static const char *const kChannelNames[DEBUG_CH_COUNT] = {
    "air_temp", "air_temp_red", "skin_temp",      "ambient_temp",
    "humidity", "fan_rpm",      "system_voltage", "heater_current",
    "fan_current",
};

const char *debug_channel_name(debug_channel_t ch) {
  if (ch < 0 || ch >= DEBUG_CH_COUNT) {
    return "";
  }
  return kChannelNames[ch];
}

debug_channel_t debug_channel_from_name(const char *name) {
  if (name == NULL) {
    return DEBUG_CH_COUNT;
  }
  for (int i = 0; i < DEBUG_CH_COUNT; i++) {
    if (strcmp(name, kChannelNames[i]) == 0) {
      return (debug_channel_t)i;
    }
  }
  return DEBUG_CH_COUNT;
}

bool debug_mode_enabled(void) { return s_enabled; }

void debug_mode_set(bool on) {
  if (on == s_enabled) {
    return;
  }
  if (!on) {
    // Regla 2: apagar el modo RETIRA todo. Primero se limpian las
    // simulaciones y solo despues se baja el interruptor, para que no exista
    // ni una pasada de sensors_Task con el modo apagado y una medida falsa
    // todavia puesta.
    debug_override_clear_all();
    debug_alarm_clear_all();
    if (s_inject_q != NULL) {
      xQueueReset(s_inject_q);
    }
  }
  s_enabled = on;
  logI(on ? String("[DEBUG] MODO DEPURACION ENCENDIDO - las medidas y las "
                   "alarmas pueden estar simuladas")
          : String("[DEBUG] modo depuracion apagado, simulaciones retiradas"));
}

bool debug_override_set(debug_channel_t ch, double value) {
  if (!s_enabled || ch < 0 || ch >= DEBUG_CH_COUNT || isnan(value)) {
    return false;
  }
  s_ovr[ch].value = value;
  s_ovr[ch].active = true;
  logI(String("[DEBUG] simula ") + debug_channel_name(ch) + "=" + String(value));
  return true;
}

void debug_override_clear(debug_channel_t ch) {
  if (ch < 0 || ch >= DEBUG_CH_COUNT) {
    return;
  }
  s_ovr[ch].active = false;
}

void debug_override_clear_all(void) {
  for (int i = 0; i < DEBUG_CH_COUNT; i++) {
    s_ovr[i].active = false;
  }
}

bool debug_override_active(debug_channel_t ch) {
  return ch >= 0 && ch < DEBUG_CH_COUNT && s_ovr[ch].active;
}

void debug_sensors_apply(void) {
  if (!s_enabled) {
    return;
  }
  if (s_ovr[DEBUG_CH_AIR_TEMP].active) {
    in3.temperature[ROOM_DIGITAL_TEMP_SENSOR] = s_ovr[DEBUG_CH_AIR_TEMP].value;
  }
  if (s_ovr[DEBUG_CH_AIR_TEMP_RED].active) {
    in3.airTemperatureRedundantSensor = s_ovr[DEBUG_CH_AIR_TEMP_RED].value;
  }
  if (s_ovr[DEBUG_CH_SKIN_TEMP].active) {
    in3.temperature[SKIN_SENSOR] = s_ovr[DEBUG_CH_SKIN_TEMP].value;
  }
  if (s_ovr[DEBUG_CH_AMBIENT_TEMP].active) {
    in3.temperature[AMBIENT_DIGITAL_TEMP_SENSOR] =
        s_ovr[DEBUG_CH_AMBIENT_TEMP].value;
  }
  if (s_ovr[DEBUG_CH_HUMIDITY].active) {
    in3.humidity[ROOM_DIGITAL_HUM_SENSOR] = s_ovr[DEBUG_CH_HUMIDITY].value;
  }
  if (s_ovr[DEBUG_CH_FAN_RPM].active) {
    in3.fan_rpm = s_ovr[DEBUG_CH_FAN_RPM].value;
  }
  if (s_ovr[DEBUG_CH_SYSTEM_VOLTAGE].active) {
    in3.system_voltage = s_ovr[DEBUG_CH_SYSTEM_VOLTAGE].value;
  }
  if (s_ovr[DEBUG_CH_HEATER_CURRENT].active) {
    in3.heater_current = s_ovr[DEBUG_CH_HEATER_CURRENT].value;
  }
  if (s_ovr[DEBUG_CH_FAN_CURRENT].active) {
    in3.fan_current = s_ovr[DEBUG_CH_FAN_CURRENT].value;
  }
}

bool debug_alarm_force(int alarm_id, bool present) {
  if (alarm_id <= ALARM_NONE || alarm_id >= NUM_ALARMS) {
    return false;
  }
  if (present && !s_enabled) {
    return false;
  }
  const uint32_t bit = 1u << alarm_id;
  if (present) {
    s_forced_present |= bit;
  } else {
    s_forced_present &= ~bit;
    // Al dejar de forzarla se retira la condicion UNA vez; si el detector de
    // verdad la sigue viendo, la volvera a declarar en la pasada siguiente.
    // Sin esto la condicion se quedaria puesta para siempre: la maquina
    // conserva `present` hasta que alguien declare false.
    alarm_machine_condition((AlarmId)alarm_id, false, millis());
  }
  logI(String("[DEBUG] alarma ") + String(alarm_id) +
       (present ? " FORZADA" : " liberada"));
  return true;
}

void debug_alarm_clear_all(void) {
  if (s_forced_present == 0) {
    return;
  }
  const uint32_t now = millis();
  for (int id = ALARM_NONE + 1; id < NUM_ALARMS; id++) {
    if (s_forced_present & (1u << id)) {
      alarm_machine_condition((AlarmId)id, false, now);
    }
  }
  s_forced_present = 0;
}

bool debug_alarm_forced(int alarm_id) {
  if (alarm_id <= ALARM_NONE || alarm_id >= NUM_ALARMS) {
    return false;
  }
  return (s_forced_present & (1u << alarm_id)) != 0;
}

void debug_alarms_apply(uint32_t now_ms) {
  if (!s_enabled || s_forced_present == 0) {
    return;
  }
  // Se declara DESPUES de los detectores (ver securityCheck): lo forzado gana
  // a lo medido mientras dure. Todo lo demas —retardo de anuncio, prioridad,
  // enclavamiento, corte de calefactor— sigue siendo el camino de produccion.
  for (int id = ALARM_NONE + 1; id < NUM_ALARMS; id++) {
    if (s_forced_present & (1u << id)) {
      alarm_machine_condition((AlarmId)id, true, now_ms);
    }
  }
}

// --------------------------------------------------------------------------
// Inyeccion de tramas del display
// --------------------------------------------------------------------------
#define DEBUG_INJECT_LINE_MAX 192
#define DEBUG_INJECT_DEPTH 8

typedef struct {
  char text[DEBUG_INJECT_LINE_MAX];
} InjectedLine;

bool debug_inject_line(const char *line) {
  if (!s_enabled || line == NULL) {
    return false;
  }
  const size_t len = strlen(line);
  if (len == 0 || len >= DEBUG_INJECT_LINE_MAX) {
    return false;
  }
  // Mismo filtro que el camino real: una linea sin el prefijo no la habria
  // mirado nadie, y aceptarla aqui seria probar algo que no existe.
  if (strncmp(line, "HMI,", 4) != 0) {
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

// La llama CommTask. Devuelve true y deja la linea en `out` si habia una.
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

void debug_scratch_free(void *p) { free(p); }

// Escribe en `out` sin pasarse nunca, y devuelve cuanto lleva escrito. Si no
// cabe, trunca: un JSON truncado se detecta al parsear, y es mejor que
// reventar la pila con un bufer mayor.
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
  const uint32_t mask = alarm_machine_bitmask();

  J("{\"debug\":%d", s_enabled ? 1 : 0);
  J(",\"uptime_ms\":%lu", (unsigned long)millis());
  J(",\"serial\":%d", in3.serialNumber);
  J(",\"reset_reason\":%d", in3.resetReason);

  // Reloj y zona horaria, que son dos datos distintos y se diagnostican mal
  // por separado: el epoch es UTC y SIEMPRE lo es, y `tz_origin` distingue
  // "offset 0 porque estamos en Togo" de "offset 0 porque nadie nos ha dicho
  // la zona todavia". Un reloj de pantalla que va justo dos horas atrasado en
  // Espana es lo segundo, no un fallo de NTP.
  J(",\"clock\":{\"epoch\":%lu,\"manual\":%d,\"tz_quarters\":%d"
    ",\"tz_origin\":%d,\"tz_known\":%d}",
    (unsigned long)babyStore_nowEpoch(), systemClockIsManual() ? 1 : 0,
    (int)tz_source_quarters(), (int)tz_source_origin(),
    tz_source_known() ? 1 : 0);

  J(",\"meas\":{\"air\":%.2f,\"air_red\":%.2f,\"skin\":%.2f,\"ambient\":%.2f"
    ",\"hum\":%.2f,\"fan_rpm\":%.0f,\"skin_cap\":%d}",
    in3.temperature[ROOM_DIGITAL_TEMP_SENSOR], in3.airTemperatureRedundantSensor,
    in3.temperature[SKIN_SENSOR], in3.temperature[AMBIENT_DIGITAL_TEMP_SENSOR],
    in3.humidity[ROOM_DIGITAL_HUM_SENSOR], in3.fan_rpm,
    in3.skinSensorCapacitance);

  J(",\"power\":{\"sys_v\":%.2f,\"sys_i\":%.2f,\"heater_i\":%.2f"
    ",\"fan_i\":%.2f,\"hum_i\":%.2f,\"photo_i\":%.2f,\"bat_v\":%.2f"
    ",\"bat_i\":%.2f}",
    in3.system_voltage, in3.system_current, in3.heater_current, in3.fan_current,
    in3.humidifier_current, in3.phototherapy_current, in3.BATTERY_voltage,
    in3.BATTERY_current);

  J(",\"ctl\":{\"actuation\":%d,\"mode\":%d,\"temp_ctl\":%d,\"hum_ctl\":%d"
    ",\"set_temp\":%.2f,\"set_hum\":%.2f,\"photo\":%d,\"photo_pwm\":%d}",
    in3.actuation, in3.controlMode ? 1 : 0, in3.temperatureControl ? 1 : 0,
    in3.humidityControl ? 1 : 0, in3.desiredControlTemperature,
    in3.desiredControlHumidity, in3.phototherapy ? 1 : 0,
    (int)in3.phototherapy_intensity);

  J(",\"fan\":{\"commanded\":%d,\"feedback\":%d,\"pid_en\":%d,\"pid_out\":%.0f"
    ",\"ctl_pwm\":%d,\"supply_pwm\":%d}",
    in3.fanCommandedOn ? 1 : 0, in3.fanHasSpeedFeedback ? 1 : 0,
    in3.fanPidEnabled ? 1 : 0, fanControlPIDOutput, in3.fanCtlPWM,
    in3.fanPwrSupplyPWM);

  J(",\"heater\":{\"pid_out\":%.0f,\"max_pwm\":%d,\"must_cut\":%d}",
    HeaterPIDOutput, in3.heaterSafeMAXPWM,
    alarm_machine_heater_must_cut() ? 1 : 0);

  // Alarmas: el bitmask de las que senalizan, mas el estado y si esta forzada
  // de cada una. Un test puede afirmar sobre el bitmask sin recorrer nada.
  J(",\"alarms\":{\"mask\":%lu,\"top_prio\":%d,\"forced_mask\":%lu,\"list\":[",
    (unsigned long)mask, (int)alarm_machine_top_priority(),
    (unsigned long)s_forced_present);
  bool first = true;
  for (int id = ALARM_NONE + 1; id < NUM_ALARMS; id++) {
    const AlarmState st = alarm_machine_state((AlarmId)id);
    const bool forced = (s_forced_present & (1u << id)) != 0;
    if (st == ALARM_STATE_INACTIVE && !forced) {
      continue; // solo lo que no esta en reposo, para que el JSON quepa
    }
    J("%s{\"id\":%d,\"state\":%d,\"latched\":%d,\"forced\":%d}",
      first ? "" : ",", id, (int)st,
      alarm_machine_is_latched((AlarmId)id) ? 1 : 0, forced ? 1 : 0);
    first = false;
  }
  J("]}");

  // Simulaciones puestas ahora mismo.
  J(",\"overrides\":{");
  first = true;
  for (int i = 0; i < DEBUG_CH_COUNT; i++) {
    if (!s_ovr[i].active) {
      continue;
    }
    J("%s\"%s\":%.3f", first ? "" : ",", kChannelNames[i], s_ovr[i].value);
    first = false;
  }
  J("}");

  // Salud: es lo que hacia falta para diagnosticar la caida de banco. El
  // minimo historico de pila por tarea es lo unico que delata un desbordamiento
  // ANTES de que ocurra.
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

  // SISTEMA DE FICHEROS. Se informa porque llenarlo no se notaba desde fuera:
  // en banco (2026-09-14) las ventanas de PPG de DriveUpload llenaron los
  // 2,625 MB de la particion en minutos y lo primero que se vio fue un abort.
  // En esta particion viven tambien los perfiles de bebe y el historico de
  // pesos, asi que quedarse sin sitio no es solo perder un diagnostico.
  J(",\"fs\":{\"total\":%u,\"used\":%u}",
    (unsigned)LittleFS.totalBytes(), (unsigned)LittleFS.usedBytes());

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
        // Sin el core: TaskStatus_t no lo expone en IDF 6 y lo que hacia
        // falta de esta tabla es stack_min, que es el unico dato que delata un
        // desbordamiento de pila ANTES de que ocurra.
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

// volatile para que el optimizador no se lleve por delante el fallo.
static volatile debug_crash_kind_t s_crash_kind = DEBUG_CRASH_NONE;
static volatile uint32_t s_crash_delay_ms = 0;

static void debug_burn_stack(int depth) {
  volatile uint8_t pad[512];
  for (size_t i = 0; i < sizeof(pad); i++) {
    pad[i] = (uint8_t)(depth + i);
  }
  if (pad[0] != 0xFF) { // siempre cierto: la recursion no se corta
    debug_burn_stack(depth + 1);
  }
}

static void debug_crash_task(void *pv) {
  (void)pv;
  vTaskDelay(pdMS_TO_TICKS(s_crash_delay_ms));
  logE(String("[DEBUG] provocando fallo tipo ") + String((int)s_crash_kind) +
       " para la prueba de coredump");
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
    // Sin ceder CPU ni alimentar el watchdog: lo dispara el TWDT.
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
  abort(); // por si alguna rama no mato el proceso
}

bool debug_crash_request(debug_crash_kind_t kind, uint32_t delay_ms) {
  if (!s_enabled || kind == DEBUG_CRASH_NONE) {
    return false;
  }
  s_crash_kind = kind;
  s_crash_delay_ms = delay_ms;
  // Pila generosa a proposito: la de "stack" tiene que desbordarse ELLA, no
  // fallar antes por no caber el marco de logE().
  return xTaskCreate(debug_crash_task, "DBG_CRASH", 4096, NULL, 1, NULL) ==
         pdPASS;
}
