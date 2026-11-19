#pragma once
// Politica de carga de la bateria segun VBAT (motherBoard V19).
//
// Hardware/Electronics/Motherboard/CHANGELOG.md, "Cambios de firmware para
// V19", punto 5: "Carga solo con VBAT valida (10,5-15 V)". Se aplica asi:
//
//  - VBAT > 15,0 V: INHIBIR la carga (CHRG_INHIBIT). V_SYS alimenta los
//    TPS563231 (17 V maximo) y el gate de Q8 tiene un zener de 15 V. Se libera
//    por debajo de 14,7 V.
//  - VBAT < 10,5 V: carga REDUCIDA (CHG_POLICY_REDUCED_MA), no inhibida. Una
//    AGM muy descargada tiene que poder recuperarse, y en campo es justo
//    cuando mas falta hace. Vuelve a normal por encima de 11,0 V.
//
// CELL_BATPRESZ esta fijo en la V19 (R64/R7): el BQ siempre cree que hay
// bateria, y VBAT no permite distinguir "sin bateria". Eso queda para la V20.
//
// Una lectura no valida no cambia el modo. Logica pura: test_charge_policy.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  CHG_POLICY_NORMAL = 0,
  CHG_POLICY_REDUCED = 1,
  CHG_POLICY_INHIBIT = 2,
} ChargePolicyMode;

#define CHG_POLICY_VBAT_MAX_MV 15000u
#define CHG_POLICY_VBAT_MAX_RELEASE_MV 14700u
#define CHG_POLICY_VBAT_LOW_MV 10500u
#define CHG_POLICY_VBAT_LOW_RELEASE_MV 11000u
// Corriente de carga reducida (multiplo de 128 mA, paso del BQ con 5 mOhm).
#define CHG_POLICY_REDUCED_MA 512u

typedef struct {
  ChargePolicyMode mode;
} ChargePolicy;

void charge_policy_init(ChargePolicy *p);
ChargePolicyMode charge_policy_update(ChargePolicy *p, bool vbat_valid,
                                      uint16_t vbat_mv);
const char *charge_policy_name(ChargePolicyMode m);

#ifdef __cplusplus
}
#endif
