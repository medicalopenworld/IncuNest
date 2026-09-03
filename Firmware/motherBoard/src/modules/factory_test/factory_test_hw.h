#pragma once
// Frontera interna entre la tarea FTEST (factory_test_task.cpp) y los
// cuerpos de los 30 tests de la motherBoard (factory_test_hw.cpp). Nada de
// esto es publico fuera del modulo factory_test.
#include <stdint.h>

#include "factory_test_api.h"

// Resultado de un test que otro test posterior necesita conocer para la
// cascada de SKIP de D10/mb-factory-test (SB_* si SENSOR_SRC fallo,
// GSM_SIGNAL/GSM_NET si GSM_SIM fallo, FAN_RPM si ACTUATORS fallo).
//
// Tri-estado y no bool: UNKNOWN distingue "esta dependencia no se ha
// ejecutado en esta tanda" (RUN de un solo test, p.ej. RUN,8 sin haber
// corrido antes SENSOR_SRC) de "se ejecuto y fallo". Con un bool a secas,
// UNKNOWN y FAILED serian el mismo valor y un RUN aislado de un test
// dependiente saltaria a SKIP sin motivo real.
typedef enum {
  FTEST_DEP_UNKNOWN = 0,
  FTEST_DEP_OK,
  FTEST_DEP_FAILED,
} FtestDepState;

typedef struct {
  FtestDepState sensor_src;
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

// Espera hasta timeout_ms la respuesta del operario a un CONFIRM. El cuerpo
// del test es quien emite CTRL,FTEST,id,5 con ftest_emit() ANTES de llamar
// aqui (con su propio texto de instruccion); esta funcion solo bloquea al
// semaforo. 1 = confirmo, 0 = nego, -1 = timeout o ABORT.
int ftest_wait_confirm(unsigned id, uint32_t timeout_ms);
