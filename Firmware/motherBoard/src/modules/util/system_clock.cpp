#include "system_clock.h"

#include "time_source.h"
#include "tz_source.h"

#include <esp_sntp.h>
#include <sys/time.h>
#include <time.h>

// SNTP no pasa por systemClockSet(): el reloj lo escribe la pila lwIP por su
// cuenta en cuanto le contesta el servidor, y el firmware solo se entera
// despues. Por eso la integracion es por callback — registra el rango NTP a
// toro pasado, que es todo lo que se puede hacer.
//
// Ese "a toro pasado" es justo el motivo por el que el esp_sntp_stop() de mas
// abajo TIENE que seguir existiendo: cuando la hora es manual no basta con
// enterarse de que SNTP la movio, hay que impedir que la mueva.
static void onSntpSync(struct timeval *tv) {
  if (tv == nullptr) {
    return;
  }
  time_source_set((uint32_t)tv->tv_sec, PROTO_TIME_SOURCE_NTP);
}

void systemClockInit(void) { sntp_set_time_sync_notification_cb(onSntpSync); }

bool systemClockSet(uint32_t epoch, Proto_TimeSource src) {
  // time_source valida la ventana y arbitra el rango. Si dice que no, el
  // reloj no se toca.
  if (!time_source_set(epoch, src)) {
    return false;
  }
  struct timeval tv = {};
  tv.tv_sec = (time_t)epoch;
  tv.tv_usec = 0;
  if (settimeofday(&tv, nullptr) != 0) {
    return false;
  }
  return true;
}

bool systemClockSetManual(uint32_t epoch) {
  if (!systemClockSet(epoch, PROTO_TIME_SOURCE_MANUAL)) {
    return false;
  }
  // configTime() puede haberse llamado ya al arrancar (Wifi_OTA/DriveUpload)
  // aunque aun no hubiera enlace: SNTP queda armado y respondería mas tarde,
  // desplazando la hora recien puesta a mano. El callback de arriba se
  // enteraria, pero solo DESPUES de que el reloj ya se hubiera movido.
  // Pararlo es la unica forma de que la entrada manual sea la que manda.
  if (esp_sntp_enabled()) {
    esp_sntp_stop();
  }
  // El epoch que se acaba de guardar YA es hora local: es lo que tecleo el
  // operador, sin zona. Se declara offset CERO para que el display lo pinte
  // verbatim y ninguna fuente automatica le sume nada encima.
  tz_source_set(0, TZ_SOURCE_MANUAL);
  return true;
}

Proto_TimeSource systemClockSource(void) { return time_source_origin(); }

bool systemClockIsManual(void) {
  return time_source_origin() == PROTO_TIME_SOURCE_MANUAL;
}
