#include "time_protocol.h"

#include "time_source.h" // ventana de epoch valido
#include "tz_source.h"   // TZ_QUARTER_MIN/MAX, TZ_SOURCE_*

#include <stdlib.h>

// Un entero decimal con signo y NADA mas: ni espacios, ni signo suelto, ni
// basura pegada detras. strtoll() se para en el primer caracter que no encaja
// y deja el resto en `end`, asi que "12abc" o "" se detectan comprobando que
// se consumio algo y que lo consumido llega justo hasta el separador.
//
// sscanf("%d") no vale aqui: acepta "12abc" como 12 sin decir nada.
//
// 64 bits, no `long`: en el ESP32 `long` son 32 bits, asi que el techo de la
// ventana (4102444800, el 2100) NO CABE — se lee como negativo y entonces
// cualquier epoch valido parece estar por encima del techo. Un parser de
// 32 bits habria rechazado tambien cualquier fecha posterior a enero de 2038.
static bool parse_int_field(const char *start, const char *end,
                            long long *out) {
  if (start == end) {
    return false; // campo vacio, p.ej. "HMI,RTC_TIME,,0,0"
  }
  char *stop = nullptr;
  const long long v = strtoll(start, &stop, 10);
  if (stop != end) {
    return false; // sobro texto dentro del campo
  }
  *out = v;
  return true;
}

bool time_protocol_parse_rtc_seed(const char *line, uint32_t *outEpoch,
                                  int *outTzQuarters, int *outTzSrc) {
  if (line == nullptr || outEpoch == nullptr || outTzQuarters == nullptr ||
      outTzSrc == nullptr) {
    return false;
  }
  static const char kPrefix[] = "HMI,RTC_TIME,";
  const size_t kPrefixLen = sizeof(kPrefix) - 1;
  for (size_t i = 0; i < kPrefixLen; i++) {
    if (line[i] != kPrefix[i]) {
      return false; // incluye el caso de linea truncada antes del prefijo
    }
  }

  // Tres campos separados por comas, el ultimo terminado por fin de cadena o
  // por el salto de linea que trae el enlace. Se recorren buscando el
  // separador en vez de confiar en un sscanf con tres "%d": asi un campo de
  // menos se detecta como tal en vez de dejar una salida sin escribir.
  const char *p = line + kPrefixLen;
  long long field[3] = {0, 0, 0};
  for (int i = 0; i < 3; i++) {
    const char *end = p;
    while (*end != '\0' && *end != ',' && *end != '\n' && *end != '\r') {
      end++;
    }
    const bool isLast = (i == 2);
    // Los dos primeros campos TIENEN que acabar en coma, y el ultimo NO puede
    // acabar en coma: eso descarta tanto "epoch,tzq" (falta uno) como
    // "epoch,tzq,tzsrc,extra" (sobra).
    if (isLast ? (*end == ',') : (*end != ',')) {
      return false;
    }
    if (!parse_int_field(p, end, &field[i])) {
      return false;
    }
    p = end + 1;
  }

  const long long epoch = field[0];
  const long long tzq = field[1];
  const long long tzsrc = field[2];

  // Ventana de epoch: la misma que civil_to_unix_utc() y que time_source. Un
  // PCF8563 con la pila agotada devuelve basura y buena parte cae en 1970.
  if (epoch < (long long)TIME_SOURCE_MIN_EPOCH ||
      epoch >= (long long)TIME_SOURCE_MAX_EPOCH) {
    return false;
  }
  if (tzq < TZ_QUARTER_MIN || tzq > TZ_QUARTER_MAX) {
    return false;
  }
  if (tzsrc < TZ_SOURCE_NONE || tzsrc > TZ_SOURCE_MANUAL) {
    return false;
  }

  *outEpoch = (uint32_t)epoch;
  *outTzQuarters = (int)tzq;
  *outTzSrc = (int)tzsrc;
  return true;
}
