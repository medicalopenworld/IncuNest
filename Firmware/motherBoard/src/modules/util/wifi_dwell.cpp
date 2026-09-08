#include "wifi_dwell.h"

#include <string.h>

#define SECONDS_PER_DAY 86400u

static bool clockValid(uint32_t epoch) {
  return epoch >= WIFI_DWELL_EPOCH_VALID;
}

static uint32_t dayIndexOf(uint32_t epoch) { return epoch / SECONDS_PER_DAY; }

void wifi_dwell_clear(WifiDwell *st) {
  if (!st) return;
  memset(st, 0, sizeof(*st));
}

// Arranca el seguimiento de una red nueva. Con reloj valido el primer dia ya
// cuenta; sin el, queda el SSID apuntado y el contador a cero esperando que
// llegue la hora.
static void startTracking(WifiDwell *st, const char *ssid, uint32_t nowEpoch) {
  memset(st->ssid, 0, sizeof(st->ssid));
  // strncpy con el tamano del campo menos el NUL: un SSID de 32 bytes entra
  // entero, y el memset de arriba garantiza la terminacion.
  strncpy(st->ssid, ssid, WIFI_DWELL_SSID_MAX);
  if (clockValid(nowEpoch)) {
    st->firstEpoch = nowEpoch;
    st->lastDayIndex = dayIndexOf(nowEpoch);
    st->days = 1;
  } else {
    st->firstEpoch = 0;
    st->lastDayIndex = 0;
    st->days = 0;
  }
}

bool wifi_dwell_update(WifiDwell *st, const char *ssid, uint32_t nowEpoch) {
  if (!st || !ssid || ssid[0] == '\0') return false;

  // Comparacion acotada a los 32 bytes del campo, no a la cadena entrante:
  // asi dos redes que solo difieren en el ultimo byte del maximo siguen
  // siendo redes distintas.
  if (strncmp(st->ssid, ssid, WIFI_DWELL_SSID_MAX) != 0) {
    startTracking(st, ssid, nowEpoch);
    return true; // el reset SIEMPRE se persiste, con reloj o sin el
  }

  if (!clockValid(nowEpoch)) return false;

  // La asociacion ocurrio antes de que el reloj estuviera en hora: este es el
  // primer instante que sirve de ancla.
  if (st->firstEpoch == 0) {
    st->firstEpoch = nowEpoch;
    st->lastDayIndex = dayIndexOf(nowEpoch);
    st->days = 1;
    return true;
  }

  const uint32_t today = dayIndexOf(nowEpoch);
  if (today == st->lastDayIndex) return false;

  st->lastDayIndex = today;
  if (st->days < UINT16_MAX) st->days++;
  return true;
}

uint16_t wifi_dwell_span_days(const WifiDwell *st, uint32_t nowEpoch) {
  if (!st || st->firstEpoch == 0) return 0;
  if (!clockValid(nowEpoch) || nowEpoch < st->firstEpoch) return 0;
  const uint32_t span = (nowEpoch - st->firstEpoch) / SECONDS_PER_DAY;
  return (span > UINT16_MAX) ? UINT16_MAX : (uint16_t)span;
}

void wifi_dwell_sanitize_ssid(const char *in, char *out, size_t outSize) {
  if (!out || outSize == 0) return;
  size_t n = 0;
  if (in) {
    for (; in[n] != '\0' && n + 1 < outSize; n++) {
      // unsigned char a proposito: `char` es con signo en esta plataforma y
      // un byte >= 0x80 daria negativo en la comparacion.
      const unsigned char c = (unsigned char)in[n];
      out[n] = (c >= 0x20 && c <= 0x7E) ? (char)c : '?';
    }
  }
  out[n] = '\0';
}
