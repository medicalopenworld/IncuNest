#pragma once
// Maquina de estados del recordatorio de pesada: alta, cambio de bebe, sello
// pendiente sin hora, "visto por primera vez" y reancla de fechas en el
// futuro. Sin Arduino, NVS, LVGL ni BabyWizard: todo lo que necesita del
// mundo entra por `weigh_env_t` (hora, bebe al mando, formacion) y por los
// argumentos, asi que se prueba en host (`[env:native]`).
//
// `weigh_tracker.cpp` es la cola fina que carga/guarda `weigh_state_t` en NVS
// y rellena `weigh_env_t` con HMI_GetEpochNow(), BabyWizard y Training.
//
// La decision de "toca avisar" sigue en `weigh_reminder.h`; aqui solo se
// mantiene el estado del que sale.
#include <stdbool.h>
#include <stdint.h>

#include "modules/maintenance/weigh_reminder.h"

// Un sello a menos de este margen por delante de la hora actual no se toca:
// la hora se corrige a saltos (NTP, NITZ, ajuste manual) y reanclar por unos
// segundos de diferencia reescribiria la NVS sin motivo. Mas alla, el sello
// esta en el futuro de verdad y el plazo no se cumpliria hasta alcanzarlo.
#define WEIGH_FUTURE_TOLERANCE_SECS 60u

typedef struct {
  // --- Persistido (NVS) ---
  uint32_t weighSeq;     // bebe del sello de pesada
  uint32_t weighEpoch;   // epoch de la ultima pesada real, 0 = ninguna
  uint32_t seenSeq;      // bebe del "visto por primera vez"
  uint32_t seenEpoch;    // epoch de esa primera vez, 0 = ninguna
  uint32_t snoozeUntil;  // fin del "mas tarde", 0 = no hay
  bool enabled;          // interruptor de Ajustes (por defecto ON)
  // --- Solo RAM ---
  uint32_t trackedSeq;  // ultimo bebe al mando visto por tick()
  uint32_t pendingSeq;  // pesada enviada sin hora, a sellar con la primera
  bool dirty;           // hay algo que escribir en NVS
} weigh_state_t;

// Lo que el estado necesita saber del exterior en este instante.
typedef struct {
  uint32_t nowEpoch;   // 0 = sin hora sincronizada
  uint32_t activeSeq;  // bebe al mando, 0 = ninguno
  bool training;       // Training_IsActive(): nada se anota ni se escribe
} weigh_env_t;

// Estado de fabrica: sin sellos, avisos activados.
void weigh_state_init(weigh_state_t *s);

// Una vez por vuelta de UI_Task. En formacion no hace nada.
void weigh_state_tick(weigh_state_t *s, const weigh_env_t *env);

// Una pesada ha salido hacia la placa (y, para el aviso, ha sido confirmada o
// se da por buena). Devuelve true si ha quedado sellada o pendiente de fecha.
// Se ignora: 0 g (SKIP), seq 0, formacion, bebe de practicas, y un bebe que no
// es el que esta al mando (hay un unico hueco de sello).
bool weigh_state_note_weighed(weigh_state_t *s, const weigh_env_t *env,
                              uint32_t seq, uint16_t grams, bool practiceSeq);

// MAS TARDE. Sin hora no se puede fechar: devuelve false y no cambia nada.
bool weigh_state_snooze(weigh_state_t *s, uint32_t nowEpoch);

// Interruptor de Ajustes. Volver a activarlo no arrastra un "mas tarde" viejo.
void weigh_state_set_enabled(weigh_state_t *s, bool on);

// Instantanea para la decision pura.
weigh_inputs_t weigh_state_inputs(const weigh_state_t *s,
                                  const weigh_env_t *env, bool liveSession);

// Avisa ahora: activado, sin pesada pendiente de fechar y
// weigh_reminder_should_warn().
bool weigh_state_should_warn(const weigh_state_t *s, const weigh_env_t *env,
                             bool liveSession);
