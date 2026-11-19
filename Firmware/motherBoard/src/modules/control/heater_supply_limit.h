#pragma once
// Tope del duty del calefactor segun la fuente de alimentacion (motherBoard V19).
//
// Hardware/Electronics/Motherboard/CHANGELOG.md, "Cambios de firmware para
// V19", punto 6: con el calefactor a 10 A y el resto a ~2 A, V_SYS llega a
// ~12 A, y la V19 tiene cuellos de botella por debajo de eso (J9 de 10 A, R59
// en 1206, pistas del calefactor de 2,54 mm). Hasta la V20:
//
//   fuente                                  duty maximo
//   adaptador de 24 V (CHRG_OK=1, VBUS>18V)  100 %
//   coche (CHRG_OK=1, VBUS<16V)              ~50 %  (12 V x 8 A ~ 96 W; por
//                                                    encima completa la bateria)
//   solo bateria (CHRG_OK=0)                 ~40 %  (eficaz de bateria <~8 A)
//
// Con CHRG_OK=1 y VBUS desconocido o en la franja 16-18 V se aplica el tope de
// coche: ante la duda, el lado que no sobrecarga J9.
//
// Logica pura (sin Arduino) para poder probarla en host: test_heater_supply_limit.

#include <stdint.h>

typedef enum {
  HEATER_SUPPLY_ADAPTER = 0,
  HEATER_SUPPLY_CAR = 1,
  HEATER_SUPPLY_BATTERY = 2,
} HeaterSupply;

#define HEATER_SUPPLY_ADAPTER_MIN_MV 18000u
#define HEATER_SUPPLY_CAR_PCT 50u
#define HEATER_SUPPLY_BATTERY_PCT 40u
// Lectura de VBUS mas vieja que esto no cuenta (el BQ se lee cada 1 s).
#define HEATER_SUPPLY_VBUS_MAX_AGE_MS 15000u

// chrg_ok: pin CHRG_OK del BQ25730 (IO42). vbus_valid: hay una lectura de VBUS
// del ADC del BQ25730 reciente. vbus_mv: esa lectura.
HeaterSupply heater_supply_classify(bool chrg_ok, bool vbus_valid,
                                    uint16_t vbus_mv);

// Duty maximo (en cuentas de 0..pwm_max) para esa fuente. Redondea hacia abajo.
uint16_t heater_supply_max_pwm(HeaterSupply supply, uint16_t pwm_max);

const char *heater_supply_name(HeaterSupply supply);
