#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "alarm_policy.h"

#ifdef __cplusplus
extern "C" {
#endif

// Prueba de funcionamiento de las senales de alarma, a peticion del operador.
//
// IEC 60601-2-19 201.12.3.105: "Means shall be provided for the OPERATOR to
// check the operation of audible and visual alarms. Such means shall be
// described in the instructions for use." Es un requisito, no una comodidad:
// sin el, un zumbador averiado o un banner que no se pinta pasan inadvertidos
// hasta que hace falta la alarma de verdad.
//
// La prueba reproduce UNA rafaga de cada prioridad, de menor a mayor, por el
// MISMO camino de audio que las alarmas reales (buzzerAlarmUpdate). Eso es
// justamente lo que le da valor: no comprueba una imitacion, comprueba la
// cadena que sonara cuando haya una alarma. La senal visual la pinta el
// display a partir de la prioridad que se publica aqui.
//
// NO toca actuadores ni declara condicion alguna en la maquina de alarmas.
//
// Logica pura con el tiempo inyectado, para poder verificar la secuencia en
// host sin esperar a que suene.

// Prioridad que la prueba esta reproduciendo ahora mismo, o este valor si no
// hay prueba en curso. No se usa -1 porque AlarmPriority es un enum sin
// signo garantizado.
#define ALARM_TEST_IDLE 0xFF

// Duracion de cada tramo con audio. Cada una tiene que superar la rafaga de su
// prioridad; lo comprueban static_assert en Buzzer.cpp, que es el unico sitio
// que ve a la vez estas constantes y las del patron (main.h no llega al
// entorno de test nativo). El sobrante de cada tramo es silencio.
#define ALARM_TEST_PHASE_MS_LOW     600u
#define ALARM_TEST_PHASE_MS_MEDIUM 1200u
#define ALARM_TEST_PHASE_MS_HIGH   3000u
#define ALARM_TEST_GAP_MS           500u

void alarm_test_init(void);

// Arranca la secuencia. Devuelve false si ya habia una en curso.
//
// El llamante debe rechazarla si hay cualquier condicion senalizando: una
// prueba no puede pisar una alarma real. Ver driveAlarmBuzzer().
bool alarm_test_start(uint32_t now_ms);

// Hace avanzar la secuencia. Llamar periodicamente.
void alarm_test_tick(uint32_t now_ms);

// Corta la prueba de inmediato. Idempotente. La llama el motor de audio en
// cuanto aparece una alarma real.
void alarm_test_abort(void);

bool alarm_test_active(void);

// true mientras toca emitir audio. Alterna con los huecos entre rafagas.
bool alarm_test_audio_required(void);

// Prioridad en curso, o ALARM_TEST_IDLE. El display la usa para pintar el
// banner con el color y el parpadeo de esa prioridad, de modo que la prueba
// ejercita tambien la senal visual y no solo el zumbador.
uint8_t alarm_test_priority(void);

#ifdef __cplusplus
}
#endif
