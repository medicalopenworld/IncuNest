#include "modules/config/runtime_config.h"

#include <Arduino.h>
#include <Preferences.h>

#include <atomic>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "main.h"
#include "alarm_policy.h"
#include "config/preferences_keys.h"
#include "modules/baby_profile/baby_profile_store.h"
#include "system/hw_selftest.h"

extern IncuNest_parameters in3;
extern PID fanControlPID;

// Corriente del calefactor por encima de la cual se considera que esta
// ACTUANDO (para la regla de ventilador remoto). Por debajo es ruido del
// sensor de corriente con el calefactor apagado.
#define RCFG_HEATER_ACTING_AMPS 0.3

namespace {

const char *TAG = "CFG";

constexpr uint8_t SRC_NONE = 0;  // origen + 1; 0 = nunca cambiado desde aqui

// Estado persistente por clave. 12 bytes, un blob "k<i>" en NS_RCFG. Se escribe
// solo cuando cambia algo: un valor remoto nuevo, un cambio local o el cambio
// efectivo de un parametro -- no en cada reconexion.
struct KeyState {
  float lastRemote;   // ultimo valor remoto visto (aceptado o no)
  uint8_t hasRemote;  // lastRemote es valido
  uint8_t overrideLocal;  // un cambio local posterior gana al remoto
  uint8_t src;        // origen del ultimo cambio efectivo + 1
  uint8_t pad;
  uint32_t epoch;     // cuando (0 = reloj sin sincronizar)
};
static_assert(sizeof(KeyState) == 12, "blob de NVS de 12 bytes");

KeyState s_state[RCFG_COUNT];
bool s_loaded = false;

// Cambio remoto aplazado (terapia activa o ritmo de 10 s).
struct Pending {
  bool valid;
  float requested;
  RuntimeConfigOrigin origin;
};
Pending s_pending[RCFG_COUNT];
std::atomic<uint32_t> s_pendingMask{0};

// Ritmo: millis() del ultimo cambio remoto de cada clave.
uint32_t s_lastRemoteChangeMs[RCFG_COUNT];
bool s_hasLastRemoteChange[RCFG_COUNT];

// Eco pendiente. Atomico para poder mirarlo sin coger el mutex.
std::atomic<uint32_t> s_echoPending{0};

// Cola de auditoria cfg_change: 4 eventos, el mas viejo se descarta si se llena.
struct AuditEvent {
  uint32_t seq;
  RuntimeConfigId id;
  float oldValue;
  float newValue;
  RuntimeConfigOrigin origin;
  uint32_t epoch;
};
constexpr size_t AUDIT_CAP = 4;
AuditEvent s_audit[AUDIT_CAP];
size_t s_auditHead = 0;
size_t s_auditCount = 0;
uint32_t s_auditSeq = 0;

// Ultimo valor remoto rechazado, ya formateado, esperando a publicarse.
char s_rejectedJson[RCFG_AUDIT_JSON_MAX];
bool s_rejectedValid = false;
uint32_t s_rejectedSeq = 0;

// --- mutex ----------------------------------------------------------------

SemaphoreHandle_t mutex() {
  // Inicializacion de estatico local: la garantiza el compilador aunque dos
  // tareas lleguen a la vez por primera vez.
  static SemaphoreHandle_t m = xSemaphoreCreateMutex();
  return m;
}

class Lock {
 public:
  Lock() : m_(mutex()), ok_(m_ != nullptr && xSemaphoreTake(m_, pdMS_TO_TICKS(500)) == pdTRUE) {}
  ~Lock() {
    if (ok_) {
      xSemaphoreGive(m_);
    }
  }
  bool ok() const { return ok_; }
  Lock(const Lock &) = delete;
  Lock &operator=(const Lock &) = delete;

 private:
  SemaphoreHandle_t m_;
  bool ok_;
};

// --- estado persistente ----------------------------------------------------

void state_key(int i, char *out, size_t n) { snprintf(out, n, "k%d", i); }

// Se llama con el mutex cogido.
void ensure_loaded() {
  if (s_loaded) {
    return;
  }
  s_loaded = true;
  memset(s_state, 0, sizeof(s_state));
  Preferences p;
  if (!p.begin(NS_RCFG, true)) {
    return;  // namespace aun inexistente: estado limpio
  }
  for (int i = 0; i < RCFG_COUNT; ++i) {
    char k[8];
    state_key(i, k, sizeof(k));
    KeyState tmp;
    if (p.getBytesLength(k) == sizeof(tmp) &&
        p.getBytes(k, &tmp, sizeof(tmp)) == sizeof(tmp)) {
      s_state[i] = tmp;
    }
  }
  p.end();
}

void save_state(int i) {
  Preferences p;
  if (!p.begin(NS_RCFG, false)) {
    ESP_LOGE(TAG, "no se puede abrir %s para guardar el estado de la clave %d",
             NS_RCFG, i);
    return;
  }
  char k[8];
  state_key(i, k, sizeof(k));
  if (p.putBytes(k, &s_state[i], sizeof(KeyState)) != sizeof(KeyState)) {
    ESP_LOGE(TAG, "no se pudo guardar el estado de la clave %d", i);
  }
  p.end();
}

// --- lectura del estado de la unidad ---------------------------------------

float current_value(RuntimeConfigId id) {
  switch (id) {
    case RCFG_FAN_SUPPLY_PWM:
      return (float)in3.fanPwrSupplyPWM;
    case RCFG_FAN_CTL_PWM:
      return (float)in3.fanCtlPWM;
    case RCFG_FAN_PID_EN:
      return in3.fanPidEnabled ? 1.0f : 0.0f;
    case RCFG_HEATER_AMPS:
      return in3.heaterMaxPowerAmps;
    case RCFG_AIR_TMAX:
      return in3.airTemperatureSetMax;
    case RCFG_SKIN_TMAX:
      return in3.skinTemperatureSetMax;
    case RCFG_GPRS_ACT:
      return (float)in3.actuating_gprs_period;
    case RCFG_GPRS_PHOTO:
      return (float)in3.phototherapy_gprs_period;
    case RCFG_GPRS_STBY:
      return (float)in3.standby_gprs_period;
    case RCFG_COUNT:
      break;
  }
  return 0.0f;
}

// Terapia en marcha: control de temperatura o humedad, o fototerapia.
bool therapy_active() {
  return in3.temperatureControl || in3.humidityControl || in3.phototherapy;
}

RuntimeConfigRemoteCtx build_ctx() {
  RuntimeConfigRemoteCtx c;
  // Valores de FABRICA compilados: el techo de lo que un origen remoto puede
  // dejar. Subir un limite por encima de esto sigue siendo una accion local.
  c.air_factory_max = (float)AIR_THERMAL_CUTOUT_DEFAULT_C;
  c.skin_factory_max = alarm_clamp_skin_cutout((float)SKIN_TEMPERATURE_SET_MAX);
  c.heater_factory_max = (float)HEATER_MAX_POWER_AMPS;
  c.fan_has_tach = in3.fanHasSpeedFeedback;
  c.heater_acting =
      in3.temperatureControl && in3.heater_current > RCFG_HEATER_ACTING_AMPS;
  c.air_setpoint_c = 0.0f;
  c.skin_setpoint_c = 0.0f;
  if (in3.temperatureControl) {
    const float sp = (float)in3.desiredControlTemperature;
    if (in3.controlMode == CONTROL_AIR) {
      c.air_setpoint_c = sp;
    } else {
      c.skin_setpoint_c = sp;
    }
  }
  return c;
}

// --- efectos ---------------------------------------------------------------

bool persist_int(const char *ns, const char *key, int v) {
  Preferences p;
  if (!p.begin(ns, false)) {
    return false;
  }
  const size_t n = p.putInt(key, v);
  p.end();
  return n == sizeof(int32_t);
}

bool persist_float(const char *ns, const char *key, float v) {
  Preferences p;
  if (!p.begin(ns, false)) {
    return false;
  }
  const size_t n = p.putFloat(key, v);
  p.end();
  return n == sizeof(float);
}

// Lleva fanCtlPWM al canal LEDC SOLO si nadie mas lo esta manejando. El POST
// de antes escribia siempre, lo que podia arrancar el canal de control de una
// unidad parada o pisar la salida del PID cerrado; ahora el valor nuevo lo
// recoge turnFans(), que se llama en cada trama del HMI, igual que siempre.
void apply_fan_ctl_pwm_now() {
#if (HW_NUM >= 8)
  if (g_factoryTestActive || !in3.fanCommandedOn) {
    return;
  }
#if defined(FAN_SPEED_FEEDBACK)
  if (in3.fanHasSpeedFeedback && in3.fanPidEnabled &&
      fanControlPID.GetMode() == AUTOMATIC) {
    return;
  }
#endif
  ledcWrite(FAN_CTL_PWM_CHANNEL, in3.fanCtlPWM);
#endif
}

// Persiste y luego cambia la RAM: si la NVS falla no se queda un valor
// aplicado que desaparecera al reiniciar sin que nadie lo sepa.
//
// NO se escribe maxDesiredTemp[]: nadie lo lee (solo se define en main.cpp y
// se declara extern en security.cpp), y su significado es el TOPE DE CONSIGNA
// (AIR_TEMPERATURE_SET_MAX = 39), no el corte termico. Volcar ahi el corte
// (hasta 40) rompia el invariante consigna < corte el dia que alguien lo leyera.
bool store(RuntimeConfigId id, float v) {
  switch (id) {
    case RCFG_FAN_SUPPLY_PWM:
      if (!persist_int(NS_CFG, KEY_FAN_PWR_SUPPLY_PWM, (int)v)) return false;
      in3.fanPwrSupplyPWM = (int)v;
      return true;
    case RCFG_FAN_CTL_PWM:
      if (!persist_int(NS_CFG, KEY_FAN_CTL_PWM, (int)v)) return false;
      in3.fanCtlPWM = (int)v;
      apply_fan_ctl_pwm_now();
      return true;
    case RCFG_FAN_PID_EN:
      // Persiste por dentro (Actuators.cpp) y devuelve false sin tocar nada si
      // la NVS falla.
      return setFanPidEnabled(v != 0.0f);
    case RCFG_HEATER_AMPS:
      if (!persist_float(NS_CFG, KEY_HEAT_MAX_A, v)) return false;
      in3.heaterMaxPowerAmps = v;
      return true;
    case RCFG_AIR_TMAX:
      if (!persist_float(NS_CFG, KEY_AIR_T_MAX, v)) return false;
      in3.airTemperatureSetMax = v;
      return true;
    case RCFG_SKIN_TMAX:
      if (!persist_float(NS_CFG, KEY_SKIN_T_MAX, v)) return false;
      in3.skinTemperatureSetMax = v;
      return true;
    case RCFG_GPRS_ACT:
      if (!persist_int(NS_GPRS, KEY_ACT_PERIOD, (int)v)) return false;
      in3.actuating_gprs_period = (int)v;
      return true;
    case RCFG_GPRS_PHOTO:
      if (!persist_int(NS_GPRS, KEY_PHOTO_PERIOD, (int)v)) return false;
      in3.phototherapy_gprs_period = (int)v;
      return true;
    case RCFG_GPRS_STBY:
      if (!persist_int(NS_GPRS, KEY_STBY_PERIOD, (int)v)) return false;
      in3.standby_gprs_period = (int)v;
      return true;
    case RCFG_COUNT:
      break;
  }
  return false;
}

void set_msg(char *msg, size_t msg_len, const char *name, const char *why) {
  if (msg != nullptr && msg_len > 0) {
    snprintf(msg, msg_len, "%s: %s", name, why);
  }
}

bool is_remote(RuntimeConfigOrigin o) {
  return o == RCFG_ORIGIN_TB_GPRS || o == RCFG_ORIGIN_TB_WIFI;
}

bool lookup_for(RuntimeConfigOrigin o, const char *name, RuntimeConfigId *id) {
  return (o == RCFG_ORIGIN_USB) ? runtime_config_lookup_console(name, id)
                                : runtime_config_lookup(name, id);
}

// --- auditoria, cfg_rejected ----------------------------------------------

void audit_enqueue(RuntimeConfigId id, float oldV, float newV,
                   RuntimeConfigOrigin origin, uint32_t epoch) {
  if (s_auditCount == AUDIT_CAP) {
    ESP_LOGW(TAG, "cola de auditoria llena: se descarta el evento mas viejo");
    s_auditHead = (s_auditHead + 1) % AUDIT_CAP;
    --s_auditCount;
  }
  AuditEvent &e = s_audit[(s_auditHead + s_auditCount) % AUDIT_CAP];
  e.seq = ++s_auditSeq;
  e.id = id;
  e.oldValue = oldV;
  e.newValue = newV;
  e.origin = origin;
  e.epoch = epoch;
  ++s_auditCount;
}

void note_rejected(const char *name, const char *why) {
  if (runtime_config_format_rejected(name, why, s_rejectedJson,
                                     sizeof(s_rejectedJson)) > 0) {
    s_rejectedValid = true;
    ++s_rejectedSeq;
  }
}

// --- nucleo ----------------------------------------------------------------

// Aplica un valor YA validado. Con el mutex cogido.
//   changed -> persiste, anota origen/epoch, audita y (si es local) marca
//              override; si no cambia nada solo se marca el eco.
bool commit(RuntimeConfigOrigin origin, RuntimeConfigId id, float requested,
            float applied, RuntimeConfigStatus st, char *msg, size_t msg_len) {
  const char *name = runtime_config_name(id);
  const char *org = runtime_config_origin_name(origin);
  const float before = current_value(id);
  const bool changed = (before != applied);

  if (changed) {
    if (!store(id, applied)) {
      ESP_LOGE(TAG, "origin=%s name=%s requested=%.2f NVS write failed, unchanged",
               org, name, (double)requested);
      set_msg(msg, msg_len, name, "could not be saved");
      return false;
    }
    const uint32_t epoch = babyStore_nowEpoch();
    KeyState &k = s_state[id];
    k.src = (uint8_t)origin + 1;
    k.epoch = epoch;
    if (!is_remote(origin)) {
      k.overrideLocal = 1;  // un cambio local gana al reaplicado remoto
      s_pending[id].valid = false;  // y a un remoto aplazado
      s_pendingMask.fetch_and(~(uint32_t)RCFG_MASK(id));
    } else {
      s_lastRemoteChangeMs[id] = millis();
      s_hasLastRemoteChange[id] = true;
    }
    save_state(id);
    audit_enqueue(id, before, applied, origin, epoch);
    // Aviso, no info: un cambio de un limite de seguridad o de la cadencia
    // tiene que verse en el log aunque logI este compilado a nada.
    ESP_LOGW(TAG, "CHANGE origin=%s name=%s requested=%.2f applied=%.2f was=%.2f%s",
             org, name, (double)requested, (double)applied, (double)before,
             st == RCFG_OK_CLAMPED ? " (clamped)" : "");
  } else {
    ESP_LOGI(TAG, "origin=%s name=%s requested=%.2f applied=%.2f (unchanged)",
             org, name, (double)requested, (double)applied);
  }
  s_echoPending.fetch_or((uint32_t)RCFG_MASK(id));
  set_msg(msg, msg_len, name,
          st == RCFG_OK_CLAMPED ? "clamped to the allowed range" : "ok");
  return true;
}

// Origen local (web / usb). Con el mutex cogido.
bool apply_local(RuntimeConfigOrigin origin, RuntimeConfigId id, float requested,
                 char *msg, size_t msg_len) {
  const char *name = runtime_config_name(id);
  float applied = 0.0f;
  const RuntimeConfigStatus st =
      (origin == RCFG_ORIGIN_USB)
          ? runtime_config_validate_strict(id, requested, &applied)
          : runtime_config_validate(id, requested, &applied);
  if (!runtime_config_status_accepted(st)) {
    // ESP_LOGW y no logI: logI esta detras de LOG_INFORMATION (false) y un
    // rechazo de un parametro de seguridad no puede ser invisible.
    ESP_LOGW(TAG, "origin=%s name=%s requested=%.2f rejected: %s",
             runtime_config_origin_name(origin), name, (double)requested,
             runtime_config_status_text(st));
    set_msg(msg, msg_len, name, runtime_config_status_text(st));
    return false;
  }
  return commit(origin, id, requested, applied, st, msg, msg_len);
}

// Intenta aplicar un valor remoto: lista blanca, reglas remotas y, si toca,
// lo deja aplazado. Con el mutex cogido.
enum class RemoteResult { Applied, Rejected, Deferred };

RemoteResult try_remote(RuntimeConfigOrigin origin, RuntimeConfigId id,
                        float requested) {
  const char *name = runtime_config_name(id);
  const char *org = runtime_config_origin_name(origin);

  if (!runtime_config_remote_allowed(id, RCFG_REMOTE_SAFETY_PARAMS != 0)) {
    ESP_LOGW(TAG,
             "origin=%s name=%s requested=%.2f IGNORED: not allowed remotely "
             "(MQTT is not encrypted, see runtime_config_policy.h)",
             org, name, (double)requested);
    note_rejected(name, runtime_config_status_text(RCFG_REJ_NOT_ALLOWED));
    s_echoPending.fetch_or((uint32_t)RCFG_MASK(id));
    return RemoteResult::Rejected;
  }

  const RuntimeConfigRemoteCtx ctx = build_ctx();
  float applied = 0.0f;
  const RuntimeConfigStatus st =
      runtime_config_validate_remote(id, requested, &ctx, &applied);
  if (!runtime_config_status_accepted(st)) {
    ESP_LOGW(TAG, "origin=%s name=%s requested=%.2f rejected: %s", org, name,
             (double)requested, runtime_config_status_text(st));
    note_rejected(name, runtime_config_status_text(st));
    // El eco repone el valor que SI tiene la unidad, para que el panel no se
    // quede con lo escrito.
    s_echoPending.fetch_or((uint32_t)RCFG_MASK(id));
    return RemoteResult::Rejected;
  }

  const bool defer_therapy = runtime_config_should_defer(id, therapy_active());
  const bool rate_ok = runtime_config_rate_ok(
      s_hasLastRemoteChange[id], s_lastRemoteChangeMs[id], millis(),
      (uint32_t)RCFG_REMOTE_MIN_INTERVAL_MS);
  // Un valor que no cambia nada no espera a nadie.
  const bool noop = (current_value(id) == applied);
  if (!noop && (defer_therapy || !rate_ok)) {
    s_pending[id].valid = true;
    s_pending[id].requested = requested;
    s_pending[id].origin = origin;
    s_pendingMask.fetch_or((uint32_t)RCFG_MASK(id));
    ESP_LOGW(TAG, "origin=%s name=%s requested=%.2f DEFERRED: %s", org, name,
             (double)requested,
             defer_therapy ? "therapy active, waits for standby"
                           : "one change per key every 10 s");
    return RemoteResult::Deferred;
  }

  (void)commit(origin, id, requested, applied, st, nullptr, 0);
  return RemoteResult::Applied;
}

// Apunta `requested` como ultimo valor remoto visto de la clave y lo persiste
// si cambia. Con el mutex cogido.
void record_remote(RuntimeConfigId id, float requested) {
  KeyState &k = s_state[id];
  if (k.hasRemote && k.lastRemote == requested) {
    return;
  }
  k.lastRemote = requested;
  k.hasRemote = 1;
  save_state(id);
}

// Una clave de un objeto de atributos. Con el mutex cogido.
void handle_remote_key(RuntimeConfigOrigin origin, RuntimeConfigId id,
                       float requested, bool new_write) {
  KeyState &k = s_state[id];

  if (!new_write) {
    // Respuesta a la peticion de una conexion.
    if (k.hasRemote && k.lastRemote == requested) {
      return;  // reenvio de lo mismo: nada nuevo
    }
    if (k.overrideLocal) {
      // Hay un cambio local posterior: manda el local. Se apunta lo visto para
      // no repetir este aviso en cada conexion.
      ESP_LOGW(TAG,
               "origin=%s name=%s remote value %.2f NOT applied: a local change "
               "wins until a new remote write",
               runtime_config_origin_name(origin), runtime_config_name(id),
               (double)requested);
      record_remote(id, requested);
      return;
    }
  } else if (k.overrideLocal) {
    // Escritura remota nueva (suscripcion): levanta el override local.
    k.overrideLocal = 0;
    save_state(id);
  }

  const RemoteResult r = try_remote(origin, id, requested);
  // El valor se da por "visto" salvo que haya quedado APLAZADO: un aplazado
  // vive solo en RAM, y si la unidad reinicia antes de aplicarlo hay que
  // volver a intentarlo en la siguiente conexion, no darlo por reenvio.
  if (r != RemoteResult::Deferred) {
    record_remote(id, requested);
  }
}

}  // namespace

const char *runtime_config_origin_name(RuntimeConfigOrigin origin) {
  switch (origin) {
    case RCFG_ORIGIN_USB:
      return "usb";
    case RCFG_ORIGIN_WEB:
      return "web";
    case RCFG_ORIGIN_TB_GPRS:
      return "tb-gprs";
    case RCFG_ORIGIN_TB_WIFI:
      return "tb-wifi";
  }
  return "?";
}

bool runtime_config_apply(RuntimeConfigOrigin origin, const char *name,
                          float value, char *msg, size_t msg_len) {
  RuntimeConfigId id;
  if (is_remote(origin) || !lookup_for(origin, name, &id)) {
    ESP_LOGW(TAG, "origin=%s unknown or not applicable parameter, not applied",
             runtime_config_origin_name(origin));
    set_msg(msg, msg_len, name != nullptr ? name : "?", "unknown parameter");
    return false;
  }
  Lock lock;
  if (!lock.ok()) {
    set_msg(msg, msg_len, name, "busy");
    return false;
  }
  ensure_loaded();
  return apply_local(origin, id, value, msg, msg_len);
}

bool runtime_config_apply_text(RuntimeConfigOrigin origin, const char *name,
                               const char *text, char *msg, size_t msg_len) {
  RuntimeConfigId id;
  if (is_remote(origin) || !lookup_for(origin, name, &id)) {
    ESP_LOGW(TAG, "origin=%s unknown or not applicable parameter, not applied",
             runtime_config_origin_name(origin));
    set_msg(msg, msg_len, name != nullptr ? name : "?", "unknown parameter");
    return false;
  }
  float value = 0.0f;
  if (!runtime_config_parse_number(text, &value)) {
    ESP_LOGW(TAG, "origin=%s name=%s rejected: not a number",
             runtime_config_origin_name(origin), runtime_config_name(id));
    set_msg(msg, msg_len, runtime_config_name(id), "not a number");
    return false;
  }
  Lock lock;
  if (!lock.ok()) {
    set_msg(msg, msg_len, runtime_config_name(id), "busy");
    return false;
  }
  ensure_loaded();
  return apply_local(origin, id, value, msg, msg_len);
}

bool runtime_config_check_text(RuntimeConfigOrigin origin, const char *name,
                               const char *text, char *msg, size_t msg_len) {
  RuntimeConfigId id;
  if (is_remote(origin) || !lookup_for(origin, name, &id)) {
    set_msg(msg, msg_len, name != nullptr ? name : "?", "unknown parameter");
    return false;
  }
  float value = 0.0f;
  if (!runtime_config_parse_number(text, &value)) {
    set_msg(msg, msg_len, runtime_config_name(id), "not a number");
    return false;
  }
  float applied = 0.0f;
  const RuntimeConfigStatus st =
      (origin == RCFG_ORIGIN_USB)
          ? runtime_config_validate_strict(id, value, &applied)
          : runtime_config_validate(id, value, &applied);
  if (!runtime_config_status_accepted(st)) {
    set_msg(msg, msg_len, runtime_config_name(id), runtime_config_status_text(st));
    return false;
  }
  return true;
}

size_t runtime_config_apply_json(RuntimeConfigOrigin origin,
                                 JsonObjectConst data, bool new_write) {
  if (!is_remote(origin)) {
    return 0;
  }
  Lock lock;
  if (!lock.ok()) {
    ESP_LOGE(TAG, "origin=%s busy, remote attributes dropped",
             runtime_config_origin_name(origin));
    return 0;
  }
  ensure_loaded();
  size_t applied = 0;
  // Se recorre LA LISTA BLANCA CONOCIDA y se pregunta por cada nombre exacto:
  // lo que se escriba en otras claves no se mira siquiera.
  for (int i = 0; i < RCFG_COUNT; ++i) {
    const RuntimeConfigId id = (RuntimeConfigId)i;
    const char *name = runtime_config_name(id);
    JsonVariantConst v = data[name];
    if (v.isNull()) {
      continue;
    }
    float requested = 0.0f;
    // Un booleano solo vale para fan_pid_en.
    if (!runtime_config_json_number(v, id == RCFG_FAN_PID_EN, &requested)) {
      ESP_LOGW(TAG, "origin=%s name=%s rejected: wrong value type",
               runtime_config_origin_name(origin), name);
      note_rejected(name, "wrong value type");
      continue;
    }
    if (!runtime_config_remote_allowed(id, RCFG_REMOTE_SAFETY_PARAMS != 0)) {
      // No entra en el estado persistente: no es una escritura que se vaya a
      // aplicar nunca.
      (void)try_remote(origin, id, requested);
      continue;
    }
    handle_remote_key(origin, id, requested, new_write);
    ++applied;
  }
  return applied;
}

void runtime_config_on_update_gprs(JsonObjectConst const &data) {
  (void)runtime_config_apply_json(RCFG_ORIGIN_TB_GPRS, data, true);
}
void runtime_config_on_update_wifi(JsonObjectConst const &data) {
  (void)runtime_config_apply_json(RCFG_ORIGIN_TB_WIFI, data, true);
}
void runtime_config_on_request_gprs(JsonObjectConst const &data) {
  (void)runtime_config_apply_json(RCFG_ORIGIN_TB_GPRS, data, false);
}
void runtime_config_on_request_wifi(JsonObjectConst const &data) {
  (void)runtime_config_apply_json(RCFG_ORIGIN_TB_WIFI, data, false);
}

const char *const *runtime_config_keys(size_t *count) {
  static const char *keys[RCFG_COUNT];
  RuntimeConfigId ids[RCFG_COUNT];
  const size_t n =
      runtime_config_remote_keys(RCFG_REMOTE_SAFETY_PARAMS != 0, ids);
  for (size_t i = 0; i < n; ++i) {
    keys[i] = runtime_config_name(ids[i]);
  }
  if (count != nullptr) {
    *count = n;
  }
  return keys;
}

void runtime_config_poll(void) {
  if (s_pendingMask.load() == 0) {
    return;
  }
  Lock lock;
  if (!lock.ok()) {
    return;
  }
  ensure_loaded();
  for (int i = 0; i < RCFG_COUNT; ++i) {
    const RuntimeConfigId id = (RuntimeConfigId)i;
    if (!s_pending[i].valid) {
      continue;
    }
    // Todavia no toca: se queda donde esta.
    const bool wait =
        runtime_config_should_defer(id, therapy_active()) ||
        !runtime_config_rate_ok(s_hasLastRemoteChange[i], s_lastRemoteChangeMs[i],
                                millis(), (uint32_t)RCFG_REMOTE_MIN_INTERVAL_MS);
    if (wait) {
      continue;
    }
    const Pending p = s_pending[i];
    s_pending[i].valid = false;
    s_pendingMask.fetch_and(~(uint32_t)RCFG_MASK(id));
    if (try_remote(p.origin, id, p.requested) != RemoteResult::Deferred) {
      record_remote(id, p.requested);
    }
  }
}

void runtime_config_echo_all(void) {
  s_echoPending.fetch_or((uint32_t)RCFG_MASK_ALL);
}

size_t runtime_config_echo_build(char *json, size_t len,
                                 RuntimeConfigEchoToken *token) {
  if ((s_echoPending.load() & RCFG_MASK_ALL) == 0 || token == nullptr) {
    return 0;
  }
  Lock lock;
  if (!lock.ok()) {
    return 0;
  }
  ensure_loaded();
  const uint16_t pending = (uint16_t)(s_echoPending.load() & RCFG_MASK_ALL);
  const char *src[RCFG_COUNT];
  uint32_t epoch[RCFG_COUNT];
  for (int i = 0; i < RCFG_COUNT; ++i) {
    token->sent[i] = current_value((RuntimeConfigId)i);
    const uint8_t code = s_state[i].src;
    src[i] = (code == SRC_NONE)
                 ? nullptr
                 : runtime_config_origin_name((RuntimeConfigOrigin)(code - 1));
    epoch[i] = s_state[i].epoch;
  }
  const size_t n =
      runtime_config_format_echo(token->sent, pending, src, epoch, json, len);
  if (n == 0) {
    return 0;
  }
  token->mask = pending;
  return n;
}

void runtime_config_echo_done(const RuntimeConfigEchoToken *token) {
  if (token == nullptr) {
    return;
  }
  Lock lock;
  if (!lock.ok()) {
    // No se pudo coger: se deja el eco pendiente (saldra otra vez, redundante).
    return;
  }
  // Primero se limpia lo enviado, y DESPUES se vuelve a marcar lo que cambio
  // mientras el mensaje estaba en vuelo: asi el orden no puede perder un cambio.
  s_echoPending.fetch_and(~(uint32_t)token->mask);
  for (int i = 0; i < RCFG_COUNT; ++i) {
    if ((token->mask & RCFG_MASK(i)) &&
        current_value((RuntimeConfigId)i) != token->sent[i]) {
      s_echoPending.fetch_or((uint32_t)RCFG_MASK(i));
    }
  }
}

size_t runtime_config_audit_build(char *json, size_t len, uint32_t *seq) {
  Lock lock;
  if (!lock.ok() || s_auditCount == 0 || seq == nullptr) {
    return 0;
  }
  const AuditEvent &e = s_audit[s_auditHead];
  const size_t n = runtime_config_format_audit(
      e.id, e.oldValue, e.newValue, runtime_config_origin_name(e.origin),
      e.epoch, json, len);
  if (n == 0) {
    // No deberia pasar con RCFG_AUDIT_JSON_MAX; no se atasca la cola por esto.
    s_auditHead = (s_auditHead + 1) % AUDIT_CAP;
    --s_auditCount;
    return 0;
  }
  *seq = e.seq;
  return n;
}

void runtime_config_audit_done(uint32_t seq) {
  Lock lock;
  if (!lock.ok() || s_auditCount == 0) {
    return;
  }
  // Solo si el evento sigue siendo el que se publico: dos tareas no pueden
  // sacar dos eventos por uno.
  if (s_audit[s_auditHead].seq == seq) {
    s_auditHead = (s_auditHead + 1) % AUDIT_CAP;
    --s_auditCount;
  }
}

size_t runtime_config_rejected_build(char *json, size_t len, uint32_t *seq) {
  Lock lock;
  if (!lock.ok() || !s_rejectedValid || seq == nullptr) {
    return 0;
  }
  const size_t n = strlen(s_rejectedJson);
  if (n == 0 || n >= len) {
    return 0;
  }
  memcpy(json, s_rejectedJson, n + 1);
  *seq = s_rejectedSeq;
  return n;
}

void runtime_config_rejected_done(uint32_t seq) {
  Lock lock;
  if (lock.ok() && s_rejectedValid && s_rejectedSeq == seq) {
    s_rejectedValid = false;
  }
}
