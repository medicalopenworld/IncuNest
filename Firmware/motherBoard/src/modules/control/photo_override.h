#pragma once

// Encendido de fototerapia desde el modo depuracion, separado del hardware.
//
// Para que existe: probar el lazo de intensidad en banco sin que haga falta
// alguien pulsando la pantalla. Se manda por la consola UART ("PHOTO,<0|1>",
// main.cpp) y solo con el modo depuracion encendido.
//
// Por que no basta con poner in3.phototherapy a 1: el display ADOPTA el estado
// de fototerapia que emite la placa (Display_ApplyCtrlState, a 1 Hz) y lo
// devuelve en su keepalive, y la placa aplica ese keepalive y lo guarda en NVS.
// Un encendido ingenuo acaba asi:
//
//   1. la placa emite photo=1, el display lo adopta y empieza a mandar photo=1;
//   2. al terminar la prueba la placa vuelve a "lo que diga el display"... que
//      ya es 1: la lampara SE QUEDA ENCENDIDA;
//   3. y como ese 1 se ha guardado en NVS, una caida lo reanuda: una accion de
//      depuracion sobrevive a un reinicio, contra las reglas 1 y 2 del modo
//      depuracion (debug_mode.h).
//
// Esta maquina decide, trama a trama, que valor de fototerapia aplicar y si el
// del display se puede guardar. Mientras el encendido de depuracion esta
// activo, manda ON y no se guarda nada. Al soltarlo manda el valor REAL que
// habia antes de la prueba e ignora el 1 rancio que el display sigue
// devolviendo, y no guarda nada, HASTA que el display confirma devolviendo ese
// valor real (ver mas abajo). Entonces manda otra vez el display.
//
// Test de regresion: motherBoard/test/test_photo_override.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Liberacion CONFIRMADA.
//
// (Antes era una ventana fija de 5 s, PHOTO_OVERRIDE_RELEASE_GRACE_MS, durante
// la que se ignoraba al display.) Una ventana fija es una apuesta a que el display adopta el apagado dentro de
// ella, y la tarea Comm del display se queda sin CPU segundos enteros
// (known_issues #7, #11). Si vence antes de que el display lo haya adoptado,
// main.cpp aplica Y GUARDA EN NVS el ON rancio: lampara encendida y
// reanudable tras un reinicio, justo lo que esta maquina existe para evitar.
//
// Ahora, tras soltar, la placa fuerza `real_before` hasta que llega un
// keepalive cuyo valor ES `real_before` (el display ya adopto lo que la placa
// emite). No se acepta una confirmacion antes de PHOTO_OVERRIDE_CONFIRM_MIN_MS:
// una trama del display ya en vuelo de antes de adoptar el ON tambien lleva
// `real_before` y no demuestra nada.
//
// PHOTO_OVERRIDE_RELEASE_BACKSTOP_MS es solo el plazo tras el que se AVISA
// (photo_override_unconfirmed_overdue): vencido, la placa sigue forzando
// `real_before` y sin guardar; NUNCA adopta el valor del display sin
// confirmacion.
#define PHOTO_OVERRIDE_CONFIRM_MIN_MS 2000u
#define PHOTO_OVERRIDE_RELEASE_BACKSTOP_MS 60000u

typedef struct {
  bool active;             // el encendido de depuracion esta forzando ON
  bool real_before;        // fototerapia REAL (la del display) al empezar
  bool releasing;          // soltado, a la espera de la confirmacion
  bool overdue_reported;   // ya se aviso de que vencio el plazo largo
  uint32_t release_since_ms;
} PhotoOverride;

// Verdadero UNA sola vez cuando, soltado el encendido, ha vencido
// PHOTO_OVERRIDE_RELEASE_BACKSTOP_MS sin confirmacion. Para que el llamador lo
// registre como error; el estado de la maquina no cambia.
bool photo_override_unconfirmed_overdue(PhotoOverride *o, uint32_t now_ms);

void photo_override_init(PhotoOverride *o);

// Empieza a forzar ON. `real_now` es el estado que el display habia mandado
// hasta ahora, antes de que pueda contaminarlo la adopcion. Si ya estaba
// activo, o seguia en la ventana de gracia de una prueba anterior, se conserva
// el real_before que ya habia: el de ahora esta contaminado por la prueba.
void photo_override_start(PhotoOverride *o, bool real_now, uint32_t now_ms);

// Suelta el encendido y abre la ventana de gracia. Sin efecto si no estaba
// activo.
void photo_override_release(PhotoOverride *o, uint32_t now_ms);

bool photo_override_active(const PhotoOverride *o);

// Valor de fototerapia que hay que APLICAR dada la trama del display.
bool photo_override_effective(PhotoOverride *o, bool display_value,
                              uint32_t now_ms);

// Si el valor del display se puede guardar en NVS en esta pasada.
bool photo_override_may_persist(PhotoOverride *o, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
