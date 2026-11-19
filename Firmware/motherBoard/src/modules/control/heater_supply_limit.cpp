#include "heater_supply_limit.h"

HeaterSupply heater_supply_classify(bool chrg_ok, bool vbus_valid,
                                    uint16_t vbus_mv) {
  if (!chrg_ok) {
    return HEATER_SUPPLY_BATTERY;
  }
  if (vbus_valid && vbus_mv > HEATER_SUPPLY_ADAPTER_MIN_MV) {
    return HEATER_SUPPLY_ADAPTER;
  }
  return HEATER_SUPPLY_CAR;
}

uint16_t heater_supply_max_pwm(HeaterSupply supply, uint16_t pwm_max) {
  switch (supply) {
  case HEATER_SUPPLY_ADAPTER:
    return pwm_max;
  case HEATER_SUPPLY_CAR:
    return (uint16_t)((uint32_t)pwm_max * HEATER_SUPPLY_CAR_PCT / 100u);
  case HEATER_SUPPLY_BATTERY:
  default:
    return (uint16_t)((uint32_t)pwm_max * HEATER_SUPPLY_BATTERY_PCT / 100u);
  }
}

const char *heater_supply_name(HeaterSupply supply) {
  switch (supply) {
  case HEATER_SUPPLY_ADAPTER:
    return "adaptador";
  case HEATER_SUPPLY_CAR:
    return "coche";
  case HEATER_SUPPLY_BATTERY:
  default:
    return "bateria";
  }
}
