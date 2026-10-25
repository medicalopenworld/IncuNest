#pragma once
// Decision pura del recordatorio de pesada diaria: sin LVGL, sin Arduino, sin
// NVS. Lo unico que hace es contestar "toca avisar?" a partir de una
// instantanea del estado, para poder probarlo en host (`[env:native]`).
//
// Quien monta la instantanea, la persiste y pinta el aviso es
// `weigh_tracker.cpp` / `ui/MaintenanceDialog.cpp`.
//
// Referencia = el instante contra el que se cuentan las 24 h:
//   1. el sello de la ultima pesada, si es del bebe activo;
//   2. si no, el momento en que el HMI vio por primera vez a ese bebe al
//      mando (para un bebe sin ninguna pesada sellada);
//   3. si no hay ninguno, no hay referencia y no se avisa.
//
// Todo en epoch UTC. 0 significa "no hay": sin hora no se avisa, y no se
// inventan tiempos con millis(), que no sobreviven a un reinicio.
#include <stdbool.h>
#include <stdint.h>

// Plazo sin pesada y duracion del "mas tarde". Un dia, igual que el
// mantenimiento: un recordatorio no es una alarma.
#define WEIGH_REMINDER_PERIOD_SECS 86400u
#define WEIGH_REMINDER_SNOOZE_SECS 86400u

typedef struct {
  uint32_t nowEpoch;     // hora actual, 0 = sin hora sincronizada
  uint32_t activeSeq;    // bebe al mando, 0 = ninguno
  bool liveSession;      // BabyWizard_HasLiveSession()
  uint32_t weighSeq;     // bebe al que pertenece el sello de pesada
  uint32_t weighEpoch;   // epoch de la ultima pesada real, 0 = ninguna
  uint32_t seenSeq;      // bebe cuya primera aparicion se anoto
  uint32_t seenEpoch;    // epoch de esa primera aparicion, 0 = ninguna
  uint32_t snoozeUntil;  // fin del "mas tarde", 0 = no hay
} weigh_inputs_t;

// Epoch de referencia del bebe activo, o 0 si no hay referencia.
uint32_t weigh_reference_epoch(const weigh_inputs_t *in);

// Horas enteras transcurridas desde la referencia, 0 si no hay referencia,
// no hay hora o la referencia esta en el futuro. Es para el texto del aviso.
uint32_t weigh_elapsed_hours(const weigh_inputs_t *in);

// Pesada vencida, sin mirar el "mas tarde": sesion viva, hora valida,
// referencia en el pasado y >= 24 h.
bool weigh_reminder_due(const weigh_inputs_t *in);

// Hay que sacar el aviso ya: vencida y sin "mas tarde" vigente.
bool weigh_reminder_should_warn(const weigh_inputs_t *in);

// Fin del "mas tarde" que se pide ahora, 0 si no hay hora para fecharlo.
uint32_t weigh_snooze_until(uint32_t nowEpoch);
