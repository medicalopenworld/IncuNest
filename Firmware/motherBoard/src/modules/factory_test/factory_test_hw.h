#pragma once
// Frontera interna entre la tarea FTEST (factory_test_task.cpp) y los
// cuerpos de los 28 tests de la motherBoard (factory_test_hw.cpp). Nada de
// esto es publico fuera del modulo factory_test.
#include <stdint.h>

#include "factory_test_api.h"

// Resultado de un test que otro test posterior necesita conocer para la
// cascada de SKIP de D10/mb-factory-test (SB_* si ENV_SENSOR no paso por el
// camino USB, GSM_SIGNAL/GSM_NET si GSM_SIM fallo, FAN_RPM si ACTUATORS
// fallo).
//
// Tri-estado y no bool: UNKNOWN distingue "esta dependencia no se ha
// ejecutado en esta tanda" (RUN de un solo test, p.ej. RUN,9 sin haber
// corrido antes ENV_SENSOR) de "se ejecuto y fallo". Con un bool a secas,
// UNKNOWN y FAILED serian el mismo valor y un RUN aislado de un test
// dependiente saltaria a SKIP sin motivo real.
typedef enum {
  FTEST_DEP_UNKNOWN = 0,
  FTEST_DEP_OK,
  FTEST_DEP_FAILED,
} FtestDepState;

typedef struct {
  // OK solo si ENV_SENSOR (id 6) paso por el camino USB (SensorBoard
  // enlazada); FAILED tanto si paso por I2C2/SHT4x como si no paso en
  // absoluto -- en los tres casos los tests SB_* (status/env/door/light/
  // camera) no tienen de donde leer y SKIP con detail "sin usb"
  // (shared-factory-test-bench, bench2).
  FtestDepState sb_usb;
  FtestDepState actuators;
  FtestDepState gsm_sim;
} FtestCascade;

// Ejecuta el cuerpo ACTIVO del test `id` (0..FTEST_MB_COUNT-1) hasta que
// resuelve (bucles internos, bloqueante para el runner). Escribe una cadena
// terminada en NUL en detail (cadena vacia si no hay nada que informar;
// FTEST_DETAIL_MAX+1 bytes de capacidad, el codec ya sanea y trunca). id
// fuera de la tabla de motherBoard -> FTEST_FAIL con detail="id"; id pasivo
// (defensivo, no deberia llegar aqui: el runner los sondea con
// ftest_hw_poll_passive()) -> FTEST_FAIL con detail="passive".
FtestStatus ftest_hw_run(unsigned id, char detail[FTEST_DETAIL_MAX + 1],
                         FtestCascade *cascade);

// ---- Solape cooperativo de los PASIVOS (banco 2026-09-06, quinta ronda) ----
// Los pasivos (charger, env_sensor, sb_status/sb_camera, gsm_*, wifi,
// tb_provision, time y los instantaneos sysinfo/ina3221/skin_adc/hmi_link/
// nvs/littlefs/power_src/humid_usb/sb_door/afe_probe) solo OBSERVAN estado ya
// cacheado por otra tarea o una peticion asincrona ya en vuelo: no tienen
// autoridad ninguna sobre el hardware que otro pasivo o un activo no tenga
// tambien, asi que pueden correr todos en paralelo desde el arranque de la
// bateria sin el riesgo de entrelazado que si tienen los ACTIVOS
// (actuadores/zumbador/calefactor en lazo abierto). El runner
// (factory_test_task.cpp) los sondea con ftest_hw_poll_passive() en pasos de
// <= 250 ms; cada uno lleva su propio plazo (elapsed_ms lo mide desde que
// arranca la bateria, comun a todos) y NUNCA bloquea ni hace su propio
// vTaskDelay.
#define FTEST_ACTIVE_COUNT 7
#define FTEST_PASSIVE_COUNT 21

// true si `id` es un pasivo (fila de la tabla con passive=true). false para
// cualquier id que no sea de motherBoard.
bool ftest_id_is_passive(unsigned id);

// Id del activo/pasivo en la posicion `idx` (< FTEST_ACTIVE_COUNT /
// FTEST_PASSIVE_COUNT) de su orden de ejecucion. El activo NO sigue el orden
// ascendente de id: sb_env/sb_light van al final para dar tiempo a que
// env_sensor (pasivo) resuelva la cascada `sb_usb` antes de que les toque el
// turno. FTEST_ID_NONE si idx esta fuera de rango.
unsigned ftest_hw_active_id_at(unsigned idx);
unsigned ftest_hw_passive_id_at(unsigned idx);

// Sondea un pasivo sin bloquear: FTEST_RUNNING mientras su condicion no se
// cumpla y no se haya agotado `elapsed_ms` (comun a todos los pasivos, no
// propio de este); un estado final en otro caso. `id` fuera de la tabla de
// motherBoard o de un activo (defensivo) -> FTEST_FAIL con detail="id"/
// "active".
FtestStatus ftest_hw_poll_passive(unsigned id, char detail[FTEST_DETAIL_MAX + 1],
                                   FtestCascade *cascade, uint32_t elapsed_ms);

// true si `id` depende de la cascada de otro pasivo que TODAVIA no se ha
// resuelto (sb_status/sb_camera de env_sensor via cascade->sb_usb;
// gsm_signal/gsm_net de gsm_sim via cascade->gsm_sim): el runner NO debe
// llamar a ftest_hw_poll_passive() para ese id todavia (se quedaria en
// FTEST_SKIP prematuro, confundiendo "no ha corrido" con "ya fallo"). Una vez
// la dependencia resuelve (OK o FAILED), esta funcion devuelve false y el
// propio cuerpo decide SKIP o seguir segun el valor de la cascada, igual que
// antes.
bool ftest_hw_passive_dependency_pending(unsigned id, const FtestCascade *cascade);

// Reinicia el estado propio de los pasivos que disparan una unica peticion
// asincrona (sb_status/sb_camera: `sensorboard_status_request()`/
// `sensorboard_capture_request()`, una sola vez por resolucion, no en cada
// sondeo). SHALL llamarse al arrancar cada bateria/RUN, antes del primer
// sondeo.
void ftest_hw_reset_passive_state(void);

// ---- Primitivas que factory_test_task.cpp ofrece a los cuerpos de test ----
// (implementadas alli: son las que conocen la cola TX y el flag de ABORT).

// Encola "CTRL,FTEST,id,st,detail" (design.md D2/D3). Usada por los cuerpos
// para RUNNING (ya la emite la tarea antes de llamar a ftest_hw_run(), no
// hace falta duplicarlo) y para el estado intermedio WAIT.
void ftest_emit(unsigned id, FtestStatus st, const char *detail);

// true si HMI,FTEST,ABORT ha llegado desde el ultimo factoryTestStart() /
// factoryTestRunSingle(). Los cuerpos de test la consultan dentro de
// CUALQUIER bucle de espera, en pasos de <= 250 ms (design.md D5).
bool ftest_abort_requested(void);

// NOTA (cuarta ronda, banco 2026-09-06): ftest_arm_confirm()/ftest_wait_
// confirm() se retiraron de aqui y de factory_test_task.cpp junto con su
// semaforo -- BUZZER (unico llamante) ya no usa el camino CONFIRM (ver su
// cuerpo en factory_test_hw.cpp). El comando HMI,FTEST,CONFIRM lo sigue
// aceptando el parser (CommTask.cpp) pero se descarta con log "sin uso".

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

// Solape cooperativo (banco 2026-09-06, quinta ronda): alimenta el WDT
// (igual que ftest_wdt_feed()) y ademas hace avanzar un paso a los pasivos
// que sigan pendientes (charger, env_sensor, gsm_*, wifi...) mientras un
// ACTIVO (buzzer, sb_light, sb_env) ocupa su turno en un bucle de espera de
// <= 250 ms propio -- sin esto los pasivos solo progresarian entre un activo
// y el siguiente, no MIENTRAS uno de ellos lleva varios segundos corriendo.
// Los cuerpos ACTIVOS la llaman en los mismos sitios donde antes llamaban a
// ftest_wdt_feed(); fuera de una bateria completa (RUN de un solo id) es un
// no-op salvo la alimentacion del WDT.
void ftest_yield(void);

// true si hay una bateria completa en curso con los pasivos corriendo en
// paralelo (poblados en factory_test_task.cpp al arrancar la bateria).
// SB_ENV la usa para distinguir "ENV_SENSOR esta corriendo en paralelo y
// puede resolver de un momento a otro" (bateria completa: espera) de "esto es
// un RUN,8 aislado, nadie mas va a tocar la cascada sb_usb" (RUN unico:
// SKIP inmediato "sin usb", igual que antes de esta ronda).
bool ftest_passives_running(void);
