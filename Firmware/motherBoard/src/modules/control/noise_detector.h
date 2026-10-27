#pragma once

// Detector de ruido elevado (ALARM_NOISE_HIGH), separado del hardware.
//
// La entrada es cada lectura de nivel sonoro que publica la SensorBoard
// (evento sound_level, microfono dentro de la cupula). La salida es la
// condicion que se le da a alarm_machine_condition(). Nada de Arduino aqui:
// lo ejercitan los tests de host (motherBoard/test/test_noise_detector).
//
// REGLAS
//
//  1. Declara cuando todas las lecturas validas de los ultimos
//     NOISE_ON_HOLD_MS estan a NOISE_ON_DBA o mas (la racha empieza en la
//     primera lectura por encima). Una sola lectura alta (un golpe, un portazo)
//     no basta.
//  2. Retira cuando todas las de los ultimos NOISE_OFF_HOLD_MS estan por
//     debajo de NOISE_OFF_DBA. Entre los dos umbrales (banda de histeresis) no
//     cambia nada: un ruido que ronda el umbral no hace saltar y quitarse la
//     alarma cada pocos segundos.
//  3. Lecturas con la senal acustica de OTRA alarma sonando (`suppressed`): se
//     ignoran, sin romper ni avanzar las rachas. El pitido del propio equipo no
//     es ruido ambiental.
//  4. Una lectura no valida no cambia nada. Sin lecturas durante
//     NOISE_STALE_MS (SensorBoard caida: eso lo avisa su propia alarma) la
//     condicion se retira: no saber no es lo mismo que haber ruido.
//
// El nivel es el de la SensorBoard: SPL sin ponderacion A, que en banco
// (2026-10-27, ruido rosa y fototerapia contra un sonometro en dB(A)) quedo a
// -1..+2 dB del sonometro entre 55 y 80 dB. Por eso los umbrales se dan en esa
// escala sin correccion.
//
// HILOS: noise_detector_sample() la llama UN solo hilo (la tarea de la
// SensorBoard). noise_detector_condition() es const y la puede llamar otro
// (securityCheck).

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NOISE_ON_DBA 65.0f
#define NOISE_OFF_DBA 63.0f
#define NOISE_ON_HOLD_MS 10000u
#define NOISE_OFF_HOLD_MS 10000u
// Tres periodos de publicacion de 5 s: con lecturas cada segundo sobra.
#define NOISE_STALE_MS 15000u

typedef struct {
  bool raised;
  bool have;            // alguna lectura valida
  uint32_t last_ms;     // ultima lectura valida (tambien las ignoradas)
  bool above_run;
  uint32_t above_since;
  bool below_run;
  uint32_t below_since;
} NoiseDetector;

#define NOISE_DETECTOR_INITIALIZER \
  { false, false, 0u, false, 0u, false, 0u }

void noise_detector_sample(NoiseDetector *d, bool valid, float dba,
                           bool suppressed, uint32_t now_ms);

bool noise_detector_condition(const NoiseDetector *d, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
