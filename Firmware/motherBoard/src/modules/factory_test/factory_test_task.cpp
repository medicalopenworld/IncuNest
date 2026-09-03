// Orquestacion del test de fabrica (design.md D4/D5, shared-factory-test):
// crea la tarea FTEST bajo demanda, entra/sale de estado seguro, recorre la
// tabla de tests (factory_test_hw.cpp) o uno solo, persiste el resultado en
// NVS y ofrece a los cuerpos de test las primitivas de CONFIRM/ABORT/emision
// de linea que necesitan sin que cada uno conozca el mecanismo real.
//
// Sin entorno de test (motherBoard/[env:native] solo cubre
// modules/control/{alarm_machine,pid_wrapper}.cpp y la logica pura de este
// modulo, ya cubierta en ftest_summary.cpp/test_factory_test): verificacion
// manual en banco documentada en el commit.
#include <Preferences.h>
#include <cstdio>
#include <cstring>
#include <time.h>

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

// ---- Estado de la tarea (un unico FTEST posible a la vez) ----
static volatile bool s_running = false;
static volatile bool s_abortRequested = false;
static bool s_paramsSingle = false;
static unsigned s_paramsSingleId = 0;
static bool s_prevAlarmsEnabled = true;

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

bool ftest_abort_requested(void) { return s_abortRequested; }

void factoryTestAbort(void) { s_abortRequested = true; }

void factoryTestConfirm(unsigned id, bool ok) {
  if ((int)id != s_confirmExpectedId) {
    logE("[FTEST] CONFIRM id=" + String(id) + " inesperado, descartado");
    return;
  }
  s_confirmOk = ok;
  xSemaphoreGive(s_confirmSem);
}

int ftest_wait_confirm(unsigned id, uint32_t timeout_ms) {
  ensure_confirm_sem();
  s_confirmExpectedId = (int)id;
  // Descarta cualquier "give" residual de un CONFIRM anterior que llegara
  // tarde (p.ej. tras un timeout ya consumido por el test previo).
  while (xSemaphoreTake(s_confirmSem, 0) == pdTRUE) {
  }

  int result = -1;
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < timeout_ms) {
    if (ftest_abort_requested()) break;
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
  g_factoryTestActive = true;

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
  digitalWrite(ACTUATORS_EN, LOW);
  g_factoryTestActive = false;
  in3.alarmsEnabled = s_prevAlarmsEnabled;
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
  uint32_t runMask = p.getUInt(KEY_FTEST_RUN, 0);
  ftest_summary_merge_single(&passMask, &failMask, &runMask, id, st);
  p.putUInt(KEY_FTEST_PASS, passMask);
  p.putUInt(KEY_FTEST_FAIL, failMask);
  p.putUInt(KEY_FTEST_RUN, runMask);
  p.putUInt(KEY_FTEST_EPOCH, current_epoch_or_zero());
  p.putString(KEY_FTEST_FW, FWversion);
  p.end();
}

// Ejecuta un id y, si ABORT llego DURANTE la llamada (incluidos los cuerpos
// no cooperativos como ACTUATORS, que bloquean 11-16 s sin comprobar el
// flag), fuerza el resultado a SKIP/"abort" -- design.md D5 y el escenario
// "Abort a mitad del test de actuadores" de mb-factory-test.
static FtestStatus run_one(unsigned id, char detail[FTEST_DETAIL_MAX + 1],
                           FtestCascade *cascade) {
  if (ftest_abort_requested()) {
    snprintf(detail, FTEST_DETAIL_MAX + 1, "abort");
    return FTEST_SKIP;
  }
  const FtestStatus st = ftest_hw_run(id, detail, cascade);
  if (ftest_abort_requested()) {
    snprintf(detail, FTEST_DETAIL_MAX + 1, "abort");
    return FTEST_SKIP;
  }
  return st;
}

static void factory_test_task_body(void *pv) {
  (void)pv;
  enter_safe_state();

  FtestCascade cascade;
  memset(&cascade, 0, sizeof(cascade));

  FtestSummary sum;
  ftest_summary_init(&sum);

  if (s_paramsSingle) {
    const unsigned id = s_paramsSingleId;
    char detail[FTEST_DETAIL_MAX + 1] = {0};
    ftest_emit(id, FTEST_RUNNING, NULL);
    const FtestStatus st = run_one(id, detail, &cascade);
    ftest_emit(id, st, detail);
    ftest_summary_note(&sum, id, st);
    persist_single(id, st);
  } else {
    for (unsigned id = 0; id < FTEST_MB_COUNT; id++) {
      char detail[FTEST_DETAIL_MAX + 1] = {0};
      ftest_emit(id, FTEST_RUNNING, NULL);
      const FtestStatus st = run_one(id, detail, &cascade);
      ftest_emit(id, st, detail);
      ftest_summary_note(&sum, id, st);
    }
    persist_full(&sum);
  }

  char doneLine[FTEST_TX_LINE_MAX];
  if (ftest_format_done(doneLine, sizeof(doneLine), sum.pass, sum.fail,
                        sum.skip) >= 0) {
    CommunicationHost_Enqueue(doneLine);
  }

  // restore() SIEMPRE, tambien si el bucle de arriba termino por ABORT: no
  // hay ningun "return" antes de este punto.
  restore();
  s_running = false;
  vTaskDelete(NULL);
}

static bool start_task(void) {
  ensure_confirm_sem();
  s_abortRequested = false;
  // Se marca ANTES de crear la tarea: cierra la ventana en la que un segundo
  // HMI,FTEST,START llegase entre el xTaskCreatePinnedToCore() y el primer
  // vTaskDelay() de la tarea nueva.
  s_running = true;
  const BaseType_t ok = xTaskCreatePinnedToCore(
      factory_test_task_body, "FTEST", FTEST_TASK_STACK_BYTES, NULL,
      FTEST_TASK_PRIORITY, NULL, CORE_ID_FREERTOS);
  if (ok != pdPASS) {
    s_running = false;
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
