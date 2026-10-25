#include "modules/config/runtime_config_rules.h"

// ESQUELETO EN ROJO: la API existe para que los tests compilen y fallen por
// comportamiento, no por falta de simbolos. La implementacion llega en el
// commit siguiente.

const char *runtime_config_name(RuntimeConfigId) { return nullptr; }

bool runtime_config_lookup(const char *, RuntimeConfigId *) { return false; }

RuntimeConfigStatus runtime_config_validate(RuntimeConfigId, float, float *) {
  return RCFG_REJ_RANGE;
}

const char *runtime_config_status_text(RuntimeConfigStatus) { return ""; }

bool runtime_config_parse_number(const char *, float *) { return false; }

bool runtime_config_json_number(JsonVariantConst, float *) { return false; }

size_t runtime_config_format_echo(const float[RCFG_COUNT], uint16_t, char *,
                                  size_t) {
  return 0;
}
