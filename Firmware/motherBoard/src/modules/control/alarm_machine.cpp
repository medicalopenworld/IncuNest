#include "alarm_machine.h"

namespace {

struct Entry {
  bool present;       // la condicion fisica esta ocurriendo ahora
  AlarmState state;
  uint32_t announce_delay_ms;
  uint32_t present_since_ms;
  uint32_t silenced_until_ms;
  uint32_t audio_hold_until_ms;  // rafaga minima 6.10: audio exigido hasta aqui
  // Si la ventana de arriba esta SELLADA. Sin este flag habria que distinguir
  // "nunca se sello" de "se sello en el instante 0" mirando solo
  // audio_hold_until_ms, y no se puede: el 0 no es un centinela, es un
  // instante del reloj. Con audio_hold_until_ms == 0 y millis() cruzando 2^31
  // (24,86 dias), la resta con signo se vuelve negativa y las 16 condiciones
  // que nunca se activaron declaran a la vez que estan completando su rafaga
  // minima — zumbador en patron ALTA, bitmask a 0 (sin senal visual, sin
  // CTRL,ALM, sin telemetria) e imposible de callar, porque el encoder exige
  // any_signalling() y silence() solo actua sobre ACTIVE.
  bool audio_hold_active;
};

Entry g_entries[ALARM_COUNT];

// Instante de la ultima llamada a alarm_machine_tick()/alarm_machine_condition(),
// para que alarm_machine_audio_required() pueda saber si la rafaga minima ya
// expiro sin recibir el tiempo como parametro.
uint32_t g_last_tick_ms = 0;

bool is_signalling(AlarmState s) { return s != ALARM_STATE_INACTIVE; }

bool valid(AlarmId id) { return id > ALARM_NONE && id < ALARM_COUNT; }

// Sella la ventana de rafaga minima de 6.10 al anunciarse la condicion. Unico
// punto que la abre.
void arm_audio_hold(Entry &e, AlarmId id, uint32_t now_ms) {
  e.audio_hold_until_ms =
      now_ms + (alarm_priority(id) == ALARM_PRIORITY_HIGH
                    ? ALARM_MIN_BURST_MS_HIGH
                    : ALARM_MIN_BURST_MS_MEDIUM);
  e.audio_hold_active = true;
}

// Cancela la ventana. 6.10 exige completar la rafaga "unless inactivated by
// the OPERATOR": silenciar y aceptar son esa inactivacion.
void cancel_audio_hold(Entry &e) {
  e.audio_hold_active = false;
  e.audio_hold_until_ms = 0;
}

// UNICA definicion del predicado "esta condicion sigue dentro de su ventana de
// rafaga minima". Estaba escrito por duplicado en audio_required() y en
// audible_priority(), y esa duplicacion es exactamente lo que dio dos puntos
// de escape al mismo fallo. La ventana consumida de forma natural se cierra
// aqui, para que el flag no quede armado indefinidamente.
bool audio_hold_pending(Entry &e) {
  if (!e.audio_hold_active) {
    return false;
  }
  if ((int32_t)(g_last_tick_ms - e.audio_hold_until_ms) < 0) {
    return true;
  }
  cancel_audio_hold(e);
  return false;
}

// El criterio de audio, en un solo sitio: ACTIVE, o INACTIVE todavia dentro de
// la ventana de rafaga minima. SILENCED y ACKED quedan fuera a proposito - son
// la inactivacion del OPERADOR que 6.10 exime de completar la rafaga.
bool entry_requires_audio(Entry &e) {
  return e.state == ALARM_STATE_ACTIVE ||
         (e.state == ALARM_STATE_INACTIVE && audio_hold_pending(e));
}

}  // namespace

void alarm_machine_init(void) {
  for (int i = 0; i < ALARM_COUNT; ++i) {
    g_entries[i].present = false;
    g_entries[i].state = ALARM_STATE_INACTIVE;
    g_entries[i].announce_delay_ms = 0;
    g_entries[i].present_since_ms = 0;
    g_entries[i].silenced_until_ms = 0;
    g_entries[i].audio_hold_until_ms = 0;
    g_entries[i].audio_hold_active = false;
  }
  g_last_tick_ms = 0;
}

void alarm_machine_set_announce_delay(AlarmId id, uint32_t delay_ms) {
  if (valid(id)) {
    g_entries[id].announce_delay_ms = delay_ms;
  }
}

void alarm_machine_condition(AlarmId id, bool present, uint32_t now_ms) {
  g_last_tick_ms = now_ms;
  if (!valid(id)) {
    return;
  }
  Entry &e = g_entries[id];
  e.present = present;

  if (present) {
    if (e.state == ALARM_STATE_INACTIVE) {
      e.present_since_ms = now_ms;
      // Un corte termico nunca espera: la norma exige aviso inmediato.
      const bool may_wait =
          e.announce_delay_ms > 0 && !alarm_is_latching(id);
      e.state = may_wait ? ALARM_STATE_PENDING : ALARM_STATE_ACTIVE;
      if (e.state == ALARM_STATE_ACTIVE) {
        arm_audio_hold(e, id, now_ms);
      }
    }
  } else {
    // 201.15.4.2.1 aa)/bb): un corte termico mantiene la alarma hasta reset
    // manual aunque la temperatura ya haya vuelto a rango. El resto se limpia
    // solo (senal non-latching, 6.10).
    if (!alarm_is_latching(id)) {
      e.state = ALARM_STATE_INACTIVE;
    }
  }
}

void alarm_machine_tick(uint32_t now_ms) {
  g_last_tick_ms = now_ms;
  for (int i = ALARM_NONE + 1; i < ALARM_COUNT; ++i) {
    Entry &e = g_entries[i];
    if (e.state == ALARM_STATE_PENDING && e.present &&
        (uint32_t)(now_ms - e.present_since_ms) >= e.announce_delay_ms) {
      e.state = ALARM_STATE_ACTIVE;
      arm_audio_hold(e, (AlarmId)i, now_ms);
    }
    if (e.state == ALARM_STATE_SILENCED &&
        (int32_t)(now_ms - e.silenced_until_ms) >= 0) {
      e.state = ALARM_STATE_ACTIVE;
    }
    // Caducar la ventana de rafaga minima SIEMPRE, sea cual sea el estado.
    //
    // No basta con evaluarla cuando se consulta el audio: alli solo se alcanza
    // con la entrada en INACTIVE (el otro lado del || cortocircuita), asi que
    // una condicion que aguanta ACTIVE deja el flag armado y
    // audio_hold_until_ms congelado en el instante de la activacion. Si eso
    // dura mas de 2^31 ms y despues el detector la retira, la resta con signo
    // vuelve a ser negativa y reaparece el fallo del zumbador fantasma, esta
    // vez sin poder forzar siquiera silence()/ack() porque la entrada ya esta
    // INACTIVE. Como el tick corre en cada ciclo de securityCheck(), evaluarla
    // aqui cierra la ventana a los ~1,5 s de armarse y el flag no puede
    // sobrevivir armado a un desbordamiento del reloj.
    (void)audio_hold_pending(e);
  }
}

bool alarm_machine_audio_required(void) {
  bool required = false;
  for (int i = ALARM_NONE + 1; i < ALARM_COUNT; ++i) {
    // Sin corte temprano: entry_requires_audio() tambien cierra las ventanas ya
    // consumidas, y saltarse las entradas restantes solo aplazaria ese cierre.
    if (entry_requires_audio(g_entries[i])) {
      required = true;
    }
  }
  return required;
}

AlarmState alarm_machine_state(AlarmId id) {
  return valid(id) ? g_entries[id].state : ALARM_STATE_INACTIVE;
}

uint32_t alarm_machine_bitmask(void) {
  uint32_t mask = 0;
  for (int i = ALARM_NONE + 1; i < ALARM_COUNT; ++i) {
    if (is_signalling(g_entries[i].state)) {
      mask |= (1u << i);
    }
  }
  return mask;
}

bool alarm_machine_heater_must_cut(void) {
  for (int i = ALARM_NONE + 1; i < ALARM_COUNT; ++i) {
    if (g_entries[i].present && alarm_cuts_heater((AlarmId)i)) {
      return true;
    }
  }
  return false;
}

void alarm_machine_silence(AlarmId id, uint32_t duration_ms, uint32_t now_ms) {
  if (!valid(id)) {
    return;
  }
  Entry &e = g_entries[id];
  if (e.state != ALARM_STATE_ACTIVE) {
    return;  // solo se silencia lo que se esta anunciando
  }
  e.state = ALARM_STATE_SILENCED;
  e.silenced_until_ms = now_ms + duration_ms;
  // 6.10: la rafaga minima se exige "unless inactivated by the OPERATOR" -
  // silenciar es esa inactivacion, y cancela la rafaga pendiente.
  cancel_audio_hold(e);
}

void alarm_machine_ack(AlarmId id, uint32_t now_ms) {
  (void)now_ms;
  if (!valid(id)) {
    return;
  }
  Entry &e = g_entries[id];
  if (e.state == ALARM_STATE_ACTIVE || e.state == ALARM_STATE_SILENCED) {
    e.state = ALARM_STATE_ACKED;
    // 6.10: idem que en silence() - ACK tambien es una inactivacion del
    // OPERADOR y cancela la rafaga pendiente.
    cancel_audio_hold(e);
  }
}

bool alarm_machine_is_latched(AlarmId id) {
  if (!valid(id)) {
    return false;
  }
  const Entry &e = g_entries[id];
  return alarm_is_latching(id) && !e.present &&
         e.state != ALARM_STATE_INACTIVE;
}

bool alarm_machine_reset(AlarmId id, uint32_t now_ms) {
  (void)now_ms;
  if (!alarm_machine_is_latched(id)) {
    return false;
  }
  g_entries[id].state = ALARM_STATE_INACTIVE;
  // El reset es la tercera inactivacion del OPERADOR, junto a silence() y
  // ack(), y 6.10 exime a las tres de completar la rafaga. Sin esto, resetear
  // dentro de la ventana dejaria el flag armado sobre una entrada ya INACTIVE.
  cancel_audio_hold(g_entries[id]);
  return true;
}

AlarmPriority alarm_machine_top_priority(void) {
  AlarmPriority top = ALARM_PRIORITY_LOW;
  for (int i = ALARM_NONE + 1; i < ALARM_COUNT; ++i) {
    const AlarmState s = g_entries[i].state;
    if (s == ALARM_STATE_ACTIVE || s == ALARM_STATE_SILENCED ||
        s == ALARM_STATE_ACKED) {
      const AlarmPriority p = alarm_priority((AlarmId)i);
      if (p > top) {
        top = p;
      }
    }
  }
  return top;
}

AlarmPriority alarm_machine_audible_priority(void) {
  AlarmPriority top = ALARM_PRIORITY_LOW;
  for (int i = ALARM_NONE + 1; i < ALARM_COUNT; ++i) {
    // Mismo predicado, literalmente la misma funcion, que
    // alarm_machine_audio_required().
    if (entry_requires_audio(g_entries[i])) {
      const AlarmPriority p = alarm_priority((AlarmId)i);
      if (p > top) {
        top = p;
      }
    }
  }
  return top;
}

bool alarm_machine_any_signalling(void) { return alarm_machine_bitmask() != 0; }

bool alarm_machine_any_silenceable(void) {
  for (int i = 0; i < ALARM_COUNT; ++i) {
    if (g_entries[i].state == ALARM_STATE_ACTIVE) {
      return true;
    }
  }
  return false;
}
