// Toca una vez cada primitivo de components/platform para que el compilador y
// el enlazador tengan que resolverlos de verdad. No comprueba comportamiento
// (eso son los tests de host y el banco): comprueba que el porte sigue
// hablando la misma API que la ESP-IDF instalada.

#include "platform/plat_gpio.h"
#include "platform/plat_i2c.h"
#include "platform/plat_nvs.h"
#include "platform/plat_pwm.h"
#include "platform/plat_time.h"

#include "alarm_ids.h"
#include "alarm_policy.h"
#include "esp_log.h"

static const char *TAG = "smoke";

extern "C" void app_main(void) {
  plat_nvs_init();

  const uint32_t t0 = millis();
  delay_ms(1);
  ESP_LOGI(TAG, "millis delta=%lu micros=%lu ms64=%llu",
           (unsigned long)(millis() - t0), (unsigned long)micros(),
           (unsigned long long)millis64());

  pin_mode(2, PIN_MODE_OUTPUT);
  pin_write(2, true);
  ESP_LOGI(TAG, "gpio2=%d adc_mv(1)=%lu", (int)pin_read(2),
           (unsigned long)adc_read_mv(1));

  pwm_setup(2, 400, 8);
  pwm_attach(4, 2);
  pwm_write(2, 0);

  I2cBus bus;
  if (bus.begin(8, 9, 100000)) {
    uint8_t byte = 0;
    bus.probe(0x44);
    bus.readReg(0x44, 0x00, &byte, 1);
    bus.end();
  }

  NvsPrefs prefs;
  if (prefs.begin("smoke", false)) {
    prefs.putFloat("f", 1.5f);
    prefs.putUChar("u", 7);
    prefs.putString("s", "hola");
    ESP_LOGI(TAG, "nvs f=%.2f u=%u s=%s isKey=%d", prefs.getFloat("f"),
             prefs.getUChar("u"), prefs.getString("s").c_str(),
             (int)prefs.isKey("f"));
    prefs.clear();
    prefs.end();
  }

  // shared/: que el componente enlace de verdad, no solo que exista.
  ESP_LOGI(TAG, "prioridad de alarma FAN_FAILURE=%d",
           (int)alarm_priority(ALARM_FAN_FAILURE));
}
