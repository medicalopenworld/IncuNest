#include "batt_report.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

// Mismo lector de campo que sb_env.cpp: entero decimal con signo opcional que
// termina en `,` o, si `last`, tambien en el final de la cadena. strtoll y no
// strtol: `long` es de 32 bits en el ESP32 y de 64 en el host de los tests.
static bool parse_field(const char **p, long long *value, bool last) {
  const char *s = *p;
  const char *digits = (*s == '-') ? s + 1 : s;
  if (*digits < '0' || *digits > '9') return false;

  char *end = NULL;
  errno = 0;
  const long long v = strtoll(s, &end, 10);
  if (errno == ERANGE || v < -2147483647LL || v > 2147483647LL) return false;
  if (!(*end == ',' || (last && *end == '\0'))) return false;

  *value = v;
  *p = end;
  return true;
}

bool battrep_parse(const char *line, BattRepMsg *out) {
  static const char kPrefix[] = "CTRL,BATT,";
  if (!line || !out) return false;
  if (strncmp(line, kPrefix, sizeof(kPrefix) - 1) != 0) return false;

  const char *p = line + (sizeof(kPrefix) - 1);
  long long f[3];
  for (int i = 0; i < 3; i++) {
    if (!parse_field(&p, &f[i], /*last=*/i == 2)) return false;
    if (i < 2) p++;
  }

  if (f[0] < PROTO_BATT_STATE_UNKNOWN || f[0] > PROTO_BATT_STATE_FULL) return false;
  if (f[1] < 0 || f[1] > PROTO_BATT_LEVEL_MAX) return false;
  if (f[2] < 0 || f[2] > 100) return false;

  out->state = (int)f[0];
  out->level = (int)f[1];
  out->pct = (int)f[2];
  return true;
}

BattRepView battrep_view(bool have, const BattRepMsg *msg, uint32_t rx_ms,
                         uint32_t now_ms, bool boardLinkLost) {
  BattRepView v = {BATTREP_SHOW_NONE, 0};
  if (!have || !msg || boardLinkLost) return v;
  if ((uint32_t)(now_ms - rx_ms) > BATTREP_STALE_MS) return v;

  switch (msg->state) {
    case PROTO_BATT_STATE_DISCHARGING:
      v.show = BATTREP_SHOW_DISCHARGING;
      v.level = msg->level;
      break;
    case PROTO_BATT_STATE_CHARGING: {
      // Cuartos completos: con 60 % hay 2 llenos y se esta llenando el
      // tercero. La cuarta raya solo queda fija en FULL.
      const int quarters = msg->pct / 25;
      v.show = BATTREP_SHOW_CHARGING;
      v.level = quarters < PROTO_BATT_LEVEL_MAX ? quarters : PROTO_BATT_LEVEL_MAX - 1;
      break;
    }
    case PROTO_BATT_STATE_FULL:
      v.show = BATTREP_SHOW_FULL;
      v.level = PROTO_BATT_LEVEL_MAX;
      break;
    default:
      break;
  }
  return v;
}
