#pragma once
// PCF8563 del HMI: la unica pieza del equipo que conserva la hora con la
// alimentacion cortada.
//
// Esto es SOLO transporte por el bus. Toda la decision vive en los dos modulos
// de al lado, que si se prueban en host: rtc_pcf8563_codec (que dicen los
// registros) y rtc_write_policy (cuando merece la pena escribirlos).
//
// El chip comparte bus I2C con el tactil (IO15/IO16, 400 kHz). Por eso todo
// aqui es episodico: una lectura al arrancar y una escritura solo cuando llega
// una hora mejor. Nada periodico.
#include <stdbool.h>
#include <stdint.h>

// Lee la hora del chip. Devuelve false si el chip no contesta, si el flag VL
// dice que el contenido no es fiable, o si lo que devuelve no es una fecha
// creible (ver rtc_pcf8563_codec.h).
//
// Hace hasta dos lecturas: el PCF8563 no tiene doble bufer, asi que una
// lectura puede quedar a caballo de un incremento de segundo —con el minuto ya
// actualizado y el segundo no, o al reves—. Si el registro de segundos cambia
// entre dos muestreos, se repite.
bool rtcRead(uint32_t *outEpoch);

// Escribe la hora en el chip, dejando el flag VL a 0. Devuelve false si el
// epoch no es representable o si el bus falla.
bool rtcWrite(uint32_t epoch);

// True si el chip contesta en su direccion. Para el test de fabrica y el
// diagnostico: distingue "RTC sin hora" de "RTC que no esta".
bool rtcPresent(void);
