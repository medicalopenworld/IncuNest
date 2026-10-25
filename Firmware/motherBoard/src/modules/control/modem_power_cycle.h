#pragma once

// Temporizacion del power-cycle del modem SIM800C en la motherBoard V19,
// separada del hardware para poder probarla en host.
//
// En la V19 la alimentacion del modem (VBAT, buck TPS563231) cuelga de
// GSM_PWR_EN, asi que "reiniciar el modem" puede ser un corte real y no solo
// un AT+CPOWD=1 que un modem colgado ya no atiende.
//
// Fases:
//
//   IDLE --begin(grace)--> SHUTDOWN_WAIT --(grace cumplido)--> OFF
//        --(MODEM_PC_OFF_MIN_MS cumplidos)--> IDLE
//
// poll() devuelve la accion que el llamante debe ejecutar y cada accion sale
// UNA sola vez: CUT al pasar de SHUTDOWN_WAIT a OFF, RESTORE al volver a IDLE.
// Con grace = 0 el primer poll() ya devuelve CUT.
//
// "grace" es el margen para que el modem termine su apagado ordenado tras
// AT+CPOWD=1 (el camino del timeout de GPRS). El camino del monitor, que
// borra una tarea GPRS colgada, no tiene CPOWD que esperar y usa grace = 0.
//
// La maquina no tiene reloj propio: recibe now_ms (millis()) y resta sin
// signo, asi que aguanta la vuelta del contador de 32 bits.
//
// Test de regresion: motherBoard/test/test_modem_power_cycle.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Tiempo minimo con VBAT cortado antes de volver a levantarlo.
#define MODEM_PC_OFF_MIN_MS 2000u
// Margen de apagado ordenado tras AT+CPOWD=1 antes de cortar.
#define MODEM_PC_SHUTDOWN_GRACE_MS 1500u

typedef enum {
  MODEM_PC_IDLE = 0,
  MODEM_PC_SHUTDOWN_WAIT,
  MODEM_PC_OFF,
} ModemPcPhase;

typedef enum {
  MODEM_PC_ACTION_NONE = 0,
  MODEM_PC_ACTION_CUT,     // poner GSM_PWR_EN a nivel bajo
  MODEM_PC_ACTION_RESTORE, // poner GSM_PWR_EN a nivel alto
} ModemPcAction;

typedef struct {
  ModemPcPhase phase;
  uint32_t grace_ms;
  uint32_t since_ms;
  uint32_t count; // power-cycles aceptados desde init
} ModemPowerCycle;

void modem_pc_init(ModemPowerCycle *pc);

// Pide un power-cycle. Devuelve false (y no cuenta nada ni toca el
// temporizador) si ya hay uno en curso.
bool modem_pc_begin(ModemPowerCycle *pc, uint32_t now_ms, uint32_t grace_ms);

ModemPcAction modem_pc_poll(ModemPowerCycle *pc, uint32_t now_ms);

// true mientras haya un power-cycle en curso (hasta devolver RESTORE).
bool modem_pc_busy(const ModemPowerCycle *pc);

uint32_t modem_pc_count(const ModemPowerCycle *pc);

#ifdef __cplusplus
}
#endif
