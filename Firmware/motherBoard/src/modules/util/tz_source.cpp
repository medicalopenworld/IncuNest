#include "tz_source.h"

#include <stdlib.h>
#include <string.h>

static int8_t s_quarters = 0;
static TzSource s_source = TZ_SOURCE_NONE;

void tz_source_reset(void) {
  s_quarters = 0;
  s_source = TZ_SOURCE_NONE;
}

int8_t tz_source_quarters(void) { return s_quarters; }
TzSource tz_source_origin(void) { return s_source; }
bool tz_source_known(void) { return s_source != TZ_SOURCE_NONE; }

bool tz_source_set(int quarterHours, TzSource src) {
  if (src == TZ_SOURCE_NONE) return false;
  // Rango primero: un valor imposible no entra ni aunque venga de la fuente
  // preferente, y sobre todo no tumba el offset bueno que ya hubiera.
  if (quarterHours < TZ_QUARTER_MIN || quarterHours > TZ_QUARTER_MAX) {
    return false;
  }
  // Nada desplaza una hora puesta a mano: el operador la tecleo mirando el
  // reloj de la pared y las fuentes automaticas no deben moverla bajo sus
  // pies. Misma promesa que hace systemClockIsManual() con el instante.
  if (s_source == TZ_SOURCE_MANUAL && src != TZ_SOURCE_MANUAL) {
    return false;
  }
  // La IP no pisa a NITZ. Al reves si.
  if (src == TZ_SOURCE_IP && s_source == TZ_SOURCE_NITZ) {
    return false;
  }
  s_quarters = (int8_t)quarterHours;
  s_source = src;
  return true;
}

// Segundos -> cuartos de hora, redondeando al cuarto mas proximo. El servicio
// devuelve husos reales, todos multiplos de 900 s, pero redondear en vez de
// truncar evita que un valor raro se convierta en un offset silenciosamente
// desplazado hacia el cero.
static bool seconds_to_quarters(long seconds, int *out) {
  const long maxSec = (long)TZ_QUARTER_MAX * 900L;
  const long minSec = (long)TZ_QUARTER_MIN * 900L;
  if (seconds < minSec || seconds > maxSec) return false;
  long q = (seconds >= 0) ? (seconds + 450L) / 900L : (seconds - 450L) / 900L;
  if (q < TZ_QUARTER_MIN || q > TZ_QUARTER_MAX) return false;
  *out = (int)q;
  return true;
}

bool tz_parse_ipapi_offset(const char *json, int *outQuarterHours) {
  if (!json || !outQuarterHours) return false;

  // Una consulta fallida trae status "fail" y ningun offset util. Se rechaza
  // explicitamente en vez de confiar en que el campo no aparezca.
  if (strstr(json, "\"fail\"") != NULL) return false;

  // Se busca la clave ENTRECOMILLADA para no casar con un "gmt_offset_hours"
  // ni con cualquier otro campo que contenga la palabra.
  const char *key = strstr(json, "\"offset\"");
  if (!key) return false;

  const char *p = key + strlen("\"offset\"");
  while (*p == ' ' || *p == '\t') p++;
  if (*p != ':') return false;
  p++;
  while (*p == ' ' || *p == '\t') p++;

  // Solo un entero con signo opcional. Un valor entrecomillado o cualquier
  // otra cosa se descarta: no es el contrato del servicio.
  const char *numStart = p;
  if (*p == '-' || *p == '+') p++;
  const char *digitsStart = p;
  while (*p >= '0' && *p <= '9') p++;
  if (p == digitsStart) return false;

  // El numero tiene que estar TERMINADO por la sintaxis del objeto. Sin esto,
  // una respuesta truncada a mitad de cifra pasaria por un valor completo.
  if (*p != ',' && *p != '}' && *p != ' ' && *p != '\n' && *p != '\r' &&
      *p != '\t') {
    return false;
  }

  long seconds = strtol(numStart, NULL, 10);
  return seconds_to_quarters(seconds, outQuarterHours);
}
