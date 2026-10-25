#pragma once

// Detector de parada del AFE4490 y caducidad de su dato, separado del
// hardware para probarlo en host.
//
// La libreria del AFE entrega muestras a ~500 Hz aunque no haya sonda. Si
// SPO2_Task deja de recibirlas, la libreria o el chip se han quedado colgados
// (DRDY perdido, SPI atascado). Este modulo hace DOS cosas, y ninguna de ellas
// es reiniciar nada:
//
// 1. Caducidad del dato (A2). Si pasa mas de AFE_WD_DATA_STALE_MS sin muestra,
//    el ultimo dato (FC, SpO2, probe_state = APPLIED) esta congelado y NO se
//    puede seguir enviando como si fuera actual. SPO2_Task, al ver que no es
//    fresco, sintetiza probe_state = DISCONNECTED ("sin senal"), y con eso
//    CommTask, GPRS y WiFi dejan solos de publicar vitales. Afecta a todas las
//    revisiones, V18 incluida.
//
// 2. Deteccion de parada (diagnostico). Pasados AFE_WD_STALL_MS sin muestra se
//    abre un EPISODIO y se avisa UNA vez (log + contador AFE_stalls). Una
//    muestra nueva cierra el episodio.
//
// POR QUE NO SE REINICIA LA LIBRERIA DESDE AQUI. La primera version de mb-hw19
// hacia stop() / pulso de PDN / begin() en caliente. afe.stop() hace
// vTaskDelete de las tareas de la libreria en un punto arbitrario, y justo en el
// escenario que lo dispara esa tarea esta escribiendo "DRDY timeout" con
// ESP_LOGW, que coge log_mutex con portMAX_DELAY: borrarla con el mutex cogido
// cuelga a cualquier tarea que haga log (incluida la de seguridad) con el LEDC
// del calefactor encendido. Ademas puede dejar _diag_active = true para siempre,
// filtrar ~15 KB de SRAM interna y disparar un configASSERT. La libreria vive en
// otro repo (incunest_afe4490, fijada en v0.94). El diseno del re-init blando que
// hace falta alli esta en docs/hw19.md y docs/known_issues.md.
//
// Sin reloj propio: recibe now_ms (millis()) y resta sin signo.
//
// Test de regresion: motherBoard/test/test_afe_watchdog.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Sin muestra durante mas de esto, el dato esta caducado (ya no es "actual").
#define AFE_WD_DATA_STALE_MS 1000u
// Sin muestra durante mas de esto, se declara una parada del AFE.
#define AFE_WD_STALL_MS 10000u

typedef struct {
  uint32_t last_sample_ms;
  bool episode_open; // parada ya avisada y aun sin muestra nueva
  uint32_t stalls;   // episodios de parada desde init
} AfeWatchdog;

void afe_wd_init(AfeWatchdog *wd, uint32_t now_ms);

// Llega una muestra: rearma el plazo y cierra el episodio abierto.
void afe_wd_sample(AfeWatchdog *wd, uint32_t now_ms);

// true si el ultimo dato tiene como mucho AFE_WD_DATA_STALE_MS.
bool afe_wd_data_fresh(const AfeWatchdog *wd, uint32_t now_ms);

// true UNA SOLA VEZ por episodio: la primera consulta con mas de
// AFE_WD_STALL_MS sin muestra. Cuenta el episodio. Las siguientes devuelven
// false hasta que llegue una muestra y vuelva a haber otra parada.
bool afe_wd_stall_detected(AfeWatchdog *wd, uint32_t now_ms);

uint32_t afe_wd_stalls(const AfeWatchdog *wd);

#ifdef __cplusplus
}
#endif
