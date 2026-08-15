#pragma once
#include "alarm_ids.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Prioridad asignada segun la Tabla 1 de IEC 60601-1-8. Un id desconocido
// devuelve ALTA a proposito: sobreestimar la urgencia es seguro.
AlarmPriority alarm_priority(AlarmId id);

// 201.15.4.2.1 aa)/bb): un corte termico auto-rearmable debe mantener la
// alarma activa hasta que una persona la resetee, aunque la temperatura ya
// haya bajado. El resto de condiciones son non-latching.
bool alarm_is_latching(AlarmId id);

// true si la norma exige desconectar la alimentacion del calefactor mientras
// la condicion este presente.
bool alarm_cuts_heater(AlarmId id);

#ifdef __cplusplus
}
#endif
