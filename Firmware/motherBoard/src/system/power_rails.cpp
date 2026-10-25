#include "system/power_rails.h"

#include <Arduino.h>

#include "config/board.h"
#include "modules/control/modem_power_cycle.h"

#if (HW_NUM >= 19)

static const char *TAG_PWR = "PWR";

static ModemPowerCycle s_modemPc = {MODEM_PC_IDLE, 0, 0, 0};

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
#if EXT_WDT_ENABLED
  pinMode(EXT_WDT_WDI, OUTPUT);
  digitalWrite(EXT_WDT_WDI, LOW);
#endif
}

bool mainsPresentPin(void) { return digitalRead(MAINS_OK_PIN) == HIGH; }

void extWdtToggle(void) {
#if EXT_WDT_ENABLED
  static bool level = false;
  level = !level;
  digitalWrite(EXT_WDT_WDI, level ? HIGH : LOW);
#endif
}

bool gsmPowerCycleBegin(uint32_t grace_ms) {
  if (!modem_pc_begin(&s_modemPc, millis(), grace_ms)) {
    return false;
  }
  ESP_LOGW(TAG_PWR, "[GSM] power-cycle #%lu: corte en %lu ms",
           (unsigned long)modem_pc_count(&s_modemPc), (unsigned long)grace_ms);
  // Sin margen no hay nada que esperar: se corta aqui mismo, sin depender de
  // que alguien consulte Busy() (el camino del monitor borra la tarea GPRS).
  if (grace_ms == 0) {
    (void)gsmPowerCycleBusy();
  }
  return true;
}

bool gsmPowerCycleBusy(void) {
  switch (modem_pc_poll(&s_modemPc, millis())) {
  case MODEM_PC_ACTION_CUT:
    gsmPowerSet(false);
    ESP_LOGW(TAG_PWR, "[GSM] GSM_PWR_EN=LOW (modem sin VBAT)");
    break;
  case MODEM_PC_ACTION_RESTORE:
    gsmPowerSet(true);
    ESP_LOGW(TAG_PWR, "[GSM] GSM_PWR_EN=HIGH (modem alimentado de nuevo)");
    break;
  case MODEM_PC_ACTION_NONE:
  default:
    break;
  }
  return modem_pc_busy(&s_modemPc);
}

uint32_t gsmPowerCycles(void) { return modem_pc_count(&s_modemPc); }

#else // HW_NUM < 19: sin pines, todo no-op

void pwr5vSet(bool) {}
void gsmPowerSet(bool) {}
void powerRailsInit(void) {}
bool mainsPresentPin(void) { return true; }
void extWdtToggle(void) {}
bool gsmPowerCycleBegin(uint32_t) { return false; }
bool gsmPowerCycleBusy(void) { return false; }
uint32_t gsmPowerCycles(void) { return 0; }

#endif
