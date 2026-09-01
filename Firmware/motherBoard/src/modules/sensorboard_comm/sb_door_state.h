#pragma once
// Estado de la puerta segun los eventos del SensorBoard, con el contrato de
// fail-safe de su README (#Puerta): con una sola linea digital el hall
// DRV5032 no puede distinguir "puerta abierta" de "hall desconectado o
// averiado", asi que un flapping open/closed se trata como sensor
// sospechoso, nunca como entrada de control.
//
// El SensorBoard ya publica solo cambios estables (debounce propio) y
// re-afirma el estado cada 30 s; una re-asercion del MISMO estado no es una
// transicion y no cuenta para el flapping.
#include <stdbool.h>
#include <stdint.h>

// Umbral de implausibilidad: 4 transiciones reales dentro de 60 s. Con el
// debounce del SensorBoard por debajo, esa cadencia ya no es una puerta que
// alguien abre y cierra, es una senal inestable. Se auto-limpia: cuando la
// mas vieja de las 4 sale de la ventana, la condicion desaparece sola (la
// alarma no es latching, ver alarm_policy.cpp).
#define SB_DOOR_FLAP_WINDOW_MS 60000u
#define SB_DOOR_FLAP_TRANSITIONS_FOR_FAULT 4u

typedef struct {
  bool has_state;
  bool open;
  uint32_t transitions_ms[SB_DOOR_FLAP_TRANSITIONS_FOR_FAULT];
  uint8_t transition_count;
} SbDoorState;

void sb_door_state_init(SbDoorState *s);

// event_open: true para "door_open", false para "door_closed".
void sb_door_state_note_event(SbDoorState *s, bool event_open,
                              uint32_t now_ms);

// false mientras no haya llegado ningun evento (arranque sin estado).
bool sb_door_state_get(const SbDoorState *s, bool *open_out);

// true mientras la ventana de flapping siga llena de transiciones.
bool sb_door_state_is_faulty(const SbDoorState *s, uint32_t now_ms);
