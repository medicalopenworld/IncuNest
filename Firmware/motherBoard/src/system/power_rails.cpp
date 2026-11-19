#include "system/power_rails.h"

#include <Arduino.h>

#include "config/board.h"
#include "modules/control/modem_power_cycle.h"

#if (HW_NUM >= 19)

static const char *TAG_PWR = "PWR";

// Estado del power-cycle del modem.
//
// Quien lo toca:
//   - la tarea GPRS: gsmSoftFailure() (desde GPRSForceReset), gsmNoteConnected()
//     y gsmPowerCycleBusy() (desde GPRSPowerUp, caso 0);
//   - GPRSMonitorTask (otro core): gsmPowerCycleBegin(0), SOLO tras haber
//     borrado con vTaskDelete la tarea GPRS y antes de recrearla.
//
// Con esa disciplina no hay dos escritores a la vez, pero vTaskDelete puede
// matar la tarea GPRS en MITAD de gsmPowerCycleBusy(), entre "la maquina dice
// CUT" y "se baja el pin": la fase quedaria en OFF con el pin aun alto, o peor,
// RESTORE consumido y el pin sin subir (modem apagado para siempre). Por eso
// cada transicion de estado y su escritura de pin van JUNTAS dentro de una
// seccion critica (portMUX): una tarea no se puede borrar mientras la
// ejecuta. Dentro no se loguea ni se hace pinMode(); el pin se configura una
// vez en gsmPowerSet().
//
// Tarea borrada en fase OFF (mon_kill durante un ciclo): la tarea nueva
// arranca con GPRS.powerUp = true (initGPRS) y GPRSPowerUp() caso 0 consulta
// gsmPowerCycleBusy() hasta que la maquina devuelve RESTORE, asi que el
// modem se vuelve a alimentar aunque el monitor mate la tarea a medio ciclo.
static ModemPowerCycle s_modemPc = {MODEM_PC_IDLE, 0, 0, 0, 0,
                                    MODEM_PC_BACKOFF_MIN_MS, 0, false};
static portMUX_TYPE s_modemMux = portMUX_INITIALIZER_UNLOCKED;
static bool s_extWdtArmed = false;

void pwr5vSet(bool on) {
  pinMode(PWR_5V_EN, OUTPUT);
  digitalWrite(PWR_5V_EN, on ? HIGH : LOW);
}

void gsmPowerSet(bool on) {
  pinMode(GSM_PWR_EN, OUTPUT);
  digitalWrite(GSM_PWR_EN, on ? HIGH : LOW);
}

void powerRailsInit(void) {
  // IO39..IO42 son los pines JTAG por defecto del S3 (MTCK/MTDO/MTDI/MTMS):
  // pinMode() los pasa a GPIO. Con un depurador JTAG conectado hay conflicto.
  pinMode(MAINS_OK_PIN, INPUT); // pull-up de 10k externo
  // EXT_WDT_WDI NO se configura aqui: hasta extWdtArm() el pin queda sin tocar.
}

bool mainsPresentPin(void) { return digitalRead(MAINS_OK_PIN) == HIGH; }

void extWdtArm(void) {
#if EXT_WDT_ENABLED
  pinMode(EXT_WDT_WDI, OUTPUT);
  digitalWrite(EXT_WDT_WDI, LOW);
  s_extWdtArmed = true;
#endif
}

void extWdtToggle(void) {
#if EXT_WDT_ENABLED
  if (!s_extWdtArmed) {
    return;
  }
  // Pulso completo HIGH -> LOW en cada llamada, y no un cambio de nivel: asi no
  // depende de que flanco mire el TPS3431 ni de en que nivel quedo la ultima
  // vez. Ancho minimo de pulso del TPS3431: A CONFIRMAR contra el datasheet; 5 us
  // es holgado para cualquier watchdog de esta familia.
  digitalWrite(EXT_WDT_WDI, HIGH);
  delayMicroseconds(5);
  digitalWrite(EXT_WDT_WDI, LOW);
#else
  (void)s_extWdtArmed;
#endif
}

// Ejecuta una accion de la maquina sobre el pin. Solo dentro de la seccion
// critica: sin pinMode() ni logs.
static void gsmApply(ModemPcAction a) {
  if (a == MODEM_PC_ACTION_CUT) {
    digitalWrite(GSM_PWR_EN, LOW);
  } else if (a == MODEM_PC_ACTION_RESTORE) {
    digitalWrite(GSM_PWR_EN, HIGH);
  }
}

static void gsmLogAction(ModemPcAction a) {
  if (a == MODEM_PC_ACTION_CUT) {
    ESP_LOGW(TAG_PWR, "[GSM] GSM_PWR_EN=LOW (modem sin VBAT)");
  } else if (a == MODEM_PC_ACTION_RESTORE) {
    ESP_LOGW(TAG_PWR, "[GSM] GSM_PWR_EN=HIGH (modem alimentado de nuevo)");
  }
}

bool gsmPowerCycleBegin(uint32_t grace_ms) {
  ModemPcAction a = MODEM_PC_ACTION_NONE;
  bool started;
  uint32_t count;
  portENTER_CRITICAL(&s_modemMux);
  started = modem_pc_begin(&s_modemPc, millis(), grace_ms);
  count = modem_pc_count(&s_modemPc);
  // Sin margen no hay nada que esperar: se corta aqui mismo, sin depender de
  // que alguien consulte Busy() (el camino del monitor borra la tarea GPRS).
  if (started && grace_ms == 0) {
    a = modem_pc_poll(&s_modemPc, millis());
    gsmApply(a);
  }
  portEXIT_CRITICAL(&s_modemMux);
  if (started) {
    ESP_LOGW(TAG_PWR, "[GSM] power-cycle #%lu forzado: corte en %lu ms",
             (unsigned long)count, (unsigned long)grace_ms);
    gsmLogAction(a);
  }
  return started;
}

bool gsmSoftFailure(uint32_t grace_ms) {
  bool cut;
  uint32_t count;
  uint32_t fails;
  portENTER_CRITICAL(&s_modemMux);
  cut = modem_pc_soft_failure(&s_modemPc, millis(), grace_ms);
  count = modem_pc_count(&s_modemPc);
  fails = modem_pc_soft_fails(&s_modemPc);
  portEXIT_CRITICAL(&s_modemMux);
  if (cut) {
    ESP_LOGW(TAG_PWR, "[GSM] power-cycle #%lu tras %u reinicios suaves "
                      "fallidos: corte en %lu ms",
             (unsigned long)count, (unsigned)MODEM_PC_SOFT_FAILS_BEFORE_CUT,
             (unsigned long)grace_ms);
  } else {
    ESP_LOGW(TAG_PWR, "[GSM] reinicio suave fallido %lu/%u (sin corte de VBAT)",
             (unsigned long)fails, (unsigned)MODEM_PC_SOFT_FAILS_BEFORE_CUT);
  }
  return cut;
}

void gsmNoteConnected(void) {
  portENTER_CRITICAL(&s_modemMux);
  modem_pc_connected(&s_modemPc);
  portEXIT_CRITICAL(&s_modemMux);
}

bool gsmPowerCycleBusy(void) {
  ModemPcAction a;
  bool busy;
  portENTER_CRITICAL(&s_modemMux);
  a = modem_pc_poll(&s_modemPc, millis());
  gsmApply(a);
  busy = modem_pc_busy(&s_modemPc);
  portEXIT_CRITICAL(&s_modemMux);
  gsmLogAction(a);
  return busy;
}

uint32_t gsmPowerCycles(void) {
  uint32_t n;
  portENTER_CRITICAL(&s_modemMux);
  n = modem_pc_count(&s_modemPc);
  portEXIT_CRITICAL(&s_modemMux);
  return n;
}

#else // HW_NUM < 19: sin pines, todo no-op

void pwr5vSet(bool) {}
void gsmPowerSet(bool) {}
void powerRailsInit(void) {}
bool mainsPresentPin(void) { return true; }
void extWdtArm(void) {}
void extWdtToggle(void) {}
bool gsmPowerCycleBegin(uint32_t) { return false; }
bool gsmSoftFailure(uint32_t) { return false; }
void gsmNoteConnected(void) {}
bool gsmPowerCycleBusy(void) { return false; }
uint32_t gsmPowerCycles(void) { return 0; }

#endif
