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

// Ciclos consecutivos de 1 s CON la fototerapia ON y SIN medida utilizable
// (el INA principal no contesta, o no hay muestra nueva) para declarar la
// averia. Sin medida no se sabe si la lampara funciona, y una lampara que no se
// puede vigilar tampoco se puede dar por buena.
#define PHOTO_FAULT_BLIND_CYCLES 5u

// Que ha pasado en este ciclo respecto a la medida.
typedef enum {
  // No aplica: fototerapia apagada, test de fabrica en curso, o todavia en el
  // asentamiento. No se puede afirmar nada: se olvida todo y se RETIRA la
  // averia.
  PHOTO_FAULT_INPUT_INACTIVE = 0,
  // La fototerapia esta ON y asentada, pero no hay medida utilizable: el
  // sensor no contesta o la muestra es la misma de antes. NO retira la averia
  // ni toca la cuenta de corriente baja: una sola lectura fallida no puede
  // hacer parpadear la alarma.
  PHOTO_FAULT_INPUT_NO_SAMPLE = 1,
  // Hay una muestra nueva: `current_a` vale.
  PHOTO_FAULT_INPUT_SAMPLE = 2,
} PhotoFaultInput;

typedef struct {
  uint8_t low_cycles;    // ciclos consecutivos por debajo de PHOTO_FAULT_MIN_A
  uint8_t blind_cycles;  // ciclos consecutivos sin medida utilizable
  bool present;          // la averia esta declarada
} PhotoFault;

void photo_fault_init(PhotoFault *f);

// Un ciclo del lazo. Devuelve si la condicion esta presente tras este ciclo.
//   - INACTIVE: reinicia las dos cuentas y retira la averia.
//   - NO_SAMPLE: cuenta ciclos a ciegas; al llegar a PHOTO_FAULT_BLIND_CYCLES
//     declara la averia. Nunca la retira.
//   - SAMPLE: corta la racha de ceguera y aplica umbral e histeresis.
// `current_a` solo se mira con SAMPLE.
bool photo_fault_update(PhotoFault *f, PhotoFaultInput in, float current_a);

bool photo_fault_present(const PhotoFault *f);

#ifdef __cplusplus
}
#endif
