#pragma once
// Ajuste manual del reloj de pared desde la página /config del webserver.
//
// El firmware mantiene un único reloj sin zona horaria que todo lo demás lee
// con time(nullptr): babyStore_nowEpoch(), el broadcast CTRL,TIME al HMI y los
// sellos del historial de alarmas. Normalmente lo fija SNTP por WiFi o la ruta
// NITZ/NTP del módem, pero una unidad desplegada sin ninguno de los dos no
// tenía forma de saber la fecha — y de ella dependen la edad del bebé y los
// sellos de alarma.
//
// Por eso una entrada manual tiene que mantenerse: una vez fijada, las fuentes
// automáticas no deben desplazarla en silencio bajo los pies del operador. La
// marca es solo RAM a propósito: un ciclo de alimentación pierde el reloj de
// todas formas, así que tras un reinicio las fuentes automáticas vuelven a
// tener vía libre.
#include <stdbool.h>
#include <stdint.h>

// Aplica `epoch` (segundos desde 1970-01-01 UTC) al reloj del sistema y lo
// marca como fijado a mano. Devuelve false, dejando el reloj intacto, para un
// epoch fuera de la misma ventana [2021-01-01, 2100-01-01) que ya impone
// civil_to_unix_utc().
bool systemClockSetManual(uint32_t epoch);

// True cuando el reloj se fijó a mano y no debe sobrescribirlo SNTP ni el
// módem hasta el siguiente reinicio.
bool systemClockIsManual(void);
