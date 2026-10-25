#include "backlight.h"

#include <Arduino.h>
#include <Wire.h>
#include "esp_log.h"

#include "display_config.h"
#include "main.h" // I2C_CMD_BUZZER_ON
#include "modules/brightness/brightness_policy.h"

static const char *TAG = "Backlight";

// brightness_policy.h duplica los bytes para compilar en host: aqui se
// comprueba que siguen siendo los de display_config.h.
static_assert(BRIGHTNESS_BYTE_FULL == DISPLAY_BL_ON_VALUE,
              "bytes de brillo desincronizados con display_config.h");
static_assert(BRIGHTNESS_BYTE_OFF == DISPLAY_BL_OFF_VALUE,
              "byte de apagado desincronizado con display_config.h");
// Los bytes de brillo jamas pueden alcanzar los opcodes de audio (246..249).
static_assert(BRIGHTNESS_BYTE_DIMMEST < DISPLAY_BL_OFF_VALUE,
              "el byte mas oscuro debe quedar por debajo del apagado");
static_assert(BRIGHTNESS_BYTE_DIMMEST < I2C_CMD_BUZZER_ON &&
                  BRIGHTNESS_BYTE_OFF < I2C_CMD_BUZZER_ON,
              "un byte de brillo no puede alcanzar los opcodes de audio");

// Una transaccion de un byte. true si el chip la ACKeo.
static bool writeByte(uint8_t value) {
  Wire.beginTransmission((uint8_t)DISPLAY_I2C_ADDR_BL);
  Wire.write(value);
  const uint8_t err = Wire.endTransmission();
  if (err != 0) {
    ESP_LOGE(TAG, "write 0x%02X: I2C err=%u", (unsigned)value, (unsigned)err);
    return false;
  }
  return true;
}

bool Backlight_SetPercent(uint8_t pct, uint8_t attempts) {
  if (attempts < 1)
    attempts = 1;
  const uint8_t level = Brightness_ClampPct(pct);
  const uint8_t value =
      Brightness_PercentToByte(level, DISPLAY_BL_LEVELS_SUPPORTED != 0);
  for (uint8_t i = 0; i < attempts; i++) {
    if (writeByte(value)) {
      if (i > 0)
        ESP_LOGI(TAG, "%u%% (0x%02X): OK tras %u reintento(s)", (unsigned)level,
                 (unsigned)value, (unsigned)i);
      return true;
    }
    if (i + 1 < attempts)
      vTaskDelay(pdMS_TO_TICKS(50));
  }
  return false;
}

bool Backlight_Off(void) {
  for (uint8_t i = 0; i < 3; i++) {
    if (writeByte((uint8_t)DISPLAY_BL_OFF_VALUE))
      return true;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  return false;
}
