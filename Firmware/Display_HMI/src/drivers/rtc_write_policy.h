#pragma once
// Cuando merece la pena escribir el PCF8563.
//
// Aparte del driver porque es una decision, no una transaccion de bus, y
// porque equivocarse aqui tiene dos formas caras y opuestas: escribir de mas
// castiga un bus I2C que COMPARTE con el tactil, y escribir de menos deja el
// chip con una hora peor que la que el equipo ya conoce.
//
// La politica: solo se escribe si la fuente que trae la hora nueva es MEJOR
// que la que produjo lo que hay guardado, o si es la misma fuente y el reloj
// ha derivado lo bastante como para que valga la pena.
#include <stdbool.h>
#include <stdint.h>

#include "protocol.h" // Proto_TimeSource

#ifdef __cplusplus
extern "C" {
#endif

// Deriva a partir de la cual se reescribe con una fuente del mismo rango.
// 2 s es de sobra para lo unico que el HMI hace con la hora —pintar fechas—
// y deja fuera el goteo de diferencias de un segundo que produciria el
// redondeo entre difusiones.
#define RTC_WRITE_DRIFT_TOLERANCE_S 2

// `incomingSrc` / `incomingEpoch`: lo que acaba de llegar en CTRL,TIME.
// `storedSrc`: el rango de lo ultimo que se escribio en el chip (de la NVS).
// `rtcEpoch`: lo que el chip tiene AHORA, o 0 si no tiene hora valida.
//
// Devuelve true cuando hay que escribir. Nunca escribe con epoch fuera de la
// ventana valida ni con `epoch` 0 ("la motherBoard aun no sabe la hora"), que
// es el caso mas frecuente en un arranque sin red.
bool rtc_should_write(uint32_t incomingEpoch, Proto_TimeSource incomingSrc,
                      uint32_t rtcEpoch, Proto_TimeSource storedSrc);

#ifdef __cplusplus
}
#endif
