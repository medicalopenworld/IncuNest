#pragma once
#include "alarm_ids.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Patron de rafaga y de pulso de las Tablas 3 y 4 de IEC 60601-1-8.
//
// Vive en shared/ porque lo emiten DOS transductores distintos: el zumbador
// pasivo de la motherBoard (PWM, Buzzer.cpp) para todas las condiciones, y el
// zumbador del display (STC8H1K28 por I2C, on/off) para la unica condicion que
// el display detecta por su cuenta — la perdida de enlace con la placa.
//
// Que las dos copias divergieran seria un fallo SILENCIOSO y ademas normativo:
// dos senales anunciando la misma condicion con ritmos distintos es la
// inconsistencia que prohibe 6.3.3.1. Estuvieron a punto de ser dos copias.
//
// Tabla 3, numero de pulsos e intervalo ENTRE rafagas:
//   ALTA  10 pulsos, 2,5 s a 15 s
//   MEDIA  3 pulsos, 2,5 s a 30 s
//   BAJA   1 o 2 pulsos, > 15 s o sin repeticion
//
// Tabla 3, espaciado ENTRE pulsos dentro de la rafaga:
//   x entre 50 y 125 ms, y entre 125 y 250 ms, con variacion <= 20 % dentro
//   de la misma rafaga. En ALTA el hueco entre el 5o y el 6o pulso vale
//   2x + y: eso parte los diez pulsos en DOS GRUPOS DE CINCO, que es lo que
//   hace reconocible el patron de prioridad ALTA frente a otro equipo de la
//   misma sala. No es adorno: sin ese hueco la rafaga suena como un tren
//   monotono de diez y deja de ser el patron de la norma.
//   En MEDIA el espaciado es y.
//
// Tabla 4, duracion efectiva del pulso:
//   ALTA 75 a 200 ms; MEDIA y BAJA 125 a 250 ms. 150 ms cumple las dos, por
//   eso hay un unico valor.
//   RISE TIME entre el 10 % y el 40 % de la duracion del pulso; FALL TIME lo
//   bastante corto para que dos pulsos no se solapen.
//
// Las ventanas de la norma se comprueban con static_assert en Buzzer.cpp, que
// es el unico sitio que ve a la vez estas constantes y las de alarm_machine.h.
// ATADAS a ALARM_MIN_BURST_MS_HIGH/_MEDIUM (alarm_machine.h): esas dos fijan
// cuanto audio exige 6.10 completar aunque la condicion se haya ido, y esa
// duracion sale de los pulsos de aqui. Ya divergieron una vez.
#define ALARM_PULSE_MS            150u
#define ALARM_PULSE_RISE_MS        30u  // 20 % de 150: dentro de 10..40 %
#define ALARM_PULSE_FALL_MS        30u
#define ALARM_PULSE_SPACING_X_MS  100u  // x, ventana 50..125 ms
#define ALARM_PULSE_SPACING_Y_MS  200u  // y, ventana 125..250 ms
// Hueco entre el 5o y el 6o pulso de ALTA (2x + y).
#define ALARM_GROUP_GAP_MS   (2u * ALARM_PULSE_SPACING_X_MS + ALARM_PULSE_SPACING_Y_MS)
#define ALARM_BURST_PULSES_HIGH   10u
#define ALARM_BURST_PULSES_MEDIUM 3u
#define ALARM_BURST_PULSES_LOW    1u
#define ALARM_BURST_PERIOD_MS_HIGH   10000u
#define ALARM_BURST_PERIOD_MS_MEDIUM 25000u
#define ALARM_BURST_PERIOD_MS_LOW    30000u

// Duracion de cada rafaga completa, derivada de lo de arriba. El intervalo
// ENTRE rafagas que ve el oyente es periodo - duracion:
//   ALTA  10*150 + 8*100 + 400 = 2700 ms -> 7300 ms de silencio
//   MEDIA  3*150 + 2*200       =  850 ms -> 24150 ms
//   BAJA   1*150               =  150 ms -> 29850 ms
// Se cumple ademas el orden que pide la Tabla 3: el intervalo entre rafagas
// de MEDIA es >= el de ALTA, y el de BAJA >= el de MEDIA.
#define ALARM_BURST_LEN_MS_HIGH                                                \
  (ALARM_BURST_PULSES_HIGH * ALARM_PULSE_MS +                                  \
   (ALARM_BURST_PULSES_HIGH - 2u) * ALARM_PULSE_SPACING_X_MS +                 \
   ALARM_GROUP_GAP_MS)
#define ALARM_BURST_LEN_MS_MEDIUM                                              \
  (ALARM_BURST_PULSES_MEDIUM * ALARM_PULSE_MS +                                \
   (ALARM_BURST_PULSES_MEDIUM - 1u) * ALARM_PULSE_SPACING_Y_MS)

// Periodo de rafaga de una prioridad. Prioridad no reconocida -> ALTA, misma
// eleccion seguridad-primero que alarm_priority().
uint32_t alarm_audio_burst_period_ms(int priority);

// true si el transductor debe estar ENCENDIDO en el instante `elapsed_ms`,
// contado desde el inicio de la rafaga en curso.
//
// Sin estado y sin reloj: no llama a millis() ni toca hardware. Esa es la
// razon de que exista como funcion aparte del motor de la motherBoard — es lo
// que la hace ejercitable en el [env:native], el unico entorno de test real
// del proyecto. El display no tiene entorno propio, asi que sacar aqui la
// aritmetica es la unica forma de que su temporizacion tenga tests.
//
// `elapsed_ms` mayor que la duracion de la rafaga devuelve false (silencio
// entre rafagas); el llamante decide si aplica el modulo del periodo para
// repetir indefinidamente o si la rafaga es unica.
bool alarm_audio_pulse_on(uint32_t elapsed_ms, int priority);

#ifdef __cplusplus
}
#endif
