#include "modules/config/runtime_config_core.h"

#include <string.h>

#include "modules/config/runtime_config_policy.h"

namespace {

enum class Remote { Applied, Rejected, Deferred };

void emit(RcfgCore *c, RcfgEventType type, RuntimeConfigOrigin origin,
          RuntimeConfigId id, float requested, float applied, float before,
          RuntimeConfigStatus status, uint32_t suppressed) {
  RcfgEvent e;
  e.type = type;
  e.origin = origin;
  e.id = id;
  e.requested = requested;
  e.applied = applied;
  e.before = before;
  e.status = status;
  e.epoch = c->pf.epoch();
  e.suppressed = suppressed;
  c->pf.notify(&e);
}

// Un aviso por clave y RCFG_REJECT_NOTE_MS; los demas se cuentan. Devuelve
// true si toca avisar, con la cuenta de los callados desde el ultimo aviso.
bool note_allowed(RcfgCore *c, RuntimeConfigId id, uint32_t *suppressed) {
  const uint32_t now = c->pf.now_ms();
  if (!c->hasNote[id] ||
      (uint32_t)(now - c->lastNoteMs[id]) >= (uint32_t)RCFG_REJECT_NOTE_MS) {
    c->hasNote[id] = true;
    c->lastNoteMs[id] = now;
    *suppressed = c->suppressed[id];
    c->suppressed[id] = 0;
    return true;
  }
  ++c->suppressed[id];
  return false;
}

void note_rejected(RcfgCore *c, RuntimeConfigOrigin origin, RuntimeConfigId id,
                   float requested, RuntimeConfigStatus status) {
  uint32_t sup = 0;
  if (note_allowed(c, id, &sup)) {
    emit(c, RCFG_EV_REJECTED, origin, id, requested, c->pf.current(id),
         c->pf.current(id), status, sup);
  }
}

// Una ficha del cubo de escrituras de origen remoto. El cubo arranca lleno
// (un reinicio con varias claves nuevas en la peticion las persiste todas) y
// recupera una ficha cada RCFG_NVS_REFILL_MS.
bool take_token(RcfgCore *c) {
  const uint32_t now = c->pf.now_ms();
  if (!c->bucketStarted) {
    c->bucketStarted = true;
    c->tokens = RCFG_NVS_BUCKET_CAP;
    c->lastRefillMs = now;
  } else if (c->tokens < RCFG_NVS_BUCKET_CAP) {
    const uint32_t n = (uint32_t)(now - c->lastRefillMs) / (uint32_t)RCFG_NVS_REFILL_MS;
    if (n > 0) {
      c->tokens = (c->tokens + n > RCFG_NVS_BUCKET_CAP) ? RCFG_NVS_BUCKET_CAP
                                                        : c->tokens + n;
      c->lastRefillMs += n * (uint32_t)RCFG_NVS_REFILL_MS;
    }
  }
  if (c->tokens >= RCFG_NVS_BUCKET_CAP) {
    c->lastRefillMs = now;  // lleno: la cuenta de recarga arranca al gastar
  }
  if (c->tokens == 0) {
    return false;
  }
  --c->tokens;
  ++c->remoteNvsFlushes;
  return true;
}

// Persiste lo que un origen remoto dejo aplicado en RAM, una clave por ficha.
// Escribe el valor ACTUAL, no el que habia cuando se marco sucio.
void flush_dirty(RcfgCore *c) {
  for (int i = 0; i < RCFG_COUNT; ++i) {
    if (!c->dirtyValue[i] && !c->dirtyState[i]) {
      continue;
    }
    if (!take_token(c)) {
      return;  // sin fichas: el resto espera
    }
    const RuntimeConfigId id = (RuntimeConfigId)i;
    if (c->dirtyValue[i]) {
      if (c->pf.persist_value(id, c->pf.current(id))) {
        c->dirtyValue[i] = false;
      } else {
        emit(c, RCFG_EV_PERSIST_FAIL, RCFG_ORIGIN_TB_GPRS, id, 0, 0, 0,
             RCFG_OK, 0);
      }
    }
    if (c->dirtyState[i]) {
      c->st[i].version = RCFG_STATE_VERSION;
      if (c->pf.persist_state(id, &c->st[i])) {
        c->dirtyState[i] = false;
      } else {
        emit(c, RCFG_EV_PERSIST_FAIL, RCFG_ORIGIN_TB_GPRS, id, 0, 0, 0,
             RCFG_OK, 0);
      }
    }
  }
}

// Intenta aplicar un valor remoto: lista blanca, reglas remotas y, si toca, lo
// deja aplazado. NO persiste: deja la RAM cambiada y las marcas de sucio.
Remote try_remote(RcfgCore *c, RuntimeConfigOrigin origin, RuntimeConfigId id,
                  float requested) {
  if (!runtime_config_remote_allowed(id, c->safetyRemote)) {
    note_rejected(c, origin, id, requested, RCFG_REJ_NOT_ALLOWED);
    return Remote::Rejected;
  }

  RuntimeConfigRemoteCtx ctx;
  c->pf.context(&ctx);
  float applied = 0.0f;
  const RuntimeConfigStatus st =
      runtime_config_validate_remote(id, requested, &ctx, &applied);
  if (!runtime_config_status_accepted(st)) {
    note_rejected(c, origin, id, requested, st);
    return Remote::Rejected;
  }

  const float before = c->pf.current(id);
  if (before == applied) {
    emit(c, RCFG_EV_UNCHANGED, origin, id, requested, applied, before, st, 0);
    return Remote::Applied;  // ya valia: no espera a nadie
  }

  const bool defer = runtime_config_should_defer(id, c->pf.therapy_active());
  const bool rate = runtime_config_rate_ok(
      c->hasLastRemoteChange[id], c->lastRemoteChangeMs[id], c->pf.now_ms(),
      (uint32_t)RCFG_REMOTE_MIN_INTERVAL_MS);
  if (defer || !rate) {
    c->pending[id].valid = true;
    c->pending[id].value = requested;
    c->pending[id].origin = origin;
    uint32_t sup = 0;
    if (note_allowed(c, id, &sup)) {
      emit(c, RCFG_EV_DEFERRED, origin, id, requested, applied, before, st, sup);
    }
    return Remote::Deferred;
  }

  c->pf.apply_ram(id, applied);
  RcfgKeyState &k = c->st[id];
  k.src = (uint8_t)origin + 1;
  k.epoch = c->pf.epoch();
  k.version = RCFG_STATE_VERSION;
  c->lastRemoteChangeMs[id] = c->pf.now_ms();
  c->hasLastRemoteChange[id] = true;
  c->dirtyValue[id] = true;
  c->dirtyState[id] = true;
  emit(c, RCFG_EV_CHANGED, origin, id, requested, applied, before, st, 0);
  return Remote::Applied;
}

// Un valor remoto aplicado (o que ya valia): ese si se recuerda en NVS.
void remember_applied(RcfgCore *c, RuntimeConfigId id, float requested) {
  RcfgKeyState &k = c->st[id];
  c->seenValid[id] = false;
  if (!(k.hasRemote && k.lastRemote == requested)) {
    k.lastRemote = requested;
    k.hasRemote = 1;
    k.version = RCFG_STATE_VERSION;
    c->dirtyState[id] = true;
  }
}

void lift_override(RcfgCore *c, RuntimeConfigId id) {
  RcfgKeyState &k = c->st[id];
  if (k.overrideLocal) {
    k.overrideLocal = 0;
    k.version = RCFG_STATE_VERSION;
    c->dirtyState[id] = true;
  }
}

}  // namespace

void rcfg_init(RcfgCore *c, const RcfgPlatform *pf, bool safety_remote) {
  memset(c, 0, sizeof(*c));
  c->pf = *pf;
  c->safetyRemote = safety_remote;
  for (int i = 0; i < RCFG_COUNT; ++i) {
    c->st[i].version = RCFG_STATE_VERSION;
  }
}

void rcfg_load_state(RcfgCore *c, RuntimeConfigId id, const RcfgKeyState *s) {
  if ((unsigned)id >= (unsigned)RCFG_COUNT || s == nullptr) {
    return;
  }
  if (s->version != RCFG_STATE_VERSION) {
    return;  // formato desconocido: estado limpio
  }
  c->st[id] = *s;
}

RuntimeConfigStatus rcfg_apply_local(RcfgCore *c, RuntimeConfigOrigin origin,
                                     RuntimeConfigId id, float requested,
                                     float *applied, bool *persisted) {
  float v = 0.0f;
  // Estricta tambien para la web: un 45 que se vuelve 40 y contesta "Saved" es
  // un cambio que nadie pidio.
  const RuntimeConfigStatus st = runtime_config_validate_strict(id, requested, &v);
  if (persisted != nullptr) {
    *persisted = false;
  }
  if (!runtime_config_status_accepted(st)) {
    return st;
  }
  if (applied != nullptr) {
    *applied = v;
  }
  const float before = c->pf.current(id);
  if (before == v) {
    emit(c, RCFG_EV_UNCHANGED, origin, id, requested, v, before, st, 0);
    if (persisted != nullptr) {
      *persisted = true;
    }
    return st;
  }
  // Persiste ANTES de cambiar la RAM: si la NVS falla no se queda un valor
  // aplicado que desaparecera al reiniciar sin que nadie lo sepa.
  if (!c->pf.persist_value(id, v)) {
    emit(c, RCFG_EV_PERSIST_FAIL, origin, id, requested, v, before, st, 0);
    return st;
  }
  c->pf.apply_ram(id, v);
  RcfgKeyState &k = c->st[id];
  k.src = (uint8_t)origin + 1;
  k.epoch = c->pf.epoch();
  k.overrideLocal = 1;  // el cambio local gana al reaplicado remoto
  k.version = RCFG_STATE_VERSION;
  c->pending[id].valid = false;  // y a un remoto aplazado
  c->dirtyValue[id] = false;     // ya esta escrito; nada rancio que persistir
  c->dirtyState[id] = false;
  if (!c->pf.persist_state(id, &k)) {
    emit(c, RCFG_EV_PERSIST_FAIL, origin, id, requested, v, before, st, 0);
  }
  if (persisted != nullptr) {
    *persisted = true;
  }
  emit(c, RCFG_EV_CHANGED, origin, id, requested, v, before, st, 0);
  return st;
}

void rcfg_remote_key(RcfgCore *c, RuntimeConfigOrigin origin, RuntimeConfigId id,
                     float requested, bool new_write) {
  if ((unsigned)id >= (unsigned)RCFG_COUNT) {
    return;
  }
  RcfgKeyState &k = c->st[id];

  if (!new_write) {
    // Respuesta a la peticion de una conexion: solo cuenta una escritura nueva.
    if (k.hasRemote && k.lastRemote == requested) {
      return;  // reenvio de lo ultimo aplicado
    }
    if (c->seenValid[id] && c->seen[id] == requested) {
      return;  // reenvio de algo que ya se miro y no se aplico
    }
    if (k.overrideLocal) {
      // Manda el cambio local. Se recuerda en RAM (NO en NVS) para no repetir
      // el aviso en cada conexion.
      c->seen[id] = requested;
      c->seenValid[id] = true;
      uint32_t sup = 0;
      if (note_allowed(c, id, &sup)) {
        emit(c, RCFG_EV_LOCAL_WINS, origin, id, requested, c->pf.current(id),
             c->pf.current(id), RCFG_OK, sup);
      }
      return;
    }
  }

  switch (try_remote(c, origin, id, requested)) {
    case Remote::Applied:
      if (new_write) {
        lift_override(c, id);
      }
      remember_applied(c, id, requested);
      flush_dirty(c);
      break;
    case Remote::Deferred:
      // Aplazado cuenta como escritura remota nueva (levanta el override) pero
      // NO se da por visto: vive en RAM, y si la unidad reinicia antes de
      // aplicarlo la siguiente peticion tiene que reintentarlo.
      if (new_write) {
        lift_override(c, id);
      }
      flush_dirty(c);
      break;
    case Remote::Rejected:
      // Un rechazo no levanta el override ni toca NVS: solo se recuerda en RAM.
      c->seen[id] = requested;
      c->seenValid[id] = true;
      break;
  }
}

void rcfg_remote_bad_type(RcfgCore *c, RuntimeConfigOrigin origin,
                          RuntimeConfigId id) {
  if ((unsigned)id >= (unsigned)RCFG_COUNT) {
    return;
  }
  note_rejected(c, origin, id, 0.0f, RCFG_REJ_WRONG_TYPE);
}

void rcfg_poll(RcfgCore *c) {
  for (int i = 0; i < RCFG_COUNT; ++i) {
    if (!c->pending[i].valid) {
      continue;
    }
    const RuntimeConfigId id = (RuntimeConfigId)i;
    const bool wait =
        runtime_config_should_defer(id, c->pf.therapy_active()) ||
        !runtime_config_rate_ok(c->hasLastRemoteChange[i], c->lastRemoteChangeMs[i],
                                c->pf.now_ms(),
                                (uint32_t)RCFG_REMOTE_MIN_INTERVAL_MS);
    if (wait) {
      continue;
    }
    const RcfgPending p = c->pending[i];
    c->pending[i].valid = false;
    switch (try_remote(c, p.origin, id, p.value)) {
      case Remote::Applied:
        remember_applied(c, id, p.value);
        break;
      case Remote::Rejected:
        c->seen[i] = p.value;
        c->seenValid[i] = true;
        break;
      case Remote::Deferred:
        break;
    }
  }
  flush_dirty(c);
}
