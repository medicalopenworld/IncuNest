#include "modem_power_cycle.h"

// Ver modem_power_cycle.h para las fases y el porque de cada margen.

void modem_pc_init(ModemPowerCycle *pc) {
  pc->phase = MODEM_PC_IDLE;
  pc->grace_ms = 0;
  pc->since_ms = 0;
  pc->count = 0;
}

bool modem_pc_begin(ModemPowerCycle *pc, uint32_t now_ms, uint32_t grace_ms) {
  if (pc->phase != MODEM_PC_IDLE) {
    return false;
  }
  pc->phase = MODEM_PC_SHUTDOWN_WAIT;
  pc->grace_ms = grace_ms;
  pc->since_ms = now_ms;
  pc->count++;
  return true;
}

// Resta sin signo y se compara el intervalo, no las marcas: asi sobrevive al
// desbordamiento de millis().
ModemPcAction modem_pc_poll(ModemPowerCycle *pc, uint32_t now_ms) {
  switch (pc->phase) {
  case MODEM_PC_SHUTDOWN_WAIT:
    if ((uint32_t)(now_ms - pc->since_ms) >= pc->grace_ms) {
      pc->phase = MODEM_PC_OFF;
      pc->since_ms = now_ms; // los 2 s se cuentan desde el corte real
      return MODEM_PC_ACTION_CUT;
    }
    break;
  case MODEM_PC_OFF:
    if ((uint32_t)(now_ms - pc->since_ms) >= MODEM_PC_OFF_MIN_MS) {
      pc->phase = MODEM_PC_IDLE;
      return MODEM_PC_ACTION_RESTORE;
    }
    break;
  case MODEM_PC_IDLE:
  default:
    break;
  }
  return MODEM_PC_ACTION_NONE;
}

bool modem_pc_busy(const ModemPowerCycle *pc) {
  return pc->phase != MODEM_PC_IDLE;
}

uint32_t modem_pc_count(const ModemPowerCycle *pc) { return pc->count; }
