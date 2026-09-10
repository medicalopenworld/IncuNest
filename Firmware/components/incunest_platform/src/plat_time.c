#include "platform/plat_time.h"

#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

uint32_t millis(void) { return (uint32_t)(esp_timer_get_time() / 1000LL); }

uint32_t micros(void) { return (uint32_t)esp_timer_get_time(); }

uint64_t millis64(void) { return (uint64_t)(esp_timer_get_time() / 1000LL); }

uint64_t micros64(void) { return (uint64_t)esp_timer_get_time(); }

void delay_ms(uint32_t ms) {
  // pdMS_TO_TICKS redondea hacia abajo: con tick de 1 kHz da lo esperado, pero
  // un delay_ms(1) con tick de 100 Hz se quedaria en 0 ticks y no cederia la
  // CPU. Se fuerza un tick minimo para que la llamada siempre ceda.
  TickType_t ticks = pdMS_TO_TICKS(ms);
  if (ticks == 0 && ms > 0) {
    ticks = 1;
  }
  vTaskDelay(ticks);
}

void delay_us(uint32_t us) { esp_rom_delay_us(us); }
