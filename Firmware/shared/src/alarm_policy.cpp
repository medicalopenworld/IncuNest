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
    case ALARM_SUPPLY_UNDERVOLTAGE:
    case ALARM_HMI_LINK_LOST:
      return ALARM_PRIORITY_MEDIUM;

    case ALARM_SKIN_SENSOR_FAULT_AIR_MODE:
    case ALARM_HUMIDITY_DEVIATION:
      return ALARM_PRIORITY_LOW;

    default:
      return ALARM_PRIORITY_HIGH;
  }
}

bool alarm_is_latching(AlarmId id) {
  return id == ALARM_AIR_THERMAL_CUTOUT || id == ALARM_SKIN_THERMAL_CUTOUT;
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
      return true;
    default:
      return false;
  }
}
