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

bool ieq(const char *a, const char *b) {
  for (; *a && *b; ++a, ++b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
      return false;
    }
  }
  return *a == *b;
}

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

}  // namespace

const char *runtime_config_name(RuntimeConfigId id) {
  if ((unsigned)id >= (unsigned)RCFG_COUNT) {
    return nullptr;
  }
  return TABLE[id].name;
}

bool runtime_config_lookup(const char *name, RuntimeConfigId *id) {
  if (name == nullptr || id == nullptr) {
    return false;
  }
  for (int i = 0; i < RCFG_COUNT; ++i) {
    if (ieq(name, TABLE[i].name)) {
      *id = (RuntimeConfigId)i;
      return true;
    }
  }
  return false;
}

RuntimeConfigStatus runtime_config_validate(RuntimeConfigId id, float in,
                                            float *out) {
  if ((unsigned)id >= (unsigned)RCFG_COUNT || out == nullptr) {
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
      *out = alarm_clamp_air_cutout(in);
      return (*out != in) ? RCFG_OK_CLAMPED : RCFG_OK;
    case Kind::SkinCutout:
      *out = alarm_clamp_skin_cutout(in);
      return (*out != in) ? RCFG_OK_CLAMPED : RCFG_OK;
  }
  return RCFG_REJ_RANGE;
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

bool runtime_config_json_number(JsonVariantConst v, float *out) {
  if (out == nullptr) {
    return false;
  }
  if (v.is<bool>()) {
    *out = v.as<bool>() ? 1.0f : 0.0f;
    return true;
  }
  if (v.is<float>()) {  // en ArduinoJson 6 cubre enteros y reales, no cadenas
    *out = v.as<float>();
    return true;
  }
  return false;
}

size_t runtime_config_format_echo(const float values[RCFG_COUNT],
                                  uint16_t mask, char *buf, size_t len) {
  if (buf == nullptr || len == 0) {
    return 0;
  }
  buf[0] = '\0';
  size_t off = 0;
  bool any = false;
  int n = snprintf(buf, len, "{");
  if (n < 0 || (size_t)n >= len) {
    buf[0] = '\0';
    return 0;
  }
  off = (size_t)n;
  for (int i = 0; i < RCFG_COUNT; ++i) {
    if (!(mask & RCFG_MASK(i))) {
      continue;
    }
    const float v = values[i];
    const char *sep = any ? "," : "";
    switch (TABLE[i].kind) {
      case Kind::PwmInt:
      case Kind::PeriodInt:
        n = snprintf(buf + off, len - off, "%s\"%s\":%ld", sep, TABLE[i].name,
                     (long)lroundf(v));
        break;
      case Kind::Bool:
        n = snprintf(buf + off, len - off, "%s\"%s\":%s", sep, TABLE[i].name,
                     v != 0.0f ? "true" : "false");
        break;
      default:
        n = snprintf(buf + off, len - off, "%s\"%s\":%.2f", sep, TABLE[i].name,
                     (double)v);
        break;
    }
    if (n < 0 || (size_t)n >= len - off) {
      buf[0] = '\0';
      return 0;
    }
    off += (size_t)n;
    any = true;
  }
  if (!any || off + 2 > len) {
    buf[0] = '\0';
    return 0;
  }
  buf[off++] = '}';
  buf[off] = '\0';
  return off;
}
