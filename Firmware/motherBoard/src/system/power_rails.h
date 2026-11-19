#pragma once

// Raíles de alimentación controlados por el micro en la motherBoard V19
// (HW_NUM >= 19), CHRG_OK y watchdog externo. Ver Firmware/docs/hw19.md.
//
// En HW < 19 NINGUNA de estas funciones toca un pin: son no-op (o devuelven
// el valor que tenia la placa antes de que existiera el pin), de modo que el
// codigo que las llama no necesita #if y la V17/V18 no cambia.

#include <stdbool.h>
#include <stdint.h>

// PWR_5V_EN (IO40): buck de 5 V (humidificador, LEDs del AFE, SensorBoard por
// USB, display HMI). Pull-down en el PCB: apagado hasta que se llame con true.
void pwr5vSet(bool on);

// GSM_PWR_EN (IO39): buck de VBAT del SIM800C. Pull-down en el PCB: modem
// apagado hasta que se llame con true. PWRKEY va a GND: se enciende solo.
void gsmPowerSet(bool on);

// Configura CHRG_OK como entrada. Llamar una vez en el arranque. NO toca
// EXT_WDT_WDI: ver extWdtArm().
void powerRailsInit(void);

// CHRG_OK (IO42) del BQ25730: true si la fuente de entrada es valida (red).
// HIGH = valida (pull-up externo de 10k). Sin antirrebote. HW < 19: true.
bool mainsPresentPin(void);

// Arma el watchdog externo TPS3431 (EXT_WDT_WDI como salida). Llamar como
// ULTIMA cosa de setup(): hasta entonces el pin queda sin configurar y
// extWdtToggle() no hace nada, porque el tWD de fabrica del TPS3431 (~1,6 s a
// confirmar) no aguanta setup(). Solo hace algo con EXT_WDT_ENABLED.
void extWdtArm(void);

// Pulso completo HIGH -> LOW en EXT_WDT_WDI. Solo hace algo con
// EXT_WDT_ENABLED y tras extWdtArm(); llamar desde el mismo sitio que alimenta
// el Task WDT del loopTask (watchdogReload()).
void extWdtToggle(void);

// Power-cycle del modem. Ver modules/control/modem_power_cycle.h.
//
// gsmPowerCycleBegin(): corte FORZADO, sin pasar por la politica (camino del
// monitor, con la tarea GPRS colgada). grace_ms es el margen de apagado
// ordenado antes de cortar (0 = cortar ya). false si ya hay un ciclo en curso
// o si la placa no puede cortar el modem (HW < 19).
bool gsmPowerCycleBegin(uint32_t grace_ms);

// Un reinicio suave del modem ha fallado (GPRSForceReset). Devuelve true si la
// politica (racha de fallos + backoff) ha decidido ademas CORTAR VBAT, y el
// ciclo ya esta iniciado; false si no hay corte (el llamante hara un reinicio
// por AT). HW < 19: false.
bool gsmSoftFailure(uint32_t grace_ms);

// El modem ha conectado (GPRS.post): vacia la racha de fallos y el backoff.
void gsmNoteConnected(void);

// true mientras el ciclo siga en curso. Ejecuta las acciones (cortar /
// restaurar GSM_PWR_EN) cuando toca, asi que hay que consultarla a menudo desde
// la tarea GPRS. HW < 19: false.
bool gsmPowerCycleBusy(void);

// Power-cycles aceptados desde el arranque (RAM; se pierde al reiniciar).
uint32_t gsmPowerCycles(void);
