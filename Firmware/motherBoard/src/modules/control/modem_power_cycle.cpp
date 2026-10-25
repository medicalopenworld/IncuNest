#include "modem_power_cycle.h"

// Ver modem_power_cycle.h para las fases y el porque de cada margen.

void modem_pc_init(ModemPowerCycle *pc) {
  pc->phase = MODEM_PC_IDLE;
  pc->grace_ms = 0;
  pc->since_ms = 0;
  pc->count = 0;
  pc->soft_fails = 0;
  pc->backoff_ms = MODEM_PC_BACKOFF_MIN_MS;
  pc->next_allowed_ms = 0;
  pc->gate_active = false;
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

bool modem_pc_soft_failure(ModemPowerCycle *pc, uint32_t now_ms,
                           uint32_t grace_ms) {
  if (pc->phase != MODEM_PC_IDLE) {
    return false;
  }
  pc->soft_fails++;
  if (pc->soft_fails < MODEM_PC_SOFT_FAILS_BEFORE_CUT) {
    return false;
  }
  // Backoff: signo del intervalo, no comparacion de marcas (millis() da la
  // vuelta a los ~49 dias).
  if (pc->gate_active && (int32_t)(now_ms - pc->next_allowed_ms) < 0) {
    return false;
  }
  if (!modem_pc_begin(pc, now_ms, grace_ms)) {
    return false;
  }
  pc->soft_fails = 0;
  pc->gate_active = true;
  pc->next_allowed_ms = now_ms + pc->backoff_ms;
  pc->backoff_ms = (pc->backoff_ms > MODEM_PC_BACKOFF_MAX_MS / 2)
                       ? MODEM_PC_BACKOFF_MAX_MS
                       : pc->backoff_ms * 2u;
  return true;
}

void modem_pc_connected(ModemPowerCycle *pc) {
  pc->soft_fails = 0;
  pc->backoff_ms = MODEM_PC_BACKOFF_MIN_MS;
  pc->gate_active = false;
}

uint32_t modem_pc_soft_fails(const ModemPowerCycle *pc) {
  return pc->soft_fails;
}

uint32_t modem_pc_backoff_ms(const ModemPowerCycle *pc) {
  return pc->backoff_ms;
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
