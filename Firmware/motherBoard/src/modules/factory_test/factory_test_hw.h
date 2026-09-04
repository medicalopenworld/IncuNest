#pragma once
// Frontera interna entre la tarea FTEST (factory_test_task.cpp) y los
// cuerpos de los 29 tests de la motherBoard (factory_test_hw.cpp). Nada de
// esto es publico fuera del modulo factory_test.
#include <stdint.h>

#include "factory_test_api.h"

// Resultado de un test que otro test posterior necesita conocer para la
// cascada de SKIP de D10/mb-factory-test (SB_* si SENSORBOARD no paso por el
// camino USB, GSM_SIGNAL/GSM_NET si GSM_SIM fallo, FAN_RPM si ACTUATORS
// fallo).
//
// Tri-estado y no bool: UNKNOWN distingue "esta dependencia no se ha
// ejecutado en esta tanda" (RUN de un solo test, p.ej. RUN,9 sin haber
// corrido antes SENSORBOARD) de "se ejecuto y fallo". Con un bool a secas,
// UNKNOWN y FAILED serian el mismo valor y un RUN aislado de un test
// dependiente saltaria a SKIP sin motivo real.
typedef enum {
  FTEST_DEP_UNKNOWN = 0,
  FTEST_DEP_OK,
  FTEST_DEP_FAILED,
} FtestDepState;

typedef struct {
  // OK solo si SENSORBOARD (id 7) paso por el camino USB (SensorBoard
  // enlazada); FAILED tanto si paso por I2C como si no paso en absoluto --
  // en los dos casos los tests SB_* (status/env/door/light/camera) no tienen
  // de donde leer y SKIP con detail "sin usb" (shared-factory-test-bench).
  FtestDepState sb_usb;
  FtestDepState actuators;
  FtestDepState gsm_sim;
} FtestCascade;

// Ejecuta el cuerpo del test `id` (0..FTEST_MB_COUNT-1). Escribe una cadena
// terminada en NUL en detail (cadena vacia si no hay nada que informar;
// FTEST_DETAIL_MAX+1 bytes de capacidad, el codec ya sanea y trunca). id
// fuera de la tabla de motherBoard -> FTEST_FAIL con detail="id".
FtestStatus ftest_hw_run(unsigned id, char detail[FTEST_DETAIL_MAX + 1],
                         FtestCascade *cascade);

// ---- Primitivas que factory_test_task.cpp ofrece a los cuerpos de test ----
// (implementadas alli: son las que conocen la cola TX, el semaforo de
// CONFIRM y el flag de ABORT).

// Encola "CTRL,FTEST,id,st,detail" (design.md D2/D3). Usada por los cuerpos
// para RUNNING (ya la emite la tarea antes de llamar a ftest_hw_run(), no
// hace falta duplicarlo) y para los estados intermedios WAIT/CONFIRM.
void ftest_emit(unsigned id, FtestStatus st, const char *detail);

// true si HMI,FTEST,ABORT ha llegado desde el ultimo factoryTestStart() /
// factoryTestRunSingle(). Los cuerpos de test la consultan dentro de
// CUALQUIER bucle de espera, en pasos de <= 250 ms (design.md D5).
bool ftest_abort_requested(void);

// Arma la espera de un CONFIRM: drena cualquier "give" residual del
// semaforo y DESPUES fija `id` como el esperado. El cuerpo del test debe
// llamarla ANTES de encolar su linea CTRL,FTEST,id,5 con ftest_emit()
// (bloqueante #8 del review de seguridad, "carrera del CONFIRM"): al reves,
// un CONFIRM que llegase justo entre encolar la linea y armar la espera se
// perderia.
void ftest_arm_confirm(unsigned id);

// Espera hasta timeout_ms la respuesta del operario a un CONFIRM. El cuerpo
// del test debe haber llamado ftest_arm_confirm(id) ANTES de emitir
// CTRL,FTEST,id,5 con ftest_emit(); esta funcion ya no arma nada, solo
// bloquea al semaforo. 1 = confirmo, 0 = nego, -1 = timeout o ABORT.
int ftest_wait_confirm(unsigned id, uint32_t timeout_ms);

// Motivo del ultimo ftest_abort_requested() == true ("abort"/"control on"/
// "hmi lost"/"max time"). Los cuerpos de test la usan para el detail del
// SKIP en vez de un "abort" fijo (bloqueantes #2/#3 del review de
// seguridad). Nunca NULL: si no hay motivo activo devuelve "abort" (no
// deberia llamarse en ese caso, pero es un valor seguro).
const char *ftest_abort_reason(void);

// Alimenta el Task WDT (esp_task_wdt_reset()) desde dentro de un cuerpo de
// test. Cada paso de <= 250 ms de un bucle de espera debe llamarla
// (bloqueante #2 del review de seguridad): sin esto, una espera de varios
// segundos entre pasos de un WHILE de sondeo dispara el panic del WDT a los
// 75 s (watchdogInit(), initHardware.cpp).
void ftest_wdt_feed(void);
