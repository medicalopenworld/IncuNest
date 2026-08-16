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

// true si el operador puede inactivar el audio de esta condicion.
//
// Solo hay una excepcion, y es ALARM_MAINS_INTERRUPTION: 60601-2-19
// 201.12.3.103 exige que el aviso de corte de red se mantenga un MINIMO de
// 10 min, y la pausa de audio dura exactamente eso
// (ALARM_AUDIO_PAUSE_MS). Silenciarla no se comeria parte de la duracion
// obligatoria: se la comeria entera.
//
// 6.8.1 lo permite: dice "Means MAY be provided" para inactivar senales, y no
// obliga a ofrecerlo condicion por condicion. La senal visual, en cambio, no
// se inactiva nunca (eso si es obligatorio).
bool alarm_is_silenceable(AlarmId id);

// true si la norma exige desconectar la alimentacion del calefactor mientras
// la condicion este presente.
bool alarm_cuts_heater(AlarmId id);

// Limites de corte termico. 201.15.4.2.1 aa): el corte por aire no puede
// exceder 38 C. bb): el de piel no puede exceder 40 C. El suelo de 34 C sale
// del rango auto-rearmable que la misma clausula admite (34-39 C).
#define ALARM_AIR_CUTOUT_MAX_C  38.0f
#define ALARM_SKIN_CUTOUT_MAX_C 40.0f
#define ALARM_CUTOUT_MIN_C      34.0f

float alarm_clamp_air_cutout(float celsius);
float alarm_clamp_skin_cutout(float celsius);

#ifdef __cplusplus
}
#endif
