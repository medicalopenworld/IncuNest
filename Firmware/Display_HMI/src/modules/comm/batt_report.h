#pragma once
// Lado display de CTRL,BATT: el estado de la bateria interna que estima la
// motherBoard. Sin LVGL ni Arduino, para probar en host el parser y la
// decision de QUE se pinta (battrep_view); el dibujo vive en UITask.cpp.
//
// SOLO INFORMATIVO. El aviso de corte de red es ALARM_MAINS_INTERRUPTION y no
// depende de esta linea.
#include <stdbool.h>
#include <stdint.h>

#include "protocol.h"  // PROTO_BATT_*

// Sin CTRL,BATT fresco el indicador se apaga: tres periodos. Una MB que se
// reinicia no debe dejar una bateria congelada en pantalla.
#define BATTREP_STALE_MS (3u * PROTO_BATT_PERIOD_MS)

// Un CTRL,BATT decodificado.
typedef struct {
  int state;  // PROTO_BATT_STATE_*
  int level;  // 0..PROTO_BATT_LEVEL_MAX
  int pct;    // 0..100
} BattRepMsg;

// Parser estricto de "CTRL,BATT,<state>,<level>,<pct>". Exige los tres
// campos, state en PROTO_BATT_STATE_*, level en 0..PROTO_BATT_LEVEL_MAX y pct
// en 0..100. Acepta campos EXTRA al final (los nuevos se anaden al final).
// Cualquier otra cosa se rechaza (false) y `out` NO se toca.
bool battrep_parse(const char *line, BattRepMsg *out);

typedef enum {
  BATTREP_SHOW_NONE = 0,     // no se pinta
  BATTREP_SHOW_DISCHARGING,  // rayas = level, color segun nivel
  BATTREP_SHOW_CHARGING,     // rayo, `level` rayas fijas y la siguiente parpadea
  BATTREP_SHOW_FULL,         // rayo y todas las rayas
} BattRepShow;

typedef struct {
  BattRepShow show;
  // Rayas fijas. Cargando: cuartos completos de carga (pct / 25, como mucho
  // 3: la cuarta solo se fija al llenarse), y parpadea la que se esta llenando.
  int level;
} BattRepView;

// `have`: ha llegado algun CTRL,BATT desde el arranque. `rx_ms`: millis() de
// su recepcion. `boardLinkLost`: Display_IsBoardLinkLost().
//
// No pinta nada si: nunca llego (equipo sin cargador), esta rancio, el enlace
// con la placa esta caido o el estado es UNKNOWN.
BattRepView battrep_view(bool have, const BattRepMsg *msg, uint32_t rx_ms,
                         uint32_t now_ms, bool boardLinkLost);
