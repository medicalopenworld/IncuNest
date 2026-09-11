// Punto de entrada de ESP-IDF.
//
// REPRODUCE EL ARRANQUE DE ARDUINO, Y NO ES UN DETALLE.
// arduino-esp32 no llamaba a setup()/loop() desde app_main: creaba una tarea
// propia, "loopTask", y las ejecutaba ALLI. Tres parametros de esa tarea
// forman parte del comportamiento del firmware:
//
//   - Pila de 8192 B (CONFIG_ARDUINO_LOOP_STACK_SIZE). La main_task de
//     ESP-IDF tiene 4096: con ella, setup() de la motherBoard desbordaba la
//     pila nada mas cargar la configuracion de NVS ("A stack overflow in
//     task main has been detected", visto en banco el 2026-09-11).
//   - NUCLEO 1 (CONFIG_ARDUINO_RUNNING_CORE=1). main_task corre en el 0. Todo
//     el reparto de tareas del firmware —que fija afinidad a mano con
//     xTaskCreatePinnedToCore— se calibro con setup() y loop() en el nucleo 1.
//   - Prioridad 1, la mas baja por encima de idle.
//
// Cuando app_main() termina, ESP-IDF borra main_task; loopTask se queda, que
// es exactamente lo que pasaba antes.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "platform/plat_nvs.h"

void setup();
void loop();

static void loopTask(void *pv) {
  (void)pv;
  setup();
  for (;;) {
    loop();
  }
}

extern "C" void app_main(void) {
  // Arduino inicializaba la particion NVS por su cuenta antes de setup().
  // Aqui es explicito, y va primero porque setup() lee configuracion de NVS
  // casi de inmediato.
  plat_nvs_init();

  xTaskCreatePinnedToCore(loopTask, "loopTask", 8192, NULL, 1, NULL, 1);
}
