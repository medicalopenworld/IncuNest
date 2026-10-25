#pragma once

// Vigilante de muestras del AFE4490, separado del hardware para probarlo en
// host.
//
// La libreria del AFE entrega muestras a ~500 Hz aunque no haya sonda. Si
// SPO2_Task lleva mas de AFE_WD_STALL_MS sin recibir ninguna, la libreria o el
// chip se han quedado colgados (DRDY perdido, SPI atascado): hay que pararla,
// pulsar PDN y volver a iniciarla. Eso solo sirve en la V19, donde PDN es un
// pin real (IO7); ver design.md de mb-hw19.
//
// Limite: como mucho un reinicio por AFE_WD_MIN_RESTART_INTERVAL_MS. Un chip
// roto de verdad no debe convertir la tarea en un bucle de stop()/begin().
//
// Sin reloj propio: recibe now_ms (millis()) y resta sin signo.
//
// Test de regresion: motherBoard/test/test_afe_watchdog.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AFE_WD_STALL_MS 10000u
#define AFE_WD_MIN_RESTART_INTERVAL_MS 60000u

typedef struct {
  uint32_t last_sample_ms;
  uint32_t last_restart_ms;
  bool restarted_once;
  uint32_t restarts;
} AfeWatchdog;

void afe_wd_init(AfeWatchdog *wd, uint32_t now_ms);

// Llega una muestra: rearma el plazo.
void afe_wd_sample(AfeWatchdog *wd, uint32_t now_ms);

// true si hay que reiniciar el AFE ahora: mas de AFE_WD_STALL_MS sin muestra
// Y han pasado AFE_WD_MIN_RESTART_INTERVAL_MS desde el reinicio anterior. Al
// devolver true ya cuenta el reinicio y rearma el plazo de muestras (el AFE
// tiene otros AFE_WD_STALL_MS para volver a dar muestra).
bool afe_wd_should_restart(AfeWatchdog *wd, uint32_t now_ms);

uint32_t afe_wd_restarts(const AfeWatchdog *wd);

#ifdef __cplusplus
}
#endif
