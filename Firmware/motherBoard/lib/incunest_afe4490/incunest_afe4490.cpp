// incunest_afe4490.cpp — Medical Open World AFE4490 driver + PPG algorithms (HR, SpO2)
// Library version: v0.20 — ESP32-S3, Arduino + FreeRTOS
// Spec: incunest_afe4490_spec.md
// Chip datasheet: https://www.ti.com/lit/ds/symlink/afe4490.pdf
// Author: Medical Open World — http://medicalopenworld.org — <contact@medicalopenworld.org>

#include "incunest_afe4490.h"
#ifndef INCUNEST_OFFLINE
#include "esp_log.h"
#endif
#include <math.h>
#include <string.h>
#if INCUNEST_TIMING_STATS && !defined(INCUNEST_OFFLINE)
#include "esp_timer.h"
#endif

#ifdef INCUNEST_OFFLINE
[[maybe_unused]] static const char* TAG = "";
#define ESP_LOGE(tag, ...) ((void)0)
#define ESP_LOGI(tag, ...) ((void)0)
#define ESP_LOGW(tag, ...) ((void)0)
#else
static const char* TAG = "incunest_afe4490";
#endif

namespace {
    // ── Math ──────────────────────────────────────────────────────────────────
    constexpr float    pi                  = 3.14159265358979f;

    // ── SpO2 ──────────────────────────────────────────────────────────────────
    constexpr float    spo2_warmup_s       = 5.0f;     // s  — warmup before reporting SpO2
    constexpr float    spo2_dc_iir_tau_s   = 1.6f;     // s  — DC IIR time constant
    constexpr float    spo2_ac_ema_tau_s   = 1.0f;     // s  — AC² EMA time constant
    // Calibration coefficients derived from experimental data with a
    // UpnMed U401-D(01AS-F) probe, type Nellcor Non-Oximax.
    // Override at runtime with setSpO2Coefficients() for a different probe.
    constexpr float    spo2_a_default      = 114.9208f; // SpO2 = a - b·R
    constexpr float    spo2_b_default      =  30.5547f;
    constexpr float    spo2_min            =  70.0f;   // % — valid output lower bound
    constexpr float    spo2_max            = 100.0f;   // % — valid output upper bound
    constexpr float    spo2_clamp_margin   =   3.0f;   // % — clamp to spo2_max if within margin above
    constexpr float    spo2_min_dc         = 1000.0f;  // ADC counts — no-finger threshold
    // SQI: Perfusion Index thresholds (Nellcor/Masimo clinical reference).
    // PI < spo2_pi_sqi_lo → SQI=0. PI ≥ spo2_pi_sqi_hi → SQI=1. Linear ramp in between.
    constexpr float    spo2_pi_sqi_lo      = 0.5f;     // % — PI below this → SQI = 0
    constexpr float    spo2_pi_sqi_hi      = 2.0f;     // % — PI above this → SQI = 1

    // ── HR1 ───────────────────────────────────────────────────────────────────
    constexpr float    hr1_dc_tau_s        = 1.6f;     // s  — DC removal IIR time constant
    constexpr float    hr1_ma_cutoff_hz    = 5.0f;     // Hz — low-pass cutoff for peak detection MA
    // SQI: CV = std/mean of the 5 most recent RR intervals.
    // CV=0 (perfectly regular) → SQI=1. CV ≥ hr1_sqi_cv_max → SQI=0.
    constexpr float    hr1_sqi_cv_max      = 0.15f;    // dimensionless — 15% CV threshold
    constexpr float    hr1_refractory_s    = 0.185f;   // s  — refractory period (covers 263 BPM guard band)

    // ── HR2 ───────────────────────────────────────────────────────────────────
    constexpr float    hr2_min_corr        = 0.5f;     // normalised autocorrelation threshold for SQI=1

    // ── HR3 ───────────────────────────────────────────────────────────────────
    constexpr int      hr3_decim_factor    = 10;       // 500 Hz → 50 Hz effective sample rate

    // ── HR (all algorithms) ───────────────────────────────────────────────────
    // hr_search_{min,max}_bpm are derived inline as hr_{min,max}_bpm ± 3 BPM guard band.
    constexpr float    hr_min_bpm          =  30.0f;   // bpm — valid lower bound (ISO 80601-2-61; neonatal)
    constexpr float    hr_max_bpm          = 260.0f;   // bpm — valid upper bound (neonatal tachycardia)

    // ── HR3 FFT — radix-2 Cooley-Tukey DIT (in-place, complex interleaved) ──
    // x: float array of 2N elements [re0,im0, re1,im1, ..., re(N-1),im(N-1)]
    // N must be a power of two. Twiddle factors computed per stage (9 calls to
    // cosf/sinf for N=512), not per butterfly — negligible overhead at 0.5 s update rate.
    static void _fft_r2(float* x, int N) {
        // Bit-reversal permutation
        for (int i = 1, j = 0; i < N; i++) {
            int bit = N >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) {
                float t;
                t = x[2*i];   x[2*i]   = x[2*j];   x[2*j]   = t;
                t = x[2*i+1]; x[2*i+1] = x[2*j+1]; x[2*j+1] = t;
            }
        }
        // Butterfly stages
        for (int len = 2; len <= N; len <<= 1) {
            float w_re = cosf(-2.0f * pi / (float)len);
            float w_im = sinf(-2.0f * pi / (float)len);
            for (int i = 0; i < N; i += len) {
                float c_re = 1.0f, c_im = 0.0f;
                for (int j = 0; j < len / 2; j++) {
                    int u = 2 * (i + j), v = 2 * (i + j + len / 2);
                    float vt_re = c_re * x[v]   - c_im * x[v + 1];
                    float vt_im = c_re * x[v + 1] + c_im * x[v];
                    x[v]     = x[u]     - vt_re;
                    x[v + 1] = x[u + 1] - vt_im;
                    x[u]     = x[u]     + vt_re;
                    x[u + 1] = x[u + 1] + vt_im;
                    float tmp = c_re * w_re - c_im * w_im;
                    c_im      = c_re * w_im + c_im * w_re;
                    c_re      = tmp;
                }
            }
        }
    }

    // ── AFE4490 register addresses ────────────────────────────────────────────
    constexpr uint8_t REG_CONTROL0      = 0x00;
    constexpr uint8_t REG_LED2STC       = 0x01;
    constexpr uint8_t REG_LED2ENDC      = 0x02;
    constexpr uint8_t REG_LED2LEDSTC    = 0x03;
    constexpr uint8_t REG_LED2LEDENDC   = 0x04;
    constexpr uint8_t REG_ALED2STC      = 0x05;
    constexpr uint8_t REG_ALED2ENDC     = 0x06;
    constexpr uint8_t REG_LED1STC       = 0x07;
    constexpr uint8_t REG_LED1ENDC      = 0x08;
    constexpr uint8_t REG_LED1LEDSTC    = 0x09;
    constexpr uint8_t REG_LED1LEDENDC   = 0x0A;
    constexpr uint8_t REG_ALED1STC      = 0x0B;
    constexpr uint8_t REG_ALED1ENDC     = 0x0C;
    constexpr uint8_t REG_LED2CONVST    = 0x0D;
    constexpr uint8_t REG_LED2CONVEND   = 0x0E;
    constexpr uint8_t REG_ALED2CONVST   = 0x0F;
    constexpr uint8_t REG_ALED2CONVEND  = 0x10;
    constexpr uint8_t REG_LED1CONVST    = 0x11;
    constexpr uint8_t REG_LED1CONVEND   = 0x12;
    constexpr uint8_t REG_ALED1CONVST   = 0x13;
    constexpr uint8_t REG_ALED1CONVEND  = 0x14;
    constexpr uint8_t REG_ADCRSTSTCT0   = 0x15;
    constexpr uint8_t REG_ADCRSTENDCT0  = 0x16;
    constexpr uint8_t REG_ADCRSTSTCT1   = 0x17;
    constexpr uint8_t REG_ADCRSTENDCT1  = 0x18;
    constexpr uint8_t REG_ADCRSTSTCT2   = 0x19;
    constexpr uint8_t REG_ADCRSTENDCT2  = 0x1A;
    constexpr uint8_t REG_ADCRSTSTCT3   = 0x1B;
    constexpr uint8_t REG_ADCRSTENDCT3  = 0x1C;
    constexpr uint8_t REG_PRPCOUNT      = 0x1D;
    constexpr uint8_t REG_CONTROL1      = 0x1E;
    constexpr uint8_t REG_TIAGAIN       = 0x20;
    constexpr uint8_t REG_TIA_AMB_GAIN  = 0x21;
    constexpr uint8_t REG_LEDCNTRL      = 0x22;
    constexpr uint8_t REG_CONTROL2      = 0x23;
    constexpr uint8_t REG_ALARM         = 0x29;
    constexpr uint8_t REG_LED2VAL       = 0x2A;
    constexpr uint8_t REG_ALED2VAL      = 0x2B;
    constexpr uint8_t REG_LED1VAL       = 0x2C;
    constexpr uint8_t REG_ALED1VAL      = 0x2D;
    constexpr uint8_t REG_LED2_ALED2VAL = 0x2E;
    constexpr uint8_t REG_LED1_ALED1VAL = 0x2F;
    constexpr uint8_t REG_DIAG          = 0x30;

    // CONTROL0 bits
    constexpr uint32_t ctrl0_spi_read   = 0x000001UL;
    constexpr uint32_t ctrl0_sw_rst     = 0x000008UL;

    // CONTROL1 bits
    constexpr uint32_t ctrl1_timeren    = 0x000100UL;

    // AFE clock frequency
    constexpr uint32_t afeclk = 4000000UL;  // 4 MHz → 1 count = 0.25 µs

    // TIAGAIN / TIA_AMB_GAIN: RF bits [2:0]
    // enum order: RF_10K=0..RF_1M=6 → register codes
    constexpr uint32_t rf_code[7] = { 5, 4, 3, 2, 1, 0, 6 };
    // Physical RF values (Ω) indexed by AFE4490TIAGain
    constexpr float tia_rf_ohm[7] = { 10e3f, 25e3f, 50e3f, 100e3f, 250e3f, 500e3f, 1e6f };

    // TIAGAIN / TIA_AMB_GAIN: CF bits [7:3] (5 pF base + parallel caps)
    // enum order: CF_5P=0..CF_155P=5 → register bits
    constexpr uint32_t cf_code[6] = { 0x000, 0x008, 0x010, 0x020, 0x040, 0x080 };
    // Physical CF values (pF) indexed by AFE4490TIACF
    constexpr float tia_cf_pF[6]  = { 5.0f, 10.0f, 20.0f, 30.0f, 55.0f, 155.0f };

    // Auto-CF settling parameters
    constexpr float    tia_settle_fraction = 0.10f;  // 10% of LED-on window reserved for TIA settling
    constexpr uint32_t tia_settle_min      = 50;     // floor: 12.5 µs @ 4 MHz (covers RF_500K/CF_5P at high Fs)
    constexpr float    tia_n_tau           = 5.0f;   // 5τ → <0.7% settling error

    // TIAGAIN: STG2GAIN bits D[10:8] only — STAGE2EN (D14) is set separately via _afe_stage2_en1/2
    constexpr uint32_t stg2_gain_code[5] = {
        0x000000UL,   // GAIN_0DB:   STG2=0
        0x000100UL,   // GAIN_3_5DB: STG2=1
        0x000200UL,   // GAIN_6DB:   STG2=2
        0x000300UL,   // GAIN_9_5DB: STG2=3
        0x000400UL    // GAIN_12DB:  STG2=4
    };
}

// ── Static member ─────────────────────────────────────────────────────────────
// Singleton pointer used by the static ISR trampoline (_drdy_isr_static) to reach
// the class instance. Static members must be defined exactly once in a .cpp file;
// the declaration in the header only reserves the name.
#ifndef INCUNEST_OFFLINE
INCUNEST_AFE4490* INCUNEST_AFE4490::_g_instance = nullptr;
#endif

// ── Constructor / destructor ──────────────────────────────────────────────────
INCUNEST_AFE4490::INCUNEST_AFE4490()
    : _pin_cs(-1), _pin_drdy(-1),
      _drdy_sem(nullptr), _spi_mutex(nullptr), _state_mutex(nullptr),
      _data_queue(nullptr), _task_handle(nullptr),
      _initialized(false),
      _diag_active(false),
      _diag_holdoff_samples(0),
      _diag_last_led1(0), _diag_last_led2(0),
      _diag_last_aled1(0), _diag_last_aled2(0),
      _diag_last_led1_aled1(0), _diag_last_led2_aled2(0),
      _hr2_calc_sem(nullptr), _hr3_calc_sem(nullptr),
      _hr2_task_handle(nullptr), _hr3_task_handle(nullptr),
      _hr2_computing(false), _hr3_computing(false),
      _hr2_result(0.0f), _hr2_sqi_result(0.0f),
      _hr3_result(0.0f), _hr3_sqi_result(0.0f),
      _afe_sample_rate_hz(500), _afe_adc_averages(8),
      _afe_led1_current_mA(11.7f), _afe_led2_current_mA(11.7f), _afe_led_range_mA(150),
      _afe_ensepgain(true),
      _afe_tia_gain_led1(AFE4490TIAGain::RF_500K),
      _afe_tia_cf_led1(AFE4490TIACF::CF_5P),
      _afe_stage2_gain_led1(AFE4490Stage2Gain::GAIN_0DB),
      _afe_stage2_en1(true),
      _afe_tia_gain_led2(AFE4490TIAGain::RF_500K),
      _afe_tia_cf_led2(AFE4490TIACF::CF_5P),
      _afe_stage2_gain_led2(AFE4490Stage2Gain::GAIN_0DB),
      _afe_stage2_en2(true),
      _afe_ambdac_uA(0),
      _ppgdisp_channel(AFE4490Channel::LED1_ALED1),
      _ppgdisp_filter_type(AFE4490Filter::BUTTERWORTH),
      _ppgdisp_bpf({0.5f, 20.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0.0f, 0.0f}, true}),
      _ppgdisp_ma_idx(0), _ppgdisp_ma_sum(0.0f),
      _hr1_ma_len(0), _hr1_ma_idx(0), _hr1_ma_sum(0.0f),
      _hr1_dc_alpha(0.0f),
      _spo2_warmup_samples(0), _hr1_refractory_samples(0),
      _spo2_dc_iir_alpha(0.0f), _spo2_ac_ema_beta(0.0f),
      _spo2_dc_ir(0.0f), _spo2_dc_red(0.0f),
      _spo2_ac2_ir(0.0f), _spo2_ac2_red(0.0f),
      _spo2_sample_count(0),
      _spo2_a(spo2_a_default), _spo2_b(spo2_b_default),
      _hr1_dc(0.0f),
      _hr1_running_max(0.0f), _hr1_ppg_above_thresh(false),
      _hr1_last_peak_idx(0), _hr1_sample_idx(0),
      _hr1_interval_count(0),
      _hr2_bpf({0.5f, 5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0.0f, 0.0f}, true}),
      _hr2_buf_idx(0), _hr2_buf_count(0), _hr2_decim_counter(0), _hr2_update_counter(0),
      _hr3_bpf({0.4f, 15.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0.0f, 0.0f}, true}),
      _hr3_buf_idx(0), _hr3_buf_count(0), _hr3_decim_counter(0), _hr3_update_counter(0),
      _spo2_warmup_s(spo2_warmup_s),
      _spo2_dc_iir_tau_s(spo2_dc_iir_tau_s),
      _spo2_ac_ema_tau_s(spo2_ac_ema_tau_s),
      _spo2_min(spo2_min),
      _spo2_max(spo2_max),
      _spo2_min_dc(spo2_min_dc),
      _spo2_pi_sqi_lo(spo2_pi_sqi_lo),
      _spo2_pi_sqi_hi(spo2_pi_sqi_hi),
      _hr1_dc_tau_s(hr1_dc_tau_s),
      _hr1_ma_cutoff_hz(hr1_ma_cutoff_hz),
      _hr1_sqi_cv_max(hr1_sqi_cv_max),
      _hr2_min_corr(hr2_min_corr),
      _hr2_update_interval(25u),
      _hr3_update_interval(25u),
      _hr_min_bpm(hr_min_bpm),
      _hr_max_bpm(hr_max_bpm)
{
    // Quantize initial LED currents to the DAC grid (range already set in init list)
    _afe_led1_current_mA = _quantize_led_mA(_afe_led1_current_mA);
    _afe_led2_current_mA = _quantize_led_mA(_afe_led2_current_mA);
    memset(_ppgdisp_ma_buf, 0, sizeof(_ppgdisp_ma_buf));
    memset(_hr1_ma_buf, 0, sizeof(_hr1_ma_buf));
    _hr1_ma_idx = 0; _hr1_ma_sum = 0.0f;
    memset(_hr1_intervals, 0, sizeof(_hr1_intervals));
    memset(_hr2_buf, 0, sizeof(_hr2_buf));
    memset(_hr3_buf, 0, sizeof(_hr3_buf));
    _current_data = AFE4490Data{};
    _recalc_rate_params();
    _reset_algorithms();
}

INCUNEST_AFE4490::~INCUNEST_AFE4490() {
#ifndef INCUNEST_OFFLINE
    if (_task_handle)      { vTaskDelete(_task_handle);      _task_handle      = nullptr; }
    if (_hr2_task_handle)  { vTaskDelete(_hr2_task_handle);  _hr2_task_handle  = nullptr; }
    if (_hr3_task_handle)  { vTaskDelete(_hr3_task_handle);  _hr3_task_handle  = nullptr; }
    if (_data_queue)       vQueueDelete(_data_queue);
    if (_drdy_sem)         vSemaphoreDelete(_drdy_sem);
    if (_hr2_calc_sem)     vSemaphoreDelete(_hr2_calc_sem);
    if (_hr3_calc_sem)     vSemaphoreDelete(_hr3_calc_sem);
    if (_spi_mutex)        vSemaphoreDelete(_spi_mutex);
    if (_state_mutex)      vSemaphoreDelete(_state_mutex);
    if (_g_instance == this) _g_instance = nullptr;
#endif
}

// ── begin() ───────────────────────────────────────────────────────────────────
// Requires SPI.begin() to have been called beforehand. This library intentionally
// does not call SPI.begin() to avoid reinitialising the bus and interfering with
// other SPI devices. Only SPI.beginTransaction() / endTransaction() are used here.
#ifndef INCUNEST_OFFLINE
void INCUNEST_AFE4490::begin(int pin_cs, int pin_drdy) {
    ESP_LOGI(TAG, "[afe4490] begin() start");
    _pin_cs   = pin_cs;
    _pin_drdy = pin_drdy;
    _g_instance = this;

    pinMode(_pin_cs, OUTPUT);
    digitalWrite(_pin_cs, HIGH);

    _drdy_sem    = xSemaphoreCreateBinary();
    _spi_mutex   = xSemaphoreCreateMutex();
    _state_mutex = xSemaphoreCreateMutex();
    _data_queue  = xQueueCreate(INCUNEST_AFE4490_QUEUE_SIZE, sizeof(AFE4490Data));
    _hr2_calc_sem = xSemaphoreCreateBinary();
    _hr3_calc_sem = xSemaphoreCreateBinary();

    if (!_drdy_sem || !_spi_mutex || !_state_mutex || !_data_queue ||
        !_hr2_calc_sem || !_hr3_calc_sem) {
        ESP_LOGE(TAG, "FreeRTOS object creation failed");
        return;
    }

    xSemaphoreTake(_spi_mutex, portMAX_DELAY);
    ESP_LOGI(TAG, "[afe4490] _chip_init() start");
    _chip_init();
    ESP_LOGI(TAG, "[afe4490] _chip_init() done");
    xSemaphoreGive(_spi_mutex);

    _initialized = true;

    pinMode(_pin_drdy, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(_pin_drdy), _drdy_isr_static, RISING);

    xTaskCreatePinnedToCore(
        _task_trampoline,     "incunest_afe4490",
        INCUNEST_AFE4490_TASK_STACK, this,
        INCUNEST_AFE4490_TASK_PRIORITY, &_task_handle, 1);

    xTaskCreatePinnedToCore(
        _hr2_task_trampoline, "incunest_hr2",
        INCUNEST_AFE4490_HR2_TASK_STACK, this,
        INCUNEST_AFE4490_HR23_TASK_PRIORITY, &_hr2_task_handle, 1);

    xTaskCreatePinnedToCore(
        _hr3_task_trampoline, "incunest_hr3",
        INCUNEST_AFE4490_HR3_TASK_STACK, this,
        INCUNEST_AFE4490_HR23_TASK_PRIORITY, &_hr3_task_handle, 1);

    ESP_LOGI(TAG, "Started: PRF=%u Hz, NUMAV=%u", _afe_sample_rate_hz, _afe_adc_averages);
    ESP_LOGI(TAG, "[afe4490] begin() done");
}
#endif

// ── Configuration setters ─────────────────────────────────────────────────────
void INCUNEST_AFE4490::setSampleRate(uint16_t hz) {
    if (hz < 63 || hz > 5000) {
        ESP_LOGE(TAG, "setSampleRate: %u Hz out of range [63, 5000]", hz);
        return;
    }

#ifndef INCUNEST_OFFLINE
    if (_initialized) xSemaphoreTake(_spi_mutex, portMAX_DELAY);
#endif

    _afe_sample_rate_hz = hz;

    // Maximum averages = floor(T_conv_window / T_conv_min) = floor(PRP/4 / 50µs) where PRP (Pulse Repetition Period)
    // With PRF 500 Hz then PRP is 2000 µs and max_averages = 10 (NUMAV = 9 = 10-1)
    // Hardware field limit: NUMAV ≤ 15 (16 averages max, datasheet CONTROL1 bits [7:0])
    uint8_t numav_max = (uint8_t)((5000u / hz) - 1u);
    if (numav_max > 15) numav_max = 15;

    if ((_afe_adc_averages - 1u) > numav_max) {
        uint8_t clamped = numav_max + 1u;
        ESP_LOGE(TAG, "setSampleRate: num_averages clamped %u→%u at %u Hz",
                 _afe_adc_averages, clamped, hz);
        _afe_adc_averages = clamped;
    }

    _recalc_rate_params();

#ifndef INCUNEST_OFFLINE
    if (_initialized) {
        _apply_timing_regs();
        _apply_control_regs();
        xSemaphoreGive(_spi_mutex);
    }
#endif
}

void INCUNEST_AFE4490::setAdcAverages(uint8_t num) {
    if (num == 0) num = 1;

    uint8_t numav_max = (uint8_t)((5000u / _afe_sample_rate_hz) - 1u);
    if (numav_max > 15) numav_max = 15;

    if ((uint8_t)(num - 1u) > numav_max) {
        uint8_t clamped = numav_max + 1u;
        ESP_LOGE(TAG, "setAdcAverages: %u clamped to %u (max at %u Hz)",
                 num, clamped, _afe_sample_rate_hz);
        num = clamped;
    }

#ifndef INCUNEST_OFFLINE
    if (_initialized) xSemaphoreTake(_spi_mutex, portMAX_DELAY);
#endif
    _afe_adc_averages = num;
#ifndef INCUNEST_OFFLINE
    if (_initialized) {
        _apply_control_regs();
        xSemaphoreGive(_spi_mutex);
    }
#endif
}

float INCUNEST_AFE4490::_quantize_led_mA(float mA) const {
    float fs = (float)_afe_led_range_mA;
    uint8_t code = (uint8_t)constrain(roundf((mA / fs) * 256.0f), 0.0f, 255.0f);
    return (code / 256.0f) * fs;
}

void INCUNEST_AFE4490::setLED1Current(float mA) {
#ifndef INCUNEST_OFFLINE
    if (_initialized) xSemaphoreTake(_spi_mutex, portMAX_DELAY);
#endif
    _afe_led1_current_mA = _quantize_led_mA(constrain(mA, 0.0f, (float)_afe_led_range_mA));
#ifndef INCUNEST_OFFLINE
    if (_initialized) {
        _apply_analog_regs();
        xSemaphoreGive(_spi_mutex);
    }
#endif
}

void INCUNEST_AFE4490::setLED2Current(float mA) {
#ifndef INCUNEST_OFFLINE
    if (_initialized) xSemaphoreTake(_spi_mutex, portMAX_DELAY);
#endif
    _afe_led2_current_mA = _quantize_led_mA(constrain(mA, 0.0f, (float)_afe_led_range_mA));
#ifndef INCUNEST_OFFLINE
    if (_initialized) {
        _apply_analog_regs();
        xSemaphoreGive(_spi_mutex);
    }
#endif
}

void INCUNEST_AFE4490::setLEDRange(uint8_t mA) {
    if (mA != 75 && mA != 150) {
        ESP_LOGE(TAG, "setLEDRange: must be 75 or 150 mA");
        return;
    }
#ifndef INCUNEST_OFFLINE
    if (_initialized) xSemaphoreTake(_spi_mutex, portMAX_DELAY);
#endif
    _afe_led_range_mA = mA;
    // Re-quantize LED currents to the new range's DAC grid
    _afe_led1_current_mA = _quantize_led_mA(_afe_led1_current_mA);
    _afe_led2_current_mA = _quantize_led_mA(_afe_led2_current_mA);
#ifndef INCUNEST_OFFLINE
    if (_initialized) {
        _apply_analog_regs();
        xSemaphoreGive(_spi_mutex);
    }
#endif
}

// Joint setters — apply the same value to both LED1 and LED2 channels
void INCUNEST_AFE4490::setTIAGain(AFE4490TIAGain gain) {
#ifndef INCUNEST_OFFLINE
    if (_initialized) xSemaphoreTake(_spi_mutex, portMAX_DELAY);
#endif
    _afe_tia_gain_led1 = _afe_tia_gain_led2 = gain;
    _recalc_afe_tia_cf_led1();
    _recalc_afe_tia_cf_led2();
#ifndef INCUNEST_OFFLINE
    if (_initialized) {
        _apply_analog_regs();
        xSemaphoreGive(_spi_mutex);
    }
#endif
}

void INCUNEST_AFE4490::setTIACF(AFE4490TIACF cf) {
#ifndef INCUNEST_OFFLINE
    if (_initialized) xSemaphoreTake(_spi_mutex, portMAX_DELAY);
#endif
    _afe_tia_cf_led1 = _afe_tia_cf_led2 = cf;
#ifndef INCUNEST_OFFLINE
    if (_initialized) {
        _apply_analog_regs();
        xSemaphoreGive(_spi_mutex);
    }
#endif
}

void INCUNEST_AFE4490::setStage2Gain(AFE4490Stage2Gain gain) {
#ifndef INCUNEST_OFFLINE
    if (_initialized) xSemaphoreTake(_spi_mutex, portMAX_DELAY);
#endif
    _afe_stage2_gain_led1 = _afe_stage2_gain_led2 = gain;
#ifndef INCUNEST_OFFLINE
    if (_initialized) {
        _apply_analog_regs();
        xSemaphoreGive(_spi_mutex);
    }
#endif
}

// Separate gain mode
void INCUNEST_AFE4490::setEnSepGain(bool enable) {
#ifndef INCUNEST_OFFLINE
    if (_initialized) xSemaphoreTake(_spi_mutex, portMAX_DELAY);
#endif
    _afe_ensepgain = enable;
#ifndef INCUNEST_OFFLINE
    if (_initialized) {
        _apply_analog_regs();
        xSemaphoreGive(_spi_mutex);
    }
#endif
}

// Per-channel setters — LED1 (IR)
void INCUNEST_AFE4490::setTIAGainLED1(AFE4490TIAGain gain) {
#ifndef INCUNEST_OFFLINE
    if (_initialized) xSemaphoreTake(_spi_mutex, portMAX_DELAY);
#endif
    _afe_tia_gain_led1 = gain;
    _recalc_afe_tia_cf_led1();
#ifndef INCUNEST_OFFLINE
    if (_initialized) {
        _apply_analog_regs();
        xSemaphoreGive(_spi_mutex);
    }
#endif
}

void INCUNEST_AFE4490::setTIACFLED1(AFE4490TIACF cf) {
#ifndef INCUNEST_OFFLINE
    if (_initialized) xSemaphoreTake(_spi_mutex, portMAX_DELAY);
#endif
    _afe_tia_cf_led1 = cf;
#ifndef INCUNEST_OFFLINE
    if (_initialized) {
        _apply_analog_regs();
        xSemaphoreGive(_spi_mutex);
    }
#endif
}

void INCUNEST_AFE4490::setStage2GainLED1(AFE4490Stage2Gain gain) {
#ifndef INCUNEST_OFFLINE
    if (_initialized) xSemaphoreTake(_spi_mutex, portMAX_DELAY);
#endif
    _afe_stage2_gain_led1 = gain;
#ifndef INCUNEST_OFFLINE
    if (_initialized) {
        _apply_analog_regs();
        xSemaphoreGive(_spi_mutex);
    }
#endif
}

// Per-channel setters — LED2 (RED)
void INCUNEST_AFE4490::setTIAGainLED2(AFE4490TIAGain gain) {
#ifndef INCUNEST_OFFLINE
    if (_initialized) xSemaphoreTake(_spi_mutex, portMAX_DELAY);
#endif
    _afe_tia_gain_led2 = gain;
    _recalc_afe_tia_cf_led2();
#ifndef INCUNEST_OFFLINE
    if (_initialized) {
        _apply_analog_regs();
        xSemaphoreGive(_spi_mutex);
    }
#endif
}

void INCUNEST_AFE4490::setTIACFLED2(AFE4490TIACF cf) {
#ifndef INCUNEST_OFFLINE
    if (_initialized) xSemaphoreTake(_spi_mutex, portMAX_DELAY);
#endif
    _afe_tia_cf_led2 = cf;
#ifndef INCUNEST_OFFLINE
    if (_initialized) {
        _apply_analog_regs();
        xSemaphoreGive(_spi_mutex);
    }
#endif
}

void INCUNEST_AFE4490::setStage2GainLED2(AFE4490Stage2Gain gain) {
#ifndef INCUNEST_OFFLINE
    if (_initialized) xSemaphoreTake(_spi_mutex, portMAX_DELAY);
#endif
    _afe_stage2_gain_led2 = gain;
#ifndef INCUNEST_OFFLINE
    if (_initialized) {
        _apply_analog_regs();
        xSemaphoreGive(_spi_mutex);
    }
#endif
}

void INCUNEST_AFE4490::setStage2En1(bool en) {
#ifndef INCUNEST_OFFLINE
    if (_initialized) xSemaphoreTake(_spi_mutex, portMAX_DELAY);
#endif
    _afe_stage2_en1 = en;
#ifndef INCUNEST_OFFLINE
    if (_initialized) {
        _apply_analog_regs();
        xSemaphoreGive(_spi_mutex);
    }
#endif
}

void INCUNEST_AFE4490::setStage2En2(bool en) {
#ifndef INCUNEST_OFFLINE
    if (_initialized) xSemaphoreTake(_spi_mutex, portMAX_DELAY);
#endif
    _afe_stage2_en2 = en;
#ifndef INCUNEST_OFFLINE
    if (_initialized) {
        _apply_analog_regs();
        xSemaphoreGive(_spi_mutex);
    }
#endif
}

void INCUNEST_AFE4490::setAmbDac(uint8_t uA) {
    if (uA > 10) uA = 10;
#ifndef INCUNEST_OFFLINE
    if (_initialized) xSemaphoreTake(_spi_mutex, portMAX_DELAY);
#endif
    _afe_ambdac_uA = uA;
#ifndef INCUNEST_OFFLINE
    if (_initialized) {
        _apply_analog_regs();
        xSemaphoreGive(_spi_mutex);
    }
#endif
}

void INCUNEST_AFE4490::setPPGChannel(AFE4490Channel channel) {
    if (_initialized) xSemaphoreTake(_state_mutex, portMAX_DELAY);
    _ppgdisp_channel = channel;
    // Reset filter state: changing channel means a different signal enters the filter
    _ppgdisp_bpf.state          = {0.0f, 0.0f};
    _ppgdisp_bpf.needs_precharge = true;
    memset(_ppgdisp_ma_buf, 0, sizeof(_ppgdisp_ma_buf));
    _ppgdisp_ma_idx = 0;
    _ppgdisp_ma_sum = 0.0f;
    if (_initialized) xSemaphoreGive(_state_mutex);
}

void INCUNEST_AFE4490::setFilter(AFE4490Filter type, float f_low_hz, float f_high_hz) {
    if (_initialized) xSemaphoreTake(_state_mutex, portMAX_DELAY);
    _ppgdisp_filter_type        = type;
    _ppgdisp_bpf.f_low      = f_low_hz;
    _ppgdisp_bpf.f_high     = f_high_hz;
    if (type == AFE4490Filter::BUTTERWORTH) _recalc_biquad(_ppgdisp_bpf);
    _ppgdisp_bpf.state           = {0.0f, 0.0f};
    _ppgdisp_bpf.needs_precharge = true;
    memset(_ppgdisp_ma_buf, 0, sizeof(_ppgdisp_ma_buf));
    _ppgdisp_ma_idx = 0;
    _ppgdisp_ma_sum = 0.0f;
    if (_initialized) xSemaphoreGive(_state_mutex);
}

void INCUNEST_AFE4490::setHR2Filter(float f_low_hz, float f_high_hz) {
    if (_initialized) xSemaphoreTake(_state_mutex, portMAX_DELAY);
    _hr2_bpf.f_low      = f_low_hz;
    _hr2_bpf.f_high     = f_high_hz;
    _recalc_biquad(_hr2_bpf);
    _hr2_bpf.state           = {0.0f, 0.0f};
    _hr2_bpf.needs_precharge = true;
    if (_initialized) xSemaphoreGive(_state_mutex);
}

void INCUNEST_AFE4490::setHR3Filter(float f_low_hz, float f_high_hz) {
    if (_initialized) xSemaphoreTake(_state_mutex, portMAX_DELAY);
    _hr3_bpf.f_low           = f_low_hz;
    _hr3_bpf.f_high          = f_high_hz;
    _recalc_biquad(_hr3_bpf);
    _hr3_bpf.state           = {0.0f, 0.0f};
    _hr3_bpf.needs_precharge = true;
    if (_initialized) xSemaphoreGive(_state_mutex);
}

void INCUNEST_AFE4490::setSpO2Coefficients(float a, float b) {
    if (_initialized) xSemaphoreTake(_state_mutex, portMAX_DELAY);
    _spo2_a = a;
    _spo2_b = b;
    if (_initialized) xSemaphoreGive(_state_mutex);
}

void INCUNEST_AFE4490::setSpO2WarmupS(float s) {
    if (_initialized) xSemaphoreTake(_state_mutex, portMAX_DELAY);
    _spo2_warmup_s = s;
    _recalc_rate_params();
    if (_initialized) xSemaphoreGive(_state_mutex);
}

void INCUNEST_AFE4490::setSpO2DcIirTauS(float tau_s) {
    if (_initialized) xSemaphoreTake(_state_mutex, portMAX_DELAY);
    _spo2_dc_iir_tau_s = tau_s;
    _recalc_rate_params();
    if (_initialized) xSemaphoreGive(_state_mutex);
}

void INCUNEST_AFE4490::setSpO2AcEmaTauS(float tau_s) {
    if (_initialized) xSemaphoreTake(_state_mutex, portMAX_DELAY);
    _spo2_ac_ema_tau_s = tau_s;
    _recalc_rate_params();
    if (_initialized) xSemaphoreGive(_state_mutex);
}

void INCUNEST_AFE4490::setSpO2Range(float min_pct, float max_pct) {
    if (_initialized) xSemaphoreTake(_state_mutex, portMAX_DELAY);
    _spo2_min = min_pct;
    _spo2_max = max_pct;
    if (_initialized) xSemaphoreGive(_state_mutex);
}

void INCUNEST_AFE4490::setSpO2MinDC(float counts) {
    if (_initialized) xSemaphoreTake(_state_mutex, portMAX_DELAY);
    _spo2_min_dc = counts;
    if (_initialized) xSemaphoreGive(_state_mutex);
}

void INCUNEST_AFE4490::setSpO2PiSqiThresholds(float lo_pct, float hi_pct) {
    if (_initialized) xSemaphoreTake(_state_mutex, portMAX_DELAY);
    _spo2_pi_sqi_lo = lo_pct;
    _spo2_pi_sqi_hi = hi_pct;
    if (_initialized) xSemaphoreGive(_state_mutex);
}

void INCUNEST_AFE4490::setHR1DcTauS(float tau_s) {
    if (_initialized) xSemaphoreTake(_state_mutex, portMAX_DELAY);
    _hr1_dc_tau_s = tau_s;
    _recalc_rate_params();
    if (_initialized) xSemaphoreGive(_state_mutex);
}

void INCUNEST_AFE4490::setHR1MaCutoffHz(float hz) {
    if (_initialized) xSemaphoreTake(_state_mutex, portMAX_DELAY);
    _hr1_ma_cutoff_hz = hz;
    _recalc_rate_params();
    if (_initialized) xSemaphoreGive(_state_mutex);
}

void INCUNEST_AFE4490::setHR1SqiCvMax(float cv) {
    if (_initialized) xSemaphoreTake(_state_mutex, portMAX_DELAY);
    _hr1_sqi_cv_max = cv;
    if (_initialized) xSemaphoreGive(_state_mutex);
}

void INCUNEST_AFE4490::setHR2MinCorr(float corr) {
    if (_initialized) xSemaphoreTake(_state_mutex, portMAX_DELAY);
    _hr2_min_corr = corr;
    if (_initialized) xSemaphoreGive(_state_mutex);
}

void INCUNEST_AFE4490::setHR2UpdateInterval(uint32_t decimated_samples) {
    if (_initialized) xSemaphoreTake(_state_mutex, portMAX_DELAY);
    _hr2_update_interval = decimated_samples;
    if (_initialized) xSemaphoreGive(_state_mutex);
}

void INCUNEST_AFE4490::setHR3UpdateInterval(uint32_t decimated_samples) {
    if (_initialized) xSemaphoreTake(_state_mutex, portMAX_DELAY);
    _hr3_update_interval = decimated_samples;
    if (_initialized) xSemaphoreGive(_state_mutex);
}

void INCUNEST_AFE4490::setHRValidRange(float min_bpm, float max_bpm) {
    if (_initialized) xSemaphoreTake(_state_mutex, portMAX_DELAY);
    _hr_min_bpm = min_bpm;
    _hr_max_bpm = max_bpm;
    if (_initialized) xSemaphoreGive(_state_mutex);
}

// ── getConfig() ───────────────────────────────────────────────────────────────
AFE4490Config INCUNEST_AFE4490::getConfig() {
    AFE4490Config cfg;

    // SPI-mutex fields: sample rate, averages, LED currents, analog front-end settings
#ifndef INCUNEST_OFFLINE
    if (_initialized) xSemaphoreTake(_spi_mutex, portMAX_DELAY);
#endif
    cfg.afe_sample_rate_hz  = _afe_sample_rate_hz;
    cfg.afe_adc_averages    = _afe_adc_averages;
    cfg.afe_led1_current_mA = _afe_led1_current_mA;
    cfg.afe_led2_current_mA = _afe_led2_current_mA;
    cfg.afe_led_range_mA    = _afe_led_range_mA;
    cfg.afe_ensepgain        = _afe_ensepgain;
    cfg.afe_tia_gain_led1    = _afe_tia_gain_led1;
    cfg.afe_tia_cf_led1      = _afe_tia_cf_led1;
    cfg.afe_stage2_gain_led1 = _afe_stage2_gain_led1;
    cfg.afe_stage2_en1       = _afe_stage2_en1;
    cfg.afe_tia_gain_led2    = _afe_tia_gain_led2;
    cfg.afe_tia_cf_led2      = _afe_tia_cf_led2;
    cfg.afe_stage2_gain_led2 = _afe_stage2_gain_led2;
    cfg.afe_stage2_en2       = _afe_stage2_en2;
    cfg.afe_ambdac_uA        = _afe_ambdac_uA;
#ifndef INCUNEST_OFFLINE
    if (_initialized) xSemaphoreGive(_spi_mutex);
#endif

    // state-mutex fields: PPG channel, filters, SpO2 calibration
#ifndef INCUNEST_OFFLINE
    if (_initialized) xSemaphoreTake(_state_mutex, portMAX_DELAY);
#endif
    cfg.ppgdisp_channel      = _ppgdisp_channel;
    cfg.ppgdisp_filter_type  = _ppgdisp_filter_type;
    cfg.ppgdisp_f_low_hz     = _ppgdisp_bpf.f_low;
    cfg.ppgdisp_f_high_hz    = _ppgdisp_bpf.f_high;
    cfg.hr2_f_low_hz     = _hr2_bpf.f_low;
    cfg.hr2_f_high_hz    = _hr2_bpf.f_high;
    cfg.hr3_f_low_hz     = _hr3_bpf.f_low;
    cfg.hr3_f_high_hz    = _hr3_bpf.f_high;
    cfg.spo2_a           = _spo2_a;
    cfg.spo2_b           = _spo2_b;
    cfg.spo2_warmup_s         = _spo2_warmup_s;
    cfg.spo2_dc_iir_tau_s     = _spo2_dc_iir_tau_s;
    cfg.spo2_ac_ema_tau_s     = _spo2_ac_ema_tau_s;
    cfg.spo2_min              = _spo2_min;
    cfg.spo2_max              = _spo2_max;
    cfg.spo2_min_dc           = _spo2_min_dc;
    cfg.spo2_pi_sqi_lo        = _spo2_pi_sqi_lo;
    cfg.spo2_pi_sqi_hi        = _spo2_pi_sqi_hi;
    cfg.hr1_dc_tau_s          = _hr1_dc_tau_s;
    cfg.hr1_ma_cutoff_hz      = _hr1_ma_cutoff_hz;
    cfg.hr1_sqi_cv_max        = _hr1_sqi_cv_max;
    cfg.hr2_min_corr          = _hr2_min_corr;
    cfg.hr2_update_interval   = _hr2_update_interval;
    cfg.hr3_update_interval   = _hr3_update_interval;
    cfg.hr_min_bpm            = _hr_min_bpm;
    cfg.hr_max_bpm            = _hr_max_bpm;
#ifndef INCUNEST_OFFLINE
    if (_initialized) xSemaphoreGive(_state_mutex);
#endif

    return cfg;
}

// ── getTimingConfig() — reads all timing registers from chip via SPI ──────────
AFE4490TimingConfig INCUNEST_AFE4490::getTimingConfig() {
    AFE4490TimingConfig t = {};
#ifndef INCUNEST_OFFLINE
    if (!_initialized) return t;
    xSemaphoreTake(_spi_mutex, portMAX_DELAY);
    // Enable SPI_READ once, read all 28 registers sequentially, then disable.
    _write_reg(REG_CONTROL0, ctrl0_spi_read);
    t.t1  = _read_spi_raw(0x01); t.t2  = _read_spi_raw(0x02);
    t.t3  = _read_spi_raw(0x03); t.t4  = _read_spi_raw(0x04);
    t.t5  = _read_spi_raw(0x05); t.t6  = _read_spi_raw(0x06);
    t.t7  = _read_spi_raw(0x07); t.t8  = _read_spi_raw(0x08);
    t.t9  = _read_spi_raw(0x09); t.t10 = _read_spi_raw(0x0A);
    t.t11 = _read_spi_raw(0x0B); t.t12 = _read_spi_raw(0x0C);
    t.t13 = _read_spi_raw(0x0D); t.t14 = _read_spi_raw(0x0E);
    t.t15 = _read_spi_raw(0x0F); t.t16 = _read_spi_raw(0x10);
    t.t17 = _read_spi_raw(0x11); t.t18 = _read_spi_raw(0x12);
    t.t19 = _read_spi_raw(0x13); t.t20 = _read_spi_raw(0x14);
    t.t21 = _read_spi_raw(0x15); t.t22 = _read_spi_raw(0x16);
    t.t23 = _read_spi_raw(0x17); t.t24 = _read_spi_raw(0x18);
    t.t25 = _read_spi_raw(0x19); t.t26 = _read_spi_raw(0x1A);
    t.t27 = _read_spi_raw(0x1B); t.t28 = _read_spi_raw(0x1C);
    _write_reg(REG_CONTROL0, 0x000000UL);
    xSemaphoreGive(_spi_mutex);
#endif
    return t;
}

// ── setTimingReg() — writes one timing register directly to chip ──────────────
void INCUNEST_AFE4490::setTimingReg(uint8_t addr, uint32_t value) {
#ifndef INCUNEST_OFFLINE
    if (!_initialized) return;
    xSemaphoreTake(_spi_mutex, portMAX_DELAY);
    _write_reg(addr, value);
    xSemaphoreGive(_spi_mutex);
#endif
}

// ── getData() ─────────────────────────────────────────────────────────────────
#ifndef INCUNEST_OFFLINE
bool INCUNEST_AFE4490::getData(AFE4490Data& data) {
    return xQueueReceive(_data_queue, &data, 0) == pdTRUE;
}
#endif

// ── stop() ────────────────────────────────────────────────────────────────────
#ifndef INCUNEST_OFFLINE
void INCUNEST_AFE4490::stop() {
    if (!_initialized) return;

    detachInterrupt(digitalPinToInterrupt(_pin_drdy));

    // Take mutex to wait for any in-progress SPI transaction to finish
    if (_spi_mutex) xSemaphoreTake(_spi_mutex, portMAX_DELAY);

    if (_task_handle)     { vTaskDelete(_task_handle);      _task_handle     = nullptr; }
    if (_hr2_task_handle) { vTaskDelete(_hr2_task_handle); _hr2_task_handle = nullptr; }
    if (_hr3_task_handle) { vTaskDelete(_hr3_task_handle); _hr3_task_handle = nullptr; }

    // Delete FreeRTOS objects (mutexes last since we may hold _spi_mutex)
    if (_data_queue)   { vQueueDelete(_data_queue);          _data_queue   = nullptr; }
    if (_drdy_sem)     { vSemaphoreDelete(_drdy_sem);        _drdy_sem     = nullptr; }
    if (_hr2_calc_sem) { vSemaphoreDelete(_hr2_calc_sem);    _hr2_calc_sem = nullptr; }
    if (_hr3_calc_sem) { vSemaphoreDelete(_hr3_calc_sem);    _hr3_calc_sem = nullptr; }
    if (_spi_mutex)    { vSemaphoreDelete(_spi_mutex);       _spi_mutex    = nullptr; }
    if (_state_mutex)  { vSemaphoreDelete(_state_mutex);     _state_mutex  = nullptr; }

    _initialized = false;
    _reset_algorithms();

    ESP_LOGI(TAG, "Stopped");
}
#endif

// ── _reset_algorithms() ───────────────────────────────────────────────────────
void INCUNEST_AFE4490::_reset_algorithms() {
    _spo2_dc_ir  = 0.0f; _spo2_dc_red  = 0.0f;
    _spo2_ac2_ir = 0.0f; _spo2_ac2_red = 0.0f;
    _spo2_sample_count = 0;
    _hr1_dc                    = 0.0f;
    _hr1_running_max           = 0.0f;
    _hr1_ppg_above_thresh   = false;
    _hr1_last_peak_idx  = 0;
    _hr1_sample_idx     = 0;
    _hr1_interval_count = 0;
    memset(_hr1_intervals, 0, sizeof(_hr1_intervals));
    _ppgdisp_bpf.state           = {0.0f, 0.0f};
    _ppgdisp_bpf.needs_precharge = true;
    memset(_ppgdisp_ma_buf, 0, sizeof(_ppgdisp_ma_buf));
    _ppgdisp_ma_idx = 0; _ppgdisp_ma_sum = 0.0f;
    _hr2_bpf.state           = {0.0f, 0.0f};
    _hr2_bpf.needs_precharge = true;
    _hr2_buf_idx = 0; _hr2_buf_count = 0;
    _hr2_decim_counter = 0; _hr2_update_counter = 0;
    memset(_hr2_buf, 0, sizeof(_hr2_buf));
    _hr3_bpf.state           = {0.0f, 0.0f};
    _hr3_bpf.needs_precharge = true;
    _hr3_buf_idx = 0; _hr3_buf_count = 0;
    _hr3_decim_counter = 0; _hr3_update_counter = 0;
    memset(_hr3_buf, 0, sizeof(_hr3_buf));
    _hr2_computing = false;  _hr3_computing = false;
    _hr2_result = 0.0f; _hr2_sqi_result = 0.0f;
    _hr3_result = 0.0f; _hr3_sqi_result = 0.0f;
    _current_data = AFE4490Data{};
    // Precompute Hann window — needed for HR3 FFT (moved here from begin() to support INCUNEST_OFFLINE)
    for (int i = 0; i < hr3_buf_len; i++)
        _hr3_hann[i] = 0.5f * (1.0f - cosf(2.0f * pi * (float)i / (float)(hr3_buf_len - 1)));
}

// ── SPI primitives ────────────────────────────────────────────────────────────
#ifndef INCUNEST_OFFLINE
void INCUNEST_AFE4490::_write_reg(uint8_t addr, uint32_t data) {
    SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_pin_cs, LOW);
    SPI.transfer(addr);
    SPI.transfer((data >> 16) & 0xFF);
    SPI.transfer((data >>  8) & 0xFF);
    SPI.transfer( data        & 0xFF);
    digitalWrite(_pin_cs, HIGH);
    SPI.endTransaction();
}

// Raw read — caller must have enabled SPI_READ in CONTROL0 beforehand
uint32_t INCUNEST_AFE4490::_read_spi_raw(uint8_t addr) {
    SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_pin_cs, LOW);
    SPI.transfer(addr);
    uint32_t data = ((uint32_t)SPI.transfer(0x00) << 16) |
                    ((uint32_t)SPI.transfer(0x00) <<  8) |
                     (uint32_t)SPI.transfer(0x00);
    digitalWrite(_pin_cs, HIGH);
    SPI.endTransaction();
    return data;
}

uint32_t INCUNEST_AFE4490::_read_reg(uint8_t addr) {
    _write_reg(REG_CONTROL0, ctrl0_spi_read);
    uint32_t val = _read_spi_raw(addr);
    _write_reg(REG_CONTROL0, 0x000000UL);
    return val;
}

uint32_t INCUNEST_AFE4490::runDiagnostics(uint32_t diag_holdoff_ms) {
    if (!_initialized || !_spi_mutex) return 0;
    // Signal _task_body to skip reads during the diagnostic window.
    // Mutex is held only for brief SPI transactions, not for the full 10 ms sleep.
    _diag_active = true;
    xSemaphoreTake(_spi_mutex, portMAX_DELAY);  // wait for any in-progress ADC read to finish
    _write_reg(REG_CONTROL0, 0x000004UL);       // DIAG_EN=1, SPI_READ=0
    xSemaphoreGive(_spi_mutex);                 // release — _task_body skips reads while _diag_active
    vTaskDelay(pdMS_TO_TICKS(10));              // t_DIAG = 8 ms (fast) + 2 ms margin
    xSemaphoreTake(_spi_mutex, portMAX_DELAY);
    _write_reg(REG_CONTROL0, ctrl0_spi_read);   // SPI_READ=1 to read DIAG register
    uint32_t result = _read_spi_raw(REG_DIAG);
    _write_reg(REG_CONTROL0, 0x000000UL);       // restore CONTROL0 to idle
    xSemaphoreGive(_spi_mutex);
    // Set holdoff before clearing _diag_active so _task_body() cannot slip through
    // with a real SPI read between the two assignments.
    _diag_holdoff_samples = (uint32_t)((uint64_t)diag_holdoff_ms * _afe_sample_rate_hz / 1000);
    _diag_active = false;                       // resume; holdoff takes over if diag_holdoff_ms > 0
    return result;
}
#endif

int32_t INCUNEST_AFE4490::_sign_extend_22(uint32_t raw) {
    // AFE4490 ADC output is 22-bit two's complement in bits [21:0]
    return ((int32_t)(raw << 10)) >> 10;
}

// ── Chip init ─────────────────────────────────────────────────────────────────
#ifndef INCUNEST_OFFLINE
void INCUNEST_AFE4490::_chip_init() {
    // Step 2: Set SPI write mode
    _write_reg(REG_CONTROL0, 0x000000UL);

    // Step 3: Software reset
    _write_reg(REG_CONTROL0, ctrl0_sw_rst);
    ESP_LOGI(TAG, "[afe4490] delay 10ms");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGI(TAG, "[afe4490] delay done");

    // Step 4: Analog front-end
    _apply_analog_regs();

    // Step 5: Timing registers
    _apply_timing_regs();

    // Step 6: CONTROL1 (enables timer — must be last)
    _apply_control_regs();

    // Step 7: Stabilization
    ESP_LOGI(TAG, "[afe4490] delay 1000ms");
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "[afe4490] delay done");
}

void INCUNEST_AFE4490::_apply_timing_regs() {
    // Datasheet Table 2 formulas, PRF = _afe_sample_rate_hz
    // AFECLK = 4 MHz → 1 count = 0.25 µs  (afeclk is in the anonymous namespace)
    const uint32_t tia_margin = _compute_settle_margin();  // TIA settling after LED ON
    const uint32_t ambient_margin  = 200;  // counts (50 µs) — LED OFF decay before ambient sampling
    const uint32_t adc_reset       = 3;    // counts (0.75 µs → -60 dB crosstalk)

    uint32_t phase = afeclk / _afe_sample_rate_hz;
    uint32_t prp   = phase - 1;
    uint32_t q     = phase / 4;        // quarter period

    // LED drive windows (25% duty cycle)
    _write_reg(REG_LED2LEDSTC,   3*q);          // t3
    _write_reg(REG_LED2LEDENDC,  prp);           // t4
    _write_reg(REG_LED2STC,      3*q + tia_margin); // t1
    _write_reg(REG_LED2ENDC,     prp - 1);       // t2
    _write_reg(REG_ALED2STC,     ambient_margin);  // t5 — starts 50 µs after LED2 OFF
    _write_reg(REG_ALED2ENDC,    q - 2);           // t6
    _write_reg(REG_LED1LEDSTC,   q);             // t9
    _write_reg(REG_LED1LEDENDC,  2*q - 1);       // t10
    _write_reg(REG_LED1STC,      q + tia_margin); // t7
    _write_reg(REG_LED1ENDC,     2*q - 2);       // t8
    _write_reg(REG_ALED1STC,     2*q + ambient_margin); // t11 — starts 50 µs after LED1 OFF
    _write_reg(REG_ALED1ENDC,    3*q - 2);              // t12

    // ADC reset pulses (3 counts at each phase boundary)
    _write_reg(REG_ADCRSTSTCT0,  0);             // t21
    _write_reg(REG_ADCRSTENDCT0, adc_reset);     // t22
    _write_reg(REG_ADCRSTSTCT1,  q);             // t23
    _write_reg(REG_ADCRSTENDCT1, q  + adc_reset); // t24
    _write_reg(REG_ADCRSTSTCT2,  2*q);           // t25
    _write_reg(REG_ADCRSTENDCT2, 2*q + adc_reset); // t26
    _write_reg(REG_ADCRSTSTCT3,  3*q);           // t27
    _write_reg(REG_ADCRSTENDCT3, 3*q + adc_reset); // t28

    // ADC conversion windows (CONVST = adc_reset_end + 1, CONVEND = next_reset_start - 1)
    _write_reg(REG_LED2CONVST,   adc_reset + 1);            // t13
    _write_reg(REG_LED2CONVEND,  q - 1);                    // t14
    _write_reg(REG_ALED2CONVST,  q  + adc_reset + 1);       // t15
    _write_reg(REG_ALED2CONVEND, 2*q - 1);                  // t16
    _write_reg(REG_LED1CONVST,   2*q + adc_reset + 1);      // t17
    _write_reg(REG_LED1CONVEND,  3*q - 1);                  // t18
    _write_reg(REG_ALED1CONVST,  3*q + adc_reset + 1);      // t19
    _write_reg(REG_ALED1CONVEND, prp);                      // t20

    _write_reg(REG_PRPCOUNT,     prp);                      // t29
}

uint32_t INCUNEST_AFE4490::_build_tiagain_led1() {
    // TIAGAIN register (0x20): RF_LED1[2:0] | CF_LED1[4:0] | STG2GAIN1[2:0] | STAGE2EN1 | ENSEPGAIN
    uint32_t reg = rf_code[(int)_afe_tia_gain_led1];           // bits D[2:0]
    reg         |= cf_code[(int)_afe_tia_cf_led1];             // bits D[7:3]
    reg         |= stg2_gain_code[(int)_afe_stage2_gain_led1]; // bits D[10:8]: gain only
    if (_afe_stage2_en1)  reg |= 0x004000UL;                   // D14: STAGE2EN1 (explicit)
    if (_afe_ensepgain)   reg |= 0x008000UL;                   // D15: ENSEPGAIN
    return reg;
}

uint32_t INCUNEST_AFE4490::_build_tia_amb_gain_led2() {
    // TIA_AMB_GAIN register (0x21): RF_LED2[2:0] | CF_LED2[4:0] | STG2GAIN2[2:0] | STAGE2EN2 | AMBDAC[3:0]
    // RF/CF/Stage2 fields control LED2 (RED) when ENSEPGAIN=1, or both channels when ENSEPGAIN=0.
    uint32_t reg = rf_code[(int)_afe_tia_gain_led2];           // bits D[2:0]
    reg         |= cf_code[(int)_afe_tia_cf_led2];             // bits D[7:3]
    reg         |= stg2_gain_code[(int)_afe_stage2_gain_led2]; // bits D[10:8]: gain only
    reg         |= ((uint32_t)_afe_ambdac_uA << 16);           // bits D[19:16]: AMBDAC[3:0]
    // STAGE2EN2 (D14): set when explicitly enabled OR when AMBDAC > 0.
    // When AMBDAC > 0, Stage 2 must be active — the cancellation current has no effect without it.
    if (_afe_stage2_en2 || _afe_ambdac_uA > 0) reg |= 0x004000UL;
    return reg;
}

void INCUNEST_AFE4490::_apply_analog_regs() {
    // TIAGAIN (0x20): LED1 gain fields + ENSEPGAIN flag.
    // When ENSEPGAIN=0: chip uses TIA_AMB_GAIN for both channels; LED1 fields in TIAGAIN are ignored.
    // When ENSEPGAIN=1: TIAGAIN → LED1 (IR); TIA_AMB_GAIN → LED2 (RED).
    _write_reg(REG_TIAGAIN, _build_tiagain_led1());
    // TIA_AMB_GAIN (0x21): LED2 gain fields + AMBDAC.
    // Controls LED2 (RED) independently when ENSEPGAIN=1; controls both channels when ENSEPGAIN=0.
    _write_reg(REG_TIA_AMB_GAIN, _build_tia_amb_gain_led2());

    // LEDCNTRL: LED_RANGE | (code_led1 << 8) | code_led2
    // I (mA) = (code / 256) * full_scale_mA
    float fs = (float)_afe_led_range_mA;
    uint8_t code1 = (uint8_t)constrain(roundf((_afe_led1_current_mA / fs) * 256.0f), 0.0f, 255.0f);
    uint8_t code2 = (uint8_t)constrain(roundf((_afe_led2_current_mA / fs) * 256.0f), 0.0f, 255.0f);
    uint32_t range_bit = (_afe_led_range_mA == 75) ? 0x010000UL : 0x000000UL;
    _write_reg(REG_LEDCNTRL, range_bit | ((uint32_t)code1 << 8) | code2);

    // CONTROL2: TX_REF=0x00 (0.75 V), all subsystems powered on, H-bridge, crystal enabled
    _write_reg(REG_CONTROL2, 0x000000UL);
}

void INCUNEST_AFE4490::_apply_control_regs() {
    // CONTROL1: TIMEREN | NUMAV
    uint8_t numav = (_afe_adc_averages > 0) ? (_afe_adc_averages - 1u) : 0u;
    _write_reg(REG_CONTROL1, ctrl1_timeren | numav);
}
#endif  // !INCUNEST_OFFLINE

void INCUNEST_AFE4490::_recalc_rate_params() {
    float fs              = (float)_afe_sample_rate_hz;
    _spo2_warmup_samples    = (uint32_t)(_spo2_warmup_s    * fs);
    _hr1_refractory_samples = (uint32_t)(hr1_refractory_s  * fs);
    _spo2_dc_iir_alpha           = expf(-1.0f / (_spo2_dc_iir_tau_s * fs));
    _spo2_ac_ema_beta            = 1.0f - expf(-1.0f / (_spo2_ac_ema_tau_s * fs));
    _hr1_dc_alpha           = expf(-1.0f / (_hr1_dc_tau_s * fs));
    _hr1_ma_len             = (uint32_t)roundf(fs / (2.0f * _hr1_ma_cutoff_hz));
    if (_hr1_ma_len < 1) _hr1_ma_len = 1;
    if (_hr1_ma_len > (uint32_t)hr1_ma_max_len) _hr1_ma_len = (uint32_t)hr1_ma_max_len;
    _recalc_biquad(_ppgdisp_bpf);
    _recalc_biquad(_hr2_bpf);
    _recalc_biquad(_hr3_bpf);
    _recalc_afe_tia_cf_led1();
    _recalc_afe_tia_cf_led2();
}

void INCUNEST_AFE4490::_recalc_biquad_lp(BiquadFilter& filt) {
    // 2nd-order Butterworth low-pass via bilinear transform.
    // Uses filt.f_high as the -3 dB cutoff frequency.
    // DC gain = 1.0; state unchanged (caller is responsible for resetting if needed).
    float fs   = (float)_afe_sample_rate_hz;
    float Ohm  = tanf(pi * filt.f_high / fs);  // prewarped cutoff
    float Ohm2 = Ohm * Ohm;
    float sqrt2 = 1.41421356f;
    float d    = 1.0f + sqrt2 * Ohm + Ohm2;
    filt.b0    =  Ohm2 / d;
    filt.b1    =  2.0f * filt.b0;
    filt.b2    =  filt.b0;
    filt.a1    =  2.0f * (Ohm2 - 1.0f) / d;
    filt.a2    = (1.0f - sqrt2 * Ohm + Ohm2) / d;
}

uint32_t INCUNEST_AFE4490::_compute_settle_margin() const {
    // Settle window = max(tia_settle_min, tia_settle_fraction × LED-on window)
    // LED-on window = quarter period = afeclk / (4 × _afe_sample_rate_hz) counts
    uint32_t q      = (afeclk / _afe_sample_rate_hz) / 4u;
    uint32_t margin = (uint32_t)((float)q * tia_settle_fraction);
    return (margin < tia_settle_min) ? tia_settle_min : margin;
}

void INCUNEST_AFE4490::_recalc_afe_tia_cf_led1() {
    // Constraint: 5τ ≤ settle_time  →  CF ≤ settle_time / (5 × RF_LED1)
    float settle_s  = (float)_compute_settle_margin() / (float)afeclk;
    float tau_max   = settle_s / tia_n_tau;
    float cf_max_pF = (tau_max / tia_rf_ohm[(int)_afe_tia_gain_led1]) * 1e12f;
    _afe_tia_cf_led1 = AFE4490TIACF::CF_5P;
    for (int i = (int)AFE4490TIACF::CF_155P; i > (int)AFE4490TIACF::CF_5P; --i) {
        if (tia_cf_pF[i] <= cf_max_pF) { _afe_tia_cf_led1 = (AFE4490TIACF)i; break; }
    }
}

void INCUNEST_AFE4490::_recalc_afe_tia_cf_led2() {
    // Constraint: 5τ ≤ settle_time  →  CF ≤ settle_time / (5 × RF_LED2)
    float settle_s  = (float)_compute_settle_margin() / (float)afeclk;
    float tau_max   = settle_s / tia_n_tau;
    float cf_max_pF = (tau_max / tia_rf_ohm[(int)_afe_tia_gain_led2]) * 1e12f;
    _afe_tia_cf_led2 = AFE4490TIACF::CF_5P;
    for (int i = (int)AFE4490TIACF::CF_155P; i > (int)AFE4490TIACF::CF_5P; --i) {
        if (tia_cf_pF[i] <= cf_max_pF) { _afe_tia_cf_led2 = (AFE4490TIACF)i; break; }
    }
}

void INCUNEST_AFE4490::_recalc_biquad(BiquadFilter& filt) {
    // 2nd-order Butterworth bandpass via bilinear transform.
    // Analog prototype: H(s) = BW·s / (s² + BW·s + Ω₀²)
    float fs    = (float)_afe_sample_rate_hz;
    float k     = 2.0f * fs;
    float o_low = k * tanf(pi * filt.f_low  / fs);
    float o_hi  = k * tanf(pi * filt.f_high / fs);
    float o0sq  = o_low * o_hi;
    float bw    = o_hi - o_low;
    float d     = k*k + bw*k + o0sq;
    filt.b0 =  bw * k / d;
    filt.b1 =  0.0f;
    filt.b2 = -bw * k / d;
    filt.a1 =  2.0f * (o0sq - k*k) / d;
    filt.a2 =  (k*k - bw*k + o0sq) / d;
}

// ── FreeRTOS task ─────────────────────────────────────────────────────────────
// FreeRTOS requires the task entry point to be a plain C function (static or free function).
// _task_trampoline satisfies that requirement: it receives the INCUNEST_AFE4490 instance pointer
// via the pvParameters argument and immediately forwards execution to _task_body(), which is
// the actual member function with full access to private state. This pattern (trampoline +
// member body) is the standard idiom for running a C++ method as a FreeRTOS task.
#ifndef INCUNEST_OFFLINE
void INCUNEST_AFE4490::_task_trampoline(void* pv) {
    static_cast<INCUNEST_AFE4490*>(pv)->_task_body();
    vTaskDelete(nullptr); // should never reach here
}

void INCUNEST_AFE4490::_task_body() {
    for (;;) {
        // Block until DRDY fires (100 ms watchdog — warns if chip stops outputting)
        if (xSemaphoreTake(_drdy_sem, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "DRDY timeout: no sample in 100 ms");
            continue;
        }

        if (_diag_active) {
            // Chip cannot produce valid ADC data during diagnostics (~10 ms, ≈5 samples).
            // Repeat last valid output to preserve temporal equi-spacing for HR2/HR3.
            xSemaphoreTake(_state_mutex, portMAX_DELAY);
            if (xQueueSend(_data_queue, &_current_data, 0) != pdTRUE) {
                AFE4490Data dummy;
                xQueueReceive(_data_queue, &dummy, 0);
                xQueueSend(_data_queue, &_current_data, 0);
            }
            xSemaphoreGive(_state_mutex);
            continue;
        }

        if (_diag_holdoff_samples > 0) {
            // Post-diagnostic holdoff: analog front-end is re-settling after DIAG_EN cleared.
            // Feed the frozen last-valid raw values into _process_sample() instead of reading
            // the chip. A constant input is absorbed by IIR/BPF/LP filters with near-zero AC
            // contribution — far less disruptive to the algorithms than a transient outlier.
            --_diag_holdoff_samples;
            xSemaphoreTake(_state_mutex, portMAX_DELAY);
            _process_sample(_diag_last_led1, _diag_last_led2,
                            _diag_last_aled1, _diag_last_aled2,
                            _diag_last_led1_aled1, _diag_last_led2_aled2);
            xSemaphoreGive(_state_mutex);
            continue;
        }

        // _spi_mutex: protects the SPI bus while reading all 6 channels.
#if INCUNEST_TIMING_STATS
        uint64_t _t_cycle = esp_timer_get_time();
#endif
        xSemaphoreTake(_spi_mutex, portMAX_DELAY);
        // Enable SPI read mode once, burst-read all 6 channels, disable
        _write_reg(REG_CONTROL0, ctrl0_spi_read);
        int32_t led2      = _sign_extend_22(_read_spi_raw(REG_LED2VAL));
        int32_t aled2     = _sign_extend_22(_read_spi_raw(REG_ALED2VAL));
        int32_t led1      = _sign_extend_22(_read_spi_raw(REG_LED1VAL));
        int32_t aled1     = _sign_extend_22(_read_spi_raw(REG_ALED1VAL));
        int32_t led2_diff = _sign_extend_22(_read_spi_raw(REG_LED2_ALED2VAL));
        int32_t led1_diff = _sign_extend_22(_read_spi_raw(REG_LED1_ALED1VAL));
        _write_reg(REG_CONTROL0, 0x000000UL);
        xSemaphoreGive(_spi_mutex);

        // Save raw values for use during any subsequent diagnostic holdoff.
        // Written and read exclusively from _task_body() — no mutex required.
        _diag_last_led1       = led1;
        _diag_last_led2       = led2;
        _diag_last_aled1      = aled1;
        _diag_last_aled2      = aled2;
        _diag_last_led1_aled1 = led1_diff;
        _diag_last_led2_aled2 = led2_diff;

        // _state_mutex: protects internal processing state (_ppgdisp_channel, filter
        // buffers, SpO2/HR accumulators) against concurrent config setter calls.
        xSemaphoreTake(_state_mutex, portMAX_DELAY);
        _process_sample(led1, led2, aled1, aled2, led1_diff, led2_diff);
        xSemaphoreGive(_state_mutex);
#if INCUNEST_TIMING_STATS
        _ts_cycle.update(esp_timer_get_time() - _t_cycle);
        if (++_ts_emit_counter >= ts_emit_interval) {
            _ts_emit_counter = 0;
            _emit_timing();
        }
#endif
    }
}

// ── ISR ───────────────────────────────────────────────────────────────────────
// Trampoline required because attachInterrupt() only accepts a plain C function pointer;
// C++ member functions are not compatible. _drdy_isr_static is registered with
// attachInterrupt() and forwards the call to the actual member ISR (_drdy_isr) via
// the singleton pointer _g_instance. The null-check guards against a spurious interrupt
// arriving after stop() has cleared _g_instance.
void IRAM_ATTR INCUNEST_AFE4490::_drdy_isr_static() {
    if (_g_instance) _g_instance->_drdy_isr();
}

void IRAM_ATTR INCUNEST_AFE4490::_drdy_isr() {
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(_drdy_sem, &woken);
    portYIELD_FROM_ISR(woken);
}
#endif  // !INCUNEST_OFFLINE

// ── Signal processing ─────────────────────────────────────────────────────────

// Direct Form II Transposed biquad.
// On the first call (needs_precharge=true), pre-charges the state to DC steady-state
// so the first output is ~0 instead of a large transient. Derivation:
//   y_ss  = x0 * (b0+b1+b2) / (1+a1+a2)   (= 0 for a bandpass filter)
//   v1_ss = y_ss - b0*x0
//   v2_ss = b2*x0 - a2*y_ss
float INCUNEST_AFE4490::_biquad_process(float x, BiquadFilter& filt) {
    if (filt.needs_precharge) {
        float y_ss      = x * (filt.b0 + filt.b1 + filt.b2) / (1.0f + filt.a1 + filt.a2);
        filt.state.v1   = y_ss - filt.b0 * x;
        filt.state.v2   = filt.b2 * x - filt.a2 * y_ss;
        filt.needs_precharge = false;
    }
    float y       = filt.b0 * x + filt.state.v1;
    filt.state.v1 = filt.b1 * x - filt.a1 * y + filt.state.v2;
    filt.state.v2 = filt.b2 * x - filt.a2 * y;
    return y;
}

void INCUNEST_AFE4490::_process_sample(int32_t led1, int32_t led2, int32_t aled1, int32_t aled2,
                                   int32_t led1_aled1, int32_t led2_aled2) {
    // Select PPG source
    float raw_ppg;
    switch (_ppgdisp_channel) {
        case AFE4490Channel::LED1:       raw_ppg = (float)led1;       break;
        case AFE4490Channel::LED2:       raw_ppg = (float)led2;       break;
        case AFE4490Channel::ALED1:      raw_ppg = (float)aled1;      break;
        case AFE4490Channel::ALED2:      raw_ppg = (float)aled2;      break;
        case AFE4490Channel::LED2_ALED2: raw_ppg = (float)led2_aled2; break;
        default: /* LED1_ALED1 */        raw_ppg = (float)led1_aled1; break;
    }

    // Apply filter
    float filtered;
    switch (_ppgdisp_filter_type) {
        case AFE4490Filter::BUTTERWORTH:
            filtered = _biquad_process(raw_ppg, _ppgdisp_bpf);
            break;
        case AFE4490Filter::MOVING_AVERAGE: {
            _ppgdisp_ma_sum -= _ppgdisp_ma_buf[_ppgdisp_ma_idx];
            _ppgdisp_ma_buf[_ppgdisp_ma_idx] = raw_ppg;
            _ppgdisp_ma_sum += raw_ppg;
            _ppgdisp_ma_idx = (_ppgdisp_ma_idx + 1) % ma_len;
            filtered = _ppgdisp_ma_sum / (float)ma_len;
            break;
        }
        default: /* NONE */
            filtered = raw_ppg;
            break;
    }

    _current_data.ppg        = -(int32_t)filtered;  // negated: AFE raw falls on systole; invert for conventional PPG polarity (peaks up)
    _current_data.led1       = led1;
    _current_data.led2       = led2;
    _current_data.aled1      = aled1;
    _current_data.aled2      = aled2;
    _current_data.led1_aled1 = led1_aled1;
    _current_data.led2_aled2 = led2_aled2;

    // SpO2 uses ambient-corrected channels (unfiltered, spec §1.3)
    // HR1 runs fully in this task. HR2/HR3 fast paths run here; slow computation in Task B/C.
#if INCUNEST_TIMING_STATS
    { uint64_t _t = esp_timer_get_time(); _update_spo2(led1_aled1, led2_aled2); _ts_spo2.update(esp_timer_get_time() - _t); }
    { uint64_t _t = esp_timer_get_time(); _update_hr1(led1_aled1);              _ts_hr1.update(esp_timer_get_time() - _t); }
    uint64_t _t_hr2 = esp_timer_get_time();
#else
    _update_spo2(led1_aled1, led2_aled2);
    _update_hr1(led1_aled1);
#endif

    // HR2 fast path: filter + decimate + buffer; trigger Task B when interval fires
    if (_update_hr2_sample(led1_aled1)) {
        if (!_hr2_computing) {
            _linearize_hr2();
            _hr2_computing = true;
            xSemaphoreGive(_hr2_calc_sem);
        }
    }

#if INCUNEST_TIMING_STATS
    _ts_hr2.update(esp_timer_get_time() - _t_hr2);
    uint64_t _t_hr3 = esp_timer_get_time();
#endif

    // HR3 fast path: LP filter + decimate + buffer; trigger Task C when interval fires
    if (_update_hr3_sample(led1_aled1)) {
        if (!_hr3_computing) {
            _linearize_hr3();
            _hr3_computing = true;
            xSemaphoreGive(_hr3_calc_sem);
        }
    }

#if INCUNEST_TIMING_STATS
    _ts_hr3.update(esp_timer_get_time() - _t_hr3);
#endif

    // Push to queue; if full, drop oldest to keep most recent
    if (xQueueSend(_data_queue, &_current_data, 0) != pdTRUE) {
        AFE4490Data dummy;
        xQueueReceive(_data_queue, &dummy, 0);
        xQueueSend(_data_queue, &_current_data, 0);
    }
}

// ── Timing report ─────────────────────────────────────────────────────────────
// Emits a $TIMING serial frame with per-algorithm mean/max execution times (µs)
// and remaining task stack (words). Called every ts_emit_interval samples.
// Frame: $TIMING,hr1_mean,hr1_max,hr2fp_mean,hr2fp_max,hr3fp_mean,hr3fp_max,
//                spo2_mean,spo2_max,cycle_mean,cycle_max,
//                hr2cmp_mean,hr2cmp_max,hr3cmp_mean,hr3cmp_max,stack_free*XX
// Task A fast-path (first 10 values): budget reference = 2000 µs (1/500 Hz).
// Task B/C compute (values 11-14): CPU load % = mean / 500000 µs (0.5 s period).
#if INCUNEST_TIMING_STATS && !defined(INCUNEST_OFFLINE)
void INCUNEST_AFE4490::_emit_timing() {
    UBaseType_t stack_free = uxTaskGetStackHighWaterMark(nullptr);
    char buf[256];
    int n = snprintf(buf, sizeof(buf),
        "TIMING,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%u",
        (unsigned long long)_ts_hr1.mean_us(),          (unsigned long long)_ts_hr1.max_us,
        (unsigned long long)_ts_hr2.mean_us(),          (unsigned long long)_ts_hr2.max_us,
        (unsigned long long)_ts_hr3.mean_us(),          (unsigned long long)_ts_hr3.max_us,
        (unsigned long long)_ts_spo2.mean_us(),         (unsigned long long)_ts_spo2.max_us,
        (unsigned long long)_ts_cycle.mean_us(),        (unsigned long long)_ts_cycle.max_us,
        (unsigned long long)_ts_hr2_compute.mean_us(),  (unsigned long long)_ts_hr2_compute.max_us,
        (unsigned long long)_ts_hr3_compute.mean_us(),  (unsigned long long)_ts_hr3_compute.max_us,
        (unsigned)stack_free);
    uint8_t chk = 0;
    for (int i = 0; i < n; i++) chk ^= (uint8_t)buf[i];
    Serial.printf("$%s*%02X\r\n", buf, chk);
    // Emit task CPU stats BEFORE resetting accumulators (needs sum_us values)
    _emit_tasks();
    _ts_spo2.reset(); _ts_hr1.reset(); _ts_hr2.reset(); _ts_hr3.reset(); _ts_cycle.reset();
    _ts_hr2_compute.reset(); _ts_hr3_compute.reset();
}

// Emits one $TASK frame per library task + $TASKS_END.
// CPU% is computed from sum_us over the emit window (current interval, not cumulative).
// Window = ts_emit_interval samples / sample_rate_hz.
// stack_words = uxTaskGetStackHighWaterMark() for each task handle.
// Does NOT use uxTaskGetSystemState() — avoids requiring configUSE_TRACE_FACILITY=1
// in the precompiled Arduino ESP32 FreeRTOS library.
void INCUNEST_AFE4490::_emit_tasks() {
    uint64_t window_us = (uint64_t)ts_emit_interval * 1000000u / _afe_sample_rate_hz;

    struct LibTask { const char* name; uint64_t sum_us; TaskHandle_t handle; };
    LibTask lib_tasks[] = {
        { "incunest_afe4490", _ts_cycle.sum_us,        _task_handle      },
        { "incunest_hr2",     _ts_hr2_compute.sum_us,   _hr2_task_handle  },
        { "incunest_hr3",     _ts_hr3_compute.sum_us,   _hr3_task_handle  },
    };
    for (const auto& t : lib_tasks) {
        uint32_t pct_x10 = (window_us > 0) ?
            (uint32_t)(t.sum_us * 1000u / window_us) : 0u;
        uint32_t stack = t.handle ? (uint32_t)uxTaskGetStackHighWaterMark(t.handle) : 0u;
        char buf[96];
        int len = snprintf(buf, sizeof(buf), "TASK,%s,%lu,%lu",
            t.name, (unsigned long)pct_x10, (unsigned long)stack);
        uint8_t chk = 0;
        for (int j = 0; j < len; j++) chk ^= (uint8_t)buf[j];
        Serial.printf("$%s*%02X\r\n", buf, chk);
    }
    const char* end_str = "TASKS_END";
    uint8_t end_chk = 0;
    for (int i = 0; end_str[i]; i++) end_chk ^= (uint8_t)end_str[i];
    Serial.printf("$%s*%02X\r\n", end_str, end_chk);
}
#endif

// ── SpO2 algorithm ────────────────────────────────────────────────────────────
// R = (AC_rms_RED / DC_RED) / (AC_rms_IR / DC_IR)
// SpO2 = a - b * R
// ir_corr  : IR  signal ambient-corrected (led1 - aled1)
// red_corr : RED signal ambient-corrected (led2 - aled2)
void INCUNEST_AFE4490::_update_spo2(int32_t ir_corr, int32_t red_corr) {
    float ir  = (float)ir_corr;
    float red = (float)red_corr;

    // IIR DC extraction
    _spo2_dc_ir  = _spo2_dc_iir_alpha * _spo2_dc_ir  + (1.0f - _spo2_dc_iir_alpha) * ir;
    _spo2_dc_red = _spo2_dc_iir_alpha * _spo2_dc_red + (1.0f - _spo2_dc_iir_alpha) * red;

    // EMA of AC²
    float ac_ir  = ir  - _spo2_dc_ir;
    float ac_red = red - _spo2_dc_red;
    _spo2_ac2_ir  = _spo2_ac_ema_beta * ac_ir  * ac_ir  + (1.0f - _spo2_ac_ema_beta) * _spo2_ac2_ir;
    _spo2_ac2_red = _spo2_ac_ema_beta * ac_red * ac_red + (1.0f - _spo2_ac_ema_beta) * _spo2_ac2_red;

    // Perfusion Index (always updated, independent of SpO2 warmup)
    _current_data.pi = (_spo2_dc_ir > 1.0f) ? (sqrtf(_spo2_ac2_ir) / _spo2_dc_ir) * 100.0f : -1.0f;

    // Probe state — reuses thresholds already used by the SpO2 algorithm.
    if (_spo2_dc_ir < _spo2_min_dc || _spo2_dc_red < _spo2_min_dc) {
        _current_data.probe_state = ProbeState::DISCONNECTED;
    } else if (_current_data.pi < _spo2_pi_sqi_lo) {
        _current_data.probe_state = ProbeState::NOT_APPLIED;
    } else {
        _current_data.probe_state = ProbeState::APPLIED;
    }

    _spo2_sample_count++;

    // Skip during warmup or if DC is too low (no finger)
    if (_spo2_sample_count < _spo2_warmup_samples ||
        _spo2_dc_ir < _spo2_min_dc || _spo2_dc_red < _spo2_min_dc) {
        _current_data.spo2_sqi = 0.0f;
        return;
    }

    float rms_ac_ir  = sqrtf(_spo2_ac2_ir);
    float rms_ac_red = sqrtf(_spo2_ac2_red);

    // Avoid division by near-zero
    if (_spo2_dc_ir < 1.0f || _spo2_dc_red < 1.0f || rms_ac_ir < 1.0f) {
        _current_data.spo2_sqi = 0.0f;
        return;
    }

    float R    = (rms_ac_red / _spo2_dc_red) / (rms_ac_ir / _spo2_dc_ir);
    float spo2 = _spo2_a - _spo2_b * R;

    _current_data.spo2_r = R;

    if (spo2 >= _spo2_min && spo2 <= _spo2_max + spo2_clamp_margin) {
        _current_data.spo2 = fminf(spo2, _spo2_max);
        // SQI: Perfusion Index linearly mapped to [0, 1].
        // PI < spo2_pi_sqi_lo → SQI = 0 (no finger or very weak signal).
        // PI ≥ spo2_pi_sqi_hi → SQI = 1 (full quality, per Nellcor/Masimo thresholds).
        float sqi = (_current_data.pi - _spo2_pi_sqi_lo) / (_spo2_pi_sqi_hi - _spo2_pi_sqi_lo);
        _current_data.spo2_sqi = fmaxf(0.0f, fminf(1.0f, sqi));
    } else {
        _current_data.spo2_sqi = 0.0f;
    }
}

// ── HR algorithm ──────────────────────────────────────────────────────────────
// Adaptive-threshold peak detection on filtered PPG.
// Threshold = 0.6 × running_max; refractory = _hr1_refractory_samples.
// HR reported from average of 5 consecutive RR intervals.
void INCUNEST_AFE4490::_update_hr1(int32_t led1_aled1) {
    _hr1_sample_idx++;

    // DC removal: IIR estimator (tau = hr1_dc_tau_s), then negate for conventional PPG polarity (peaks up)
    float s = (float)led1_aled1;
    _hr1_dc = _hr1_dc_alpha * _hr1_dc + (1.0f - _hr1_dc_alpha) * s;
    // Apply dedicated MA low-pass filter (5 Hz cutoff, independent of PPG display filter)
    float raw = -(s - _hr1_dc);
    _hr1_ma_sum -= _hr1_ma_buf[_hr1_ma_idx];
    _hr1_ma_buf[_hr1_ma_idx] = raw;
    _hr1_ma_sum += raw;
    _hr1_ma_idx = (_hr1_ma_idx + 1) % (int)_hr1_ma_len;
    float ppg_filtered = _hr1_ma_sum / (float)_hr1_ma_len;

    // Running max: slow exponential decay keeps it tracking signal amplitude
    _hr1_running_max = fmaxf(_hr1_running_max * 0.9999f, ppg_filtered);

    float threshold = 0.6f * _hr1_running_max;

    // Threshold crossing (rising edge only)
    if (ppg_filtered > threshold && !_hr1_ppg_above_thresh) {
        _hr1_ppg_above_thresh = true;

        uint32_t elapsed = _hr1_sample_idx - _hr1_last_peak_idx;
        if (_hr1_last_peak_idx > 0 && elapsed > _hr1_refractory_samples) {
            // Shift interval buffer and store new interval
            for (int i = 4; i > 0; i--) _hr1_intervals[i] = _hr1_intervals[i - 1];
            _hr1_intervals[0] = (int32_t)elapsed;
            if (_hr1_interval_count < 5) _hr1_interval_count++;
        }
        _hr1_last_peak_idx = _hr1_sample_idx;

    } else if (ppg_filtered <= threshold) {
        _hr1_ppg_above_thresh = false;
    }

    // Need 5 intervals for a stable estimate
    if (_hr1_interval_count < 5) {
        _current_data.hr1_sqi = 0.0f;
        return;
    }

    float sum = 0.0f;
    for (int i = 0; i < 5; i++) sum += (float)_hr1_intervals[i];
    float avg_interval = sum / 5.0f;

    float hr1 = ((float)_afe_sample_rate_hz * 60.0f) / avg_interval;

    if (hr1 >= _hr_min_bpm && hr1 <= _hr_max_bpm) {
        _current_data.hr1 = hr1;
        // SQI: coefficient of variation (CV = std / mean) of the 5 RR intervals.
        // Perfectly regular rhythm → CV = 0 → SQI = 1.
        // CV ≥ hr1_sqi_cv_max (15%) → SQI = 0 (arrhythmia or motion artefact).
        float var = 0.0f;
        for (int i = 0; i < 5; i++) {
            float diff = (float)_hr1_intervals[i] - avg_interval;
            var += diff * diff;
        }
        float cv  = (avg_interval > 0.0f) ? sqrtf(var / 5.0f) / avg_interval : 1.0f;
        float sqi = 1.0f - cv / _hr1_sqi_cv_max;
        _current_data.hr1_sqi = fmaxf(0.0f, fminf(1.0f, sqi));
    } else {
        _current_data.hr1_sqi = 0.0f;
    }
}

// ── HR2 algorithm — split into fast path + async computation ──────────────────

// Fast path: called every raw sample from Task A.
// Returns true when the computation window fires (buffer full, interval elapsed).
bool INCUNEST_AFE4490::_update_hr2_sample(int32_t led1_aled1) {
    float filtered = -_biquad_process((float)led1_aled1, _hr2_bpf);

    _hr2_decim_counter++;
    if (_hr2_decim_counter < (uint32_t)hr2_decim_factor) return false;
    _hr2_decim_counter = 0;

    _hr2_buf[_hr2_buf_idx] = filtered;
    _hr2_buf_idx = (_hr2_buf_idx + 1) % hr2_buf_len;
    if (_hr2_buf_count < (uint32_t)hr2_buf_len) _hr2_buf_count++;

    _hr2_update_counter++;
    if (_hr2_update_counter < _hr2_update_interval) return false;
    _hr2_update_counter = 0;

    return _hr2_buf_count >= (uint32_t)hr2_buf_len;
}

// Linearize circular buffer (oldest → newest) into _hr2_seg.
// Called under _state_mutex by Task A immediately before signalling Task B.
void INCUNEST_AFE4490::_linearize_hr2() {
    for (int i = 0; i < hr2_buf_len; i++)
        _hr2_seg[i] = _hr2_buf[(_hr2_buf_idx + i) % hr2_buf_len];
}

// Slow path: autocorrelation on _hr2_seg → _hr2_result / _hr2_sqi_result.
// Called by Task B. Reads only _hr2_seg (no other shared state written).
void INCUNEST_AFE4490::_compute_hr2() {
    float acorr0 = 0.0f;
    for (int i = 0; i < hr2_buf_len; i++) acorr0 += _hr2_seg[i] * _hr2_seg[i];
    if (acorr0 < 1.0f) { _hr2_sqi_result = 0.0f; return; }

    float fs2     = (float)_afe_sample_rate_hz / (float)hr2_decim_factor;
    int   min_lag = (int)(60.0f / (_hr_max_bpm + 3.0f) * fs2);
    if (min_lag < 1) min_lag = 1;
    int   max_lag = hr2_acorr_max_lag;
    int   n_lags  = max_lag - min_lag + 1;

    // Unbiased normalised autocorrelation: divide by (acorr0 * (N-lag)/N) instead of acorr0.
    // The biased estimator (/ acorr0) systematically underestimates because its numerator
    // sums only (N-lag) terms while the denominator reflects N terms. For a perfectly
    // periodic signal this yields SQI = (N-lag)/N < 1 — e.g. ~0.875 at 60 BPM (lag=50,
    // N=400). The unbiased correction restores SQI ≈ 1.0 for a clean periodic signal.
    float acorr_buf[hr2_acorr_max_lag + 1];
    for (int lag = min_lag; lag <= max_lag; lag++) {
        float sum   = 0.0f;
        int   n     = hr2_buf_len - lag;
        for (int i = 0; i < n; i++) sum += _hr2_seg[i] * _hr2_seg[i + lag];
        acorr_buf[lag - min_lag] = (n > 0) ? sum * (float)hr2_buf_len / (acorr0 * (float)n) : 0.0f;
    }

    int   peak_idx = -1;
    float y_prev = 0.0f, y_peak = 0.0f, y_next = 0.0f;
    for (int i = 1; i < n_lags - 1; i++) {
        if (acorr_buf[i] > acorr_buf[i - 1] &&
            acorr_buf[i] > acorr_buf[i + 1] &&
            acorr_buf[i] >= _hr2_min_corr) {
            peak_idx = i;
            y_prev   = acorr_buf[i - 1];
            y_peak   = acorr_buf[i];
            y_next   = acorr_buf[i + 1];
            break;
        }
    }

    if (peak_idx < 0) { _hr2_sqi_result = 0.0f; return; }

    float denom      = y_prev - 2.0f * y_peak + y_next;
    float delta      = (denom < 0.0f) ? 0.5f * (y_prev - y_next) / denom : 0.0f;
    float peak_lag_s = (float)(min_lag + peak_idx + delta) / fs2;

    if (peak_lag_s <= 0.0f) { _hr2_sqi_result = 0.0f; return; }

    float hr2 = 60.0f / peak_lag_s;
    if (hr2 >= _hr_min_bpm && hr2 <= _hr_max_bpm) {
        _hr2_result     = hr2;
        _hr2_sqi_result = y_peak;
    } else {
        _hr2_sqi_result = 0.0f;
    }
}

// Synchronous wrapper kept for unit-test compatibility.
void INCUNEST_AFE4490::_update_hr2(int32_t led1_aled1) {
    if (!_update_hr2_sample(led1_aled1)) return;
    _linearize_hr2();
    _compute_hr2();
    if (_hr2_sqi_result > 0.0f) _current_data.hr2 = _hr2_result;
    _current_data.hr2_sqi = _hr2_sqi_result;
}

// ── HR3 algorithm — split into fast path + async computation ──────────────────

// Fast path: called every raw sample from Task A.
// Returns true when the computation window fires (buffer full, interval elapsed).
bool INCUNEST_AFE4490::_update_hr3_sample(int32_t led1_aled1) {
    float filtered = -_biquad_process((float)led1_aled1, _hr3_bpf);  // negate: peaks up

    _hr3_decim_counter++;
    if (_hr3_decim_counter < (uint32_t)hr3_decim_factor) return false;
    _hr3_decim_counter = 0;

    _hr3_buf[_hr3_buf_idx] = filtered;
    _hr3_buf_idx = (_hr3_buf_idx + 1) % hr3_buf_len;
    if (_hr3_buf_count < (uint32_t)hr3_buf_len) _hr3_buf_count++;

    _hr3_update_counter++;
    if (_hr3_update_counter < _hr3_update_interval) return false;
    _hr3_update_counter = 0;

    return _hr3_buf_count >= (uint32_t)hr3_buf_len;
}

// Linearize circular buffer + DC removal + Hann window → complex FFT input in _hr3_fft.
// Called under _state_mutex by Task A immediately before signalling Task C.
void INCUNEST_AFE4490::_linearize_hr3() {
    float mean = 0.0f;
    for (int i = 0; i < hr3_buf_len; i++)
        mean += _hr3_buf[(_hr3_buf_idx + i) % hr3_buf_len];
    mean /= (float)hr3_buf_len;

    for (int i = 0; i < hr3_buf_len; i++) {
        float sample = _hr3_buf[(_hr3_buf_idx + i) % hr3_buf_len] - mean;
        _hr3_fft[2 * i]     = sample * _hr3_hann[i];  // real — uses precomputed Hann window
        _hr3_fft[2 * i + 1] = 0.0f;                   // imag
    }
}

// Slow path: FFT + HPS on _hr3_fft → _hr3_result / _hr3_sqi_result.
// Called by Task C. Reads/writes only _hr3_fft and result floats (no other shared state).
void INCUNEST_AFE4490::_compute_hr3() {
    // ── In-place radix-2 DIT FFT ──
    _fft_r2(_hr3_fft, hr3_buf_len);

    // ── Find HPS peak in guard-band search range ──
    float fs_dec   = (float)_afe_sample_rate_hz / (float)hr3_decim_factor;
    float bin_res  = fs_dec / (float)hr3_buf_len;

    int search_min = (int)ceilf((_hr_min_bpm - 3.0f) / 60.0f / bin_res);
    int search_max = (int)floorf((_hr_max_bpm + 3.0f) / 60.0f / bin_res);
    int nyquist    = hr3_buf_len / 2;
    if (search_max >= nyquist)    search_max = nyquist - 2;
    if (search_max > nyquist / 3) search_max = nyquist / 3;  // 3rd harmonic must stay inside Nyquist
    if (search_min < 1)           search_min = 1;
    if (search_min >= search_max) { _hr3_sqi_result = 0.0f; return; }

    // HPS = P[k] * P[2k] * P[3k]  where P[k] = re[k]^2 + im[k]^2
    int   peak_bin  = -1;
    float peak_hps  = 0.0f;
    float hps_sum   = 0.0f;  // sum of HPS values across search range (used for SQI)
    for (int k = search_min; k <= search_max; k++) {
        float p1 = _hr3_fft[2*k]   * _hr3_fft[2*k]   + _hr3_fft[2*k+1]   * _hr3_fft[2*k+1];
        float p2 = _hr3_fft[4*k]   * _hr3_fft[4*k]   + _hr3_fft[4*k+1]   * _hr3_fft[4*k+1];
        float p3 = _hr3_fft[6*k]   * _hr3_fft[6*k]   + _hr3_fft[6*k+1]   * _hr3_fft[6*k+1];
        float hps = p1 * p2 * p3;
        hps_sum += hps;
        if (hps > peak_hps) { peak_hps = hps; peak_bin = k; }
    }

    if (peak_bin < 1 || peak_bin >= nyquist - 1 || peak_hps <= 0.0f) {
        _hr3_sqi_result = 0.0f;
        return;
    }

    // ── Parabolic interpolation on original spectrum around HPS peak ──
    float yp = _hr3_fft[2*(peak_bin-1)] * _hr3_fft[2*(peak_bin-1)] + _hr3_fft[2*(peak_bin-1)+1] * _hr3_fft[2*(peak_bin-1)+1];
    float y0 = _hr3_fft[2* peak_bin]    * _hr3_fft[2* peak_bin]    + _hr3_fft[2* peak_bin+1]    * _hr3_fft[2* peak_bin+1];
    float yn = _hr3_fft[2*(peak_bin+1)] * _hr3_fft[2*(peak_bin+1)] + _hr3_fft[2*(peak_bin+1)+1] * _hr3_fft[2*(peak_bin+1)+1];
    float denom = yp - 2.0f * y0 + yn;
    float delta = (denom < 0.0f) ? 0.5f * (yp - yn) / denom : 0.0f;
    float peak_freq = ((float)peak_bin + delta) * bin_res;  // Hz

    if (peak_freq <= 0.0f) { _hr3_sqi_result = 0.0f; return; }

    float hr3 = 60.0f * peak_freq;
    if (hr3 >= _hr_min_bpm && hr3 <= _hr_max_bpm) {
        _hr3_result = hr3;
        // SQI: HPS peak prominence — fraction of total HPS energy at the interpolated peak.
        //
        // Problem with integer-bin HPS: when the true fundamental falls between bins k and k+1
        // (e.g. 85 BPM = bin 14.5), the Hann window splits energy across both bins. Since
        // HPS = P[k]*P[2k]*P[3k] is a *product*, the loss is cubic — a 50% split at each
        // harmonic yields only ~12% of the ideal HPS peak, making the peak appear non-dominant
        // and collapsing SQI to ~0.5 even for a clean signal.
        //
        // Fix: parabolic interpolation on the HPS values themselves, consistent with how delta
        // is already used to interpolate the frequency. This recovers the true HPS peak height
        // at the fractional bin position.
        auto hps_at = [&](int k) -> float {
            float p1 = _hr3_fft[2*k]   * _hr3_fft[2*k]   + _hr3_fft[2*k+1]   * _hr3_fft[2*k+1];
            float p2 = _hr3_fft[4*k]   * _hr3_fft[4*k]   + _hr3_fft[4*k+1]   * _hr3_fft[4*k+1];
            float p3 = _hr3_fft[6*k]   * _hr3_fft[6*k]   + _hr3_fft[6*k+1]   * _hr3_fft[6*k+1];
            return p1 * p2 * p3;
        };
        float hps_p = hps_at(peak_bin - 1);
        float hps_n = hps_at(peak_bin + 1);
        float hps_denom = hps_p - 2.0f * peak_hps + hps_n;
        // Parabolic peak value: h(delta) = h0 - (hp-hn)^2 / (8*(hp-2*h0+hn))
        float hps_interp = (hps_denom < 0.0f)
            ? peak_hps - 0.125f * (hps_p - hps_n) * (hps_p - hps_n) / hps_denom
            : peak_hps;

        int   n_bins   = search_max - search_min + 1;
        float baseline = (n_bins > 1) ? 1.0f / (float)n_bins : 0.0f;
        float fraction = (hps_sum > 0.0f) ? hps_interp / hps_sum : 0.0f;
        float sqi      = (n_bins > 1) ? (fraction - baseline) / (1.0f - baseline) : 0.0f;
        _hr3_sqi_result = fmaxf(0.0f, fminf(1.0f, sqi));
    } else {
        _hr3_sqi_result = 0.0f;
    }
}

// Synchronous wrapper kept for unit-test compatibility.
void INCUNEST_AFE4490::_update_hr3(int32_t led1_aled1) {
    if (!_update_hr3_sample(led1_aled1)) return;
    _linearize_hr3();
    _compute_hr3();
    if (_hr3_sqi_result > 0.0f) _current_data.hr3 = _hr3_result;
    _current_data.hr3_sqi = _hr3_sqi_result;
}

// ── HR2 async task (Task B) ────────────────────────────────────────────────────
#ifndef INCUNEST_OFFLINE
void INCUNEST_AFE4490::_hr2_task_trampoline(void* pv) {
    static_cast<INCUNEST_AFE4490*>(pv)->_hr2_task_body();
    vTaskDelete(nullptr);
}

// Blocks on _hr2_calc_sem. When signalled by Task A:
//   1. Runs autocorrelation on the already-linearized _hr2_seg snapshot.
//   2. Takes _state_mutex to write results into _current_data.
//   3. Clears _hr2_computing so Task A knows the slot is free.
void INCUNEST_AFE4490::_hr2_task_body() {
    for (;;) {
        xSemaphoreTake(_hr2_calc_sem, portMAX_DELAY);

#if INCUNEST_TIMING_STATS
        { uint64_t _t = esp_timer_get_time(); _compute_hr2(); _ts_hr2_compute.update(esp_timer_get_time() - _t); }
#else
        _compute_hr2();  // reads _hr2_seg only — no shared-state write
#endif

        xSemaphoreTake(_state_mutex, portMAX_DELAY);
        if (_hr2_sqi_result > 0.0f) _current_data.hr2 = _hr2_result;
        _current_data.hr2_sqi = _hr2_sqi_result;
        xSemaphoreGive(_state_mutex);

        _hr2_computing = false;  // release slot after results committed
    }
}
#endif  // !INCUNEST_OFFLINE

// ── HR3 async task (Task C) ────────────────────────────────────────────────────
#ifndef INCUNEST_OFFLINE
void INCUNEST_AFE4490::_hr3_task_trampoline(void* pv) {
    static_cast<INCUNEST_AFE4490*>(pv)->_hr3_task_body();
    vTaskDelete(nullptr);
}

// Blocks on _hr3_calc_sem. When signalled by Task A:
//   1. Runs FFT + HPS on the already-prepared _hr3_fft buffer.
//   2. Takes _state_mutex to write results into _current_data.
//   3. Clears _hr3_computing so Task A knows the slot is free.
void INCUNEST_AFE4490::_hr3_task_body() {
    for (;;) {
        xSemaphoreTake(_hr3_calc_sem, portMAX_DELAY);

#if INCUNEST_TIMING_STATS
        { uint64_t _t = esp_timer_get_time(); _compute_hr3(); _ts_hr3_compute.update(esp_timer_get_time() - _t); }
#else
        _compute_hr3();  // reads/writes _hr3_fft only — no other shared state
#endif

        xSemaphoreTake(_state_mutex, portMAX_DELAY);
        if (_hr3_sqi_result > 0.0f) _current_data.hr3 = _hr3_result;
        _current_data.hr3_sqi = _hr3_sqi_result;
        xSemaphoreGive(_state_mutex);

        _hr3_computing = false;  // release slot after results committed
    }
}
#endif  // !INCUNEST_OFFLINE
