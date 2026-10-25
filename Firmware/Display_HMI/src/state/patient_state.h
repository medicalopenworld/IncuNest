#pragma once
// Estado vivo del bebe al mando: edad capturada, y (mas adelante) minutos de
// terapia acumulados. Sin LVGL. La logica de calculo es pura y vive en
// modules/patient/patient_session; aqui solo estan el estado, la NVS y el
// pegamento con el asistente.
//
// HILO: todo se llama desde UI_Task (asistente, dialogo de salida, refresco),
// asi que no hay cerrojos. Los ganchos OnXxx corren DENTRO de LVGL_Lock y por
// eso solo marcan banderas; la NVS se toca unicamente en Service() y
// Restore(), FUERA de LVGL_Lock (ARQ-LOCK-001).
//
// Modo formacion: el bebe de practicas (ZOE) jamas entra aqui. Nada de lo que
// pasa en una leccion se persiste (ADR-0002).
#include <stdint.h>

#include "modules/patient/patient_session.h"

// Arranque, antes de Maintenance_Init()/Maintenance_Tick(): lee la NVS y, si
// hay un bebe valido, lo devuelve al asistente con BabyWizard_SetSession().
// Es identidad RECORDADA: si la placa dice otra cosa, manda la placa.
void PatientState_Restore(void);

// Un bebe pasa a estar al mando (PROFILE_ACK, o SetSession). Mismo seq =
// actualiza nombre/semanas y conserva la edad ya capturada.
void PatientState_OnSessionSet(uint32_t seq, const char *name, uint8_t gestWeeks);

// CTRL,PROFILE_RANGE del `seq` en curso. `known`=false borra la edad.
void PatientState_OnAge(uint32_t seq, uint16_t ageDays, bool known);

// Alta o "la placa no conoce ese bebe": olvida identidad y acumulados, tambien
// en NVS.
void PatientState_OnClear(void);

// Edad del bebe `seq`, calculada con la hora de ahora. Con otro seq o sin
// identidad devuelve todo a false. `gestWeeks` sale de la identidad guardada.
PatientAge PatientState_GetAge(uint32_t seq, uint8_t gestWeeks);

// Volcado a NVS pendiente. Llamar desde UI_Task FUERA de LVGL_Lock.
void PatientState_Service(void);
