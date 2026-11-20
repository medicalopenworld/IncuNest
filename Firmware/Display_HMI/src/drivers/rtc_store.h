#pragma once
// Empaquetado de la terna (huso, origen del huso, rango de la fuente) en la
// unica palabra de NVS que la acompaña al RTC.
//
// El PCF8563 guarda el INSTANTE y nada mas: no tiene RAM de usuario respaldada
// por pila. Sin esto, tras un ciclo de alimentacion el equipo recuperaria la
// hora pero no la zona, y el display pintaria UTC sin avisar de que es UTC.
//
// Una sola palabra y no tres claves: la NVS no da transaccionalidad, asi que
// con claves sueltas un corte entre la primera y la tercera escritura dejaria
// el huso de una hora junto al rango de otra. Empaquetado, o se escribe entero
// o no se escribe.
//
// Pura y aparte para poder probarla: un fallo de desplazamiento aqui no
// revienta nada, solo devuelve una zona equivocada, y eso son horas enteras de
// error en la fecha que sella el historial de alarmas.
#include <stdbool.h>
#include <stdint.h>

#include "protocol.h" // Proto_TimeSource

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int8_t tzQuarters;      // -48..+56
  uint8_t tzSource;       // TzSource: 0=desconocido, 1=NITZ, 2=IP, 3=manual
  Proto_TimeSource src;   // rango de la fuente del epoch escrito en el chip
} RtcStoredTz;

// Valor de una NVS virgen. Se interpreta como "no hay nada guardado", que es
// justo el caso «hay hora, no hay zona» que el protocolo ya contempla.
#define RTC_STORE_EMPTY 0u

uint32_t rtc_store_pack(const RtcStoredTz *tz);

// Devuelve false —dejando `out` en el estado "no se sabe"— para una palabra
// vacia o con campos fuera de rango. Una NVS corrupta no debe producir un huso
// plausible pero falso: es preferible pintar la hora sin offset, que es lo que
// el firmware ya hace cuando no conoce la zona.
bool rtc_store_unpack(uint32_t word, RtcStoredTz *out);

#ifdef __cplusplus
}
#endif
