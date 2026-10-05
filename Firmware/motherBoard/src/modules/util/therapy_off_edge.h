#pragma once

#include <stdbool.h>
#include <stdint.h>

// Publicacion inmediata a ThingsBoard al apagar la fototerapia o el control
// (termorregulacion/humedad).
//
// Para que existe: por GPRS el periodo de publicacion depende del estado
// (GPRSSetPostPeriod(), GPRS.cpp): 60 s controlando, 180 s solo con
// fototerapia y 3600 s en standby. Al apagar, el equipo pasa a standby y la
// siguiente publicacion podia tardar HASTA UNA HORA; mientras tanto el panel
// seguia mostrando phototherapy_active / control_active a true. Este modulo
// detecta el flanco de apagado y deja un envio pendiente que la tarea GPRS
// despacha en cuanto puede publicar.
//
// Solo el apagado, a proposito: es el alcance acordado. El encendido sigue
// esperando al periodo normal.
//
// Logica pura a proposito, como wifi_dwell: sin Arduino, para que entre en
// [env:native] y se pruebe con Unity. El estado lo guarda el llamante
// (GPRS.cpp) y los tiempos son millis() de 32 bits.

#ifdef __cplusplus
extern "C" {
#endif

// Separacion minima entre una publicacion (de cualquier tipo) y un envio
// forzado. Encender y apagar varias veces seguidas no debe convertirse en una
// rafaga de publicaciones por el SIM800; el pendiente no se pierde, solo
// espera a que venza.
#define THERAPY_OFF_EDGE_MIN_GAP_MS 10000u

typedef struct {
  bool primed;             // ya hay un estado de partida con el que comparar
  bool photoWasOn;         // ultimo estado observado de la fototerapia
  bool controlWasOn;       // ultimo estado observado del control
  bool pending;            // hay un apagado que ThingsBoard aun no ha visto
  bool anyPublished;       // ha salido alguna publicacion desde el arranque
  uint32_t lastPublishMs;  // millis() de la ultima publicacion
} TherapyOffEdge;

// Estado de un equipo recien arrancado: sin estado de partida ni pendiente.
void therapy_off_edge_init(TherapyOffEdge *st);

// Aplica el estado actual. Llamar en cada vuelta de la tarea, haya conexion o
// no. La primera llamada solo fija el estado de partida: un equipo que arranca
// apagado no ha apagado nada. Un paso de encendido a apagado en cualquiera de
// las dos deja un envio pendiente.
void therapy_off_edge_observe(TherapyOffEdge *st, bool photoOn,
                              bool controlOn);

// true si hay que publicar YA: hay un apagado pendiente y ha pasado la
// separacion minima desde la ultima publicacion (o no ha habido ninguna).
bool therapy_off_edge_due(const TherapyOffEdge *st, uint32_t nowMs);

// Avisa de que ha salido una publicacion de telemetria, forzada o periodica.
// Cualquiera de las dos despacha el pendiente: se construye con el estado
// actual, que ya es el apagado.
void therapy_off_edge_published(TherapyOffEdge *st, uint32_t nowMs);

#ifdef __cplusplus
}
#endif
