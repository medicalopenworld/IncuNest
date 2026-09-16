#include "platform/plat_gpio.h"

#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "plat_gpio";

void pin_mode(uint8_t pin, pin_mode_t mode) {
  if (pin == PIN_NONE) {
    return; // senal no cableada en esta revision; ver plat_gpio.h
  }
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

void pin_write(uint8_t pin, bool level) {
  if (pin == PIN_NONE) {
    return;
  }
  gpio_set_level(pin, level ? 1 : 0);
}

bool pin_read(uint8_t pin) {
  if (pin == PIN_NONE) {
    return false;
  }
  return gpio_get_level(pin) != 0;
}

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

// --- Interrupciones de GPIO -------------------------------------------------
// ESP-IDF pasa un argumento al manejador y Arduino no. Se guarda el puntero a
// la funcion del usuario por pin y un trampolin hace de puente. La tabla se
// indexa por GPIO, asi que no hay busqueda dentro de la ISR.

#include "hal/gpio_types.h"

static void (*s_isr_handlers[GPIO_NUM_MAX])(void);
static bool s_isr_service_installed;

static void IRAM_ATTR plat_gpio_isr_trampoline(void *arg) {
  const uint32_t pin = (uint32_t)(uintptr_t)arg;
  if (pin < GPIO_NUM_MAX && s_isr_handlers[pin] != NULL) {
    s_isr_handlers[pin]();
  }
}

void pin_attach_interrupt(uint8_t pin, void (*handler)(void),
                          pin_int_mode_t mode) {
  if (pin == PIN_NONE) {
    return;
  }
  if (pin >= GPIO_NUM_MAX || handler == NULL) {
    ESP_LOGE(TAG, "pin_attach_interrupt(%u) invalido", pin);
    return;
  }

  gpio_int_type_t type = GPIO_INTR_POSEDGE;
  switch (mode) {
  case PIN_INT_FALLING:    type = GPIO_INTR_NEGEDGE;  break;
  case PIN_INT_CHANGE:     type = GPIO_INTR_ANYEDGE;  break;
  case PIN_INT_LOW_LEVEL:  type = GPIO_INTR_LOW_LEVEL;  break;
  case PIN_INT_HIGH_LEVEL: type = GPIO_INTR_HIGH_LEVEL; break;
  case PIN_INT_RISING:
  default:                 type = GPIO_INTR_POSEDGE;  break;
  }

  if (!s_isr_service_installed) {
    // Arduino instalaba el servicio por su cuenta en el primer
    // attachInterrupt(); aqui se hace igual, la primera vez que hace falta.
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
      ESP_LOGE(TAG, "gpio_install_isr_service -> %s", esp_err_to_name(err));
      return;
    }
    s_isr_service_installed = true;
  }

  s_isr_handlers[pin] = handler;
  gpio_set_intr_type(pin, type);
  gpio_isr_handler_add(pin, plat_gpio_isr_trampoline,
                       (void *)(uintptr_t)pin);
  gpio_intr_enable(pin);
}

void pin_detach_interrupt(uint8_t pin) {
  if (pin >= GPIO_NUM_MAX) {
    return;
  }
  gpio_intr_disable(pin);
  gpio_isr_handler_remove(pin);
  s_isr_handlers[pin] = NULL;
}
