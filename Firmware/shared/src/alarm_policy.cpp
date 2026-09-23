#include "alarm_policy.h"

AlarmPriority alarm_priority(AlarmId id) {
  switch (id) {
    case ALARM_AIR_THERMAL_CUTOUT:
    case ALARM_SKIN_THERMAL_CUTOUT:
    case ALARM_AIR_SENSOR_FAULT:
    case ALARM_SKIN_SENSOR_FAULT_SKIN_MODE:
    case ALARM_FAN_FAILURE:
    case ALARM_AIR_OUTLET_BLOCKED:
    case ALARM_MAINS_INTERRUPTION:
      return ALARM_PRIORITY_HIGH;

    case ALARM_AIR_TEMP_DEVIATION_HIGH:
    case ALARM_AIR_TEMP_DEVIATION_LOW:
    case ALARM_SKIN_TEMP_DEVIATION_HIGH:
    case ALARM_SKIN_TEMP_DEVIATION_LOW:
    case ALARM_HEATER_FAULT:
    case ALARM_HEATER_SENSOR_FAULT:
    case ALARM_SUPPLY_UNDERVOLTAGE:
    case ALARM_HMI_LINK_LOST:
    case ALARM_SENSORBOARD_LINK_LOST:
      return ALARM_PRIORITY_MEDIUM;

    case ALARM_SKIN_SENSOR_FAULT_AIR_MODE:
    case ALARM_HUMIDITY_DEVIATION:
    case ALARM_SENSORBOARD_DOOR_FAULT:
      return ALARM_PRIORITY_LOW;

    default:
      return ALARM_PRIORITY_HIGH;
  }
}

// QUE ALARMA OBLIGA A INTERVENIR ANTES DE SEGUIR.
//
// Una alarma latching no se retira aunque su causa desaparezca: hace falta un
// acto humano. Eso solo esta justificado cuando lo que hay que revisar NO se
// puede comprobar con el equipo funcionando.
//
// **Los cortes termicos NO son latching** (decision del responsable del
// producto, 2026-09-11). Al bajar la temperatura, el equipo vuelve solo a
// regulacion normal y el aviso se retira con ella. Antes si lo eran, apoyandose
// en que 201.15.4.2.1 aa)/bb) pide que un corte auto-rearmable "opere
// continuamente hasta reset manual" — pero el episodio NO se pierde por
// retirarlo: `publishAlarmChanges()` lo escribe en el registro persistido de
// alarmas (6.12.2, `alarm_history_record_raise`), que el operador consulta en
// el centro de alarmas. La constancia la da el registro; obligar ademas a
// reconocer un aviso cuya causa ya no existe solo anade una alarma atascada.
//
// **ALARM_HEATER_FAULT si lo es**, y es la unica. La declara UNICAMENTE el
// autotest de arranque (`initHardware.cpp`) cuando la corriente del calefactor
// se sale de rango, que es la firma de un calefactor mal cableado. Revisar ese
// cableado exige el equipo APAGADO, asi que la instruccion correcta para el
// operador es apagar, revisar y volver a encender — y el arranque siguiente
// vuelve a medir. Su condicion no se retira nunca mientras el equipo sigue
// encendido, asi que el reset manual la rechaza por si solo: no hay forma de
// hacerla desaparecer con un boton, que es justo lo que se quiere.
bool alarm_is_latching(AlarmId id) {
  return id == ALARM_HEATER_FAULT;
}

// ANUNCIO INMEDIATO: sin retardo, pase lo que pase.
//
// Es una politica DISTINTA de alarm_is_latching(), aunque durante un tiempo
// fueran la misma funcion. La maquina preguntaba "es latching?" para decidir
// tambien si podia esperar el retardo de anuncio, y colaba porque las unicas
// latching eran los cortes termicos, que son justo las que no pueden esperar.
// Al dejar de ser latching (2026-09-11) ese atajo habria RETRASADO el aviso de
// un corte termico, que es una regresion de seguridad y no un efecto
// secundario aceptable. Lo cazo test_thermal_cutout_ignores_any_delay.
//
// Un corte termico es la ultima barrera antes del dano: cuando salta, la
// temperatura YA se ha ido de rango con el calefactor cortado. No hay nada que
// confirmar esperando, asi que no espera.
bool alarm_announces_immediately(AlarmId id) {
  return id == ALARM_AIR_THERMAL_CUTOUT || id == ALARM_SKIN_THERMAL_CUTOUT;
}

bool alarm_is_silenceable(AlarmId id) {
  return id != ALARM_MAINS_INTERRUPTION;
}

bool alarm_cuts_heater(AlarmId id) {
  switch (id) {
    case ALARM_AIR_THERMAL_CUTOUT:
    case ALARM_SKIN_THERMAL_CUTOUT:
    case ALARM_AIR_SENSOR_FAULT:
    case ALARM_SKIN_SENSOR_FAULT_SKIN_MODE:
    case ALARM_FAN_FAILURE:
    case ALARM_AIR_OUTLET_BLOCKED:
    case ALARM_AIR_TEMP_DEVIATION_HIGH:
    case ALARM_SKIN_TEMP_DEVIATION_HIGH:
    case ALARM_HEATER_FAULT:
    // Corta igual que ALARM_HEATER_FAULT, del que se separo. Sin el sensor de
    // corriente no hay forma de saber lo que consume el calefactor, y dejarlo
    // calentando sin vigilancia seria relajar la seguridad: la separacion
    // sirve para decirle al operador QUE revisar, no para actuar distinto.
    case ALARM_HEATER_SENSOR_FAULT:
      return true;
    default:
      return false;
  }
}

static float clamp_range(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

float alarm_clamp_air_cutout(float celsius) {
  return clamp_range(celsius, ALARM_CUTOUT_MIN_C, ALARM_AIR_CUTOUT_MAX_C);
}

float alarm_clamp_skin_cutout(float celsius) {
  return clamp_range(celsius, ALARM_CUTOUT_MIN_C, ALARM_SKIN_CUTOUT_MAX_C);
}
