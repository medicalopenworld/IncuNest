#include "sb_env.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

// Un entero decimal con signo opcional, sin espacios ni '+', que termina en
// `,` o, si `last`, tambien en el final de la cadena. Avanza *p hasta ese
// terminador (sin consumirlo). false ante cualquier otra cosa: campo vacio,
// basura pegada al numero ("62x", "3.5") o desbordamiento.
//
// strtoll y no strtol: `long` es de 32 bits en el ESP32 y de 64 en el host de
// los tests; el rango se comprueba aqui a mano para que ambos coincidan.
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

bool sbenv_parse(const char *line, SbEnvMsg *out) {
  static const char kPrefix[] = "CTRL,ENV,";
  if (!line || !out) return false;
  if (strncmp(line, kPrefix, sizeof(kPrefix) - 1) != 0) return false;

  const char *p = line + (sizeof(kPrefix) - 1);
  long long f[4];
  for (int i = 0; i < 4; i++) {
    if (!parse_field(&p, &f[i], /*last=*/i == 3)) return false;
    // Entre campos queda la coma; tras el ultimo se aceptan campos extra o el
    // fin de la linea, y no hay nada mas que leer.
    if (i < 3) p++;
  }

  if (f[0] != 0 && f[0] != 1) return false;
  if (f[1] < PROTO_ENV_DOOR_UNKNOWN || f[1] > PROTO_ENV_DOOR_FAULT) return false;

  out->link = (f[0] == 1);
  out->door = (int)f[1];
  out->lux = (f[2] < 0) ? PROTO_ENV_UNAVAILABLE : (int)f[2];
  out->dba = (f[3] < 0) ? PROTO_ENV_UNAVAILABLE : (int)f[3];
  return true;
}

SbEnvNoise sbenv_noise_level(int dba) {
  if (dba < 0) return SBENV_NOISE_NONE;
  if (dba < SBENV_NOISE_MID_DBA) return SBENV_NOISE_LOW;
  if (dba < SBENV_NOISE_HIGH_DBA) return SBENV_NOISE_MID;
  return SBENV_NOISE_HIGH;
}

SbEnvView sbenv_view(bool have, const SbEnvMsg *msg, uint32_t rx_ms,
                     uint32_t now_ms, bool boardLinkLost) {
  SbEnvView v = {false, SBENV_NOISE_NONE};
  if (!have || !msg || boardLinkLost) return v;
  // Resta sin signo: correcta a traves de la vuelta de millis().
  if ((uint32_t)(now_ms - rx_ms) > SBENV_STALE_MS) return v;
  // Sin enlace con la SensorBoard no hay nada que contar: lo senaliza
  // ALARM_SENSORBOARD_LINK_LOST, no este indicador.
  if (!msg->link) return v;

  v.doorOpen = (msg->door == PROTO_ENV_DOOR_OPEN);
  v.noise = sbenv_noise_level(msg->dba);
  return v;
}
