#include "modules/control/fan_guard.h"

#include <string.h>

void fan_guard_init(FanGuard *g) {
  if (g == NULL) {
    return;
  }
  memset(g, 0, sizeof(*g));
}

FanGuardOutcome fan_guard_update(FanGuard *g, const FanGuardConfig *cfg,
                                 bool has_feedback, bool energised, double rpm,
                                 uint32_t now_ms) {
  if (g == NULL || cfg == NULL) {
    return FAN_GUARD_SILENT;
  }

  if (!has_feedback) {
    // Sin tacometro no hay nada que observar, asi que tampoco nada que
    // afirmar: ni siquiera se retira, porque esta guarda no ha declarado nunca
    // nada en esta unidad y hacerlo pisaria a otro declarante.
    return FAN_GUARD_SILENT;
  }

  // Flanco apagado->alimentado: empieza a contar el margen mecanico. El sello
  // se toma AQUI y no al ordenar el ventilador: mientras una alarma mantenga
  // cortada la alimentacion, el ventilador no gira por mucho que este ordenado.
  if (energised && !g->was_energised) {
    g->energised_since_ms = now_ms;
    g->have_energised_stamp = true;
  }
  g->was_energised = energised;

  if (!energised) {
    // Sin alimentar no se puede afirmar que falle. Pero RETIRAR es obligatorio:
    // la maquina de alarmas conserva `present` hasta que alguien declara false,
    // asi que callarse aqui deja viva la ultima averia declarada. Ademas se
    // reinicia la histeresis para que el proximo arranque se juzgue con el
    // umbral limpio y no con el relajado.
    g->failure_present = false;
    return FAN_GUARD_ABSENT;
  }

  if (g->have_energised_stamp &&
      (uint32_t)(now_ms - g->energised_since_ms) < cfg->spinup_grace_ms) {
    return FAN_GUARD_SILENT; // arrancando; la condicion ya quedo retirada
  }

  // Histeresis con el signo invertido respecto de un umbral normal, porque
  // aqui alarma el valor BAJO: se declara por debajo de min_rpm y no se retira
  // hasta superar min_rpm + hysteresis_rpm.
  g->failure_present = g->failure_present
                           ? (rpm < cfg->min_rpm + cfg->hysteresis_rpm)
                           : (rpm < cfg->min_rpm);
  return g->failure_present ? FAN_GUARD_PRESENT : FAN_GUARD_ABSENT;
}
