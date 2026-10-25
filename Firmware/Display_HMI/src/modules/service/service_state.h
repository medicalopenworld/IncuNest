#pragma once
// Estado de servicio por horas de uso tal como lo cuenta la motherBoard
// (`CTRL,SERVICE`, ver shared/include/service_hours_proto.h).
//
// El HMI NO calcula horas ni decide umbrales: la placa es la duena del dato y
// del umbral, aqui solo se guarda lo ultimo que dijo y se decide CUANDO avisar.
// Sin LVGL ni Arduino a proposito, para probarlo en host. El pop-up que lo
// pinta es la vista de servicio de `ui/MaintenanceDialog.cpp`.
//
// No es una alarma: no hay AlarmId, no suena el zumbador. Es el mismo tipo de
// aviso suave que el recordatorio de limpieza de modules/maintenance.
//
// El "MAS TARDE" vive solo en RAM: un reinicio del display lo olvida y el
// aviso vuelve en el siguiente desbloqueo. Es una molestia aceptada para no
// escribir NVS por un aviso de baja prioridad.
#include <stdbool.h>
#include <stdint.h>

#include "service_hours_proto.h"

// Lo que dura el "MAS TARDE" (24 h, como el de la limpieza).
#define SERVICE_SNOOZE_SECS (24u * 3600u)

// Olvida todo (arranque, tests, enlace caido).
void ServiceState_Reset(void);

// Aplica un informe ya validado por service_proto_parse_report(). Solo
// actualiza los actuadores que trae.
void ServiceState_Apply(const ServiceReport *r);

// True si ha llegado al menos un informe.
bool ServiceState_HasData(void);

// Horas desde servicio y umbral de un actuador. False si el id no existe o aun
// no se ha recibido nada de el.
bool ServiceState_Get(uint8_t id, uint32_t *hours, uint32_t *thresholdH);

// Vencido = horas >= umbral, con umbral 0 = sin aviso.
bool ServiceState_IsDue(uint8_t id);
bool ServiceState_AnyDue(void);
uint8_t ServiceState_DueCount(void);

// True si hay que sacar el aviso suave ahora: algo vencido y sin un "mas
// tarde" que lo cubra. Un actuador que vence DESPUES de callar el aviso lo
// rompe: es algo que el operador aun no ha visto. `nowSec` es un reloj
// monotono en segundos (millis()/1000), no el epoch: no depende de tener hora.
bool ServiceState_ShouldWarn(uint32_t nowSec);

// Boton MAS TARDE: calla el aviso de lo vencido ahora durante 24 h.
void ServiceState_Snooze(uint32_t nowSec);
