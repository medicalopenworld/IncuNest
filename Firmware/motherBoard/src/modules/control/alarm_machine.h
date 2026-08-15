#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "alarm_ids.h"
#include "alarm_policy.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  ALARM_STATE_INACTIVE = 0,
  ALARM_STATE_PENDING,   // condicion presente, dentro del retardo de anuncio
  ALARM_STATE_ACTIVE,    // anunciandose: visual + audio
  ALARM_STATE_SILENCED,  // audio inactivo por accion del operador, visual sigue
  ALARM_STATE_ACKED,     // audio inactivo indefinidamente, visual sigue
} AlarmState;

void alarm_machine_init(void);

// Informa de si la condicion fisica esta presente. Idempotente.
void alarm_machine_condition(AlarmId id, bool present, uint32_t now_ms);

// Hace avanzar los temporizadores. Debe llamarse periodicamente.
void alarm_machine_tick(uint32_t now_ms);

AlarmState alarm_machine_state(AlarmId id);

// Bit por AlarmId de las condiciones que estan generando senal visual, en
// cualquiera de los estados anunciables (ACTIVE, SILENCED, ACKED, PENDING).
uint32_t alarm_machine_bitmask(void);

// true si alguna condicion presente exige desconectar el calefactor.
bool alarm_machine_heater_must_cut(void);

#ifdef __cplusplus
}
#endif
