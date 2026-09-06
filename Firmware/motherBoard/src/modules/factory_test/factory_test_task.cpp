// Orquestacion del test de fabrica (design.md D4/D5, shared-factory-test):
// crea la tarea FTEST bajo demanda, entra/sale de estado seguro, recorre la
// tabla de tests (factory_test_hw.cpp) o uno solo, persiste el resultado en
// NVS y ofrece a los cuerpos de test las primitivas de ABORT/emision de
// linea que necesitan sin que cada uno conozca el mecanismo real.
//
// ---- CONFIRM retirado (banco 2026-09-06, cuarta ronda) ----
// factoryTestConfirm()/ftest_arm_confirm()/ftest_wait_confirm() y el
// semaforo binario que los respaldaba se eliminaron: BUZZER (id 15), unico
// llamante, ya no pregunta al operario -- sin microfono de la SensorBoard
// SKIP directo (factory_test_hw.cpp). El comando HMI,FTEST,CONFIRM sigue
// aceptandolo el parser (CommTask.cpp parse_line) pero se descarta con log
// "sin uso": no queda ningun consumidor al que reenviarlo.
//
// ---- Estado seguro completo (respuesta al review de seguridad) ----
// Mientras `g_factoryTestActive` este a true (puesto en start_task() ANTES de
// crear la tarea, bajado por restore() al final, tambien en ABORT/fallo):
//   - PIDHandler() (PID.cpp) y turnFans() (Actuators.cpp) retornan sin tocar
//     ningun canal PWM ni ACTUATORS_EN.
//   - El keepalive `newCommand` del HMI (main.cpp) sigue actualizando
//     in3.actuation/in3.phototherapy/etc. desde la trama (eso NO se inhibe:
//     el HMI debe poder seguir mandando ordenes que se aplicaran al salir),
//     pero no vuelve a escribir HEATER_PWM_CHANNEL, el humidificador ni
//     PHOTOTHERAPY_PWM_CHANNEL.
//   - La regulacion de intensidad de fototerapia (sensors_module.cpp
//     currentMonitor()) no recalcula ni escribe PHOTOTHERAPY_PWM_CHANNEL.
//   - buzzerHandler() (Buzzer.cpp, confirmacion sonora de comandos HMI) no
//     escribe BUZZER_PWM_CHANNEL; el unico dueno del zumbador es el cuerpo
//     del test BUZZER (id 15), que lo usa directamente.
// Cotas duras de la tarea FTEST (comprobadas en `ftest_abort_requested()`,
// compuesta, evaluada en cada punto de comprobacion, nunca una sola vez):
//   - ABORT explicito del operario (`s_abortRequested`).
//   - Control encendido a mitad (`in3.actuation != ACTUATION_OFF ||
//     in3.phototherapy`): alguien reactivo el control por otra via.
//   - Dead-man del HMI: `CommunicationHost_HmiAlive(FTEST_HMI_DEADMAN_MS)`
//     en false (5 s sin una linea del display, habiendo visto alguna antes)
//     -- reinicio o cable del HMI durante la bateria.
//   - Cota temporal dura `FTEST_BATTERY_MAX_MS` (6 min) desde que arranca la
//     tarea: superada, el resto de tests SKIP con detail "max time".
// Cualquiera de los cuatro motivos hace que el resto de la bateria termine en
// SKIP (el motivo exacto va en el detail y en un logE), `restore()` corre
// SIEMPRE al final (nunca hay un `return` que se lo salte) y `CTRL,FTEST_DONE`
// se emite igual que en una bateria completa.
//
// ---- Cota por test (distinta de las cuatro de arriba) ----
// `FTEST_TEST_TIMEOUT_MS` (90 s, factory_test_api.h) es POR TEST, no de toda
// la bateria: `run_one()` refresca `s_testStartMs` al arrancar cada cuerpo, y
// si `compute_abort_reason()` devuelve "timeout" el resultado es FAIL, no
// SKIP, y la bateria CONTINUA con el siguiente test (el proximo `run_one()`
// arranca con cota fresca). Se apoya en `ftest_abort_requested()`, ya
// consultada en todos los bucles de <= 250 ms de factory_test_hw.cpp, asi que
// ningun cuerpo necesita tocarse para quedar cubierto. Un cuerpo bloqueado en
// una llamada I2C/USB NO cooperativa (sin bucle de sondeo) no lo detecta
// esta cota -- solo lo cubre el Task WDT global (75 s, `watchdogInit()`),
// que reinicia la placa; es exactamente el motivo por el que ningun cuerpo
// de este modulo hace I2C directo salvo dentro de
// `actuatorsTest()`/`testStandByCurrent()` (factory_test_hw.cpp).
// La tarea se suscribe al Task WDT (`esp_task_wdt_add`) al entrar y se
// desuscribe (`esp_task_wdt_delete`) antes de `vTaskDelete`; se alimenta en
// cada iteracion del bucle de tests y en cada paso de <= 250 ms de todos los
// bucles de espera (factory_test_hw.cpp usa `ftest_wdt_feed()` para eso).
// Reconciliacion al terminar: `restore()` deja los PWM a 0 y baja
// `g_factoryTestActive`, pero NO toca `in3.phototherapy`/`in3.actuation` (son
// del operario, no del test). Es el SIGUIENTE `newCommand` del HMI el que, ya
// con `g_factoryTestActive` en false, vuelve a escribir
// PHOTOTHERAPY_PWM_CHANNEL/HEATER_PWM_CHANNEL/humidificador segun el estado
// real de `in3.*` -- el display reenvia su ultimo comando en cada ciclo, asi
// que esa reconciliacion llega sola en <= 1 periodo de trama, sin que este
// modulo tenga que conocer el keepalive del HMI.
//
// ---- Solape cooperativo de los PASIVOS (banco 2026-09-06, quinta ronda) ----
// La bateria en fabrica sin cobertura celular ni AP tardaba 4-5 min porque
// cada test de conectividad agotaba su plazo secuencialmente uno tras otro
// (gsm_at 45s + gsm_sim 15s + gsm_signal 15s + gsm_net/wifi/tb_provision/time
// 30s c/u). Los PASIVOS (factory_test_hw.h/.cpp: charger, env_sensor,
// sb_status, sb_camera, gsm_*, wifi, tb_provision, time y los instantaneos)
// arrancan TODOS a la vez, en RUNNING, nada mas entrar en estado seguro, con
// un cronometro comun (s_passiveT0); poll_passives() los sondea sin bloquear
// (ftest_hw_poll_passive(), <= 250 ms de granularidad) desde tres sitios:
//   - el propio bucle de los ACTIVOS (entre cada uno y el siguiente),
//   - ftest_yield() (factory_test_hw.h), llamada desde DENTRO de los bucles
//     de espera de buzzer/sb_light/sb_env en vez de (o ademas de)
//     ftest_wdt_feed(), para que los pasivos avancen tambien MIENTRAS un
//     activo ocupa su turno varios segundos,
//   - un bucle final tras el ultimo activo, hasta que no quede ninguno
//     pendiente (todos tienen plazo propio, asi que siempre termina).
// El peor caso pasa de 4-5 min a ~45 s (el plazo mas largo, gsm_at). Un
// ABORT/dead-man del HMI/control reactivado/tope de bateria fuerza SKIP
// inmediato de los pasivos que sigan pendientes, igual que al activo en
// curso -- pero NO la cota por test (90 s) de un activo concreto: esa es
// suya, no de los pasivos, que ya tienen su propio plazo (<=45 s, siempre
// menor). Ver PROTOCOL.md, seccion FTEST, "Orden de ejecucion".
//
// Sin entorno de test (motherBoard/[env:native] solo cubre
// modules/control/{alarm_machine,pid_wrapper}.cpp y la logica pura de este
// modulo, ya cubierta en ftest_summary.cpp/test_factory_test): verificacion
// manual en banco documentada en el commit.
#include <Preferences.h>
#include <cstdio>
#include <cstring>
#include <time.h>

#include "esp_task_wdt.h"

#include "CommTask.h"
#include "factory_test_api.h"
#include "factory_test_hw.h"
#include "ftest_summary.h"
#include "main.h"
#include "modules/sensorboard_comm/sensorboard_comm.h"
#include "preferences_keys.h"
#include "system/hw_selftest.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern IncuNest_parameters in3;
extern MAM_IncuNest_Humidifier in3_hum;
extern PID fanControlPID;

#define FTEST_TASK_STACK_BYTES 8192
#define FTEST_TASK_PRIORITY 3

// Dead-man del HMI (bloqueante #3 del review de seguridad): si ya se ha visto
// alguna linea del display y pasa esto sin ver otra, se asume reinicio o
// cable y se aborta la bateria en curso.
#define FTEST_HMI_DEADMAN_MS 5000u
// Cota dura de la bateria completa (bloqueante #2): a partir de aqui, el
// resto de tests pendientes SKIP con detail "max time" en vez de seguir
// ejecutando hardware sin supervision humana indefinidamente.
#define FTEST_BATTERY_MAX_MS (6u * 60u * 1000u)

// ---- Estado de la tarea (un unico FTEST posible a la vez) ----
static volatile bool s_running = false;
static volatile bool s_abortRequested = false;
static bool s_paramsSingle = false;
static unsigned s_paramsSingleId = 0;
static bool s_prevAlarmsEnabled = true;
static uint32_t s_batteryStartMs = 0;
// Cota por test (FTEST_TEST_TIMEOUT_MS, factory_test_api.h): run_one() la
// refresca al arrancar CADA cuerpo, no solo al arrancar la bateria -- a
// diferencia de s_batteryStartMs, que cubre toda la bateria y persiste.
static uint32_t s_testStartMs = 0;
// Evita repetir el mismo logE en cada poll de ftest_abort_requested() (hasta
// varias veces por segundo mientras un cuerpo de test espera): se loguea una
// sola vez por bateria, en el primer poll que detecta el motivo.
static bool s_abortReasonLogged = false;

volatile bool g_factoryTestActive = false;

bool factoryTestRunning(void) { return s_running; }

int factoryTestPrecheck(unsigned id) {
  if (s_running) return FTEST_REJECT_BUSY;
  if (in3.actuation != ACTUATION_OFF || in3.phototherapy) {
    return FTEST_REJECT_CONTROL_ACTIVE;
  }
  if (id != FTEST_ID_NONE && !ftest_id_is_mb(id)) {
    return FTEST_REJECT_UNKNOWN_ID;
  }
  return -1;
}

// Las CUATRO cotas que abortan el resto de la bateria (abort explicito,
// control reactivado, HMI perdido, tope de bateria) -- SIN la cota por test
// (90 s, mas abajo). poll_passives() la usa TAL CUAL, sin pasar por
// compute_abort_reason(): un activo concreto llevando > 90 s NO debe cortar
// de golpe a los pasivos que corren en paralelo con su propio plazo (siempre
// <= 45 s, ver cabecera del fichero) -- esa cota es del activo en curso, no
// de la bateria entera.
static const char *compute_abort_reason_common(void) {
  if (s_abortRequested) return "abort";
  // Alguien reactivo el control (u otra fototerapia) por otra via mientras
  // la bateria estaba en curso: la precondicion de arranque no basta, hay
  // que seguir vigilandola (design.md D4, "un test de fabrica no debe tener
  // autoridad para apagar un control que alguien encendio" aplica tambien al
  // reves: no debe seguir corriendo con un control ya encendido).
  if (in3.actuation != ACTUATION_OFF || in3.phototherapy) return "control on";
  // El HMI dejo de hablar (reinicio o cable desconectado): sin operario
  // vigilando la pantalla no hay quien atienda un WAIT (p.ej. sb_light,
  // "tape el sensor de luz") ni quien detenga un ABORT manual si algo va
  // mal.
  if (!CommunicationHost_HmiAlive(FTEST_HMI_DEADMAN_MS)) return "hmi lost";
  // Cota temporal dura de toda la bateria.
  if (s_batteryStartMs != 0 &&
      (uint32_t)(millis() - s_batteryStartMs) > FTEST_BATTERY_MAX_MS) {
    return "max time";
  }
  return nullptr;
}

// Precondicion compuesta del bloqueante #3: evaluada en CADA punto de
// comprobacion (los cuerpos de test la consultan en pasos de <= 250 ms), no
// una sola vez al arrancar. Devuelve el motivo ("abort"/"control on"/
// "hmi lost"/"max time"/"timeout") o NULL si esta permitido seguir.
static const char *compute_abort_reason(void) {
  const char *common = compute_abort_reason_common();
  if (common != nullptr) return common;
  // Cota POR TEST (bloqueante nuevo, "cota cooperativa por test"): distinta
  // de las cuatro de arriba. run_one() refresca s_testStartMs al arrancar
  // cada cuerpo ACTIVO, asi que esto solo dispara si ESE cuerpo concreto
  // lleva mas de FTEST_TEST_TIMEOUT_MS corriendo -- run_one() traduce este
  // motivo especifico a FAIL/"timeout" (no SKIP) y NO detiene el resto de la
  // bateria, a diferencia de los motivos de compute_abort_reason_common().
  // Los PASIVOS no pasan por aqui (poll_passives() usa la version _common):
  // les basta su propio plazo, siempre bastante menor que 90 s.
  if (s_testStartMs != 0 &&
      (uint32_t)(millis() - s_testStartMs) > FTEST_TEST_TIMEOUT_MS) {
    return "timeout";
  }
  return nullptr;
}

bool ftest_abort_requested(void) {
  const char *reason = compute_abort_reason();
  if (reason != nullptr && !s_abortReasonLogged) {
    logE("[FTEST] abortando bateria: " + String(reason));
    s_abortReasonLogged = true;
  }
  return reason != nullptr;
}

const char *ftest_abort_reason(void) {
  const char *reason = compute_abort_reason();
  return (reason != nullptr) ? reason : "abort";
}

void ftest_wdt_feed(void) { esp_task_wdt_reset(); }

void factoryTestAbort(void) { s_abortRequested = true; }

// ---- Solape cooperativo de los PASIVOS (ver cabecera del fichero) ----
// Estado de la bateria completa en curso; queda todo a 0/nullptr fuera de
// ella (RUN de un solo test, o battery ya terminada) para que poll_passives()
// sea un no-op seguro si algo la llamase fuera de sitio.
struct FtestPassiveSlot {
  unsigned id;
  bool resolved;
};
static FtestPassiveSlot s_passiveSlots[FTEST_PASSIVE_COUNT];
static unsigned s_passiveSlotCount = 0;
static uint32_t s_passiveT0 = 0;
static FtestCascade *s_passiveCascade = nullptr;
static FtestSummary *s_passiveSum = nullptr;

// Emite SKIP con `reason` para todo pasivo que siga sin resolver (bloqueante
// #2/#3: mismo trato que el activo en curso cuando la bateria aborta).
// Respeta la misma pausa anti-desborde de 60 ms cada 4 lineas que la rafaga
// de resultados de los activos (mb-factory-test, "Rafaga de tests omitidos").
static void force_skip_pending_passives(const char *reason) {
  unsigned emitted = 0;
  for (unsigned i = 0; i < s_passiveSlotCount; i++) {
    if (s_passiveSlots[i].resolved) continue;
    char detail[FTEST_DETAIL_MAX + 1];
    snprintf(detail, sizeof(detail), "%s", reason);
    const unsigned id = s_passiveSlots[i].id;
    ftest_emit(id, FTEST_SKIP, detail);
    if (s_passiveSum != nullptr) ftest_summary_note(s_passiveSum, id, FTEST_SKIP);
    s_passiveSlots[i].resolved = true;
    emitted++;
    if ((emitted % 4) == 0) vTaskDelay(pdMS_TO_TICKS(60));
  }
}

// Un paso, sin bloquear: sondea todos los pasivos que sigan pendientes y
// emite el resultado final de los que se resuelvan. No-op si no hay bateria
// completa en curso (s_passiveSlotCount == 0, p.ej. durante un RUN de un solo
// test ACTIVO -- ftest_yield() sigue siendo seguro de llamar ahi, solo
// alimenta el WDT).
static void poll_passives(void) {
  if (s_passiveSlotCount == 0 || s_passiveCascade == nullptr) return;

  // Las cuatro cotas de bateria (SIN la de 90 s por test, que es del activo
  // en curso, no de estos pasivos): si dispara alguna, el resto de pasivos
  // pendientes termina en SKIP de una vez, igual que el activo en curso.
  const char *commonAbort = compute_abort_reason_common();
  if (commonAbort != nullptr) {
    force_skip_pending_passives(commonAbort);
    return;
  }

  const uint32_t elapsed = (uint32_t)(millis() - s_passiveT0);
  unsigned emittedThisSweep = 0;
  for (unsigned i = 0; i < s_passiveSlotCount; i++) {
    if (s_passiveSlots[i].resolved) continue;
    const unsigned id = s_passiveSlots[i].id;
    // sb_status/sb_camera esperan a env_sensor; gsm_signal/gsm_net esperan a
    // gsm_sim: mientras la dependencia siga UNKNOWN no se les llama todavia
    // (se quedarian en SKIP prematuro, ver factory_test_hw.h).
    if (ftest_hw_passive_dependency_pending(id, s_passiveCascade)) continue;
    char detail[FTEST_DETAIL_MAX + 1] = {0};
    const FtestStatus st =
        ftest_hw_poll_passive(id, detail, s_passiveCascade, elapsed);
    if (st == FTEST_RUNNING) continue;
    ftest_emit(id, st, detail);
    if (s_passiveSum != nullptr) ftest_summary_note(s_passiveSum, id, st);
    s_passiveSlots[i].resolved = true;
    emittedThisSweep++;
    // Varios pasivos instantaneos (sysinfo, ina3221, skin_adc...) suelen
    // resolverse en el mismo barrido: misma regla anti-desborde que la
    // rafaga inicial de RUNNING y que la de resultados de los activos.
    if ((emittedThisSweep % 4) == 0) vTaskDelay(pdMS_TO_TICKS(60));
  }
}

void ftest_yield(void) {
  ftest_wdt_feed();
  poll_passives();
}

bool ftest_passives_running(void) { return s_passiveSlotCount > 0; }

void ftest_emit(unsigned id, FtestStatus st, const char *detail) {
  char line[FTEST_TX_LINE_MAX];
  if (ftest_format_result(line, sizeof(line), id, st, detail) < 0) {
    logE("[FTEST] resultado id=" + String(id) + " no cabe en la linea");
    return;
  }
  CommunicationHost_Enqueue(line);
}

// ---- Estado seguro (design.md D4) ----
static void enter_safe_state(void) {
  s_prevAlarmsEnabled = in3.alarmsEnabled;
  in3.alarmsEnabled = false;
  // g_factoryTestActive YA esta a true aqui: start_task() lo marca antes de
  // crear esta tarea (bloqueante #9 del review de seguridad), para que no
  // haya ninguna ventana entre la creacion de la tarea y su primera
  // instruccion en la que PIDHandler()/turnFans() sigan escribiendo PWM.

  stopPID(airPID);
  stopPID(skinPID);
  stopPID(humidityPID);
  fanControlPID.SetMode(MANUAL);

  ledcWrite(HEATER_PWM_CHANNEL, 0);
  ledcWrite(PHOTOTHERAPY_PWM_CHANNEL, 0);
  ledcWrite(FAN_PWM_CHANNEL, 0);
  ledcWrite(FAN_CTL_PWM_CHANNEL, 0);
  ledcWrite(BUZZER_PWM_CHANNEL, 0);
  in3_hum.turn(OFF);
  digitalWrite(ACTUATORS_EN, LOW);
}

// Se llama SIEMPRE al terminar la tarea (fin de bateria, RUN unico, ABORT o
// cualquier salida): deja los actuadores exactamente donde enter_safe_state()
// los puso y devuelve el control a PIDHandler()/turnFans().
static void restore(void) {
  ledcWrite(HEATER_PWM_CHANNEL, 0);
  ledcWrite(PHOTOTHERAPY_PWM_CHANNEL, 0);
  ledcWrite(FAN_PWM_CHANNEL, 0);
  ledcWrite(FAN_CTL_PWM_CHANNEL, 0);
  ledcWrite(BUZZER_PWM_CHANNEL, 0);
  // Simetria con enter_safe_state() (bloqueante #7 del review de seguridad):
  // el humidificador tiene DOS caminos de salida (in3_hum.turn(), que ya se
  // llamaba, y el acceso directo de HUMID_USB via USB_EN/HUMIDIFIER_PWM_
  // CHANNEL) y solo el primero se restauraba.
  digitalWrite(USB_EN, LOW);
  in3_hum.turn(OFF);
  ledcWrite(HUMIDIFIER_PWM_CHANNEL, 0);
  digitalWrite(ACTUATORS_EN, LOW);
  g_factoryTestActive = false;
  // Solo si alarmsEnabled sigue en false: si alguien la puso a true mientras
  // la bateria corria (p.ej. el HMI reenvio un comando que la reactivo), esa
  // decision no es de este modulo y no se pisa.
  if (!in3.alarmsEnabled) {
    in3.alarmsEnabled = s_prevAlarmsEnabled;
  }
}

// ---- Persistencia NVS (mb_ftest) ----
static uint32_t current_epoch_or_zero(void) {
  const time_t now = time(nullptr);
  return (now >= 1609459200) ? (uint32_t)now : 0u;
}

static void persist_full(const FtestSummary *sum) {
  SbSnapshot sb;
  sensorboard_get_snapshot(&sb);
  Preferences p;
  p.begin(NS_FTEST, false);
  p.putUInt(KEY_FTEST_EPOCH, current_epoch_or_zero());
  p.putUInt(KEY_FTEST_PASS, sum->pass_mask);
  p.putUInt(KEY_FTEST_FAIL, sum->fail_mask);
  p.putUInt(KEY_FTEST_WARN, sum->warn_mask);
  p.putUInt(KEY_FTEST_RUN, sum->run_mask);
  p.putString(KEY_FTEST_FW, FWversion);
  p.putString(KEY_FTEST_SB_FW, sb.sb_fw);
  p.end();
}

static void persist_single(unsigned id, FtestStatus st) {
  Preferences p;
  p.begin(NS_FTEST, false);
  uint32_t passMask = p.getUInt(KEY_FTEST_PASS, 0);
  uint32_t failMask = p.getUInt(KEY_FTEST_FAIL, 0);
  uint32_t warnMask = p.getUInt(KEY_FTEST_WARN, 0);
  uint32_t runMask = p.getUInt(KEY_FTEST_RUN, 0);
  ftest_summary_merge_single(&passMask, &failMask, &warnMask, &runMask, id,
                              st);
  p.putUInt(KEY_FTEST_PASS, passMask);
  p.putUInt(KEY_FTEST_FAIL, failMask);
  p.putUInt(KEY_FTEST_WARN, warnMask);
  p.putUInt(KEY_FTEST_RUN, runMask);
  p.putUInt(KEY_FTEST_EPOCH, current_epoch_or_zero());
  p.putString(KEY_FTEST_FW, FWversion);
  p.end();
}

// Ejecuta un id y, si ABORT llego DURANTE la llamada (incluidos los cuerpos
// no cooperativos como ACTUATORS, que bloquean 11-16 s sin comprobar el
// flag), fuerza el resultado a SKIP/"abort" -- design.md D5 y el escenario
// "Abort a mitad del test de actuadores" de mb-factory-test.
//
// Cota por test (FTEST_TEST_TIMEOUT_MS): s_testStartMs se refresca AQUI, al
// arrancar cada cuerpo, asi que el proximo test siempre arranca con cota
// fresca aunque el anterior la haya agotado. Si compute_abort_reason()
// devuelve "timeout" (via ftest_abort_requested(), ya consultada en todos
// los bucles de espera de <= 250 ms de los cuerpos), NO se trata como el
// resto de motivos de ABORT: se traduce a FAIL/"timeout", y la bateria
// continua con el siguiente test.
static FtestStatus run_one(unsigned id, char detail[FTEST_DETAIL_MAX + 1],
                           FtestCascade *cascade) {
  s_testStartMs = millis();
  if (ftest_abort_requested()) {
    snprintf(detail, FTEST_DETAIL_MAX + 1, "%s", ftest_abort_reason());
    return FTEST_SKIP;
  }
  const FtestStatus st = ftest_hw_run(id, detail, cascade);
  if (ftest_abort_requested()) {
    const char *reason = ftest_abort_reason();
    if (strcmp(reason, "timeout") == 0) {
      snprintf(detail, FTEST_DETAIL_MAX + 1, "timeout");
      return FTEST_FAIL;
    }
    snprintf(detail, FTEST_DETAIL_MAX + 1, "%s", reason);
    return FTEST_SKIP;
  }
  return st;
}

static void factory_test_task_body(void *pv) {
  (void)pv;
  // Bloqueante #2: suscribe esta tarea al Task WDT global (75 s,
  // watchdogInit() en initHardware.cpp) ANTES de tocar ningun hardware. Sin
  // esto una espera larga sin resets (GSM, estimulos de SensorBoard) dispara
  // el panic del WDT y reinicia la placa a mitad de bateria.
  esp_task_wdt_add(NULL);
  enter_safe_state();

  FtestCascade cascade;
  memset(&cascade, 0, sizeof(cascade));

  FtestSummary sum;
  ftest_summary_init(&sum);

  // Limpia el flag de "peticion ya enviada" de sb_status/sb_camera (quinta
  // ronda, banco 2026-09-06): sin esto, una bateria o un RUN posterior
  // heredaria el flag de la tanda anterior y no volveria a pedir nada.
  ftest_hw_reset_passive_state();

  if (s_paramsSingle) {
    const unsigned id = s_paramsSingleId;
    char detail[FTEST_DETAIL_MAX + 1] = {0};
    esp_task_wdt_reset();
    ftest_emit(id, FTEST_RUNNING, NULL);
    FtestStatus st;
    if (ftest_id_is_passive(id)) {
      // RUN unico de un pasivo (PROTOCOL.md, "Orden de ejecucion"): se
      // sondea el solo, sin el resto de la bateria corriendo debajo (no hay
      // s_passiveSlots que poblar aqui). Las CUATRO cotas de bateria siguen
      // aplicando (compute_abort_reason_common(), via ftest_abort_requested()
      // de mas abajo) -- la cota de 90 s POR TEST no: es de los ACTIVOS
      // (run_one() la refresca en s_testStartMs); un pasivo se basta con su
      // propio plazo (<= 45 s), asi que se deja s_testStartMs a 0 para que
      // compute_abort_reason() no la evalue con un valor rancio de una
      // ejecucion anterior de esta misma tarea (modulo, no de instancia).
      s_testStartMs = 0;
      const uint32_t t0 = millis();
      for (;;) {
        if (ftest_abort_requested()) {
          snprintf(detail, sizeof(detail), "%s", ftest_abort_reason());
          st = FTEST_SKIP;
          break;
        }
        st = ftest_hw_poll_passive(id, detail, &cascade,
                                    (uint32_t)(millis() - t0));
        if (st != FTEST_RUNNING) break;
        ftest_wdt_feed();
        vTaskDelay(pdMS_TO_TICKS(250));
      }
    } else {
      st = run_one(id, detail, &cascade);
    }
    ftest_emit(id, st, detail);
    ftest_summary_note(&sum, id, st);
    persist_single(id, st);
  } else {
    // ---- Bateria completa: solape cooperativo (ver cabecera del fichero,
    // banco 2026-09-06, quinta ronda) ----
    // 1) Todos los PASIVOS arrancan en paralelo, en RUNNING, con un
    // cronometro comun.
    s_passiveSlotCount = FTEST_PASSIVE_COUNT;
    for (unsigned i = 0; i < FTEST_PASSIVE_COUNT; i++) {
      s_passiveSlots[i].id = ftest_hw_passive_id_at(i);
      s_passiveSlots[i].resolved = false;
    }
    s_passiveCascade = &cascade;
    s_passiveSum = &sum;
    s_passiveT0 = millis();
    for (unsigned i = 0; i < FTEST_PASSIVE_COUNT; i++) {
      esp_task_wdt_reset();
      ftest_emit(s_passiveSlots[i].id, FTEST_RUNNING, NULL);
      // Misma regla anti-desborde que la rafaga de resultados de abajo (el
      // HMI drena su anillo de recepcion una sola vez por pasada de UI):
      // ~20 RUNNING de golpe la desbordarian igual que una rafaga de
      // resultados.
      if ((i % 4) == 3) vTaskDelay(pdMS_TO_TICKS(60));
    }

    // 2) Un primer sondeo antes de arrancar los ACTIVOS: los pasivos
    // instantaneos (sysinfo, ina3221, skin_adc...) ya resuelven aqui.
    poll_passives();

    // 3) ACTIVOS, en el orden nuevo de kFtestActiveOrder (standby,
    // actuators, fan_rpm, buzzer, afe_spi, sb_env, sb_light) -- NO el orden
    // ascendente de id: sb_env/sb_light van al final para dar tiempo a que
    // env_sensor (pasivo) resuelva la cascada sb_usb en paralelo.
    for (unsigned i = 0; i < FTEST_ACTIVE_COUNT; i++) {
      const unsigned id = ftest_hw_active_id_at(i);
      char detail[FTEST_DETAIL_MAX + 1] = {0};
      esp_task_wdt_reset();
      ftest_emit(id, FTEST_RUNNING, NULL);
      const FtestStatus st = run_one(id, detail, &cascade);
      ftest_emit(id, st, detail);
      ftest_summary_note(&sum, id, st);
      // Deja avanzar los pasivos que se hayan resuelto mientras corria este
      // activo: standby/fan_rpm/afe_spi no tienen bucle de espera propio (no
      // llaman a ftest_yield()), asi que sin este sondeo explicito no se
      // comprobarian hasta el paso 4.
      poll_passives();
      // Pausa entre el resultado final de este test y el RUNNING del
      // siguiente (cuarta ronda, banco 2026-09-06): con varios tests
      // omitidos seguidos el cuerpo vuelve casi al instante y la MB emite
      // varias lineas en rafaga; el HMI drena su anillo de recepcion una
      // sola vez por pasada de UI (10 ms) y se desbordaba sin esta pausa.
      vTaskDelay(pdMS_TO_TICKS(60));
    }

    // 4) Tras los ACTIVOS, seguir sondeando lo que quede pendiente hasta que
    // se resuelva: todos los pasivos tienen plazo propio (<= 45 s desde
    // s_passiveT0), asi que este bucle siempre termina.
    for (;;) {
      bool anyPending = false;
      for (unsigned i = 0; i < s_passiveSlotCount; i++) {
        if (!s_passiveSlots[i].resolved) {
          anyPending = true;
          break;
        }
      }
      if (!anyPending) break;
      esp_task_wdt_reset();
      poll_passives();
      vTaskDelay(pdMS_TO_TICKS(250));
    }

    s_passiveSlotCount = 0;
    s_passiveCascade = nullptr;
    s_passiveSum = nullptr;

    persist_full(&sum);
  }

  char doneLine[FTEST_TX_LINE_MAX];
  if (ftest_format_done(doneLine, sizeof(doneLine), sum.pass, sum.fail,
                        sum.skip, sum.warn) >= 0) {
    CommunicationHost_Enqueue(doneLine);
  }

  // restore() SIEMPRE, tambien si el bucle de arriba termino por ABORT: no
  // hay ningun "return" antes de este punto.
  restore();
  s_running = false;
  // Desuscribirse ANTES de vTaskDelete(): un handle de WDT que sobreviva a la
  // tarea que lo registro dispara el panic en el siguiente periodo aunque
  // nadie mas este usando ese slot.
  esp_task_wdt_delete(NULL);
  vTaskDelete(NULL);
}

static bool start_task(void) {
  s_abortRequested = false;
  s_abortReasonLogged = false;
  s_batteryStartMs = millis();
  // Se marca ANTES de crear la tarea: cierra la ventana en la que un segundo
  // HMI,FTEST,START llegase entre el xTaskCreatePinnedToCore() y el primer
  // vTaskDelay() de la tarea nueva.
  s_running = true;
  // g_factoryTestActive TAMBIEN antes de crear la tarea (bloqueante #9 del
  // review de seguridad): si se marcara dentro de la tarea nueva
  // (enter_safe_state()), habria una ventana entre xTaskCreatePinnedToCore()
  // y la primera instruccion de esa tarea en la que PIDHandler()/turnFans()/
  // el keepalive del HMI seguirian escribiendo PWM como si no hubiera
  // bateria en curso.
  g_factoryTestActive = true;
  const BaseType_t ok = xTaskCreatePinnedToCore(
      factory_test_task_body, "FTEST", FTEST_TASK_STACK_BYTES, NULL,
      FTEST_TASK_PRIORITY, NULL, CORE_ID_FREERTOS);
  if (ok != pdPASS) {
    s_running = false;
    g_factoryTestActive = false;
    logE("[FTEST] no se pudo crear la tarea FTEST");
    return false;
  }
  return true;
}

bool factoryTestStart(void) {
  if (s_running) return false;
  s_paramsSingle = false;
  s_paramsSingleId = 0;
  return start_task();
}

bool factoryTestRunSingle(unsigned id) {
  if (s_running) return false;
  if (!ftest_id_is_mb(id)) return false;
  s_paramsSingle = true;
  s_paramsSingleId = id;
  return start_task();
}
