#pragma once

// Detector de interrupcion de red (ALARM_MAINS_INTERRUPTION), separado del
// hardware.
//
// La entrada es "hay red / no hay red" segun el cargador BQ25730 (bit AC_STAT
// de su registro de estado), leida cada 5 s por sensors_Task. La salida es la
// condicion que se le da a alarm_machine_condition(). Nada de Arduino aqui: lo
// ejercitan los tests de host (motherBoard/test/test_mains_detector).
//
// REGLAS (razonadas en openspec/changes/shared-mains-detector/design.md)
//
//  1. Antirrebote de entrada: CONFIRM_SAMPLES lecturas VALIDAS consecutivas con
//     el mismo valor para cambiar de estado, en los dos sentidos. Con el
//     periodo de 5 s son ~10 s de deteccion. Una sola lectura de ausencia no
//     levanta una alarma ALTA que el operador no puede silenciar.
//
//  2. Minimo de 10 min. 60601-2-19 201.12.3.103: el aviso "shall be provided
//     for a minimum time of 10 min" (docs/alarms_normative_analysis.md, 2.5).
//     alarm_machine NO lo garantiza (solo la rafaga minima de audio de 6.10),
//     asi que lo hace este detector: una vez declarada, la condicion se
//     mantiene MAINS_ALARM_MIN_HOLD_MS DESDE LA DECLARACION aunque la red
//     vuelva antes. Si el corte sigue, sigue presente mientras dure. Un corte
//     nuevo confirmado antes de vencer el plazo lo reinicia.
//     La norma no dice "o hasta que vuelva la red, lo que ocurra antes": se
//     aplica la lectura literal. Pregunta abierta para el analisis de riesgos;
//     poner la constante a 0 adopta la otra lectura sin tocar la logica.
//
//  3. Una lectura NO valida (fallo de I2C del cargador, o vbat_mv == 0 = el
//     ADC aun no ha convertido y el resto de la trama no es de fiar) no cambia
//     nada: ni declara, ni retira, ni rompe la racha. Un fallo de bus no es un
//     corte de red ni un restablecimiento.
//
// HILOS: mains_detector_sample() la llama UN solo hilo (sensors_Task) y
// mains_detector_condition() es const y la puede llamar otro (securityCheck):
// el vencimiento del plazo se calcula al consultar, sin escribir estado.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Lecturas validas consecutivas para confirmar un cambio (en ambos sentidos).
#define MAINS_DETECTOR_CONFIRM_SAMPLES 2u

// Plazo minimo de aviso desde que se declara (201.12.3.103).
#define MAINS_ALARM_MIN_HOLD_MS 600000u

typedef struct {
  uint8_t absent_streak;   // lecturas validas consecutivas de "sin red"
  uint8_t present_streak;  // lecturas validas consecutivas de "con red"
  bool absent_confirmed;   // el estado de la red ya confirmado: sin red
  bool declared;           // alguna vez se ha declarado (since_ms valido)
  uint32_t since_ms;       // instante de la ultima declaracion
} MainsDetector;

void mains_detector_init(MainsDetector *d);

// Una lectura del cargador. `reading_valid` es el resultado de charge_status();
// con vbat_mv == 0 la lectura se trata como no valida.
void mains_detector_sample(MainsDetector *d, bool reading_valid, bool ac_present,
                           uint16_t vbat_mv, uint32_t now_ms);

// true si la condicion ALARM_MAINS_INTERRUPTION esta presente en `now_ms`.
bool mains_detector_condition(const MainsDetector *d, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
