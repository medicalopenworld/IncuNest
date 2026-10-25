#pragma once
// Estado vivo del bebe al mando: edad capturada, minutos de terapia acumulados
// y, sobre todo, si la placa CONFIRMA que ese bebe es el que esta dentro. Sin
// LVGL. La logica de decision es pura y vive en modules/patient/patient_session
// (con tests de host); aqui estan el estado, la NVS y el pegamento con el
// asistente y con el protocolo.
//
// CONFIRMACION. Una identidad restaurada de la NVS nace SIN CONFIRMAR: el HMI
// solo sabe lo que vio, y puede estar delante de otra placa (los seq de dos
// placas colisionan), de una NVS borrada o de un alta hecha por otra via. La
// placa confirma con activeSeq (campo 24 de CTRL,STATE); con una placa antigua
// se contrasta con CTRL,PROFILE_LIST. Una identidad que viene de un
// PROFILE_ACK del asistente nace confirmada. Mientras no lo este no se pinta
// como cierta, no se acreditan minutos y el asistente no la reutiliza.
//
// HILO: todo se llama desde UI_Task (asistente, dialogo de salida, refresco),
// asi que no hay cerrojos. Los ganchos OnXxx corren DENTRO de LVGL_Lock y por
// eso solo marcan banderas; la NVS y las peticiones a la placa se hacen unicamente
// en Service() y Restore(), FUERA de LVGL_Lock (ARQ-LOCK-001). Las unicas vias
// que cruzan de hilo (espejo de PROFILE_LIST, instantanea de CTRL,STATE) son
// seguras por si mismas, en CommTask.
//
// Modo formacion: el bebe de practicas (ZOE) jamas entra aqui. Nada de lo que
// pasa en una leccion se persiste (ADR-0002).
#include <stdint.h>

#include "modules/patient/patient_session.h"

// Arranque, antes de Maintenance_Init()/Maintenance_Tick(): lee la NVS y, si
// hay un bebe valido, lo devuelve al asistente con BabyWizard_SetSession(),
// SIN CONFIRMAR. Un registro invalido (version vieja, corrupto) se descarta y
// se borra.
void PatientState_Restore(void);

// Un bebe pasa a estar al mando. `fromBoardAck` = viene de un PROFILE_ACK (la
// placa acaba de responder por el): nace confirmado. Mismo seq, nombre y
// semanas = el mismo bebe: conserva edad y acumulados. Cualquier diferencia
// (incluido el MISMO seq con otro nombre: la placa reasigno el seq) = bebe
// nuevo, sin edad ni acumulados. Los acumulados arrancan en `seed` (minutos que
// la placa tenia de el; nullptr = no se sabe, quedan ocultos hasta que una
// lista los fije).
void PatientState_OnSessionSet(uint32_t seq, const char *name,
                               uint8_t gestWeeks, const PatientMinutes *seed,
                               bool fromBoardAck);

// CTRL,PROFILE_RANGE del `seq` en curso. `known`=false borra la edad.
void PatientState_OnAge(uint32_t seq, uint16_t ageDays, bool known);

// Alta o "la placa no conoce ese bebe": olvida identidad y acumulados, tambien
// en NVS.
void PatientState_OnClear(void);

// Que hay que pintar del bebe `seq`: nada, la identidad completa, o el nombre
// gris con "?" (placa antigua con el bebe en su lista). Un seq que no es el de
// esta identidad (formacion, bebe de practicas) se da por CONFIRMED: no es
// cosa de este modulo.
PatientDisplay PatientState_GetDisplay(uint32_t seq);

// Atajo para el asistente: true si NO hay que dudar del bebe `seq`.
bool PatientState_IsConfirmed(uint32_t seq);

// Edad del bebe `seq`, calculada con la hora de ahora. Con otro seq, sin
// identidad o sin hora devuelve todo a false.
PatientAge PatientState_GetAge(uint32_t seq, uint8_t gestWeeks);

// Minutos acumulados del bebe `seq`. false si no es el bebe que se esta
// contando o aun no se saben (entonces `out` no se toca).
bool PatientState_GetTherapy(uint32_t seq, PatientMinutes *out);

// Pasada de servicio desde UI_Task, FUERA de LVGL_Lock: confirma la identidad
// con la placa, acumula terapia (~1 Hz), resincroniza con las listas y vuelca a
// NVS lo pendiente.
void PatientState_Service(void);
