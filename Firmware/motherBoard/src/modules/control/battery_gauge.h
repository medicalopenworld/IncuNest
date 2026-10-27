#pragma once

// Indicador de carga de la bateria interna (plomo-acido 12 V 9 Ah), separado
// del hardware. Lo alimenta sensors_Task con cada lectura completa del
// cargador BQ25730 (1 Hz) y lo pinta el display (CTRL,BATT). Nada de Arduino
// aqui: lo ejercitan los tests de host (motherBoard/test/test_battery_gauge).
//
// SIN RED (descarga), por tension:
//
//  1. Tension corregida por la caida resistiva: Vbat + Idescarga * R, con R la
//     resistencia interna de la bateria mas cables, fusible y conectores
//     (BATT_GAUGE_RINT_MOHM). Sin esto, el calefactor al encender hunde la
//     tension y el indicador bajaria y subiria con cada ciclo de la PWM.
//  2. Media movil exponencial (1/BATT_GAUGE_EMA_DIV por muestra, ~8 s): las
//     la tension es gruesa (VBAT a 64 mV por paso) y la carga cambia a
//     saltos.
//  3. Porcentaje por la curva de tension en reposo de una bateria de plomo de
//     12 V (tabla en battery_gauge.cpp), interpolada. Al empezar la descarga
//     se toma el MENOR entre ese valor y el ultimo conocido: recien quitada la
//     red, la carga superficial de la flotacion hace que la tension diga "llena"
//     aunque la carga no hubiese terminado.
//  4. Rayas (0..4) y porcentaje solo BAJAN mientras dure la descarga: una
//     bateria que se recupera al quitarle carga no "se recarga" en pantalla.
//
// CON RED (carga), por lo que entra: con el cargador conectado la tension la
// impone el y no dice nada de la carga. Se parte del ultimo porcentaje conocido
// y se suma la corriente de carga integrada (mAh) sobre la capacidad, con un
// rendimiento de carga de BATT_GAUGE_CHARGE_EFF_PCT. Tope en 99 % hasta que el
// cargador pasa a flotacion, que es el unico "lleno" fiable: 100 %.
//
// El ultimo porcentaje conocido sobrevive a un apagado: lo guarda quien llama
// (main.cpp, en NVS) y lo devuelve al arrancar con battery_gauge_restore(). Sin
// nada guardado se parte de 0 %: con red es el lado seguro (se ve menos carga
// de la que hay) y se corrige en la primera flotacion.
//
// Es una estimacion, no un contador de carga calibrado: el error tipico es de
// 10-20 puntos, mas con la bateria envejecida o fria. Las rayas son
// deliberadamente gruesas por eso.
//
// HILOS: battery_gauge_sample() la llama UN solo hilo (sensors_Task). Quien
// la lea desde otro hilo usa battery_gauge_pack(), un uint32_t que se publica
// de una vez.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Resistencia interna + cableado. Una 12 V 9 Ah nueva ronda 20-25 mOhm; el
// resto es el camino hasta el cargador (fusible, conectores, pistas).
#define BATT_GAUGE_RINT_MOHM 50u
#define BATT_GAUGE_EMA_DIV 8
#define BATT_GAUGE_CAPACITY_MAH 9000u
// Plomo-acido: ~85 % de la carga que entra queda almacenada.
#define BATT_GAUGE_CHARGE_EFF_PCT 85u
#define BATT_GAUGE_CHARGING_MAX_PCT 99u
// Umbrales de rayas sobre el porcentaje.
#define BATT_GAUGE_PCT_4_BARS 75u
#define BATT_GAUGE_PCT_3_BARS 50u
#define BATT_GAUGE_PCT_2_BARS 25u
#define BATT_GAUGE_PCT_1_BAR 10u
#define BATT_GAUGE_LEVEL_MAX 4u

typedef enum {
  BATT_GAUGE_UNKNOWN = 0,      // sin cargador o sin lectura valida aun
  BATT_GAUGE_DISCHARGING = 1,  // sin red
  BATT_GAUGE_CHARGING = 2,     // con red, absorcion
  BATT_GAUGE_FULL = 3,         // con red, flotacion (carga completa)
} BattGaugeState;

typedef struct {
  BattGaugeState state;
  uint8_t level;    // 0..BATT_GAUGE_LEVEL_MAX
  uint8_t percent;  // 0..100
  bool known;       // percent viene de una medida o de lo guardado
  bool filtered;    // hay media en curso (descarga)
  uint32_t vcomp_mv_x16;  // media de la tension corregida, en 1/16 mV
  uint32_t charge_upct;   // carga, en millonesimas de punto (con red)
} BatteryGauge;

#define BATTERY_GAUGE_INITIALIZER \
  { BATT_GAUGE_UNKNOWN, 0u, 0u, false, false, 0u, 0u }

// El ultimo porcentaje guardado, al arrancar. No cambia el estado (sigue
// UNKNOWN hasta la primera lectura): solo da el punto de partida.
void battery_gauge_restore(BatteryGauge *g, uint8_t percent);

// Una lectura del cargador. `valid` false (fallo de I2C) no cambia nada.
// `ac_present`: hay red. `full`: el cargador ya paso a flotacion.
// `dt_ms`: tiempo desde la lectura anterior (para integrar la carga).
// `dischg_ma`: corriente que sale de la bateria. Sin red la da el shunt de
// sistema de la placa (in3.system_current), no el ADC IDCHG del BQ25730: en
// banco ese marcaba 0 con 6,7 A saliendo de la bateria. Con red si vale el
// ADC IDCHG (la bateria completando al convertidor), y se descuenta de la
// carga integrada.
void battery_gauge_sample(BatteryGauge *g, bool valid, bool ac_present,
                          bool full, uint16_t vbat_mv, uint16_t dischg_ma,
                          uint16_t ichg_ma, uint32_t dt_ms);

// Curva de reposo: tension (mV) -> porcentaje 0..100.
uint8_t battery_gauge_percent_from_mv(uint32_t mv);

// Porcentaje -> rayas 0..BATT_GAUGE_LEVEL_MAX.
uint8_t battery_gauge_level_from_percent(uint8_t percent);

// Estado empaquetado para publicarlo entre hilos de una sola escritura:
// state (bits 16..23), level (8..15), percent (0..7).
uint32_t battery_gauge_pack(const BatteryGauge *g);

// Condicion de ALARM_BATTERY_LOW sobre un estado empaquetado: descargando y
// con BATT_GAUGE_LOW_PCT o menos. Con una 12 V 9 Ah y el consumo de la
// incubadora en bateria, el 10 % son del orden de 10-15 min: el margen para
// conectar la red antes de que el equipo se apague. Como el porcentaje solo
// baja durante la descarga, no necesita antirrebote; al volver la red se
// retira en la misma lectura.
#define BATT_GAUGE_LOW_PCT 10u
bool battery_gauge_report_low(uint32_t report);

#ifdef __cplusplus
}
#endif
