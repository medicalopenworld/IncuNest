#include "hmi_mic.h"

#include <Arduino.h>
#include <math.h>

#include "driver/i2s_pdm.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/usb_serial_jtag_reg.h"

static const char *TAG = "HMI_MIC";

namespace {

constexpr int kPinClk = 19;  // IO19_MIC_CLK
constexpr int kPinDin = 20;  // IO20_MIC_SD
constexpr uint32_t kSampleRate = 16000;
constexpr uint32_t kWindowMs = 100;
constexpr size_t kWindowSamples = kSampleRate * kWindowMs / 1000;
// Pico a pico minimo de una ventana con senal real: una linea de datos pegada
// o flotante da muestras constantes (mismo criterio que la SensorBoard).
constexpr int32_t kAliveMinPp = 8;
// Ventanas seguidas sin senal antes de probar el otro slot del PDM (el pin
// SELECT del microfono decide si sale por izquierdo o derecho).
constexpr int kDeadBeforeSwap = 5;
constexpr size_t kRingLen = 64;  // 6,4 s de historia
constexpr float kDbOffset = 120.0f;

// Tono del zumbador de la motherBoard (BUZZER_PWM_FREQUENCY = 400 Hz, onda
// cuadrada): fundamental y armonicos impares 3 y 5, que son los que lleva una
// cuadrada al 50 %. Para cada uno se prueban tres frecuencias (+-1 bin de
// 10 Hz, escalado por el armonico) y se queda la mayor: el PWM no sale
// exactamente a 400 Hz y una ventana de 100 ms tiene bins de 10 Hz.
constexpr float kToneHz = 400.0f;
constexpr int kToneHarmonics[] = {1, 3, 5};
constexpr float kToneBinHz = 1000.0f / (float)kWindowMs;

struct Window {
  uint32_t endMs;
  float rms;
  float toneRms;  // RMS equivalente de la energia en el tono del zumbador
  bool alive;
};

// Potencia (amplitud^2 / 2, es decir RMS^2) de la componente a `freq` en las
// `n` muestras ya sin continua. Goertzel generalizado: vale para frecuencias
// que no caen justo en un bin.
float goertzelPower(const int16_t *x, size_t n, float mean, float freq) {
  const float w = 2.0f * (float)M_PI * freq / (float)kSampleRate;
  const float coeff = 2.0f * cosf(w);
  float s1 = 0.0f, s2 = 0.0f;
  for (size_t i = 0; i < n; i++) {
    const float s0 = ((float)x[i] - mean) + coeff * s1 - s2;
    s2 = s1;
    s1 = s0;
  }
  const float re = s1 - s2 * cosf(w);
  const float im = s2 * sinf(w);
  const float amp = 2.0f * sqrtf(re * re + im * im) / (float)n;
  return amp * amp * 0.5f;
}

float toneRms(const int16_t *x, size_t n, float mean) {
  float energy = 0.0f;
  for (int h : kToneHarmonics) {
    float best = 0.0f;
    for (int off = -1; off <= 1; off++) {
      const float p =
          goertzelPower(x, n, mean, kToneHz * h + off * kToneBinHz * h);
      if (p > best) best = p;
    }
    energy += best;
  }
  return sqrtf(energy);
}

i2s_chan_handle_t s_chan = nullptr;
TaskHandle_t s_task = nullptr;
volatile bool s_stopRequested = false;
portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
Window s_ring[kRingLen];
size_t s_ringHead = 0;   // siguiente hueco
size_t s_ringCount = 0;
i2s_pdm_slot_mask_t s_slot = I2S_PDM_SLOT_LEFT;

void pushWindow(const Window &w) {
  portENTER_CRITICAL(&s_mux);
  s_ring[s_ringHead] = w;
  s_ringHead = (s_ringHead + 1) % kRingLen;
  if (s_ringCount < kRingLen) s_ringCount++;
  portEXIT_CRITICAL(&s_mux);
}

bool configureSlot(i2s_pdm_slot_mask_t slot) {
  i2s_pdm_rx_slot_config_t slotCfg =
      I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  slotCfg.slot_mask = slot;
  i2s_channel_disable(s_chan);
  const esp_err_t err = i2s_channel_reconfig_pdm_rx_slot(s_chan, &slotCfg);
  i2s_channel_enable(s_chan);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "no se pudo cambiar de slot: %s", esp_err_to_name(err));
    return false;
  }
  s_slot = slot;
  return true;
}

void micTask(void *) {
  static int16_t buf[kWindowSamples];
  int deadInARow = 0;
  bool reportedAlive = false;

  while (!s_stopRequested) {
    size_t bytesRead = 0;
    const esp_err_t err = i2s_channel_read(s_chan, buf, sizeof(buf), &bytesRead,
                                           pdMS_TO_TICKS(kWindowMs * 3));
    const size_t n = bytesRead / sizeof(int16_t);
    if (err != ESP_OK || n < kWindowSamples / 2) {
      continue;
    }

    int32_t mn = buf[0], mx = buf[0];
    double sum = 0.0;
    for (size_t i = 0; i < n; i++) {
      if (buf[i] < mn) mn = buf[i];
      if (buf[i] > mx) mx = buf[i];
      sum += buf[i];
    }
    const double mean = sum / (double)n;
    double acc = 0.0;
    for (size_t i = 0; i < n; i++) {
      const double d = (double)buf[i] - mean;
      acc += d * d;
    }
    Window w;
    w.endMs = millis();
    w.rms = (float)sqrt(acc / (double)n);
    w.toneRms = toneRms(buf, n, (float)mean);
    w.alive = (mx - mn) >= kAliveMinPp;
    pushWindow(w);

    if (w.alive) {
      deadInARow = 0;
      if (!reportedAlive) {
        reportedAlive = true;
        ESP_LOGI(TAG, "senal viva en slot %s (pp=%ld rms=%.1f)",
                 s_slot == I2S_PDM_SLOT_LEFT ? "izquierdo" : "derecho",
                 (long)(mx - mn), (double)w.rms);
      }
    } else if (!reportedAlive && ++deadInARow >= kDeadBeforeSwap) {
      deadInARow = 0;
      const i2s_pdm_slot_mask_t other =
          (s_slot == I2S_PDM_SLOT_LEFT) ? I2S_PDM_SLOT_RIGHT : I2S_PDM_SLOT_LEFT;
      ESP_LOGW(TAG, "sin senal en slot %s (pp=%ld), probando el otro",
               s_slot == I2S_PDM_SLOT_LEFT ? "izquierdo" : "derecho",
               (long)(mx - mn));
      configureSlot(other);
    }
  }

  // La propia tarea libera el canal: HmiMic_Stop() se llama con LVGL_Lock()
  // tomado y no debe quedarse esperando a que termine una lectura.
  i2s_channel_disable(s_chan);
  i2s_del_channel(s_chan);
  s_chan = nullptr;
  s_task = nullptr;
  vTaskDelete(nullptr);
}

}  // namespace

bool HmiMic_Start(void) {
  if (s_task != nullptr) {
    // En marcha, o saliendo tras un Stop() reciente: en este ultimo caso no se
    // puede crear otro canal hasta que libere el suyo (como mucho 300 ms).
    return !s_stopRequested;
  }

  // IO20 es D+ del USB nativo: con el pad USB habilitado su pull-up interno
  // sigue colgado de la linea de datos del microfono.
  CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE);

  // PDM RX solo existe en I2S0 en el S3.
  i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  esp_err_t err = i2s_new_channel(&chanCfg, nullptr, &s_chan);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "i2s_new_channel: %s", esp_err_to_name(err));
    s_chan = nullptr;
    return false;
  }

  i2s_pdm_rx_config_t pdmCfg = {
      .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(kSampleRate),
      .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                 I2S_SLOT_MODE_MONO),
      .gpio_cfg =
          {
              .clk = (gpio_num_t)kPinClk,
              .din = (gpio_num_t)kPinDin,
              .invert_flags = {.clk_inv = false},
          },
  };
  pdmCfg.slot_cfg.slot_mask = I2S_PDM_SLOT_LEFT;
  s_slot = I2S_PDM_SLOT_LEFT;
  err = i2s_channel_init_pdm_rx_mode(s_chan, &pdmCfg);
  if (err == ESP_OK) err = i2s_channel_enable(s_chan);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "init PDM RX: %s", esp_err_to_name(err));
    i2s_del_channel(s_chan);
    s_chan = nullptr;
    return false;
  }

  portENTER_CRITICAL(&s_mux);
  s_ringHead = 0;
  s_ringCount = 0;
  portEXIT_CRITICAL(&s_mux);
  s_stopRequested = false;
  if (xTaskCreate(micTask, "hmi_mic", 3072, nullptr, 2, &s_task) != pdPASS) {
    ESP_LOGE(TAG, "no se pudo crear la tarea");
    s_task = nullptr;
    i2s_channel_disable(s_chan);
    i2s_del_channel(s_chan);
    s_chan = nullptr;
    return false;
  }
  ESP_LOGI(TAG, "microfono arrancado (CLK=IO%d DIN=IO%d, %lu Hz)", kPinClk,
           kPinDin, (unsigned long)kSampleRate);
  return true;
}

// No bloquea: la tarea sale en su siguiente lectura (como mucho
// kWindowMs * 3) y libera el canal ella misma.
void HmiMic_Stop(void) {
  if (s_task == nullptr) return;
  s_stopRequested = true;
}

bool HmiMic_Running(void) { return s_task != nullptr && !s_stopRequested; }

bool HmiMic_Alive(void) {
  const uint32_t now = millis();
  bool alive = false;
  portENTER_CRITICAL(&s_mux);
  for (size_t k = 0; k < s_ringCount; k++) {
    const Window &w = s_ring[(s_ringHead + kRingLen - 1 - k) % kRingLen];
    if ((uint32_t)(now - w.endMs) > 1000u) break;
    if (w.alive) {
      alive = true;
      break;
    }
  }
  portEXIT_CRITICAL(&s_mux);
  return alive;
}

#ifdef HMI_MIC_BOOT_PROBE
// Verificacion en banco del microfono (temporal): -DHMI_MIC_BOOT_PROBE.
static void micProbeTask(void *) {
  vTaskDelay(pdMS_TO_TICKS(8000));  // tras el arranque de UI/WiFi
  if (!HmiMic_Start()) {
    ESP_LOGE(TAG, "[PROBE] el microfono no arranca");
    vTaskDelete(nullptr);
  }
  for (int i = 0; i < 60; i++) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    float db = 0.0f;
    const uint32_t now = millis();
    const bool ok = HmiMic_LevelBetween(now - 1000, now, &db);
    ESP_LOGW(TAG, "[PROBE] t=%ds alive=%d nivel=%s%.1f dB", i + 1,
             (int)HmiMic_Alive(), ok ? "" : "(sin ventanas) ", (double)db);
  }
  HmiMic_Stop();
  ESP_LOGW(TAG, "[PROBE] fin");
  vTaskDelete(nullptr);
}

void HmiMic_BootProbe(void) {
  xTaskCreate(micProbeTask, "mic_probe", 3072, nullptr, 1, nullptr);
}
#endif

static bool levelBetween(uint32_t fromMs, uint32_t toMs, bool tone,
                         float *dbOut) {
  double energy = 0.0;
  int count = 0;
  portENTER_CRITICAL(&s_mux);
  for (size_t k = 0; k < s_ringCount; k++) {
    const Window &w = s_ring[(s_ringHead + kRingLen - 1 - k) % kRingLen];
    // Comparaciones por diferencia: aguantan el desborde de millis().
    if ((int32_t)(w.endMs - fromMs) <= 0) break;
    if ((int32_t)(w.endMs - toMs) > 0 || !w.alive) continue;
    const double r = tone ? w.toneRms : w.rms;
    energy += r * r;
    count++;
  }
  portEXIT_CRITICAL(&s_mux);
  if (count == 0) return false;
  const double rms = sqrt(energy / count);
  *dbOut = (rms > 0.0) ? (float)(20.0 * log10(rms / 32768.0) + kDbOffset) : 0.0f;
  return true;
}

bool HmiMic_LevelBetween(uint32_t fromMs, uint32_t toMs, float *dbOut) {
  return levelBetween(fromMs, toMs, false, dbOut);
}

bool HmiMic_ToneBetween(uint32_t fromMs, uint32_t toMs, float *dbOut) {
  return levelBetween(fromMs, toMs, true, dbOut);
}
