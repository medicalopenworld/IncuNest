#pragma once

// Decision de ALARM_FAN_FAILURE, separada del hardware.
//
// Vivia entera dentro de checkFanSpeed() (security.cpp), que no entra en los
// tests de host porque el fichero toca GPIO, PWM e I2C. El resultado fue que su
// maquina de estados —tres salidas tempranas, una histeresis con memoria y una
// gracia de arranque— no la ejercitaba nadie hasta tener la incubadora
// delante. Y llevaba dos fallos que solo se vieron en banco:
//
//   1. Se realimentaba: declarar la alarma cortaba la alimentacion del
//      ventilador, con lo que las rpm eran 0 por construccion y la condicion no
//      se podia retirar jamas. Se arreglo sacando ALARM_FAN_FAILURE de
//      ongoingFanCriticalAlarm() (ver el comentario alli).
//   2. Las salidas tempranas no RETIRABAN la condicion, y la maquina de alarmas
//      conserva `present` hasta que alguien declara false: apagar la actuacion
//      con la alarma puesta la dejaba viva para siempre.
//
// Aqui esta la misma logica sin nada que dependa de una placa, para que los dos
// fallos tengan test de regresion (motherBoard/test/test_fan_guard).
//
//   3. (campo, 18.52) Declaraba con UNA sola muestra baja. Al entrar el
//      calefactor se hunde la tension de alimentacion y el ventilador pierde
//      vueltas hasta que su PID sube el duty; esa caida pasajera bastaba para
//      declarar la averia, que corta el calefactor, con lo que la tension
//      volvia, la alarma se retiraba, volvia el calefactor y otra vez: un ciclo
//      sin fin. Ahora la caida tiene que durar failure_sustain_ms seguidos.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  double min_rpm;         // por debajo -> se declara la averia
  double hysteresis_rpm;  // hay que superar min_rpm + esto para retirarla
  uint32_t spinup_grace_ms; // margen mecanico desde que se alimenta
  // Cuanto tiempo SEGUIDO tienen que estar las rpm por debajo de min_rpm para
  // declarar la averia. Cuenta solo pasada la gracia de arranque, y cualquier
  // muestra en o por encima de min_rpm (o un corte de alimentacion) lo
  // reinicia. 0 = declarar con la primera muestra baja.
  uint32_t failure_sustain_ms;
} FanGuardConfig;

typedef struct {
  bool failure_present;
  bool was_energised;
  bool have_energised_stamp;
  uint32_t energised_since_ms;
  bool have_low_stamp;   // hay una caida por debajo de min_rpm en curso
  uint32_t low_since_ms; // desde cuando, si have_low_stamp
} FanGuard;

// Que hay que declarar a la maquina de alarmas en esta pasada.
typedef enum {
  FAN_GUARD_SILENT = 0, // no declarar nada (todavia girando hasta regimen)
  FAN_GUARD_ABSENT,     // declarar la condicion AUSENTE
  FAN_GUARD_PRESENT,    // declarar la condicion PRESENTE
} FanGuardOutcome;

void fan_guard_init(FanGuard *g);

// `energised` es que el ventilador este ALIMENTADO, no solo ordenado: son
// cosas distintas en cuanto hay una puerta de alarma por medio, y medir la
// orden hacia que al volver la tension el arranque mecanico contase como averia.
//
// `has_feedback` false = unidad sin tacometro: no hay nada que afirmar nunca.
FanGuardOutcome fan_guard_update(FanGuard *g, const FanGuardConfig *cfg,
                                 bool has_feedback, bool energised, double rpm,
                                 uint32_t now_ms);

#ifdef __cplusplus
} // extern "C"
#endif
