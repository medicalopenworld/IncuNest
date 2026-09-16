#pragma once
// Parseo de las lineas de reloj que llegan del HMI por el UART.
//
// Vive aparte de CommTask, y sin Arduino, por una razon concreta: el enlace es
// ASCII CSV sin CRC, asi que el parseo es la frontera por la que entra al
// firmware texto que puede llegar truncado, a medias o con basura. Esa
// frontera tiene que poder probarse en host con casos hostiles
// (.claude/rules/security.md), no solo mirandola en banco.
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Parsea `HMI,RTC_TIME,epoch,tzq,tzsrc`, la semilla que el HMI ofrece desde su
// PCF8563 al arrancar.
//
// Devuelve false —sin tocar ninguna salida— ante cualquier duda: prefijo que
// no cuadra, campos de menos, campo no numerico, basura pegada detras de un
// numero, o valores fuera de rango. El epoch se valida contra la ventana
// [2021-01-01, 2100-01-01) y `tzq`/`tzsrc` contra sus rangos del protocolo.
//
// Descarte total y silencioso, nunca un dato a medias: un huso aceptado junto
// a un epoch corrupto desplazaria la fecha que sella el historial de alarmas.
bool time_protocol_parse_rtc_seed(const char *line, uint32_t *outEpoch,
                                  int *outTzQuarters, int *outTzSrc);

#ifdef __cplusplus
}
#endif
