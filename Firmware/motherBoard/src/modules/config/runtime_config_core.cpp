#include "modules/config/runtime_config_core.h"

#include <string.h>

// ESQUELETO EN ROJO: la API existe para que los tests compilen y fallen por
// comportamiento. La implementacion llega en el commit siguiente.

void rcfg_init(RcfgCore *c, const RcfgPlatform *pf, bool safety_remote) {
  memset(c, 0, sizeof(*c));
  c->pf = *pf;
  c->safetyRemote = safety_remote;
}

void rcfg_load_state(RcfgCore *, RuntimeConfigId, const RcfgKeyState *) {}

RuntimeConfigStatus rcfg_apply_local(RcfgCore *, RuntimeConfigOrigin,
                                     RuntimeConfigId, float, float *,
                                     bool *) {
  return RCFG_REJ_RANGE;
}

void rcfg_remote_key(RcfgCore *, RuntimeConfigOrigin, RuntimeConfigId, float,
                     bool) {}

void rcfg_remote_bad_type(RcfgCore *, RuntimeConfigOrigin, RuntimeConfigId) {}

void rcfg_poll(RcfgCore *) {}
