#pragma once

// Caducidad del temporizador de fototerapia frente a una trama rezagada del
// display, separada del hardware.
//
// Cuando el temporizador de la placa vence, la placa apaga la lampara y emite
// photo=0. Pero el display puede tener ya en vuelo una trama anterior con
// photo=1 y SIN minutos (photoMin=0), o la tiene retenida hasta 10 s por su
// guarda de confirmacion (localCmdHoldsLocal, known_issues #10/#11). Esa trama
// parece "una sesion continua nueva" y reenciende la lampara que acaba de
// apagarse por caducidad: sobretratamiento silencioso.
//
// Regla: tras la caducidad se IGNORA photo=1 sin minutos (photoMin<=0) hasta
// ver un photo=0 del display (que confirma que ya adopto el apagado). Una
// sesion nueva con temporizador (photo=1 y photoMin>0) es una orden inequivoca
// del operador y se acepta siempre.
//
// EXCEPCION POR DISENO, MODO FORMACION: el display manda photoMin=5
// (TRAINING_PHOTO_TIMER_MIN, Display_HMI/src/state/training_mode.h) para la
// fototerapia "continua" en formacion, como watchdog de la lampara. Al ser
// photoMin>0 esa trama se acepta SIEMPRE y se salta el cerrojo: no se filtra, y
// es lo correcto, porque en formacion la sesion es una orden explicita del
// operador y el temporizador de 5 min es el que la apaga.
//
// ADEMAS lleva dos avisos para el llamador, que no pueden hacerse dentro de la
// seccion critica ni en una ruta caliente: "persistir OFF pendiente" (al
// caducar hay que guardar KEY_PHOTO_ACTIVE=0, o una caida antes de la siguiente
// trama del display arranca en fototerapia continua sin limite; lo consume UNA
// sola tarea) y "loguear que se filtro una trama" (uno por episodio).
//
// Test: motherBoard/test/test_photo_expiry_latch.

#include <stdbool.h>

// Maximo de minutos de un temporizador de fototerapia del display (10 h). Es el
// mismo limite que PHOTO_TIMER_MAX_MINUTES de Display_HMI/include/main.h.
#ifndef PHOTO_TIMER_MAX_MINUTES
#define PHOTO_TIMER_MAX_MINUTES 600
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  bool expired;            // el temporizador vencio y aun no hemos visto photo=0
  bool persist_off_pending; // al caducar: falta guardar KEY_PHOTO_ACTIVE=0
  bool log_pending;        // se filtro una trama y falta loguearlo
  bool log_done;           // ya se logueo en este episodio
} PhotoExpiryLatch;

void photo_expiry_latch_init(PhotoExpiryLatch *l);

// El temporizador de fototerapia acaba de vencer.
void photo_expiry_latch_expire(PhotoExpiryLatch *l);

// Filtra el `photo` de una trama del display. Devuelve el valor que hay que
// usar en su lugar.
bool photo_expiry_latch_filter(PhotoExpiryLatch *l, bool photo, int photo_min);

// Verdadero UNA vez por caducidad: hay que guardar KEY_PHOTO_ACTIVE=0 en NVS.
// Lo consume una sola tarea (fuera de la seccion critica y de rutas calientes).
// Una segunda llamada a expire() dentro del mismo episodio no lo vuelve a armar,
// asi que dos tareas que detecten la caducidad a la vez no duplican la escritura.
bool photo_expiry_latch_take_persist_off(PhotoExpiryLatch *l);

// Verdadero UNA vez por episodio (desde expire()) cuando el cerrojo filtra una
// trama: el llamador la registra en el log, fuera de la seccion critica.
bool photo_expiry_latch_take_filter_log(PhotoExpiryLatch *l);

// Valida los campos de fototerapia de una trama HMI: photo en {0,1} y photoMin
// en 0..PHOTO_TIMER_MAX_MINUTES. Una trama corrupta (sin CRC, known_issues #2)
// se descarta ENTERA.
bool photo_hmi_fields_valid(int photo, int photo_min);

#ifdef __cplusplus
}
#endif
