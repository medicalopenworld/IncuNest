// Punto de entrada de ESP-IDF para la motherBoard.
//
// Antes lo ponia Arduino: su loopTask llamaba a setup() una vez y luego a
// loop() en bucle. setup() y loop() siguen viviendo en src/main.cpp con su
// contenido intacto, para que el diff del porte no se mezcle con un
// reordenado del arranque.

#include "platform/plat_nvs.h"

void setup();
void loop();

extern "C" void app_main(void) {
  // Arduino inicializaba la particion NVS por su cuenta antes de setup().
  // Aqui es explicito, y va primero porque setup() lee configuracion de NVS
  // casi de inmediato.
  plat_nvs_init();

  setup();
  for (;;) {
    loop();
  }
}
