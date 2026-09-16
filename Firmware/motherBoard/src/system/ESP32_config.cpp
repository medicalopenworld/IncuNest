/*
  MIT License

  Copyright (c) 2022 Medical Open World, Pablo Sánchez Bergasa

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

*/

#include "ESP32_config.h"

#include "esp_log.h"
void watchdogInit(uint32_t wdt_timeout) {
  // PORTE A ESP-IDF 6: la API del Task WDT cambio de
  //     esp_task_wdt_init(uint32_t timeout_SEGUNDOS, bool panic)
  // a una estructura de configuracion cuyo campo es timeout_MILISEGUNDOS. El
  // *1000 no es cosmetico: sin el, el watchdog de 75 s pasaria a 75 ms y la
  // placa entraria en bucle de reinicio nada mas arrancar.
  //
  // Ademas, en IDF 4.4 volver a llamar a esp_task_wdt_init() sobre un TWDT ya
  // arrancado reconfiguraba el plazo. En IDF 6 devuelve ESP_ERR_INVALID_STATE
  // y hay que usar esp_task_wdt_reconfigure(). Aqui pasa siempre, porque
  // sdkconfig trae CONFIG_ESP_TASK_WDT_INIT=y y el TWDT ya esta en marcha
  // (a 5 s) cuando initHardware lo sube a WDT_TIMEOUT.
  esp_task_wdt_config_t cfg = {
      .timeout_ms = wdt_timeout * 1000U,
      // Mismos nucleos vigilados que tuviera la configuracion de arranque: se
      // toma de sdkconfig en vez de fijarlo aqui, para no cambiar en silencio
      // que tareas idle estan suscritas.
      .idle_core_mask =
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0
          (1 << 0) |
#endif
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1
          (1 << 1) |
#endif
          0,
      .trigger_panic = true, // que la placa reinicie, como antes
  };

  // Se RECONFIGURA primero y solo se inicializa si hiciera falta, no al reves:
  // sdkconfig trae CONFIG_ESP_TASK_WDT_INIT=y, asi que al llegar aqui el TWDT
  // SIEMPRE esta ya en marcha (a 5 s) y llamar antes a esp_task_wdt_init()
  // hacia que ESP-IDF soltara un ESP_LOGE "TWDT already initialized" en cada
  // arranque, visto en banco el 2026-09-11. El plazo quedaba bien igualmente,
  // pero un error en rojo en el log de arranque de la placa que gobierna el
  // calefactor no es ruido aceptable.
  esp_err_t err = esp_task_wdt_reconfigure(&cfg);
  if (err == ESP_ERR_INVALID_STATE) {
    err = esp_task_wdt_init(&cfg);
  }
  if (err != ESP_OK) {
    ESP_LOGE("WDT", "no se pudo configurar el Task WDT: %s",
             esp_err_to_name(err));
  }

  esp_task_wdt_add(NULL); // add current thread to WDT watch
}

void watchdogReload() { esp_task_wdt_reset(); }

void brownOutConfig(uint32_t val) {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, val);
}
