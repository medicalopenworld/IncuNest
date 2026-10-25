#pragma once

// Detector de interrupcion de red (ALARM_MAINS_INTERRUPTION), separado del
// hardware.
//
// La entrada es "hay red / no hay red" segun el cargador BQ25730 (bit AC_STAT
// de su registro de estado), leida cada 5 s por sensors_Task. La salida es la
// condicion que se le da a alarm_machine_condition(). Nada de Arduino aqui: lo
// ejercitan los tests de host (motherBoard/test/test_mains_detector).
//
// REGLAS (razonadas en openspec/changes/archive/*-shared-mains-detector*/ y
// *-shared-mains-detector-fixes/design.md)
//
//  1. Antirrebote de entrada: CONFIRM_SAMPLES lecturas VALIDAS consecutivas con
//     el mismo valor para cambiar de estado, en los dos sentidos. Con el
//     periodo de 5 s son ~10 s de deteccion. Una sola lectura de ausencia no
//     levanta una alarma ALTA que el operador no puede silenciar.
//
//  2. Plazo minimo de aviso (MAINS_ALARM_MIN_HOLD_MS), contado DESDE LA
//     DECLARACION: si la red vuelve antes de vencerlo, la condicion se mantiene
//     hasta el vencimiento. Un corte nuevo confirmado antes de vencer lo
//     reinicia.
//
//     VALE 0 POR DECISION DE PRODUCTO: la condicion se retira en cuanto se
//     confirma que la red ha vuelto. docs/alarms_normative_analysis.md 2.5 lee
//     201.12.3.103 ("shall be provided for a minimum time of 10 min") como un
//     requisito de AUTONOMIA —la alarma tiene que poder sostenerse 10 min sin
//     red, que es hardware— y no como la obligacion de mantenerla con la red
//     ya presente: eso enmascaraba otras alarmas ALTAS, mostraba "BATERIA" en
//     falso y contradecia 6.10. El mecanismo se conserva (y esta probado con un
//     plazo de prueba): si el responsable de riesgos decide lo contrario, se
//     revierte poniendo la constante a 600000u.
//
//  3. Una lectura NO valida (fallo de I2C del cargador) no cambia nada: ni
//     declara, ni retira, ni rompe la racha. Un fallo de bus no es un corte de
//     red ni un restablecimiento. Pero el detector NO puede quedarse ciego en
//     silencio: mains_detector_blind() dice si el cargador, detectado en el
//     arranque, lleva mas de MAINS_DETECTOR_BLIND_MS sin dar una lectura
//     valida (~6 lecturas) para que el llamante lo haga visible. Con la alarma
//     ya declarada y el detector ciego, la condicion se MANTIENE (lado seguro).
//
// HILOS: mains_detector_sample() la llama UN solo hilo (sensors_Task). Las
// consultas mains_detector_condition() y mains_detector_blind() son const y las
// puede llamar otro (securityCheck): no escriben estado. Es sample() quien, al
// ser el unico escritor, da por cerrado un plazo vencido.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Lecturas validas consecutivas para confirmar un cambio (en ambos sentidos).
#define MAINS_DETECTOR_CONFIRM_SAMPLES 2u

// Plazo minimo de aviso desde que se declara. 0 = se retira al confirmarse el
// retorno de la red (ver regla 2).
#define MAINS_ALARM_MIN_HOLD_MS 0u

// Sin lectura valida del cargador durante mas de esto, el detector esta ciego
// (6 lecturas a 5 s).
#define MAINS_DETECTOR_BLIND_MS 30000u

typedef struct {
  uint8_t absent_streak;   // lecturas validas consecutivas de "sin red"
  uint8_t present_streak;  // lecturas validas consecutivas de "con red"
  bool absent_confirmed;   // el estado de la red ya confirmado: sin red
  bool declared;           // alguna vez se ha declarado (since_ms valido)
  uint32_t since_ms;       // instante de la ultima declaracion
  uint32_t hold_ms;        // plazo minimo de aviso de esta instancia
  uint32_t last_valid_ms;  // instante de la ultima lectura valida
} MainsDetector;

// Con el plazo por defecto (MAINS_ALARM_MIN_HOLD_MS).
void mains_detector_init(MainsDetector *d);
// Con un plazo explicito: lo usan los tests del mecanismo.
void mains_detector_init_hold(MainsDetector *d, uint32_t hold_ms);

// Una lectura del cargador. `reading_valid` es el resultado de charge_status().
void mains_detector_sample(MainsDetector *d, bool reading_valid, bool ac_present,
                           uint32_t now_ms);

// true si la condicion ALARM_MAINS_INTERRUPTION esta presente en `now_ms`.
bool mains_detector_condition(const MainsDetector *d, uint32_t now_ms);

// true si el cargador (`charger_detected`: contesto por I2C en el arranque)
// lleva mas de MAINS_DETECTOR_BLIND_MS sin dar una lectura valida. Sin
// cargador detectado nunca esta ciego: no hay nada que vigilar. Antes de la
// primera lectura valida cuenta desde el arranque (millis() == 0).
bool mains_detector_blind(const MainsDetector *d, bool charger_detected,
                          uint32_t now_ms);

#ifdef __cplusplus
}
#endif
