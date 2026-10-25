#include "modules/control/photo_fault.h"

void photo_fault_init(PhotoFault *f) {
  if (f == nullptr) return;
  f->low_cycles = 0;
  f->blind_cycles = 0;
  f->present = false;
}

bool photo_fault_update(PhotoFault *f, PhotoFaultInput in, float current_a) {
  if (f == nullptr) return false;

  // Sin aplicar (apagada, test de fabrica, asentamiento): no se puede afirmar
  // nada, se olvida todo Y se retira la averia. La maquina de alarmas conserva
  // `present` hasta que alguien declara false (mismo fallo que sufrio
  // ALARM_FAN_FAILURE con sus salidas tempranas), asi que apagar la fototerapia
  // con la alarma puesta la dejaria viva para siempre si aqui no se retirase.
  if (in == PHOTO_FAULT_INPUT_INACTIVE) {
    f->low_cycles = 0;
    f->blind_cycles = 0;
    f->present = false;
    return false;
  }

  // Aplica pero sin medida utilizable (INA mudo o muestra rancia). Distinto del
  // caso anterior: aqui la lampara SI esta ordenada ON, asi que no se retira
  // nada ni se toca la cuenta de corriente baja (una lectura fallida aislada no
  // puede hacer parpadear la alarma). Solo se cuentan los ciclos a ciegas: una
  // lampara que no se puede vigilar no se da por buena.
  if (in == PHOTO_FAULT_INPUT_NO_SAMPLE) {
    // Satura en el umbral: la cuenta no puede desbordar el uint8_t.
    if (f->blind_cycles < PHOTO_FAULT_BLIND_CYCLES) f->blind_cycles++;
    if (f->blind_cycles >= PHOTO_FAULT_BLIND_CYCLES) f->present = true;
    return f->present;
  }

  // Muestra nueva: rompe la racha de ceguera (tiene que ser consecutiva).
  f->blind_cycles = 0;

  if (f->present) {
    // Histeresis: se retira al ALCANZAR PHOTO_FAULT_CLEAR_A, no al superar el
    // umbral de declaracion. Entre ambos se mantiene.
    if (current_a >= PHOTO_FAULT_CLEAR_A) {
      f->present = false;
      f->low_cycles = 0;
    }
    return f->present;
  }

  if (current_a < PHOTO_FAULT_MIN_A) {
    if (f->low_cycles < PHOTO_FAULT_CYCLES) f->low_cycles++;
    if (f->low_cycles >= PHOTO_FAULT_CYCLES) f->present = true;
  } else {
    // Un ciclo bueno rompe la racha: tienen que ser consecutivos.
    f->low_cycles = 0;
  }
  return f->present;
}

bool photo_fault_present(const PhotoFault *f) {
  return f != nullptr && f->present;
}
