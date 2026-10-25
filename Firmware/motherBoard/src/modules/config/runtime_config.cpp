#include "modules/config/runtime_config.h"

#include <Arduino.h>
#include <Preferences.h>

#include <atomic>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "main.h"
#include "config/preferences_keys.h"
#include "system/hw_selftest.h"

extern IncuNest_parameters in3;
extern float maxDesiredTemp[2];
extern PID fanControlPID;

namespace {

const char *TAG = "CFG";

// Parametros con cambio pendiente de eco. std::atomic<uint32_t> y no uint16_t:
// el Xtensa tiene atomicos nativos de 32 bits y los de 16 se resuelven con una
// emulacion que no hace falta traer.
std::atomic<uint32_t> s_echoPending{0};

// Valor que llevaba el ultimo JSON de eco construido; ver
// runtime_config_echo_done(). Solo lo tocan build y done, que corren en la
// misma tarea.
float s_echoSent[RCFG_COUNT] = {};

// Valor que tiene ahora la placa para cada parametro.
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
      // Persiste por dentro (Actuators.cpp) y baja el PID a manual si hace falta.
      setFanPidEnabled(v != 0.0f);
      return true;
    case RCFG_HEATER_AMPS:
      if (!persist_float(NS_CFG, KEY_HEAT_MAX_A, v)) return false;
      in3.heaterMaxPowerAmps = v;
      return true;
    case RCFG_AIR_TMAX:
      if (!persist_float(NS_CFG, KEY_AIR_T_MAX, v)) return false;
      in3.airTemperatureSetMax = v;
      maxDesiredTemp[CONTROL_AIR] = v;
      return true;
    case RCFG_SKIN_TMAX:
      if (!persist_float(NS_CFG, KEY_SKIN_T_MAX, v)) return false;
      in3.skinTemperatureSetMax = v;
      maxDesiredTemp[CONTROL_SKIN] = v;
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

void set_msg(char *msg, size_t msg_len, const char *fmt, const char *name,
             const char *why) {
  if (msg != nullptr && msg_len > 0) {
    snprintf(msg, msg_len, fmt, name, why);
  }
}

// Nucleo comun: regla -> (si cambia) persistencia + efecto -> eco -> log.
bool apply_id(RuntimeConfigOrigin origin, RuntimeConfigId id, float requested,
              char *msg, size_t msg_len) {
  const char *name = runtime_config_name(id);
  const char *org = runtime_config_origin_name(origin);

  float applied = 0.0f;
  const RuntimeConfigStatus st = runtime_config_validate(id, requested, &applied);
  if (!runtime_config_status_accepted(st)) {
    // ESP_LOGW y no logI: logI esta detras de LOG_INFORMATION (false) y un
    // rechazo de un parametro de seguridad no puede ser invisible.
    ESP_LOGW(TAG, "origin=%s name=%s requested=%.2f rejected: %s", org, name,
             (double)requested, runtime_config_status_text(st));
    set_msg(msg, msg_len, "%s: %s", name, runtime_config_status_text(st));
    return false;
  }

  // Idempotente: lo que TB reenvia en cada conexion no gasta escrituras de
  // flash cuando ya coincide.
  const bool changed = (current_value(id) != applied);
  if (changed && !store(id, applied)) {
    ESP_LOGE(TAG, "origin=%s name=%s requested=%.2f NVS write failed, unchanged",
             org, name, (double)requested);
    set_msg(msg, msg_len, "%s: %s", name, "could not be saved");
    return false;
  }

  s_echoPending.fetch_or((uint32_t)RCFG_MASK(id));

  ESP_LOGI(TAG, "origin=%s name=%s requested=%.2f applied=%.2f%s%s", org, name,
           (double)requested, (double)applied,
           (st == RCFG_OK_CLAMPED) ? " (clamped)" : "",
           changed ? "" : " (unchanged)");
  set_msg(msg, msg_len, "%s: %s", name,
          st == RCFG_OK_CLAMPED ? "clamped to the allowed range" : "ok");
  return true;
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
  if (!runtime_config_lookup(name, &id)) {
    ESP_LOGW(TAG, "origin=%s unknown parameter, not applied",
             runtime_config_origin_name(origin));
    set_msg(msg, msg_len, "%s: %s", name != nullptr ? name : "?",
            "unknown parameter");
    return false;
  }
  return apply_id(origin, id, value, msg, msg_len);
}

bool runtime_config_apply_text(RuntimeConfigOrigin origin, const char *name,
                               const char *text, char *msg, size_t msg_len) {
  RuntimeConfigId id;
  if (!runtime_config_lookup(name, &id)) {
    ESP_LOGW(TAG, "origin=%s unknown parameter, not applied",
             runtime_config_origin_name(origin));
    set_msg(msg, msg_len, "%s: %s", name != nullptr ? name : "?",
            "unknown parameter");
    return false;
  }
  float value = 0.0f;
  if (!runtime_config_parse_number(text, &value)) {
    ESP_LOGW(TAG, "origin=%s name=%s rejected: not a number",
             runtime_config_origin_name(origin), runtime_config_name(id));
    set_msg(msg, msg_len, "%s: %s", runtime_config_name(id), "not a number");
    return false;
  }
  return apply_id(origin, id, value, msg, msg_len);
}

size_t runtime_config_apply_json(RuntimeConfigOrigin origin,
                                 JsonObjectConst data) {
  size_t accepted = 0;
  // Se recorre LA LISTA BLANCA y se pregunta por cada nombre exacto: lo que
  // el atacante escriba en otras claves no se mira siquiera.
  for (int i = 0; i < RCFG_COUNT; ++i) {
    const RuntimeConfigId id = (RuntimeConfigId)i;
    const char *name = runtime_config_name(id);
    JsonVariantConst v = data[name];
    if (v.isNull()) {
      continue;
    }
    float requested = 0.0f;
    if (!runtime_config_json_number(v, &requested)) {
      ESP_LOGW(TAG, "origin=%s name=%s rejected: not a number or boolean",
               runtime_config_origin_name(origin), name);
      continue;
    }
    if (apply_id(origin, id, requested, nullptr, 0)) {
      ++accepted;
    }
  }
  return accepted;
}

void runtime_config_on_shared_gprs(JsonObjectConst const &data) {
  (void)runtime_config_apply_json(RCFG_ORIGIN_TB_GPRS, data);
}

void runtime_config_on_shared_wifi(JsonObjectConst const &data) {
  (void)runtime_config_apply_json(RCFG_ORIGIN_TB_WIFI, data);
}

const char *const *runtime_config_keys(size_t *count) {
  static const char *keys[RCFG_COUNT];
  for (int i = 0; i < RCFG_COUNT; ++i) {
    keys[i] = runtime_config_name((RuntimeConfigId)i);
  }
  if (count != nullptr) {
    *count = (size_t)RCFG_COUNT;
  }
  return keys;
}

void runtime_config_echo_all(void) {
  s_echoPending.fetch_or((uint32_t)RCFG_MASK_ALL);
}

size_t runtime_config_echo_build(char *json, size_t len, uint16_t *mask) {
  const uint16_t pending = (uint16_t)(s_echoPending.load() & RCFG_MASK_ALL);
  if (pending == 0) {
    return 0;
  }
  for (int i = 0; i < RCFG_COUNT; ++i) {
    s_echoSent[i] = current_value((RuntimeConfigId)i);
  }
  const size_t n = runtime_config_format_echo(s_echoSent, pending, json, len);
  if (n == 0) {
    return 0;
  }
  if (mask != nullptr) {
    *mask = pending;
  }
  return n;
}

void runtime_config_echo_done(uint16_t mask) {
  for (int i = 0; i < RCFG_COUNT; ++i) {
    const RuntimeConfigId id = (RuntimeConfigId)i;
    // Si el valor cambio entre el build y el done, el mensaje que acaba de
    // salir ya no lo refleja: el bit se queda y sale en el siguiente.
    if ((mask & RCFG_MASK(id)) && current_value(id) == s_echoSent[i]) {
      s_echoPending.fetch_and(~(uint32_t)RCFG_MASK(id));
    }
  }
}
