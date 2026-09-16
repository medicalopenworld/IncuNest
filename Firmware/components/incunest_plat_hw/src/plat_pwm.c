#include "platform/plat_pwm.h"

#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "plat_pwm";

// El ESP32-S3 solo tiene modo de baja velocidad: 8 canales y 4 timers.
#define PWM_MODE LEDC_LOW_SPEED_MODE
#define PWM_NUM_CHANNELS 8

// Misma regla que aplicaba arduino-esp32 2.x en esp32-hal-ledc.c. Ver el
// comentario largo de plat_pwm.h: NO cambiar sin medir el calefactor.
static inline ledc_timer_t timer_of(uint8_t channel) {
  return (ledc_timer_t)((channel / 2) % 4);
}

typedef struct {
  bool timer_ready;
  bool attached;
  uint8_t resolution_bits;
  uint32_t pending_duty;
  bool has_pending_duty;
} pwm_chan_state_t;

static pwm_chan_state_t s_chan[PWM_NUM_CHANNELS];

uint32_t pwm_setup(uint8_t channel, uint32_t freq_hz, uint8_t resolution_bits) {
  if (channel >= PWM_NUM_CHANNELS) {
    ESP_LOGE(TAG, "canal %u fuera de rango", channel);
    return 0;
  }
  ledc_timer_config_t cfg = {
      .speed_mode = PWM_MODE,
      .duty_resolution = (ledc_timer_bit_t)resolution_bits,
      .timer_num = timer_of(channel),
      .freq_hz = freq_hz,
      .clk_cfg = LEDC_AUTO_CLK,
  };
  esp_err_t err = ledc_timer_config(&cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "ledc_timer_config(ch=%u timer=%d %luHz/%ub) -> %s", channel,
             (int)timer_of(channel), (unsigned long)freq_hz, resolution_bits,
             esp_err_to_name(err));
    return 0;
  }
  s_chan[channel].timer_ready = true;
  s_chan[channel].resolution_bits = resolution_bits;
  return ledc_get_freq(PWM_MODE, timer_of(channel));
}

void pwm_attach(uint8_t pin, uint8_t channel) {
  if (channel >= PWM_NUM_CHANNELS) {
    ESP_LOGE(TAG, "canal %u fuera de rango", channel);
    return;
  }
  if (!s_chan[channel].timer_ready) {
    // Arduino dejaba pasar esto en silencio y el pin se quedaba mudo. Aqui se
    // avisa, porque es justo el sintoma del canal 5 (humidificador), al que
    // nadie llama pwm_setup() — ver la nota del porte en docs/.
    ESP_LOGW(TAG, "pwm_attach(ch=%u) sin pwm_setup previo", channel);
  }
  ledc_channel_config_t cfg = {
      .gpio_num = pin,
      .speed_mode = PWM_MODE,
      .channel = (ledc_channel_t)channel,
      .intr_type = LEDC_INTR_DISABLE,
      .timer_sel = timer_of(channel),
      .duty = s_chan[channel].has_pending_duty ? s_chan[channel].pending_duty : 0,
      .hpoint = 0,
  };
  esp_err_t err = ledc_channel_config(&cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "ledc_channel_config(ch=%u pin=%u) -> %s", channel, pin,
             esp_err_to_name(err));
    return;
  }
  s_chan[channel].attached = true;
  s_chan[channel].has_pending_duty = false;
}

void pwm_write(uint8_t channel, uint32_t duty) {
  if (channel >= PWM_NUM_CHANNELS) {
    return;
  }
  if (!s_chan[channel].attached) {
    // Se guarda para aplicarlo al enrutar el pin. Reproduce el efecto neto de
    // ledcWrite() antes de ledcAttachPin() en Arduino (no salia nada por el
    // pin, porque no habia pin), sin perder el valor pedido.
    s_chan[channel].pending_duty = duty;
    s_chan[channel].has_pending_duty = true;
    return;
  }
  ledc_set_duty(PWM_MODE, (ledc_channel_t)channel, duty);
  ledc_update_duty(PWM_MODE, (ledc_channel_t)channel);
}
