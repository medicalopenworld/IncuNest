#include "ip_geoloc.h"

#include <stdlib.h>
#include <string.h>

// Lee el numero decimal asociado a `quotedKey` ("\"lat\"", "\"lon\"").
//
// La clave se busca ENTRECOMILLADA para no casar con un "latency" ni con un
// "longitude". Solo se acepta un decimal sin comillas, con signo opcional, y
// TERMINADO por la sintaxis del objeto: sin esa comprobacion final, una
// respuesta cortada a mitad de cifra pasaria por un valor completo.
static bool parseNumberField(const char *json, const char *quotedKey,
                             double *out) {
  const char *key = strstr(json, quotedKey);
  if (!key) return false;

  const char *p = key + strlen(quotedKey);
  while (*p == ' ' || *p == '\t') p++;
  if (*p != ':') return false;
  p++;
  while (*p == ' ' || *p == '\t') p++;

  const char *numStart = p;
  if (*p == '-' || *p == '+') p++;
  int digits = 0;
  while (*p >= '0' && *p <= '9') {
    p++;
    digits++;
  }
  if (*p == '.') {
    p++;
    while (*p >= '0' && *p <= '9') {
      p++;
      digits++;
    }
  }
  if (digits == 0) return false; // incluye el caso del valor entrecomillado

  if (*p != ',' && *p != '}' && *p != ' ' && *p != '\n' && *p != '\r' &&
      *p != '\t') {
    return false;
  }

  *out = strtod(numStart, NULL);
  return true;
}

bool ip_geoloc_parse(const char *json, float *lat, float *lon) {
  if (!json || !lat || !lon) return false;

  // Se exige el exito explicito. Una consulta fallida puede traer campos
  // igualmente, y confiar en que no vengan seria confiar en el servicio.
  if (strstr(json, "\"status\":\"success\"") == NULL) return false;

  double la = 0.0;
  double lo = 0.0;
  if (!parseNumberField(json, "\"lat\"", &la)) return false;
  if (!parseNumberField(json, "\"lon\"", &lo)) return false;

  if (la < -90.0 || la > 90.0) return false;
  if (lo < -180.0 || lo > 180.0) return false;
  // 0,0 es un punto real del golfo de Guinea, pero sobre todo es el "no lo
  // se" de media industria. Se descarta igual que el camino GSM descarta
  // 0/0 al montar la telemetria.
  if (la == 0.0 && lo == 0.0) return false;

  *lat = (float)la;
  *lon = (float)lo;
  return true;
}
