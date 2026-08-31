#include "system_clock.h"

#include "tz_source.h"

#include <esp_sntp.h>
#include <sys/time.h>
#include <time.h>

// Mismo suelo/techo que civil_to_unix_utc(): 2021-01-01 y 2100-01-01.
static const uint32_t MIN_VALID_EPOCH = 1609459200u;
static const uint32_t MAX_VALID_EPOCH = 4102444800u;

static bool s_manual = false;

bool systemClockSetManual(uint32_t epoch) {
  if (epoch < MIN_VALID_EPOCH || epoch >= MAX_VALID_EPOCH) {
    return false;
  }
  struct timeval tv = {};
  tv.tv_sec = (time_t)epoch;
  tv.tv_usec = 0;
  if (settimeofday(&tv, nullptr) != 0) {
    return false;
  }
  // configTime() puede haberse llamado ya al arrancar (Wifi_OTA/DriveUpload)
  // aunque aún no hubiera enlace: SNTP queda armado y respondería más tarde,
  // desplazando la hora recién puesta a mano. Pararlo es la única forma de
  // que la entrada manual sea realmente la que manda.
  if (esp_sntp_enabled()) {
    esp_sntp_stop();
  }
  // El epoch que se acaba de guardar YA es hora local: es lo que tecleo el
  // operador, sin zona. Se declara offset CERO para que el display lo pinte
  // verbatim y ninguna fuente automatica le sume nada encima.
  tz_source_set(0, TZ_SOURCE_MANUAL);
  s_manual = true;
  return true;
}

bool systemClockIsManual(void) { return s_manual; }
