#pragma once
// Punto UNICO por el que se fija el reloj de pared del equipo.
//
// El firmware mantiene un unico reloj sin zona horaria que todo lo demas lee
// con time(nullptr): babyStore_nowEpoch(), el broadcast CTRL,TIME al HMI y los
// sellos del historial de alarmas.
//
// Ese reloj lo alimentan cinco fuentes que antes no se hablaban entre si —SNTP
// por WiFi, SNTP por PPP, NITZ del modem, NTP sobre PDP y la entrada manual—,
// y ganaba la ultima en contestar, fuese la mejor o la peor. Ahora todas pasan
// por systemClockSet() y el orden lo arbitra modules/util/time_source:
//
//     manual (4) > NTP (3) > RTC del HMI (2) > NITZ (1)
//
// El RTC del HMI es la fuente que da sentido a la jerarquia: es la unica que
// conserva la hora con la alimentacion cortada, asi que una unidad desplegada
// sin cobertura ya no arranca sin fecha — y de la fecha dependen la edad del
// bebe y los sellos de alarma.
//
// La entrada manual se mantiene por encima de todo a proposito: una vez
// fijada, las fuentes automaticas no deben desplazarla en silencio bajo los
// pies del operador. El rango es solo RAM, tambien a proposito: un ciclo de
// alimentacion pierde el reloj de todas formas, asi que tras un reinicio las
// fuentes automaticas vuelven a tener via libre.
#include <stdbool.h>
#include <stdint.h>

#include "protocol.h" // Proto_TimeSource

// Registra el callback de SNTP. Hay que llamarlo en setup() ANTES de que
// arranque cualquier tarea que haga configTime(): con SNTP el reloj no lo
// escribe el firmware sino la pila lwIP, asi que la unica forma de enterarse
// de que la hora la puso NTP es que lwIP lo avise.
void systemClockInit(void);

// Aplica `epoch` (segundos desde 1970-01-01 UTC) al reloj del sistema si
// `src` gana al rango vigente. Devuelve false, dejando el reloj intacto,
// cuando no gana o cuando el epoch cae fuera de la ventana
// [2021-01-01, 2100-01-01) que ya impone civil_to_unix_utc().
//
// NO toca la zona horaria: quien la sepa llama ademas a tz_source_set(). Son
// dos escalas distintas y el orden de una no vale para la otra — NITZ es la
// peor fuente de hora y la mejor de huso.
bool systemClockSet(uint32_t epoch, Proto_TimeSource src);

// Azucar para la entrada manual, que ademas declara offset CERO: ese epoch YA
// es la hora local que tecleo el operador, asi que sumarle el offset de la red
// la desplazaria. Lo usan /config y HMI,SET_TIME.
bool systemClockSetManual(uint32_t epoch);

// Rango de la fuente que fijo el reloj vigente. Es lo que se difunde en el
// campo `src` de CTRL,TIME, y lo que el HMI necesita para decidir si lo que
// recibe merece escribirse en su RTC.
Proto_TimeSource systemClockSource(void);

// True cuando el reloj se fijo a mano y no debe sobrescribirlo ninguna fuente
// automatica hasta el siguiente reinicio.
bool systemClockIsManual(void);
