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

// true si la alarma se anuncia SIN ESPERAR su retardo. Politica separada del
// enclavamiento: ver el comentario en alarm_policy.cpp.
bool alarm_announces_immediately(AlarmId id);

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

// Duracion del AUDIO PAUSED que pide el operador con el boton de silencio del
// display HMI (unica interaccion de operador que existe: el encoder fisico es
// de una revision de hardware anterior y ya no se monta).
//
// Vive en shared/ desde que el display tiene senal acustica propia para la
// perdida de enlace: esa pausa la lleva EL, porque con el enlace caido el
// boton no llega a la placa. Un 600000u duplicado a los dos lados es una
// divergencia esperando a ocurrir, y aqui ademas seria una divergencia
// declarada en las instrucciones de uso (6.8.5).
//
// La clausula aplicable es 60601-2-19 201.12.3.104, NO 60601-1-8 6.8.3: 6.8.3
// trata de los estados globales INDEFINIDOS (ALARM OFF / AUDIO OFF) y no fija
// duracion alguna. 201.12.3.104 exige que las alarmas silenciadas
// deliberadamente "reanuden automaticamente su funcion normal dentro de un
// tiempo especificado POR EL FABRICANTE" - el limite no lo pone la norma, lo
// ponemos nosotros.
//
// VALOR ACTUAL: 10 min. 6.8.5 obliga a declararlo en las instrucciones de
// uso; esta en docs/alarms.md.
//
// Consecuencia que hay que tener presente: 201.12.3.103 exige que el aviso de
// interrupcion de alimentacion se mantenga un minimo de 10 min, justo lo que
// dura esta pausa. Silenciar esa alarma se come practicamente toda su
// duracion obligatoria. Se acepta porque 6.8.4 permite al operador terminar
// el silencio cuando quiera y la senal VISUAL nunca se inactiva (6.8.1), pero
// si el analisis de riesgos lo revisa, el candidato natural es excluir
// ALARM_MAINS_INTERRUPTION del silencio, no acortar esto.
//
// La excepcion de hasta 30 min de 201.12.3.104 es solo para el calentamiento
// desde COLD CONDITION, y se gestiona aparte con el retardo de anuncio
// (alarm_machine_set_announce_delay), no alargando esta pausa.
#define ALARM_AUDIO_PAUSE_MS 600000u

// Limites de corte termico. 201.15.4.2.1 aa): el corte por aire no puede
// exceder 38 C. bb): el de piel no puede exceder 40 C. El suelo de 34 C sale
// del rango auto-rearmable que la misma clausula admite (34-39 C).
//
// ============ DESVIACION NORMATIVA CONSCIENTE: AIRE A 40 C ============
//
// El techo del corte por aire esta en 40 C, NO en los 38 C que fija aa).
// Decision de producto de Pablo Sanchez (2026-09-14), tomada sabiendo que
// incumple, para poder subir la consigna de aire a 39 C y probarlo en banco.
// Esto NO es conforme y no se puede reclamar como tal. No lo "arregles"
// subiendo o bajando el numero sin hablarlo: el numero es lo de menos.
//
// POR QUE NO BASTA CON MOVER ESTE VALOR. La norma si contempla pasar de
// 37 C, pero no asi: pide un camino de OVERRIDE (gesto deliberado del
// clinico + indicacion permanente en pantalla) y, mientras dura, un SEGUNDO
// corte a 40 C en un canal INDEPENDIENTE del termostato. Aqui no hay
// segundo corte: hay uno solo, y encima lee ROOM_DIGITAL_TEMP_SENSOR, el
// mismo sensor del que come el PID. Un sensor pegado se lleva por delante el
// termostato y su propio corte a la vez (docs/alarms_normative_analysis.md
// section 2.4). Subir el techo de 38 a 40 ENSANCHA esa ventana: con el
// sensor averiado, el aire puede llegar 2 C mas arriba antes de que salte
// nada. Ese es el precio que se esta pagando, y hay que tenerlo delante.
//
// EL CAMINO BUENO ESTA ANALIZADO Y A MEDIO CAMINO. Es el cambio openspec
// mb-air-overtemp-override. Y el canal independiente que ese analisis da por
// inexistente SI EXISTE a medias: el SHTC3 es un chip distinto del STS35 que
// alimenta el PID, ya se lee y ya se criba (sensors_module.cpp), pero hoy
// solo se publica como telemetria Air_temp_redundant y no dispara nada.
// Enganchar el corte de override a ESE sensor es el siguiente paso real.
// Dos limites conocidos: en unidades con SensorBoard no se rellena (alli se
// funden tres lecturas con mediana) y si el STS35 falla el SHTC3 pasa a ser
// el primario, con lo que la redundancia desaparece.
#define ALARM_AIR_CUTOUT_MAX_C  40.0f
#define ALARM_SKIN_CUTOUT_MAX_C 40.0f
#define ALARM_CUTOUT_MIN_C      34.0f

// Tope de consigna de aire. Vive aqui, y no en el main.h de cada placa, porque
// las dos lo necesitan y hasta 2026-09-14 lo llevaban duplicado a mano: el
// display topaba en 38.5 y la placa en 38. Se habian desincronizado, y el
// sintoma era mudo — se podia marcar 38.5 en pantalla y, si la unidad
// reiniciaba por WDT, recapVariables() veia 38.5 > 38, lo daba por corrupto y
// devolvia la consigna al preset de 32 C.
//
// INVARIANTE: tiene que ser MENOR que el umbral del corte termico. Si se
// cruzan, el equipo no puede alcanzar lo que se le pide sin disparar
// ALARM_AIR_THERMAL_CUTOUT, que es ALTA y corta el calefactor. Lo comprueba
// test_alarm_policy.
#define ALARM_AIR_SETPOINT_MAX_C 39.0f

float alarm_clamp_air_cutout(float celsius);
float alarm_clamp_skin_cutout(float celsius);

#ifdef __cplusplus
}
#endif
