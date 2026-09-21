#pragma once

#include <Arduino.h>
#include <stddef.h>

// Ring buffer size kept in RTC slow memory. Survives software resets and
// panics, lost on cold power cycle.
#define CRASH_RING_SIZE 4096

void crashReporterInit();

// Non-blocking, ISR-unsafe (expected to be called from sync_vprintf only).
void crashReporterPut(const char *data, size_t len);

// Call after DriveUpload + LittleFS are ready. If the previous boot ended in
// a reset that looks like a crash, dumps the captured ring plus reset metadata
// into a file on LittleFS and enqueues it for Drive upload. No-op otherwise.
void crashReporterMaybeFlush();

// --------------------------------------------------------------------------
// Resumen de la ultima caida, para publicarlo como telemetria.
//
// Hasta ahora el informe solo vivia en LittleFS y en Drive: para saber POR QUE
// se habia reiniciado una unidad habia que ir a por ella. Con esto la causa
// viaja a ThingsBoard en el primer envio despues del reinicio.
//
// El resumen se arma en crashReporterInit(), antes de que nada pueda
// sobrescribir el anillo, y NO depende del sistema de archivos: una unidad que
// no consiga escribir en LittleFS publica igual.
//
// Valido durante todo el arranque. crashReportPending() es false si el
// reinicio anterior fue normal (apagado, reset por software), y entonces las
// otras tres no hay que publicarlas.
// --------------------------------------------------------------------------
#define CRASH_SUMMARY_TAIL_MAX 320

bool        crashReportPending(void);
const char *crashReportReason(void);   // "TASK_WDT", "PANIC", "INT_WDT"...
uint32_t    crashReportReboots(void);  // reinicios desde el ultimo arranque en frio
// Ultimas lineas del log antes del reinicio, en una sola linea y ya saneadas
// para que quepan en un JSON (sin comillas, barras ni caracteres de control).
// Es lo que de verdad identifica la averia: en las tres caidas de banco del
// 2026-09-20 aqui ponia "[MON] GPRS_Task hung, restarting it".
const char *crashReportTail(void);

// Del COREDUMP, no del log: nombre de la tarea que provoco la excepcion y su
// backtrace. Es lo unico que senala al culpable sin deducirlo -- en un
// TASK_WDT dice que tarea dejo de ceder CPU. Cadena vacia si no hay coredump
// legible (la particion existe desde 2026-09-06; una unidad que caiga sin
// escribirlo, o con formato distinto de ELF, devuelve vacio).
const char *crashReportTask(void);
const char *crashReportBacktrace(void);

// Log COMPLETO de la caida: el anillo entero (~4 KB), ya saneado para JSON y en
// una sola linea. Es lo que hace falta para depurar la averia concreta y sacar
// una OTA sin ir a por la unidad.
//
// No pasa por el sistema de archivos: sale del mismo buffer que ya se reserva
// en crashReporterInit() para el informe, asi que no cuesta memoria adicional.
// Devuelve cadena vacia si no hay caida pendiente o si ya se libero.
//
// crashReportFullLogRelease() suelta ese buffer y hay que llamarla DESPUES de
// publicar: son 4 KB de heap interno, y en esta placa el margen con WiFi y
// celular arriba es de ~11 KB (ver known_issues.md #12 del port).
const char *crashReportFullLog(void);
void        crashReportFullLogRelease(void);
