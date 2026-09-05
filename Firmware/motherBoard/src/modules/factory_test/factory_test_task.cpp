// Orquestacion del test de fabrica (design.md D4/D5, shared-factory-test):
// crea la tarea FTEST bajo demanda, entra/sale de estado seguro, recorre la
// tabla de tests (factory_test_hw.cpp) o uno solo, persiste el resultado en
// NVS y ofrece a los cuerpos de test las primitivas de CONFIRM/ABORT/emision
// de linea que necesitan sin que cada uno conozca el mecanismo real.
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
#include "freertos/semphr.h"
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

// ---- CONFIRM: semaforo binario + id esperado (design.md D5) ----
static SemaphoreHandle_t s_confirmSem = NULL;
static volatile int s_confirmExpectedId = -1;
static volatile bool s_confirmOk = false;

static void ensure_confirm_sem(void) {
  if (s_confirmSem == NULL) {
    s_confirmSem = xSemaphoreCreateBinary();
  }
}

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

// Precondicion compuesta del bloqueante #3: evaluada en CADA punto de
// comprobacion (los cuerpos de test la consultan en pasos de <= 250 ms), no
// una sola vez al arrancar. Devuelve el motivo ("abort"/"control on"/
// "hmi lost"/"max time") o NULL si esta permitido seguir.
static const char *compute_abort_reason(void) {
  if (s_abortRequested) return "abort";
  // Alguien reactivo el control (u otra fototerapia) por otra via mientras
  // la bateria estaba en curso: la precondicion de arranque no basta, hay
  // que seguir vigilandola (design.md D4, "un test de fabrica no debe tener
  // autoridad para apagar un control que alguien encendio" aplica tambien al
  // reves: no debe seguir corriendo con un control ya encendido).
  if (in3.actuation != ACTUATION_OFF || in3.phototherapy) return "control on";
  // El HMI dejo de hablar (reinicio o cable desconectado): sin operario
  // vigilando la pantalla no hay quien confirme CONFIRM ni quien detenga un
  // ABORT manual si algo va mal.
  if (!CommunicationHost_HmiAlive(FTEST_HMI_DEADMAN_MS)) return "hmi lost";
  // Cota POR TEST (bloqueante nuevo, "cota cooperativa por test"): distinta
  // de la cota de bateria de abajo. run_one() refresca s_testStartMs al
  // arrancar cada cuerpo, asi que esto solo dispara si ESE cuerpo concreto
  // lleva mas de FTEST_TEST_TIMEOUT_MS corriendo -- run_one() traduce este
  // motivo especifico a FAIL/"timeout" (no SKIP) y NO detiene el resto de la
  // bateria, a diferencia de los motivos de arriba y de abajo.
  if (s_testStartMs != 0 &&
      (uint32_t)(millis() - s_testStartMs) > FTEST_TEST_TIMEOUT_MS) {
    return "timeout";
  }
  // Cota temporal dura de toda la bateria.
  if (s_batteryStartMs != 0 &&
      (uint32_t)(millis() - s_batteryStartMs) > FTEST_BATTERY_MAX_MS) {
    return "max time";
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

void factoryTestConfirm(unsigned id, bool ok) {
  if ((int)id != s_confirmExpectedId) {
    logE("[FTEST] CONFIRM id=" + String(id) + " inesperado, descartado");
    return;
  }
  s_confirmOk = ok;
  xSemaphoreGive(s_confirmSem);
}

// Bloqueante #8 (carrera del CONFIRM): arma la espera ANTES de encolar la
// linea CTRL,FTEST,id,5 que se lo pide al operario. Orden correcto: primero
// drenar cualquier "give" residual de un CONFIRM anterior, DESPUES fijar el
// id esperado -- al reves dejaria una ventana en la que un CONFIRM que
// llegase justo entre fijar el id y drenar se perderia.
void ftest_arm_confirm(unsigned id) {
  ensure_confirm_sem();
  while (xSemaphoreTake(s_confirmSem, 0) == pdTRUE) {
  }
  s_confirmExpectedId = (int)id;
}

// El cuerpo del test debe haber llamado ftest_arm_confirm(id) ANTES de
// encolar su linea CONFIRM; esta funcion ya no rearma nada, solo espera.
int ftest_wait_confirm(unsigned id, uint32_t timeout_ms) {
  // id ya no se usa aqui (lo fija ftest_arm_confirm()); se conserva en la
  // firma porque es parte de la API publica declarada en factory_test_api.h.
  (void)id;
  ensure_confirm_sem();
  int result = -1;
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < timeout_ms) {
    if (ftest_abort_requested()) break;
    esp_task_wdt_reset();
    // Paso <= 250 ms (design.md D5): el ABORT y un CONFIRM que llegue justo
    // se atienden con ese margen como mucho.
    if (xSemaphoreTake(s_confirmSem, pdMS_TO_TICKS(250)) == pdTRUE) {
      result = s_confirmOk ? 1 : 0;
      break;
    }
  }
  s_confirmExpectedId = -1;
  return result;
}

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
  // esto una espera larga sin resets (GSM, CONFIRM, estimulos de
  // SensorBoard) dispara el panic del WDT y reinicia la placa a mitad de
  // bateria.
  esp_task_wdt_add(NULL);
  enter_safe_state();

  FtestCascade cascade;
  memset(&cascade, 0, sizeof(cascade));

  FtestSummary sum;
  ftest_summary_init(&sum);

  if (s_paramsSingle) {
    const unsigned id = s_paramsSingleId;
    char detail[FTEST_DETAIL_MAX + 1] = {0};
    esp_task_wdt_reset();
    ftest_emit(id, FTEST_RUNNING, NULL);
    const FtestStatus st = run_one(id, detail, &cascade);
    ftest_emit(id, st, detail);
    ftest_summary_note(&sum, id, st);
    persist_single(id, st);
  } else {
    for (unsigned id = 0; id < FTEST_MB_COUNT; id++) {
      char detail[FTEST_DETAIL_MAX + 1] = {0};
      esp_task_wdt_reset();
      ftest_emit(id, FTEST_RUNNING, NULL);
      const FtestStatus st = run_one(id, detail, &cascade);
      ftest_emit(id, st, detail);
      ftest_summary_note(&sum, id, st);
    }
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
  ensure_confirm_sem();
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
