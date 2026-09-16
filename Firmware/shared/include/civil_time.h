#pragma once
// Conversion pura entre fecha civil y epoch Unix, en los dos sentidos. Fuera
// de cualquier tarea para poder probarla en host: mktime() dependeria de la
// TZ del entorno, que es justo la ambiguedad que no se quiere al sellar la
// hora de admision de un bebe.
//
// Vive en shared/ porque la necesitan LAS DOS PLACAS: la motherBoard para
// interpretar el reloj del modem y el formulario /config, y el HMI para
// convertir los registros BCD de su PCF8563. La aritmetica de fechas de un
// equipo medico se escribe UNA vez y se prueba UNA vez; dos copias que
// divergan darian dos fechas distintas para el mismo instante.
#include <stdbool.h>
#include <stdint.h>

// Days since 1970-01-01 for a proleptic-Gregorian civil date.
// (Howard Hinnant's days_from_civil.) Month is 1-12, day 1-31.
int64_t civil_days_from_epoch(int year, unsigned month, unsigned day);

// UTC epoch for a civil date/time, minus tzQuarterHours (the offset the
// modem reports alongside its clock, in quarter-hour units, so +8 == UTC+2).
// Returns false when the fields are out of range or the result is before
// 2021-01-01 (the same "clearly not synced" floor the rest of the firmware
// uses) — an unsynced SIM800 reports 2004-01-01 and must never be trusted.
bool civil_to_unix_utc(int year, unsigned month, unsigned day, unsigned hour,
                       unsigned minute, unsigned second, int tzQuarterHours,
                       uint32_t *outEpoch);

// Fecha y hora civiles UTC de un epoch, mas el dia de la semana.
//
// El sentido inverso del de arriba. Hace falta para ESCRIBIR un RTC: el
// PCF8563 guarda campos civiles en BCD, no un contador de segundos, asi que
// sin esto no hay forma de pasarle una hora. `outWeekday` es 0=domingo, que
// es la convencion del propio chip y la de struct tm.
//
// No se usa gmtime_r para esto a proposito: su struct tm arrastra convenciones
// heredadas (tm_year desde 1900, tm_mon desde 0) que ya han causado errores de
// un mes y de un siglo en firmware de RTC, y ademas devuelve un puntero a
// estado que hay que copiar. Aqui los campos salen con su valor natural.
void civil_from_unix_utc(uint32_t epoch, int *outYear, unsigned *outMonth,
                         unsigned *outDay, unsigned *outHour,
                         unsigned *outMinute, unsigned *outSecond,
                         unsigned *outWeekday);
