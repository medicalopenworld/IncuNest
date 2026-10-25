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
// POLITICA DE CORTE (M5). Cortar VBAT es drastico: el modem pierde el registro
// en red y tarda en volver. Por eso un reinicio SUAVE fallido (GPRSForceReset
// sin haber llegado a conectar) solo desemboca en corte tras
// MODEM_PC_SOFT_FAILS_BEFORE_CUT seguidos, y entre cortes hay un backoff
// exponencial de MODEM_PC_BACKOFF_MIN_MS a MODEM_PC_BACKOFF_MAX_MS. Conectar
// (GPRS.post) vacia la racha y el backoff. El camino del monitor
// (modem_pc_begin) NO pasa por la politica: la tarea esta colgada y se corta
// en el acto.
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

// Reinicios suaves fallidos seguidos que hacen falta para cortar VBAT.
#define MODEM_PC_SOFT_FAILS_BEFORE_CUT 3u
// Backoff entre cortes: el primero es inmediato; el siguiente se permite pasado
// MIN, luego el doble... hasta MAX (30 min).
#define MODEM_PC_BACKOFF_MIN_MS 60000u
#define MODEM_PC_BACKOFF_MAX_MS 1800000u

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
  uint32_t soft_fails;      // reinicios suaves seguidos sin conectar
  uint32_t backoff_ms;      // espera que impondra el PROXIMO corte aceptado
  uint32_t next_allowed_ms; // desde cuando se permite un corte por politica
  bool gate_active;         // next_allowed_ms vale (hubo un corte)
} ModemPowerCycle;

void modem_pc_init(ModemPowerCycle *pc);

// Pide un power-cycle FORZADO (camino del monitor): no consulta la politica.
// Devuelve false (y no cuenta nada ni toca el
// temporizador) si ya hay uno en curso.
bool modem_pc_begin(ModemPowerCycle *pc, uint32_t now_ms, uint32_t grace_ms);

// Un reinicio suave fallo. Cuenta; si ya hay MODEM_PC_SOFT_FAILS_BEFORE_CUT
// seguidos y el backoff lo permite, inicia el corte (como modem_pc_begin) y
// devuelve true. Si hay un ciclo en curso no cuenta nada y devuelve false.
bool modem_pc_soft_failure(ModemPowerCycle *pc, uint32_t now_ms,
                           uint32_t grace_ms);

// El modem conecto (GPRS.post): vacia la racha de fallos y el backoff.
void modem_pc_connected(ModemPowerCycle *pc);

uint32_t modem_pc_soft_fails(const ModemPowerCycle *pc);
uint32_t modem_pc_backoff_ms(const ModemPowerCycle *pc);

ModemPcAction modem_pc_poll(ModemPowerCycle *pc, uint32_t now_ms);

// true mientras haya un power-cycle en curso (hasta devolver RESTORE).
bool modem_pc_busy(const ModemPowerCycle *pc);

uint32_t modem_pc_count(const ModemPowerCycle *pc);

#ifdef __cplusplus
}
#endif
