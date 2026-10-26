#pragma once

// Detector de interrupcion de red (ALARM_MAINS_INTERRUPTION), separado del
// hardware.
//
// La entrada es "hay red / no hay red" segun el cargador BQ25730 (bit AC_STAT
// de su registro de estado), leida cada 1 s por sensors_Task. La salida es la
// condicion que se le da a alarm_machine_condition(). Nada de Arduino aqui: lo
// ejercitan los tests de host (motherBoard/test/test_mains_detector).
//
// REGLAS (razonadas en openspec/changes/archive/*-shared-mains-detector*/ y
// *-shared-mains-detector-fixes/design.md)
//
//  1. Antirrebote de entrada, asimetrico: 2 lecturas VALIDAS consecutivas sin
//     red para declarar (~2 s con el periodo de 1 s) y 5 con red para retirar
//     (~5 s). Una sola lectura de ausencia no levanta la alarma, y una fuente
//     intermitente no la hace oscilar (ver las constantes).
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

// Histeresis ASIMETRICA: declarar rapido, retirar despacio.
//  - 2 lecturas validas seguidas SIN red declaran (~2 s a 1 s por lectura);
//  - 5 lecturas validas seguidas CON red retiran (~5 s). Cualquier ausencia
//    reinicia esa racha.
// Hasta el 2026-10-26 se leia cada 5 s (~10 s / ~30 s): en banco, 30 s para
// retirar con la red ya enchufada se percibia como un fallo.
// Una fuente intermitente (enchufe flojo, grupo inestable) mantiene asi la
// alarma continua en vez de oscilar: cada retirada y levantada es una entrada
// en el historial, una escritura en flash y un rearme del audio.
#define MAINS_DETECTOR_CONFIRM_ABSENT_SAMPLES 2u
#define MAINS_DETECTOR_CONFIRM_PRESENT_SAMPLES 5u

// Periodo de lectura del cargador en sensors_Task (main.cpp).
#define MAINS_DETECTOR_SAMPLE_PERIOD_MS 1000u

// El detector solo se ARMA tras ver red en esta cantidad de lecturas validas
// seguidas desde el arranque. Arrancar en bateria no es una interrupcion de
// red (no se ha cortado nada: no estaba), y declararla ahi da una ALTA no
// silenciable permanente en cualquier equipo que trabaje solo con bateria o
// alimentado por el conector de bateria (el banco, 2026-10-26). Dos lecturas
// y no una, para que un parpadeo del comparador AC_STAT no lo arme.
#define MAINS_DETECTOR_ARM_PRESENT_SAMPLES 2u

// Plazo minimo de aviso desde que se declara. 0 = se retira al confirmarse el
// retorno de la red (ver regla 2).
#define MAINS_ALARM_MIN_HOLD_MS 0u

// Sin lectura valida del cargador durante mas de esto, el detector esta ciego
// (30 lecturas a 1 s).
#define MAINS_DETECTOR_BLIND_MS 30000u

typedef struct {
  uint8_t absent_streak;   // lecturas validas consecutivas de "sin red"
  uint8_t present_streak;  // lecturas validas consecutivas de "con red"
  bool absent_confirmed;   // el estado de la red ya confirmado: sin red
  bool declared;           // alguna vez se ha declarado (since_ms valido)
  uint32_t since_ms;       // instante de la ultima declaracion
  uint32_t hold_ms;        // plazo minimo de aviso de esta instancia
  uint32_t last_valid_ms;  // instante de la ultima lectura valida
  bool mains_seen;         // red vista desde el arranque: detector armado
} MainsDetector;

// Inicializador ESTATICO de la instancia de produccion (main.cpp):
//   MainsDetector g_mains_detector = MAINS_DETECTOR_INITIALIZER;
// Con `= {}` el plazo valia 0 porque la estructura estaba a cero y no porque lo
// dijera MAINS_ALARM_MIN_HOLD_MS: cambiar la constante no hacia nada. Usa la
// constante, y test_mains_detector fija que coincide con mains_detector_init().
// Debe seguir el orden de los campos de MainsDetector.
#define MAINS_DETECTOR_INITIALIZER \
  { 0, 0, false, false, 0, MAINS_ALARM_MIN_HOLD_MS, 0, false }

// Con el plazo por defecto (MAINS_ALARM_MIN_HOLD_MS).
void mains_detector_init(MainsDetector *d);
// Con un plazo explicito: lo usan los tests del mecanismo.
void mains_detector_init_hold(MainsDetector *d, uint32_t hold_ms);

// Una lectura del cargador. `reading_valid` es el resultado de charge_status().
void mains_detector_sample(MainsDetector *d, bool reading_valid, bool ac_present,
                           uint32_t now_ms);

// true si la condicion ALARM_MAINS_INTERRUPTION esta presente en `now_ms`.
bool mains_detector_condition(const MainsDetector *d, uint32_t now_ms);

// true si el cargador lleva mas de MAINS_DETECTOR_BLIND_MS sin dar una lectura
// valida. `charger_expected` sale del HARDWARE (CHARGER_EXPECTED: la placa lleva
// BQ25730) o de que el chip haya contestado alguna vez, NO solo de que conteste
// al arrancar: un chip mudo en una placa que lo lleva es ceguera, no "sin
// cargador". Sin cargador esperado nunca esta ciego: no hay nada que vigilar.
// Antes de la primera lectura valida cuenta desde el arranque (millis() == 0).
bool mains_detector_blind(const MainsDetector *d, bool charger_expected,
                          uint32_t now_ms);

#ifdef __cplusplus
}
#endif
