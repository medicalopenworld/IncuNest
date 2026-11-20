#pragma once

// Encendido de fototerapia desde el modo depuracion, separado del hardware.
//
// Para que existe: probar el lazo de intensidad en banco sin que haga falta
// alguien pulsando la pantalla. Se manda por la consola UART ("PHOTO,<0|1>",
// main.cpp) y solo con el modo depuracion encendido.
//
// QUE HACE (y por que es tan pequeno):
//
//   1. Mientras la prueba esta ACTIVA, la lampara va ON (`forced`), las
//      ordenes del display se REGISTRAN (`ordered`) pero no se aplican. El ON
//      forzado nunca se guarda en NVS (solo vive en in3.phototherapy); las
//      ordenes del display si, porque nunca vienen contaminadas (ver 2).
//   2. El campo `photo` de CTRL,STATE (photo_override_state_field) NO emite el ON forzado
//      sino la ultima orden del display (`ordered`). Asi el display nunca
//      adopta el ON de depuracion, y no existe ningun 1 rancio que neutralizar
//      al soltar. Fuera de la prueba el campo es el estado REAL de la lampara
//      (known_issues #20). El ON de la prueba se ve en /debug/state y en el log,
//      que es donde mira quien la hace.
//   3. SOLTAR devuelve el control al display EN EL ACTO: la lampara pasa a la
//      ultima orden del display (`ordered`: la del operador manda, o la que
//      habia al empezar si no hubo ninguna). Sin ventana de gracia, sin
//      confirmacion, sin plazos.
//
// Historia, para no repetirla. Un primer diseno hacia que el campo `photo`
// emitiese el ON forzado: el display lo ADOPTABA, lo devolvia en su keepalive,
// y la placa lo aplicaba y lo guardaba en NVS al soltar si el display aun no
// habia adoptado el apagado. Se arreglo primero con una ventana fija de 5 s y
// despues con una confirmacion (valor del display == valor real, minimo 2 s,
// respaldo de 60 s). Ninguna de las dos aguanto: con el temporizador vencido
// durante la liberacion el ON se auto-confirmaba y se guardaba como sesion
// continua, un OFF del operador en esa ventana se perdia, y un CTRL,STATE rancio
// procesado tarde por el anillo de RX del display seguia colandose. La raiz era
// emitir el ON; quitada, no hace falta neutralizar nada.
//
// Test de regresion: motherBoard/test/test_photo_override.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  bool active;           // la prueba esta forzando ON
  bool ordered;          // ultima orden del DISPLAY (operador), registrada
  bool release_pending;  // se solto y falta aplicar `ordered` a la lampara
} PhotoOverride;

void photo_override_init(PhotoOverride *o);

// Empieza a forzar ON. `ordered_now` es la ultima orden del display hasta
// ahora (in3.phototherapy antes de la prueba). Si ya estaba activa, o hay una
// liberacion pendiente de aplicar, se conserva el `ordered` que ya hay: es el
// que lleva las ordenes del operador y `ordered_now` esta contaminado.
void photo_override_start(PhotoOverride *o, bool ordered_now);

// Registra una orden del display (o la caducidad del temporizador de
// fototerapia, que es una orden de apagado de la placa) recibida durante la
// prueba. Sin efecto si no esta activa.
void photo_override_note_order(PhotoOverride *o, bool ordered);

// Suelta la prueba y deja pendiente aplicar `ordered`. Sin efecto si no
// estaba activa.
void photo_override_release(PhotoOverride *o);

bool photo_override_active(const PhotoOverride *o);

// Valor de fototerapia que hay que APLICAR dada la trama del display: ON
// mientras esta activa (y registra la orden), la del display si no.
bool photo_override_effective(PhotoOverride *o, bool display_value);


// Si la prueba esta forzando un valor ahora mismo (ON mientras esta activa).
// Sin efectos secundarios. Permite aplicarlo sin esperar una trama.
bool photo_override_forced(const PhotoOverride *o, bool *value);

// Verdadero UNA vez tras soltar, con el valor (`ordered`) que hay que aplicar
// a la lampara ahora, sin esperar ninguna trama del display (con el display
// colgado no llega ninguna). Quien lo llama lo aplica por el camino normal y
// NO lo persiste.
bool photo_override_take_release(PhotoOverride *o, bool *value);

// Valor del campo `photo` de CTRL,STATE: la orden del display mientras la
// prueba esta activa, el estado real aplicado (`applied`) en cualquier otro
// caso.
bool photo_override_state_field(const PhotoOverride *o, bool applied);

#ifdef __cplusplus
}
#endif
