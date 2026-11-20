#pragma once
// Lado display de CTRL,ENV: el entorno (puerta, luz, ruido) que la SensorBoard
// comunica a la motherBoard y esta reenvia aqui. Sin LVGL ni Arduino, para
// poder probar en host todo lo que no es pintar: el parser, la clasificacion
// del ruido y la decision de QUE se pinta (sbenv_view).
//
// SOLO INFORMATIVO. Ni la puerta ni el ruido entran en el lazo de control ni
// generan alarmas. El fallo del hall ya lo senaliza ALARM_SENSORBOARD_DOOR_FAULT
// y no se duplica aqui.
//
// LO QUE EL DISPLAY NO HACE: pintar un numero de dB o de lux. Ni el microfono
// ni el ALS estan calibrados (SensorBoard_v2/ESTADO.md: el micro da ~60 dB en
// reposo cuando deberian ser 35-45), asi que un numero se leeria como medida
// y no lo es. Se clasifica el ruido en tres niveles RELATIVOS, y la luz viaja
// pero no se usa.
#include <stdbool.h>
#include <stdint.h>

#include "protocol.h"  // PROTO_ENV_*

// ------------------------------------------------------------ umbrales
//
// PROVISIONALES, sobre la escala CRUDA (sin calibrar) del sensor. Unico sitio
// donde viven. Al calibrar el microfono contra un sonometro hay que SUSTITUIR
// estos dos numeros por umbrales reales (p. ej. 45 / 55 dBA) y revisar los
// textos. Razonamiento en design.md (D3):
//   * reposo medido en banco ~60 -> BAJO hasta 64 (reposo + 5 dB de margen)
//   * MEDIO de 65 a 74
//   * ALTO desde 75 (>= 15 dB sobre el reposo; con el desfase de ~+20 dB
//     estimado equivale a >~55 dBA reales, por encima del objetivo de 45
//     dBA que se recomienda para una UCIN)
#define SBENV_NOISE_MID_DBA 65
#define SBENV_NOISE_HIGH_DBA 75

// Sin CTRL,ENV fresco el indicador se apaga: tres periodos de PROTO_ENV_PERIOD_MS.
// Una MB que se reinicia o que pierde la SensorBoard no debe dejar "puerta
// abierta" congelada en pantalla.
#define SBENV_STALE_MS (3u * PROTO_ENV_PERIOD_MS)

typedef enum {
  SBENV_NOISE_NONE = 0,  // no se sabe: no se pinta
  SBENV_NOISE_LOW,
  SBENV_NOISE_MID,
  SBENV_NOISE_HIGH,
} SbEnvNoise;

// Un CTRL,ENV decodificado. lux y dba son PROTO_ENV_UNAVAILABLE (-1) cuando no
// se saben; door es uno de PROTO_ENV_DOOR_*.
typedef struct {
  bool link;
  int door;
  int lux;
  int dba;
} SbEnvMsg;

// Parser estricto de "CTRL,ENV,<link>,<door>,<lux>,<dbA>". Exige los cuatro
// campos numericos, link en {0,1} y door en {-1,0,1,2}. Acepta campos EXTRA al
// final (compatibilidad hacia delante: los campos nuevos se anaden al final).
// Cualquier otra cosa se rechaza (false) y `out` NO se toca. Un lux o dbA
// negativo se guarda como PROTO_ENV_UNAVAILABLE.
bool sbenv_parse(const char *line, SbEnvMsg *out);

// dBA crudos -> nivel. PROTO_ENV_UNAVAILABLE (o cualquier negativo) -> NONE.
SbEnvNoise sbenv_noise_level(int dba);

// Que hay que pintar.
typedef struct {
  bool doorOpen;      // icono de puerta abierta
  SbEnvNoise noise;   // NONE = no pintar el ruido
} SbEnvView;

// `have`: ha llegado algun CTRL,ENV desde el arranque. `rx_ms`: millis() de su
// recepcion. `boardLinkLost`: Display_IsBoardLinkLost().
//
// No pinta nada si: nunca llego (equipo sin SensorBoard), esta rancio, el enlace
// con la placa esta caido, o el enlace con la SensorBoard (link) lo esta.
// Puerta en fallo o desconocida: no se pinta (ver cabecera). Cerrada: tampoco,
// es el estado normal.
SbEnvView sbenv_view(bool have, const SbEnvMsg *msg, uint32_t rx_ms,
                     uint32_t now_ms, bool boardLinkLost);
