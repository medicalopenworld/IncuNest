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

bool alarm_is_latching(AlarmId id) {
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
