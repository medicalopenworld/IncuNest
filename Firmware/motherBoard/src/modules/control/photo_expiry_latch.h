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
// Test: motherBoard/test/test_photo_expiry_latch.

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  bool expired;  // el temporizador vencio y aun no hemos visto photo=0
} PhotoExpiryLatch;

void photo_expiry_latch_init(PhotoExpiryLatch *l);

// El temporizador de fototerapia acaba de vencer.
void photo_expiry_latch_expire(PhotoExpiryLatch *l);

// Filtra el `photo` de una trama del display. Devuelve el valor que hay que
// usar en su lugar.
bool photo_expiry_latch_filter(PhotoExpiryLatch *l, bool photo, int photo_min);

#ifdef __cplusplus
}
#endif
