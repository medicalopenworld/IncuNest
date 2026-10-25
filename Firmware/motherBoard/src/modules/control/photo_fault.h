#pragma once

// Decision de ALARM_PHOTO_LAMP_FAULT, separada del hardware.
//
// Con la fototerapia ordenada ON, la lampara tiene que consumir corriente. El
// lazo de intensidad (sensors_module.cpp, currentMonitor) la regula hacia
// PHOTO_TARGET_CURRENT (0,45 A) y la semilla minima posible, PHOTO_MIN_PWM = 10,
// ya da una corriente medible: en banco (2026-09-23) una lectura al 10 % de PWM
// fue 0,10 A. Si la medida cae por debajo de PHOTO_FAULT_MIN_A la lampara NO
// RESPONDE a la orden (LED abierto, conector, driver), no es que este
// degradada: por eso el umbral es bajo y no se parece al objetivo.
//
// Hasta ahora nadie lo comprobaba: el equipo seguia anunciando una sesion de
// fototerapia que no iluminaba a nadie. No es un riesgo para el neonato sino una
// terapia que no se esta dando, asi que el aviso es de prioridad BAJA, no
// latching y no corta nada.
//
// Esta es la logica sin nada que dependa de una placa (contador de ciclos,
// umbral e histeresis), para que se pueda probar en host
// (motherBoard/test/test_photo_fault). El llamador decide cuando la medida es
// valida (`evaluable`) y la cadencia: una llamada por ciclo del lazo, que es
// PHOTO_CONTROL_PERIOD_MS = 1 s.
//
// Antirrebote e histeresis porque asi nace toda condicion nueva (known_issues
// #1, "Phantom Alarms"): se declara tras PHOTO_FAULT_CYCLES ciclos CONSECUTIVOS
// por debajo de PHOTO_FAULT_MIN_A, y no se retira hasta PHOTO_FAULT_CLEAR_A.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Por debajo de esto la lampara no esta respondiendo (A).
#define PHOTO_FAULT_MIN_A 0.05f
// Hay que alcanzar esto para retirar la averia (A). Mayor que MIN para que una
// corriente rondando el umbral no haga parpadear la alarma.
#define PHOTO_FAULT_CLEAR_A 0.08f
// Ciclos consecutivos de 1 s por debajo de PHOTO_FAULT_MIN_A para declararla.
#define PHOTO_FAULT_CYCLES 5u

typedef struct {
  uint8_t low_cycles;  // ciclos consecutivos por debajo de PHOTO_FAULT_MIN_A
  bool present;        // la averia esta declarada
} PhotoFault;

void photo_fault_init(PhotoFault *f);

// Un ciclo del lazo. `evaluable` es verdadero solo si la medida vale: la
// fototerapia esta ordenada ON, ha pasado PHOTO_SETTLE_MS, el sensor de
// corriente contesta y no hay un test de fabrica en curso. Si no lo es, se
// reinicia la cuenta y se RETIRA la averia: no se puede afirmar que la lampara
// falle. Devuelve si la condicion esta presente tras este ciclo.
bool photo_fault_update(PhotoFault *f, bool evaluable, float current_a);

bool photo_fault_present(const PhotoFault *f);

#ifdef __cplusplus
}
#endif
