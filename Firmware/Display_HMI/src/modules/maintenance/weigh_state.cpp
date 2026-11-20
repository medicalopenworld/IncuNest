#include "modules/maintenance/weigh_state.h"

void weigh_state_init(weigh_state_t *s) {
  if (s == nullptr) return;
  *s = weigh_state_t{};
  s->enabled = true;  // por defecto ON: es un dato clinico, no una limpieza
}

void weigh_state_tick(weigh_state_t *s, const weigh_env_t *env) {
  if (s == nullptr || env == nullptr) return;
  // El bebe al mando en formacion es de practica (ZOE o el de la leccion): ni
  // es un paciente real ni debe anotarse. Va ANTES de leer nada.
  if (env->training) return;

  const uint32_t now = env->nowEpoch;
  const uint32_t seq = env->activeSeq;

  if (seq != s->trackedSeq) {
    if (seq == 0) {
      // Alta del bebe (o perfil olvidado): no queda nada que recordar.
      s->weighSeq = s->weighEpoch = 0;
      s->seenSeq = s->seenEpoch = 0;
      s->snoozeUntil = 0;
      s->pendingSeq = 0;
    } else {
      // Bebe nuevo al mando: lo que no sea suyo se descarta. El "mas tarde"
      // solo se conserva si ya conociamos a este bebe (el mismo tras un
      // reinicio del HMI).
      const bool known = (seq == s->weighSeq) || (seq == s->seenSeq);
      if (s->weighSeq != seq) s->weighSeq = s->weighEpoch = 0;
      if (s->seenSeq != seq) s->seenSeq = s->seenEpoch = 0;
      if (!known) s->snoozeUntil = 0;
      if (s->pendingSeq != seq) s->pendingSeq = 0;
    }
    s->trackedSeq = seq;
    s->dirty = true;
  }

  if (now == 0) return;

  // Pesada enviada sin hora: ya hay reloj, se sella ahora.
  if (s->pendingSeq != 0) {
    if (s->pendingSeq == seq) {
      s->weighSeq = seq;
      s->weighEpoch = now;
      s->snoozeUntil = 0;
      s->dirty = true;
    }
    s->pendingSeq = 0;
  }

  // Primera vez que se ve a este bebe al mando: es la referencia de un bebe
  // sin ninguna pesada sellada.
  if (seq != 0 && s->seenSeq != seq) {
    s->seenSeq = seq;
    s->seenEpoch = now;
    s->dirty = true;
  }

  // Reloj hacia atras: lo guardado quedaria en el futuro y el plazo no se
  // cumpliria hasta que el reloj lo alcanzase. Se reancla a hoy, pero solo
  // pasada la tolerancia: la hora se corrige a saltos pequenos y no merece una
  // escritura de NVS cada vez.
  if (s->weighEpoch > now + WEIGH_FUTURE_TOLERANCE_SECS) {
    s->weighEpoch = now;
    s->dirty = true;
  }
  if (s->seenEpoch > now + WEIGH_FUTURE_TOLERANCE_SECS) {
    s->seenEpoch = now;
    s->dirty = true;
  }
  if (s->snoozeUntil > now + WEIGH_REMINDER_SNOOZE_SECS) {
    s->snoozeUntil = 0;
    s->dirty = true;
  }
}

bool weigh_state_note_weighed(weigh_state_t *s, const weigh_env_t *env,
                              uint32_t seq, uint16_t grams, bool practiceSeq) {
  if (s == nullptr || env == nullptr) return false;
  // 0 g es SKIP ("no hay peso"), no una pesada.
  if (seq == 0 || grams == 0) return false;
  // En formacion no se escribe nada (ADR-0002).
  if (env->training || practiceSeq) return false;
  // Un unico hueco de sello: pesar a un bebe que no esta al mando (alta desde
  // la pantalla Bebes) no debe pisar el del que si lo esta.
  if (seq != env->activeSeq) return false;

  s->snoozeUntil = 0;  // registrar un peso cancela el "mas tarde"
  if (env->nowEpoch == 0) {
    // Sin hora no se puede fechar; se sella en cuanto la haya. El sello
    // anterior no se toca (y no se avisa mientras haya pendiente).
    s->pendingSeq = seq;
  } else {
    s->weighSeq = seq;
    s->weighEpoch = env->nowEpoch;
    s->pendingSeq = 0;
  }
  s->dirty = true;
  return true;
}

bool weigh_state_snooze(weigh_state_t *s, uint32_t nowEpoch) {
  if (s == nullptr) return false;
  const uint32_t until = weigh_snooze_until(nowEpoch);
  if (until == 0) return false;
  s->snoozeUntil = until;
  s->dirty = true;
  return true;
}

void weigh_state_set_enabled(weigh_state_t *s, bool on) {
  if (s == nullptr || s->enabled == on) return;
  s->enabled = on;
  // Volver a activarlo no debe arrastrar un "mas tarde" de hace tres semanas.
  s->snoozeUntil = 0;
  s->dirty = true;
}

weigh_inputs_t weigh_state_inputs(const weigh_state_t *s,
                                  const weigh_env_t *env, bool liveSession) {
  weigh_inputs_t in = {};
  if (s == nullptr || env == nullptr) return in;
  in.nowEpoch = env->nowEpoch;
  in.activeSeq = env->activeSeq;
  in.liveSession = liveSession;
  in.weighSeq = s->weighSeq;
  in.weighEpoch = s->weighEpoch;
  in.seenSeq = s->seenSeq;
  in.seenEpoch = s->seenEpoch;
  in.snoozeUntil = s->snoozeUntil;
  return in;
}

bool weigh_state_should_warn(const weigh_state_t *s, const weigh_env_t *env,
                             bool liveSession) {
  if (s == nullptr || env == nullptr) return false;
  if (!s->enabled) return false;
  if (s->pendingSeq != 0) return false;  // acaba de pesarse, falta fecharlo
  const weigh_inputs_t in = weigh_state_inputs(s, env, liveSession);
  return weigh_reminder_should_warn(&in);
}
