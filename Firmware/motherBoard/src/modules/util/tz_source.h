#pragma once

#include <stdbool.h>
#include <stdint.h>

// Zona horaria vigente del equipo.
//
// La hora UTC es una magnitud medible y cualquier servidor NTP la da igual; la
// hora local NO es medible, es una convencion politica, asi que alguien de
// fuera tiene que comunicarla. Este modulo guarda esa comunicacion y arbitra
// entre las dos fuentes que el equipo tiene.
//
// Logica pura a proposito: sin Arduino, sin red y sin estado global del
// firmware, para que entre en [env:native] y se pruebe de verdad con Unity.
//
// El offset NO altera ningun epoch almacenado ni transmitido: todo se guarda y
// se transmite en UTC y esto solo se aplica al formatear para una persona.

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  TZ_SOURCE_NONE = 0, // no se sabe; NO es lo mismo que UTC+0
  TZ_SOURCE_NITZ = 1, // el operador movil, junto a la hora de red
  TZ_SOURCE_IP = 2,   // geolocalizacion por IP sobre WiFi
  // El operador tecleo la hora en /config. Ese epoch YA es hora local: se
  // guarda tal cual, sin zona, asi que el offset correcto es CERO y no el de
  // la red. Sumarle el de NITZ o el de IP desplazaria la hora que el operador
  // acaba de poner — dos horas de error en Espana, y en la fecha que sella el
  // historial de alarmas.
  TZ_SOURCE_MANUAL = 3,
  // Zona de fabrica, la que se aplica cuando el reloj lo pone el modem GPRS y
  // nadie ha comunicado ninguna zona. NO es un dato: es una suposicion, y por
  // eso es la fuente de MENOR prioridad de todas — cualquiera que de verdad
  // sepa la zona la corrige sin esperar a un reinicio.
  //
  // Existe porque una unidad solo-GPRS con un operador que no manda NITZ se
  // quedaba sin zona de por vida y pintaba UTC, dos horas por debajo del
  // reloj de la pared en Espana.
  TZ_SOURCE_DEFAULT = 4,
} TzSource;

// Husos civiles reales, en cuartos de hora: UTC-12:00 .. UTC+14:00.
// Cuartos y no horas porque existen husos no enteros (Nepal, UTC+5:45), y es
// ademas la unidad que ya usa civil_to_unix_utc().
#define TZ_QUARTER_MIN (-48)
#define TZ_QUARTER_MAX (56)

// Vuelve al estado "no se sabe". Existe para los tests.
void tz_source_reset(void);

// Aplica el offset si la politica de prioridad lo permite y el valor es
// valido. Devuelve true solo si el estado ha cambiado.
//
// Politica: MANUAL gana a todo y nada lo desplaza hasta el reinicio, que es
// lo que ya prometia system_clock.h. Por debajo, NITZ gana a IP, siempre. La antena esta fisicamente donde esta el
// equipo; una IP puede ser de una VPN, un enlace satelital o la sede del
// operador en otro pais. Y por debajo de todas, DEFAULT: solo entra cuando no
// hay ninguna otra, y nunca pisa a ninguna. Dentro de la misma fuente el
// valor se refresca, o cruzar una frontera dejaria el primer offset congelado
// de por vida.
//
// Un valor invalido nunca degrada un offset bueno ya obtenido.
bool tz_source_set(int quarterHours, TzSource src);

int8_t tz_source_quarters(void);
TzSource tz_source_origin(void);

// "Hay un offset que aplicar", DEFAULT incluido: es lo que decide si se pinta
// hora local o el aviso de "sin hora".
bool tz_source_known(void);

// "Alguien de fuera nos ha dicho la zona de verdad" — DEFAULT no cuenta. Es
// lo que decide con que ritmo se sigue buscando la zona real: mientras lo
// unico que haya sea la suposicion de fabrica, hay que seguir buscando
// deprisa en vez de pasar al refresco de una vez al dia.
bool tz_source_is_resolved(void);

// Extrae el offset de la respuesta de ip-api.com, donde el campo "offset"
// llega en SEGUNDOS, y lo convierte a cuartos de hora.
//
// Devuelve false ante cualquier duda —campo ausente, consulta fallida, JSON
// truncado, valor fuera de rango fisico— segun la politica de descarte
// silencioso de .claude/rules/security.md: esto parsea texto que viene de la
// red y nunca debe producir un dato a medias.
bool tz_parse_ipapi_offset(const char *json, int *outQuarterHours);

#ifdef __cplusplus
}
#endif
