// Punto de entrada de ESP-IDF.
//
// Antes lo ponia Arduino: su loopTask llamaba a setup() una vez y luego a
// loop() en bucle. Aqui se hace explicito, que es lo unico que hacia falta
// para quitar esa dependencia. setup() y loop() siguen viviendo en src/main.cpp
// con su contenido intacto, para que el diff del porte no se mezcle con un
// reordenado del arranque.

#include "platform/plat_nvs.h"

void setup();
void loop();

extern "C" void app_main(void) {
  // Arduino inicializaba la particion NVS por su cuenta antes de setup().
  // Aqui hay que hacerlo a mano, y ANTES: lo primero que hace setup() es
  // abrir el namespace "diag" para contar arranques.
  plat_nvs_init();

  setup();
  for (;;) {
    loop();
  }
}
