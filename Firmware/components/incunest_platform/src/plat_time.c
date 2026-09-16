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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "esp_sntp.h"

void yield(void) { taskYIELD(); }

void configTime(long gmtOffset_sec, int daylightOffset_sec, const char *server1,
                const char *server2, const char *server3) {
  if (esp_sntp_enabled()) {
    esp_sntp_stop();
  }
  esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
  if (server1) esp_sntp_setservername(0, server1);
  if (server2) esp_sntp_setservername(1, server2);
  if (server3) esp_sntp_setservername(2, server3);
  esp_sntp_init();

  // Misma construccion de TZ que Arduino (setTimeZone): POSIX invierte el
  // signo del offset, y el horario de verano va como segundo campo.
  char tz[40];
  long off = -gmtOffset_sec;
  int dst = daylightOffset_sec / 3600;
  snprintf(tz, sizeof(tz), "UTC%+ld:%02ld:%02ld%s", off / 3600, labs(off % 3600) / 60,
           labs(off % 60), dst != 0 ? "DST" : "");
  if (gmtOffset_sec == 0 && daylightOffset_sec == 0) {
    snprintf(tz, sizeof(tz), "UTC0");
  }
  setenv("TZ", tz, 1);
  tzset();
}
