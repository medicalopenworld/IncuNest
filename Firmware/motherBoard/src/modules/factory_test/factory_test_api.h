#pragma once
// API del test de fabrica para CommTask.cpp (design.md D4/D5,
// shared-factory-test, tasks.md 5.1).
//
// Nombrado factory_test_api.h y NO factory_test.h a proposito: el contrato
// compartido de la bateria (IDs, estados, codec de linea) ya vive en
// shared/include/factory_test.h, y ambos ficheros se resuelven con
// `#include "factory_test.h"` (quoted). El include quoted busca PRIMERO en
// el directorio del fichero que lo escribe: si este header se hubiera
// llamado igual, su propio intento de incluir la tabla compartida se habria
// resuelto contra si mismo (con #pragma once, en silencio: ese `#include` no
// habria traido ningun tipo). Un nombre distinto evita la colision sin tocar
// el contrato compartido.
#include "factory_test.h"  // shared/: FtestId, FtestStatus, FtestReject...

// Umbrales del test de fabrica (design.md D10 / mb-factory-test).
#define FTEST_SB_SPREAD_MAX_C 1.0f
#define FTEST_SB_VS_EXT_MAX_C 3.0f
#define FTEST_BUZZER_DBA_DELTA 6.0f
#define FTEST_HUMID_MIN_MA 20.0f
#define FTEST_HEAP_MIN_BYTES (40u * 1024u)
// Plazo de los tests de conectividad opcionales (gsm_net/wifi/tb_provision/
// time): fabrica puede no tener cobertura celular ni AP en la nave, asi que
// agotarlo es un AVISO (FTEST_WARN), no un FAIL (shared-factory-test-bench).
#define FTEST_CONN_TIMEOUT_MS 30000u
// Cota cooperativa POR TEST (distinta de FTEST_BATTERY_MAX_MS, que es para
// toda la bateria): ningun cuerpo individual tiene un plazo propio mayor que
// esto (el mas largo, gsm_at, agota a los 45 s; el CONFIRM del buzzer a los
// 60 s), asi que superarlo solo puede significar un cuerpo colgado. El
// runner (factory_test_task.cpp) lo detecta reutilizando
// ftest_abort_requested()/ftest_abort_reason() -- ya consultada en TODOS los
// bucles de espera de <= 250 ms de factory_test_hw.cpp, sin tocar un solo
// cuerpo -- y traduce el motivo "timeout" a FAIL en vez de SKIP, sin abortar
// el resto de la bateria. Un cuerpo bloqueado en una llamada I2C/USB NO
// cooperativa (sin bucle de sondeo que consulte nada) no lo cubre esta cota:
// solo lo cubre el Task WDT (75 s, watchdogInit()), que reinicia la placa.
// Por eso los cuerpos de esta bateria no hacen I2C directo salvo dentro de
// actuatorsTest()/testStandByCurrent() (ver cabecera de factory_test_hw.cpp).
#define FTEST_TEST_TIMEOUT_MS 90000u

// Arranca la bateria completa (HMI,FTEST,START). false si ya hay una tarea
// FTEST en marcha (no debería llegar aqui: factoryTestPrecheck() ya lo
// filtra en parse_line(), esto es un cinturon adicional).
bool factoryTestStart(void);

// Arranca un unico test (HMI,FTEST,RUN,<id>), mismo estado seguro que la
// bateria completa. false si id no es de motherBoard o ya hay una tarea en
// marcha.
bool factoryTestRunSingle(unsigned id);

// Pide que la tarea en marcha aborte. No bloquea: el test en curso se corta
// en su siguiente punto de comprobacion (bucles de espera en pasos de
// <= 250 ms) y termina en SKIP con detail="abort".
void factoryTestAbort(void);

// Respuesta del operario a un CTRL,FTEST,id,5 (CONFIRM). Un id que no
// coincide con el CONFIRM que la tarea esta esperando se descarta con log.
void factoryTestConfirm(unsigned id, bool ok);

bool factoryTestRunning(void);

// Comprobacion previa a crear la tarea (design.md D4, "Precondicion dura").
// Devuelve -1 si esta permitido; en otro caso, el FtestReject a mandar por
// CTRL,FTEST_REJECT. `id` es FTEST_ID_NONE para START/ABORT/CONFIRM (donde
// "id conocido" no aplica); para RUN es el id pedido por el HMI.
//
// int y no FtestReject de vuelta: FtestReject (shared/) no tiene un valor
// que signifique "permitido", y anadir uno ahi mezclaria el contrato de
// protocolo (lo que SI viaja por el cable) con el resultado de esta
// comprobacion local (que nunca viaja). Mismo criterio que ya usa
// shared/factory_test.h para sus propios parametros `id` (unsigned, no
// FtestId): ver el comentario de ftest_id_is_mb() ahi.
int factoryTestPrecheck(unsigned id);
