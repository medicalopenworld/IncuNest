#include "platform/plat_gpio.h"

#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "plat_gpio";

void pin_mode(uint8_t pin, pin_mode_t mode) {
  gpio_config_t cfg = {
      .pin_bit_mask = 1ULL << pin,
      .intr_type = GPIO_INTR_DISABLE,
  };
  switch (mode) {
  case PIN_MODE_OUTPUT:
    cfg.mode = GPIO_MODE_OUTPUT;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    break;
  case PIN_MODE_OUTPUT_OPEN_DRAIN:
    cfg.mode = GPIO_MODE_OUTPUT_OD;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    break;
  case PIN_MODE_INPUT_PULLUP:
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    break;
  case PIN_MODE_INPUT_PULLDOWN:
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
    break;
  case PIN_MODE_INPUT:
  default:
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    break;
  }
  esp_err_t err = gpio_config(&cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "gpio_config(%u) -> %s", pin, esp_err_to_name(err));
  }
}

void pin_write(uint8_t pin, bool level) { gpio_set_level(pin, level ? 1 : 0); }

bool pin_read(uint8_t pin) { return gpio_get_level(pin) != 0; }

// --- ADC -------------------------------------------------------------------
// Las unidades y sus handles de calibracion se crean la primera vez que se
// lee un pin de esa unidad, no en un init global: asi el ADC solo se enciende
// en las placas que de verdad lo usan (hoy, un unico pin por placa via el HAL).

static adc_oneshot_unit_handle_t s_unit[SOC_ADC_PERIPH_NUM];
static adc_cali_handle_t s_cali[SOC_ADC_PERIPH_NUM];

static bool adc_unit_ready(adc_unit_t unit) {
  if (s_unit[unit] != NULL) {
    return true;
  }
  adc_oneshot_unit_init_cfg_t init = {.unit_id = unit};
  if (adc_oneshot_new_unit(&init, &s_unit[unit]) != ESP_OK) {
    ESP_LOGE(TAG, "adc_oneshot_new_unit(%d) fallo", (int)unit);
    s_unit[unit] = NULL;
    return false;
  }
  // La curva de fabrica es lo que hacia analogReadMilliVolts(); sin ella la
  // lectura en mV seria una regla de tres y perderiamos exactitud frente al
  // firmware anterior. Si no hay eFuse de calibracion, se sigue sin ella y
  // adc_read_mv() devuelve 0 antes que un valor inventado.
  adc_cali_curve_fitting_config_t cali = {
      .unit_id = unit,
      .atten = ADC_ATTEN_DB_12,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
  };
  if (adc_cali_create_scheme_curve_fitting(&cali, &s_cali[unit]) != ESP_OK) {
    ESP_LOGW(TAG, "unidad ADC %d sin calibracion de fabrica", (int)unit);
    s_cali[unit] = NULL;
  }
  return true;
}

uint32_t adc_read_mv(uint8_t pin) {
  adc_unit_t unit;
  adc_channel_t channel;
  if (adc_oneshot_io_to_channel(pin, &unit, &channel) != ESP_OK) {
    ESP_LOGE(TAG, "GPIO %u no tiene ADC", pin);
    return 0;
  }
  if (!adc_unit_ready(unit)) {
    return 0;
  }
  adc_oneshot_chan_cfg_t chan = {
      .atten = ADC_ATTEN_DB_12,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
  };
  if (adc_oneshot_config_channel(s_unit[unit], channel, &chan) != ESP_OK) {
    return 0;
  }
  int raw = 0;
  if (adc_oneshot_read(s_unit[unit], channel, &raw) != ESP_OK) {
    return 0;
  }
  if (s_cali[unit] == NULL) {
    return 0;
  }
  int mv = 0;
  if (adc_cali_raw_to_voltage(s_cali[unit], raw, &mv) != ESP_OK) {
    return 0;
  }
  return (uint32_t)mv;
}
