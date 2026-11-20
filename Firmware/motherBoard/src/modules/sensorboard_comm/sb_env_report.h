#pragma once
// CTRL,ENV: lo que la motherboard cuenta al display del entorno que le llega
// de la SensorBoard (puerta, luz, ruido). Puro: no lee el snapshot ni el reloj
// ni la UART, se le pasan -- asi el formato y la cadencia se prueban en host,
// igual que sb_telemetry. CommTask.cpp es solo el envoltorio que toma el
// snapshot bajo mutex, llama aqui y escribe en hmiSerial.
//
// Es la misma informacion que sb_build_telemetry() manda a ThingsBoard, con el
// MISMO criterio de frescura, y con la misma propiedad que importa: lo que no
// se sabe NO viaja como cero. Un cero se lee como medida; aqui el centinela es
// PROTO_ENV_UNAVAILABLE (-1), fuera de cualquier lux o dBA posible. Con el
// enlace caido no se manda la ultima lectura buena (seria un dato congelado)
// sino todo a -1.
//
// lux y dbA viajan SIN CALIBRAR (SensorBoard_v2/ESTADO.md): el display no los
// pinta como medida. Ver design.md del change shared-sb-env-to-hmi.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sensorboard_comm.h"  // SbSnapshot

typedef struct {
  bool link;     // enlace USB con la SensorBoard vivo
  int8_t door;   // PROTO_ENV_DOOR_*
  int32_t lux;   // >= 0, o PROTO_ENV_UNAVAILABLE
  int32_t dba;   // >= 0, o PROTO_ENV_UNAVAILABLE
} SbEnvReport;

// Traduce el snapshot (con `now` = millis() del llamante) al informe.
void sb_env_report_make(SbEnvReport *out, const SbSnapshot *s, uint32_t now);

// Escribe "CTRL,ENV,<link>,<door>,<lux>,<dbA>\n" en buf (NUL-terminado).
// Devuelve la longitud escrita sin el NUL, o -1 si no cabe (buf no se toca
// como cadena valida: queda vacio).
int sb_env_report_format(char *buf, size_t size, const SbEnvReport *r);

// Decide CUANDO enviar. Un cambio de `link` o de `door` sale sin esperar; en
// reposo, cada PROTO_ENV_PERIOD_MS. lux/dbA no disparan envio por si solos:
// son lentos, el display los reduce a tres niveles y la fuente los publica a
// 5 s, asi que enviarlos antes no traeria nada.
typedef struct {
  bool has_sent;
  SbEnvReport last;
  uint32_t last_ms;
} SbEnvPacer;

void sb_env_pacer_init(SbEnvPacer *p);

// Consulta pura: nadie la modifica. Tras enviar de verdad, sb_env_pacer_sent().
bool sb_env_pacer_due(const SbEnvPacer *p, const SbEnvReport *r, uint32_t now);
void sb_env_pacer_sent(SbEnvPacer *p, const SbEnvReport *r, uint32_t now);
