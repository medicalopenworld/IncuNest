#include "sb_env_report.h"

#include <stdio.h>
#include <string.h>

#include "protocol.h"  // PROTO_ENV_*
#include "sb_protocol.h"

// Cotas de saneamiento: no son rangos fisicos del sensor, son lo que garantiza
// que la linea cabe en PROTO_ENV_LINE_MAX por muy absurdo que sea el valor que
// llegue. Un dato fuera de rango no se "arregla" a un tope plausible mas alla
// de eso: el display no pinta ninguno de los dos numeros.
#define SB_ENV_LUX_MAX 999999
#define SB_ENV_DBA_MAX 200

static bool fresh(uint32_t now, uint32_t last_ms, uint32_t stale_ms) {
  // Resta sin signo: correcta a traves de la vuelta de millis().
  return (uint32_t)(now - last_ms) <= stale_ms;
}

// float >= 0 -> entero redondeado y acotado; todo lo demas (negativo, NaN) es
// "no disponible". La comparacion se escribe en positivo para que NaN caiga en
// la rama del centinela.
static int32_t to_level(float v, int32_t max) {
  if (!(v >= 0.0f)) return PROTO_ENV_UNAVAILABLE;
  if (v >= (float)max) return max;
  return (int32_t)(v + 0.5f);
}

void sb_env_report_make(SbEnvReport *out, const SbSnapshot *s, uint32_t now) {
  out->link = s->link_ok;
  out->door = PROTO_ENV_DOOR_UNKNOWN;
  out->lux = PROTO_ENV_UNAVAILABLE;
  out->dba = PROTO_ENV_UNAVAILABLE;
  // Con el enlace caido los ultimos valores son viejos: nada de ellos viaja.
  if (!s->link_ok) return;

  // El fallo manda sobre el ultimo estado: un hall sospechoso no es fiable ni
  // para decir "abierta" ni para decir "cerrada".
  if (s->door_faulty) {
    out->door = PROTO_ENV_DOOR_FAULT;
  } else if (s->door_known && fresh(now, s->last_door_ms, SB_DOOR_STALE_MS)) {
    out->door = s->door_open ? PROTO_ENV_DOOR_OPEN : PROTO_ENV_DOOR_CLOSED;
  }

  if (s->env_seen && fresh(now, s->last_env_ms, SB_ENV_STALE_MS) &&
      s->lux_valid) {
    out->lux = to_level(s->lux, SB_ENV_LUX_MAX);
  }
  if (s->sound_seen && fresh(now, s->last_sound_ms, SB_SOUND_STALE_MS) &&
      s->dba_valid) {
    out->dba = to_level(s->dba, SB_ENV_DBA_MAX);
  }
}

int sb_env_report_format(char *buf, size_t size, const SbEnvReport *r) {
  if (size == 0) return -1;
  const int n = snprintf(buf, size, "CTRL,ENV,%d,%d,%d,%d\n", r->link ? 1 : 0,
                         (int)r->door, (int)r->lux, (int)r->dba);
  if (n < 0 || (size_t)n >= size) {
    buf[0] = '\0';
    return -1;
  }
  return n;
}

void sb_env_pacer_init(SbEnvPacer *p) { memset(p, 0, sizeof(*p)); }

bool sb_env_pacer_due(const SbEnvPacer *p, const SbEnvReport *r,
                      uint32_t now) {
  if (!p->has_sent) return true;
  if (p->last.link != r->link || p->last.door != r->door) return true;
  return (uint32_t)(now - p->last_ms) >= PROTO_ENV_PERIOD_MS;
}

void sb_env_pacer_sent(SbEnvPacer *p, const SbEnvReport *r, uint32_t now) {
  p->has_sent = true;
  p->last = *r;
  p->last_ms = now;
}
