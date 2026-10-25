#include "modules/config/runtime_config_rules.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alarm_policy.h"

namespace {

enum class Kind : uint8_t { PwmInt, Bool, HeaterAmps, AirCutout, SkinCutout, PeriodInt };

struct Entry {
  const char *name;
  Kind kind;
};

// El orden es el de RuntimeConfigId: lo comprueba el static_assert de abajo.
const Entry TABLE[RCFG_COUNT] = {
    {"fan_supply_pwm", Kind::PwmInt},
    {"fan_ctl_pwm", Kind::PwmInt},
    {"fan_pid_en", Kind::Bool},
    {"heater_amps", Kind::HeaterAmps},
    {"air_tmax", Kind::AirCutout},
    {"skin_tmax", Kind::SkinCutout},
    {"gprs_act", Kind::PeriodInt},
    {"gprs_photo", Kind::PeriodInt},
    {"gprs_stby", Kind::PeriodInt},
};

static_assert(RCFG_COUNT == 9, "la lista blanca son exactamente nueve parametros");
static_assert(RCFG_COUNT <= 16, "la mascara de pendientes es de 16 bits");

bool valid_id(RuntimeConfigId id) { return (unsigned)id < (unsigned)RCFG_COUNT; }

// Entero dentro de [lo, hi]. El orden importa: primero que sea entero, luego el
// rango, para que 130.5 se diga "no entero" y no "fuera de rango".
RuntimeConfigStatus check_int(float in, float lo, float hi, float *out) {
  if (in != floorf(in)) {
    return RCFG_REJ_NOT_INTEGER;
  }
  if (in < lo || in > hi) {
    return RCFG_REJ_RANGE;
  }
  *out = in;
  return RCFG_OK;
}

// Solo estos caracteres pueden formar parte de un numero aceptado. Deja fuera
// de un plumazo "nan", "inf" y los hexadecimales que strtof si entiende.
bool numeric_chars_only(const char *s) {
  for (; *s; ++s) {
    if (!isdigit((unsigned char)*s) && *s != '+' && *s != '-' && *s != '.' &&
        *s != 'e' && *s != 'E') {
      return false;
    }
  }
  return true;
}

// Regla base. strict = true rechaza los cortes fuera de 34..40 en vez de
// recortarlos.
RuntimeConfigStatus validate_impl(RuntimeConfigId id, float in, bool strict,
                                  float *out) {
  if (!valid_id(id) || out == nullptr) {
    return RCFG_REJ_RANGE;
  }
  if (!isfinite(in)) {
    return RCFG_REJ_NOT_FINITE;
  }
  switch (TABLE[id].kind) {
    case Kind::PwmInt:
      return check_int(in, (float)RCFG_PWM_MIN, (float)RCFG_PWM_MAX, out);
    case Kind::PeriodInt:
      return check_int(in, (float)RCFG_PERIOD_MIN_S, (float)RCFG_PERIOD_MAX_S,
                       out);
    case Kind::Bool:
      if (in != 0.0f && in != 1.0f) {
        return RCFG_REJ_NOT_BOOL;
      }
      *out = in;
      return RCFG_OK;
    case Kind::HeaterAmps:
      if (in < RCFG_HEATER_AMPS_MIN || in > RCFG_HEATER_AMPS_MAX) {
        return RCFG_REJ_RANGE;
      }
      *out = in;
      return RCFG_OK;
    case Kind::AirCutout:
      if (strict) {
        if (in < ALARM_CUTOUT_MIN_C || in > ALARM_AIR_CUTOUT_MAX_C) {
          return RCFG_REJ_RANGE;
        }
        *out = in;
        return RCFG_OK;
      }
      *out = alarm_clamp_air_cutout(in);
      return (*out != in) ? RCFG_OK_CLAMPED : RCFG_OK;
    case Kind::SkinCutout:
      if (strict) {
        if (in < ALARM_CUTOUT_MIN_C || in > ALARM_SKIN_CUTOUT_MAX_C) {
          return RCFG_REJ_RANGE;
        }
        *out = in;
        return RCFG_OK;
      }
      *out = alarm_clamp_skin_cutout(in);
      return (*out != in) ? RCFG_OK_CLAMPED : RCFG_OK;
  }
  return RCFG_REJ_RANGE;
}

// Valor de un parametro como texto JSON (sin comillas): enteros sin decimales,
// el booleano como 0/1, el resto con dos decimales.
int fmt_value(RuntimeConfigId id, float v, char *buf, size_t len) {
  switch (TABLE[id].kind) {
    case Kind::PwmInt:
    case Kind::PeriodInt:
    case Kind::Bool:
      return snprintf(buf, len, "%ld", (long)lroundf(v));
    default:
      return snprintf(buf, len, "%.2f", (double)v);
  }
}

// Copia `s` en buf[off..) cambiando por '?' lo que romperia un literal JSON.
// Devuelve el nuevo offset, o 0 si no cabe (deja al menos 1 byte para el NUL).
size_t append_json_text(char *buf, size_t len, size_t off, const char *s) {
  for (; *s; ++s) {
    if (off + 1 >= len) {
      return 0;
    }
    const unsigned char c = (unsigned char)*s;
    buf[off++] = (c < 0x20 || c == '"' || c == '\\' || c >= 0x7F) ? '?' : (char)c;
  }
  return off;
}

}  // namespace

const char *runtime_config_name(RuntimeConfigId id) {
  if (!valid_id(id)) {
    return nullptr;
  }
  return TABLE[id].name;
}

bool runtime_config_lookup(const char *name, RuntimeConfigId *id) {
  if (name == nullptr || id == nullptr) {
    return false;
  }
  for (int i = 0; i < RCFG_COUNT; ++i) {
    if (strcmp(name, TABLE[i].name) == 0) {
      *id = (RuntimeConfigId)i;
      return true;
    }
  }
  return false;
}

bool runtime_config_lookup_console(const char *name, RuntimeConfigId *id) {
  if (name == nullptr || id == nullptr) {
    return false;
  }
  for (int i = 0; i < RCFG_COUNT; ++i) {
    const char *canon = TABLE[i].name;
    size_t k = 0;
    while (canon[k] != '\0' && name[k] == (char)toupper((unsigned char)canon[k])) {
      ++k;
    }
    if (canon[k] == '\0' && name[k] == '\0') {
      *id = (RuntimeConfigId)i;
      return true;
    }
  }
  return false;
}

bool runtime_config_is_safety(RuntimeConfigId id) {
  return valid_id(id) && TABLE[id].kind != Kind::PeriodInt;
}

bool runtime_config_remote_allowed(RuntimeConfigId id, bool safety_enabled) {
  if (!valid_id(id)) {
    return false;
  }
  return safety_enabled || !runtime_config_is_safety(id);
}

size_t runtime_config_remote_keys(bool safety_enabled,
                                  RuntimeConfigId out[RCFG_COUNT]) {
  size_t n = 0;
  for (int i = 0; i < RCFG_COUNT; ++i) {
    if (runtime_config_remote_allowed((RuntimeConfigId)i, safety_enabled)) {
      out[n++] = (RuntimeConfigId)i;
    }
  }
  return n;
}

RuntimeConfigStatus runtime_config_validate(RuntimeConfigId id, float in,
                                            float *out) {
  return validate_impl(id, in, false, out);
}

RuntimeConfigStatus runtime_config_validate_strict(RuntimeConfigId id, float in,
                                                   float *out) {
  return validate_impl(id, in, true, out);
}

RuntimeConfigStatus runtime_config_validate_remote(
    RuntimeConfigId id, float in, const RuntimeConfigRemoteCtx *ctx,
    float *out) {
  if (ctx == nullptr || out == nullptr || !valid_id(id)) {
    return RCFG_REJ_RANGE;
  }
  if (!isfinite(in)) {
    return RCFG_REJ_NOT_FINITE;
  }
  const Kind kind = TABLE[id].kind;

  // Ventilador: sin tacometro no hay forma de saber que sigue girando, y con
  // el calefactor actuando un ventilador mal ajustado recalienta el aire.
  if (kind == Kind::PwmInt || kind == Kind::Bool) {
    if (!ctx->fan_has_tach) {
      return RCFG_REJ_NO_TACHOMETER;
    }
    if (ctx->heater_acting) {
      return RCFG_REJ_HEATER_ACTING;
    }
  }

  float v = 0.0f;
  const RuntimeConfigStatus base = validate_impl(id, in, true, &v);
  if (!runtime_config_status_accepted(base)) {
    return base;
  }

  switch (kind) {
    case Kind::PwmInt:
      if (v < (float)RCFG_FAN_PWM_REMOTE_MIN) {
        return RCFG_REJ_RANGE;
      }
      break;
    case Kind::HeaterAmps:
      if (v > ctx->heater_factory_max) {
        return RCFG_REJ_ABOVE_FACTORY;
      }
      break;
    case Kind::AirCutout:
      if (v > ctx->air_factory_max) {
        return RCFG_REJ_ABOVE_FACTORY;
      }
      if (ctx->air_setpoint_c > 0.0f && v <= ctx->air_setpoint_c) {
        return RCFG_REJ_BELOW_SETPOINT;
      }
      break;
    case Kind::SkinCutout:
      if (v > ctx->skin_factory_max) {
        return RCFG_REJ_ABOVE_FACTORY;
      }
      if (ctx->skin_setpoint_c > 0.0f && v <= ctx->skin_setpoint_c) {
        return RCFG_REJ_BELOW_SETPOINT;
      }
      break;
    case Kind::Bool:
    case Kind::PeriodInt:
      break;
  }
  *out = v;
  return RCFG_OK;
}

bool runtime_config_should_defer(RuntimeConfigId id, bool therapy_active) {
  return therapy_active && runtime_config_is_safety(id);
}

bool runtime_config_rate_ok(bool has_last, uint32_t last_ms, uint32_t now_ms,
                            uint32_t min_ms) {
  if (!has_last) {
    return true;
  }
  return (uint32_t)(now_ms - last_ms) >= min_ms;  // aritmetica modular
}

const char *runtime_config_status_text(RuntimeConfigStatus s) {
  switch (s) {
    case RCFG_OK:
      return "ok";
    case RCFG_OK_CLAMPED:
      return "clamped to the allowed range";
    case RCFG_REJ_NOT_FINITE:
      return "not a finite number";
    case RCFG_REJ_RANGE:
      return "out of range";
    case RCFG_REJ_NOT_INTEGER:
      return "must be an integer";
    case RCFG_REJ_NOT_BOOL:
      return "must be 0 or 1";
    case RCFG_REJ_ABOVE_FACTORY:
      return "above the factory value";
    case RCFG_REJ_BELOW_SETPOINT:
      return "below the active setpoint";
    case RCFG_REJ_NO_TACHOMETER:
      return "unit has no fan tachometer";
    case RCFG_REJ_HEATER_ACTING:
      return "heater is acting";
    case RCFG_REJ_NOT_ALLOWED:
      return "not allowed remotely";
    case RCFG_REJ_WRONG_TYPE:
      return "wrong value type";
  }
  return "unknown";
}

bool runtime_config_parse_number(const char *text, float *out) {
  if (text == nullptr || out == nullptr) {
    return false;
  }
  while (isspace((unsigned char)*text)) {
    ++text;
  }
  // Longitud del numero sin los espacios finales.
  size_t len = strlen(text);
  while (len > 0 && isspace((unsigned char)text[len - 1])) {
    --len;
  }
  if (len == 0 || len >= 32) {
    return false;
  }
  char buf[32];
  memcpy(buf, text, len);
  buf[len] = '\0';
  if (!numeric_chars_only(buf)) {
    return false;
  }
  char *end = nullptr;
  const float v = strtof(buf, &end);
  if (end == buf || *end != '\0' || !isfinite(v)) {
    return false;
  }
  *out = v;
  return true;
}

bool runtime_config_json_number(JsonVariantConst v, bool allow_bool,
                                float *out) {
  if (out == nullptr) {
    return false;
  }
  if (v.is<bool>()) {
    if (!allow_bool) {
      return false;
    }
    *out = v.as<bool>() ? 1.0f : 0.0f;
    return true;
  }
  if (v.is<float>()) {  // en ArduinoJson 6 cubre enteros y reales, no cadenas
    *out = v.as<float>();
    return true;
  }
  return false;
}

int runtime_config_sanitize_period(int v, int def) {
  return (v >= RCFG_PERIOD_MIN_S && v <= RCFG_PERIOD_MAX_S) ? v : def;
}

float runtime_config_sanitize_heater_amps(float v, float def) {
  return (isfinite(v) && v >= RCFG_HEATER_AMPS_MIN && v <= RCFG_HEATER_AMPS_MAX)
             ? v
             : def;
}

size_t runtime_config_format_echo(const float values[RCFG_COUNT],
                                  uint16_t mask,
                                  const char *const src[RCFG_COUNT],
                                  const uint32_t epoch[RCFG_COUNT], char *buf,
                                  size_t len) {
  if (buf == nullptr || len == 0) {
    return 0;
  }
  buf[0] = '\0';
  int n = snprintf(buf, len, "{");
  if (n < 0 || (size_t)n >= len) {
    buf[0] = '\0';
    return 0;
  }
  size_t off = (size_t)n;
  bool any = false;
  for (int i = 0; i < RCFG_COUNT; ++i) {
    if (!(mask & RCFG_MASK(i))) {
      continue;
    }
    const char *sep = any ? "," : "";
    const float v = values[i];
    if (TABLE[i].kind == Kind::Bool) {
      n = snprintf(buf + off, len - off, "%s\"%s\":%s", sep, TABLE[i].name,
                   v != 0.0f ? "true" : "false");
    } else {
      char num[24];
      fmt_value((RuntimeConfigId)i, v, num, sizeof(num));
      n = snprintf(buf + off, len - off, "%s\"%s\":%s", sep, TABLE[i].name, num);
    }
    if (n < 0 || (size_t)n >= len - off) {
      buf[0] = '\0';
      return 0;
    }
    off += (size_t)n;
    any = true;
    if (src != nullptr && src[i] != nullptr && src[i][0] != '\0') {
      const uint32_t e = (epoch != nullptr) ? epoch[i] : 0;
      if (e != 0) {
        n = snprintf(buf + off, len - off, ",\"cfg_src_%s\":\"%s@%lu\"",
                     TABLE[i].name, src[i], (unsigned long)e);
      } else {
        n = snprintf(buf + off, len - off, ",\"cfg_src_%s\":\"%s\"",
                     TABLE[i].name, src[i]);
      }
      if (n < 0 || (size_t)n >= len - off) {
        buf[0] = '\0';
        return 0;
      }
      off += (size_t)n;
    }
  }
  if (!any || off + 2 > len) {
    buf[0] = '\0';
    return 0;
  }
  buf[off++] = '}';
  buf[off] = '\0';
  return off;
}

size_t runtime_config_format_audit(RuntimeConfigId id, float old_v, float new_v,
                                   const char *origin, uint32_t epoch,
                                   char *buf, size_t len) {
  if (!valid_id(id) || buf == nullptr || len == 0) {
    return 0;
  }
  char o[24];
  char nw[24];
  fmt_value(id, old_v, o, sizeof(o));
  fmt_value(id, new_v, nw, sizeof(nw));
  const int n = snprintf(buf, len,
                         "{\"cfg_change\":{\"key\":\"%s\",\"old\":%s,\"new\":%s,"
                         "\"origin\":\"%s\",\"epoch\":%lu}}",
                         TABLE[id].name, o, nw, origin != nullptr ? origin : "?",
                         (unsigned long)epoch);
  if (n < 0 || (size_t)n >= len) {
    buf[0] = '\0';
    return 0;
  }
  return (size_t)n;
}

size_t runtime_config_format_rejected(const char *name, const char *why,
                                      char *buf, size_t len) {
  static const char HEAD[] = "{\"cfg_rejected\":\"";
  if (buf == nullptr || len <= sizeof(HEAD)) {
    return 0;
  }
  memcpy(buf, HEAD, sizeof(HEAD) - 1);
  size_t off = sizeof(HEAD) - 1;
  off = append_json_text(buf, len, off, name != nullptr ? name : "?");
  if (off == 0) {
    buf[0] = '\0';
    return 0;
  }
  off = append_json_text(buf, len, off, ": ");
  if (off == 0) {
    buf[0] = '\0';
    return 0;
  }
  off = append_json_text(buf, len, off, why != nullptr ? why : "?");
  if (off == 0 || off + 3 > len) {
    buf[0] = '\0';
    return 0;
  }
  buf[off++] = '"';
  buf[off++] = '}';
  buf[off] = '\0';
  return off;
}

// ESQUELETO EN ROJO (revision 2): la implementacion llega en el commit siguiente.
bool runtime_config_parse_console_line(const char *, char *, size_t, char *,
                                       size_t) {
  return false;
}
