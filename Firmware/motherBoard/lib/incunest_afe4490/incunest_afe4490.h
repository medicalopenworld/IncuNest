#pragma once

// incunest_afe4490 — Medical Open World AFE4490 driver + PPG algorithms (HR, SpO2)
// Library version: v0.25 — ESP32-S3, Arduino + FreeRTOS
// Spec: incunest_afe4490_spec.md
// Chip datasheet: https://www.ti.com/lit/ds/symlink/afe4490.pdf
// Author: Medical Open World — http://medicalopenworld.org — <contact@medicalopenworld.org>

#define INCUNEST_AFE4490_VERSION "0.25"

#ifdef INCUNEST_OFFLINE
  #ifndef UNIT_TEST
    #define UNIT_TEST
  #endif
  #include "incunest_afe4490_platform_stub.h"
#else
  #include <Arduino.h>
  #include <SPI.h>
  #include <freertos/FreeRTOS.h>
  #include <freertos/task.h>
  #include <freertos/semphr.h>
  #include <freertos/queue.h>
  #include <stdint.h>
#endif

// ── Compile-time configuration (override before including this header) ────────
#ifndef INCUNEST_AFE4490_QUEUE_SIZE
#define INCUNEST_AFE4490_QUEUE_SIZE      10
#endif

#ifndef INCUNEST_AFE4490_TASK_PRIORITY
#define INCUNEST_AFE4490_TASK_PRIORITY   5
#endif

#ifndef INCUNEST_AFE4490_TASK_STACK
#define INCUNEST_AFE4490_TASK_STACK      8192  // increased from 4096: HR3 FFT calls cosf/sinf which needs extra stack
#endif

#ifndef INCUNEST_AFE4490_HR2_TASK_STACK
#define INCUNEST_AFE4490_HR2_TASK_STACK  3072  // acorr_buf[138] on stack + overhead
#endif

#ifndef INCUNEST_AFE4490_HR3_TASK_STACK
#define INCUNEST_AFE4490_HR3_TASK_STACK  2048  // FFT data lives in _hr3_fft member, minimal stack
#endif

#ifndef INCUNEST_AFE4490_HR23_TASK_PRIORITY
#define INCUNEST_AFE4490_HR23_TASK_PRIORITY  (INCUNEST_AFE4490_TASK_PRIORITY - 1)
#endif

#ifndef INCUNEST_TIMING_STATS
#define INCUNEST_TIMING_STATS 0
#endif

// Probe contact state — updated every sample inside _update_spo2().
// Initialises to DISCONNECTED (= 0) via zero-initialisation of AFE4490Data.
enum class ProbeState : uint8_t {
    DISCONNECTED = 0,   // DC level below spo2_min_dc — no optical path
    NOT_APPLIED  = 1,   // DC valid, but PI < spo2_pi_sqi_lo — probe not on skin
    APPLIED      = 2    // PI >= spo2_pi_sqi_lo — valid contact
};

// ── Public data struct ────────────────────────────────────────────────────────
struct AFE4490Data {
    // Field order mirrors the $M1/$P1 serial frame: raw signals first, then processed outputs
    // Raw ADC outputs (6 signals from AFE4490)
    int32_t led2;        // LED2VAL  — RED raw          (frame: RED)
    int32_t led1;        // LED1VAL  — IR raw           (frame: IR)
    int32_t aled2;       // ALED2VAL — ambient after LED2 (frame: RED_Amb)
    int32_t aled1;       // ALED1VAL — ambient after LED1 (frame: IR_Amb)
    int32_t led2_aled2;  // LED2-ALED2 — RED ambient-corrected (frame: RED_Sub)
    int32_t led1_aled1;  // LED1-ALED1 — IR ambient-corrected  (frame: IR_Sub)
    // Processed outputs
    int32_t ppg;         // filtered PPG of selected channel
    float   spo2;        // SpO2 in %
    float   spo2_sqi;    // SpO2 Signal Quality Index [0–1]: PI-based; 0=invalid/no finger, 1=full quality (PI ≥ 2%)
    float   spo2_r;      // R ratio used for SpO2 calculation: (AC_red/DC_red)/(AC_ir/DC_ir)
    float   pi;          // Perfusion Index: (AC_ir / DC_ir) * 100 [%]
    float   hr1;         // HR1 (peak detection) in bpm
    float   hr1_sqi;     // HR1 Signal Quality Index [0–1]: 1 − CV/0.15; 0=arrhythmia/artefact/invalid, 1=perfectly regular
    float   hr2;         // HR2 (autocorrelation) in bpm
    float   hr2_sqi;     // HR2 Signal Quality Index [0–1]: normalised autocorrelation at dominant lag; 0=no periodicity, 1=perfect
    float   hr3;         // HR3 (FFT + HPS) in bpm
    float   hr3_sqi;     // HR3 Signal Quality Index [0–1]: HPS peak prominence in search range; 0=diffuse HPS, 1=dominant peak
    ProbeState probe_state; // contact state — DISCONNECTED / NOT_APPLIED / APPLIED
};

// ── Enumerations ──────────────────────────────────────────────────────────────
enum class AFE4490Channel {
    LED1,       // IR raw
    LED2,       // RED raw
    ALED1,      // ambient after LED1
    ALED2,      // ambient after LED2
    LED1_ALED1, // IR ambient-corrected (default)
    LED2_ALED2  // RED ambient-corrected
};

enum class AFE4490Filter {
    NONE,
    MOVING_AVERAGE,
    BUTTERWORTH  // default: 0.5–20 Hz Butterworth bandpass
};

enum class AFE4490TIAGain {
    RF_10K,
    RF_25K,
    RF_50K,
    RF_100K,
    RF_250K,
    RF_500K,  // default
    RF_1M
};

enum class AFE4490TIACF {
    CF_5P,    // 5 pF (default)
    CF_10P,
    CF_20P,
    CF_30P,
    CF_55P,
    CF_155P
};

enum class AFE4490Stage2Gain {
    GAIN_0DB,    // Stage 2 disabled (default)
    GAIN_3_5DB,
    GAIN_6DB,
    GAIN_9_5DB,
    GAIN_12DB
};

// ── Timing registers snapshot ─────────────────────────────────────────────────
// Returned by getTimingConfig(). Raw register values (counts, AFECLK = 4 MHz → 1 count = 0.25 µs).
// Field names follow datasheet Table 2 notation (t1–t28). t29 = PRPCOUNT is in AFE4490Config.
struct AFE4490TimingConfig {
    uint32_t t1,  t2;   // LED2STC,    LED2ENDC    — LED2 sampling window
    uint32_t t3,  t4;   // LED2LEDSTC, LED2LEDENDC — LED2 LED pulse
    uint32_t t5,  t6;   // ALED2STC,   ALED2ENDC   — ambient LED2 sampling
    uint32_t t7,  t8;   // LED1STC,    LED1ENDC    — LED1 sampling window
    uint32_t t9,  t10;  // LED1LEDSTC, LED1LEDENDC — LED1 LED pulse
    uint32_t t11, t12;  // ALED1STC,   ALED1ENDC   — ambient LED1 sampling
    uint32_t t13, t14;  // LED2CONVST,  LED2CONVEND  — LED2 ADC conversion
    uint32_t t15, t16;  // ALED2CONVST, ALED2CONVEND — ambient LED2 ADC conversion
    uint32_t t17, t18;  // LED1CONVST,  LED1CONVEND  — LED1 ADC conversion
    uint32_t t19, t20;  // ALED1CONVST, ALED1CONVEND — ambient LED1 ADC conversion
    uint32_t t21, t22;  // ADCRSTSTCT0, ADCRSTENDCT0 — ADC reset pulse 0
    uint32_t t23, t24;  // ADCRSTSTCT1, ADCRSTENDCT1 — ADC reset pulse 1
    uint32_t t25, t26;  // ADCRSTSTCT2, ADCRSTENDCT2 — ADC reset pulse 2
    uint32_t t27, t28;  // ADCRSTSTCT3, ADCRSTENDCT3 — ADC reset pulse 3
};

// ── Chip configuration snapshot ───────────────────────────────────────────────
// Returned by getConfig(). Groups all runtime-configurable parameters.
struct AFE4490Config {
    uint16_t          afe_sample_rate_hz;    // ADC sample rate [Hz]
    uint8_t           afe_adc_averages;      // ADC hardware averages (1 = no averaging)
    float             afe_led1_current_mA;   // IR LED drive current [mA]
    float             afe_led2_current_mA;   // RED LED drive current [mA]
    uint8_t           afe_led_range_mA;      // LED full-scale range: 75 or 150 mA
    // TIA gain — separate per-channel (ENSEPGAIN=1) or shared (ENSEPGAIN=0)
    // When afe_ensepgain=false: hardware uses LED2 fields for both channels (TIAGAIN LED1 fields ignored by chip)
    // When afe_ensepgain=true:  TIAGAIN register controls LED1 (IR); TIA_AMB_GAIN controls LED2 (RED)
    bool              afe_ensepgain;         // ENSEPGAIN bit D15 in TIAGAIN register
    AFE4490TIAGain    afe_tia_gain_led1;     // RF for LED1 (IR)  — active when afe_ensepgain=true
    AFE4490TIACF      afe_tia_cf_led1;       // CF for LED1 (IR)  — active when afe_ensepgain=true
    AFE4490Stage2Gain afe_stage2_gain_led1;  // Stage 2 STG2GAIN1 for LED1 (IR) — gain only, no enable
    bool              afe_stage2_en1;        // STAGE2EN1 (D14 of TIAGAIN) — explicit Stage 2 enable for LED1
    AFE4490TIAGain    afe_tia_gain_led2;     // RF for LED2 (RED) — always active (both channels when ENSEPGAIN=0)
    AFE4490TIACF      afe_tia_cf_led2;       // CF for LED2 (RED) — always active
    AFE4490Stage2Gain afe_stage2_gain_led2;  // Stage 2 STG2GAIN2 for LED2 (RED) — gain only, no enable
    bool              afe_stage2_en2;        // STAGE2EN2 (D14 of TIA_AMB_GAIN) — explicit Stage 2 enable for LED2
    uint8_t           afe_ambdac_uA;         // Ambient cancellation DAC [0–10 µA]; AMBDAC[3:0] in TIA_AMB_GAIN D19:D16
    AFE4490Channel    ppgdisp_channel;       // PPG display channel selection
    AFE4490Filter     ppgdisp_filter_type;   // PPG display filter type
    float             ppgdisp_f_low_hz;      // PPG display bandpass lower cutoff [Hz]
    float             ppgdisp_f_high_hz;     // PPG display bandpass upper cutoff [Hz]
    float             hr2_f_low_hz;      // HR2 bandpass lower cutoff [Hz]
    float             hr2_f_high_hz;     // HR2 bandpass upper cutoff [Hz]
    float             hr3_f_low_hz;      // HR3 bandpass lower cutoff [Hz]
    float             hr3_f_high_hz;     // HR3 bandpass upper cutoff [Hz]
    float             spo2_a;                   // SpO2 calibration: SpO2 = a - b*R
    float             spo2_b;
    // SpO2 algorithm parameters
    float             spo2_warmup_s;             // warmup before reporting SpO2 [s]
    float             spo2_dc_iir_tau_s;         // DC removal IIR time constant [s]
    float             spo2_ac_ema_tau_s;         // AC² EMA time constant [s]
    float             spo2_min;                  // valid output lower bound [%]
    float             spo2_max;                  // valid output upper bound [%]
    float             spo2_min_dc;               // no-finger DC threshold [ADC counts]
    float             spo2_pi_sqi_lo;            // PI below this → SQI=0 [%]
    float             spo2_pi_sqi_hi;            // PI above this → SQI=1 [%]
    // HR1 algorithm parameters
    float             hr1_dc_tau_s;              // DC removal IIR time constant [s]
    float             hr1_ma_cutoff_hz;          // peak detection LP MA cutoff [Hz]
    float             hr1_sqi_cv_max;            // CV above this → SQI=0 [dimensionless]
    // HR2 algorithm parameters
    float             hr2_min_corr;              // min normalised autocorrelation for SQI=1
    uint32_t          hr2_update_interval;       // autocorrelation recompute period [decimated samples]
    // HR3 algorithm parameters
    uint32_t          hr3_update_interval;       // FFT+HPS recompute period [decimated samples]
    // HR valid range (HR1 + HR2 + HR3)
    float             hr_min_bpm;                // valid output lower bound [bpm]
    float             hr_max_bpm;                // valid output upper bound [bpm]
};

// ── INCUNEST_AFE4490 class ─────────────────────────────────────────────────────────
class INCUNEST_AFE4490 {
public:
    INCUNEST_AFE4490();
    ~INCUNEST_AFE4490();

    // Initialization — configures chip with defaults, attaches DRDY ISR, starts task.
    // Requires SPI.begin() to have been called beforehand by the application.
    // This library does not call SPI.begin() internally to avoid interfering with
    // other SPI devices sharing the same bus.
#ifndef INCUNEST_OFFLINE
    void begin(int pin_cs, int pin_drdy);
#endif

    // Chip configuration setters (callable before or after begin())
    void setSampleRate(uint16_t hz);        // 63–5000 Hz; recalculates NUMAV_max
    void setAdcAverages(uint8_t num);       // 1=no averaging; clamped to floor(5000/PRF)
    void setLED1Current(float mA);
    void setLED2Current(float mA);
    void setLEDRange(uint8_t mA);           // 75 or 150 mA
    // Joint setters — apply the same value to both LED1 and LED2 channels; also sets ENSEPGAIN=0
    void setTIAGain(AFE4490TIAGain gain);   // sets afe_tia_gain_led1 = afe_tia_gain_led2 = gain
    void setTIACF(AFE4490TIACF cf);         // sets afe_tia_cf_led1 = afe_tia_cf_led2 = cf
    void setStage2Gain(AFE4490Stage2Gain gain); // sets afe_stage2_gain_led1 = afe_stage2_gain_led2 = gain
    // Separate gain mode — enable independent LED1/LED2 TIA settings (ENSEPGAIN bit D15 in TIAGAIN)
    void setEnSepGain(bool enable);
    // Per-channel setters — only meaningful when ENSEPGAIN=1 (setEnSepGain(true))
    void setTIAGainLED1(AFE4490TIAGain gain);   // RF for LED1 (IR)  — writes TIAGAIN RF_LED1[2:0]
    void setTIACFLED1(AFE4490TIACF cf);         // CF for LED1 (IR)  — writes TIAGAIN CF_LED1[4:0]
    void setStage2GainLED1(AFE4490Stage2Gain gain); // STG2GAIN1[2:0] for LED1 (IR) — gain only, does not touch STAGE2EN1
    void setStage2En1(bool en);             // STAGE2EN1 (D14 of TIAGAIN) — explicit Stage 2 enable for LED1
    void setTIAGainLED2(AFE4490TIAGain gain);   // RF for LED2 (RED) — writes TIA_AMB_GAIN RF_LED2[2:0]
    void setTIACFLED2(AFE4490TIACF cf);         // CF for LED2 (RED) — writes TIA_AMB_GAIN CF_LED2[4:0]
    void setStage2GainLED2(AFE4490Stage2Gain gain); // STG2GAIN2[2:0] for LED2 (RED) — gain only, does not touch STAGE2EN2
    void setStage2En2(bool en);             // STAGE2EN2 (D14 of TIA_AMB_GAIN) — explicit Stage 2 enable for LED2
    void setAmbDac(uint8_t uA);             // Ambient cancellation DAC current [0–10 µA]; AMBDAC[3:0] in TIA_AMB_GAIN D19:D16

    // Signal and filter configuration
    void setPPGChannel(AFE4490Channel channel);
    void setFilter(AFE4490Filter type, float f_low_hz = 0.5f, float f_high_hz = 20.0f);

    // HR2 bandpass filter cutoffs (default 0.5–5 Hz); callable before or after begin()
    void setHR2Filter(float f_low_hz = 0.5f, float f_high_hz = 5.0f);

    // HR3 bandpass filter cutoffs (default 0.4–15 Hz); callable before or after begin()
    void setHR3Filter(float f_low_hz = 0.4f, float f_high_hz = 15.0f);

    // SpO2 algorithm setters (callable before or after begin())
    void setSpO2WarmupS(float s);                              // warmup period before reporting SpO2
    void setSpO2DcIirTauS(float tau_s);                        // DC IIR time constant; triggers recalc
    void setSpO2AcEmaTauS(float tau_s);                        // AC² EMA time constant; triggers recalc
    void setSpO2Range(float min_pct, float max_pct);           // valid SpO2 output range [%]
    void setSpO2MinDC(float counts);                           // no-finger DC threshold [ADC counts]
    void setSpO2PiSqiThresholds(float lo_pct, float hi_pct);  // PI-based SQI thresholds [%]

    // HR1 algorithm setters
    void setHR1DcTauS(float tau_s);                            // DC removal IIR time constant; triggers recalc
    void setHR1MaCutoffHz(float hz);                           // peak detection LP MA cutoff; triggers recalc
    void setHR1SqiCvMax(float cv);                             // SQI CV threshold [dimensionless]

    // HR2 algorithm setters
    void setHR2MinCorr(float corr);                            // min autocorrelation for SQI=1
    void setHR2UpdateInterval(uint32_t decimated_samples);     // autocorrelation recompute period

    // HR3 algorithm setters
    void setHR3UpdateInterval(uint32_t decimated_samples);     // FFT+HPS recompute period

    // HR valid range setter (applies to HR1 + HR2 + HR3)
    void setHRValidRange(float min_bpm, float max_bpm);

    /**
     * @brief Retrieves the next available sample from the internal queue (non-blocking).
     *
     * The library samples internally at the configured rate (default 500 Hz) regardless
     * of how often this function is called.
     *
     * The internal queue holds up to INCUNEST_AFE4490_QUEUE_SIZE (10) samples. When full,
     * the oldest sample is automatically discarded to make room for the newest.
     *
     * Behaviour by consumption rate:
     *  - **> 500 Hz**: most calls return false — no data available yet.
     *  - **≈ 500 Hz**: ideal — queue stays near-empty, samples consumed in order, no discards.
     *  - **Occasional jitter**: queue buffers up to 10 samples without discards.
     *  - **Consistently < 500 Hz**: queue stays full in steady state; samples are returned in
     *    FIFO order with a fixed latency of 10 × T_sample (20 ms at 500 Hz). Intermediate
     *    samples are discarded (e.g. at 100 Hz, 1 in every 5 samples is received).
     *
     * The internal algorithms (HR1, HR2, HR3, SpO2) process every sample at the configured
     * rate regardless of consumption rate — call frequency does not affect calculation quality.
     *
     * @param data  Output: filled with the oldest queued sample if available.
     * @return true if data was available, false if the queue was empty.
     */
#ifndef INCUNEST_OFFLINE
    bool getData(AFE4490Data& data);
#endif

    // Shutdown — detaches ISR, deletes internal task and FreeRTOS objects, resets state.
    // After stop(), begin() can be called again to restart.
#ifndef INCUNEST_OFFLINE
    void stop();
#endif

    // SpO2 calibration coefficients (SpO2 = a - b*R).
    // Defaults are experimentally calibrated for UpnMed U401-D(01AS-F), Nellcor Non-Oximax type.
    void setSpO2Coefficients(float a, float b);

    // Returns a snapshot of all current chip and algorithm configuration parameters.
    // Thread-safe: takes the relevant mutexes if the library is initialized.
    // Callable before or after begin().
    AFE4490Config getConfig();

    // Returns all timing register values read directly from the AFE4490 chip via SPI.
    // Requires begin() to have been called. Returns zeros if not initialized.
    // Thread-safe: takes _spi_mutex.
    AFE4490TimingConfig getTimingConfig();

    // Writes a single timing register directly to the chip (takes effect on next PRP cycle).
    // addr: register address (0x01–0x1C). value: 16-bit count (AFECLK = 4 MHz).
    // No validation is performed — caller is responsible for constraint checking.
    // Requires begin() to have been called. No-op if not initialized.
    // Thread-safe: takes _spi_mutex.
    void setTimingReg(uint8_t addr, uint32_t value);

    // Runs the AFE4490 hardware diagnostics sequence (Table 3, section 8.4.3.3).
    // Holds _spi_mutex for ~10 ms to prevent the data task from clearing CONTROL0.
    // Returns the raw 24-bit DIAG register (0x30) with all 13 diagnostic flags.
    // Returns 0 if the library is not initialized.
    //
    // diag_holdoff_ms: after diagnostics end, _task_body() feeds the last valid
    // raw ADC values into _process_sample() for this many milliseconds instead of
    // reading the chip. This replaces transient outliers (analog front-end re-settling
    // after DIAG_EN is cleared) with neutral frozen input that the IIR/BPF/LP filters
    // absorb gracefully. Set to 0 to observe the raw diagnostic artefacts.
    // Default: 10 ms (~5 samples at 500 Hz), covers worst-case TIA settling (RF_1M + CF_155P: 5τ ≈ 775 µs).
#ifndef INCUNEST_OFFLINE
    uint32_t runDiagnostics(uint32_t diag_holdoff_ms = 10);
#endif

    // ISR entry point (must be public for static trampoline)
#ifndef INCUNEST_OFFLINE
    void _drdy_isr();
#endif

private:
    // ── Private types ─────────────────────────────────────────────────────────

    struct BiquadState { float v1, v2; };

    // BiquadFilter groups coefficients, state and cutoff frequencies for one filter instance.
    // _recalc_biquad() writes b0/b1/b2/a1/a2 from f_low/f_high and _afe_sample_rate_hz.
    // _biquad_step() and _biquad_precharge() operate on state in-place.
    struct BiquadFilter {
        float f_low, f_high;          // cutoff frequencies (Hz) — parameterisable at runtime
        float b0, b1, b2, a1, a2;    // DF-II transposed coefficients
        BiquadState state;
        bool  needs_precharge;        // true after reset; consumed on first sample
    };

    // ── SPI primitives ────────────────────────────────────────────────────────
#ifndef INCUNEST_OFFLINE
    void     _write_reg(uint8_t addr, uint32_t data);
    uint32_t _read_spi_raw(uint8_t addr);   // assumes SPI_READ already enabled
    uint32_t _read_reg(uint8_t addr);       // handles SPI_READ enable/disable
#endif

    // Sign-extend 22-bit two's complement ADC output
    static int32_t _sign_extend_22(uint32_t raw);

    // Recomputes rate-dependent algorithm parameters from _afe_sample_rate_hz
    void _recalc_rate_params();
    // Recomputes Butterworth bandpass biquad coefficients into filt from _afe_sample_rate_hz and filt.f_low/f_high
    void _recalc_biquad(BiquadFilter& filt);
    // Recomputes Butterworth low-pass biquad coefficients into filt (uses filt.f_high as cutoff)
    void _recalc_biquad_lp(BiquadFilter& filt);
    // Returns TIA settle margin in AFECLK counts: max(tia_settle_min, 10% of LED-on window)
    uint32_t _compute_settle_margin() const;
    // Selects the largest AFE4490TIACF that settles within _compute_settle_margin() for the given RF.
    // Called automatically by _recalc_rate_params() and setTIAGain*(); setTIACF*() overrides.
    void _recalc_afe_tia_cf_led1();   // updates _afe_tia_cf_led1 from _afe_tia_gain_led1
    void _recalc_afe_tia_cf_led2();   // updates _afe_tia_cf_led2 from _afe_tia_gain_led2
    // Returns mA rounded to the nearest DAC step: round(mA/range*256)/256*range
    float _quantize_led_mA(float mA) const;

    // Chip init
#ifndef INCUNEST_OFFLINE
    void _chip_init();
    void _apply_timing_regs();
    void _apply_analog_regs();
    void _apply_control_regs();
    uint32_t _build_tiagain_led1();        // TIAGAIN register value (LED1 fields + ENSEPGAIN bit)
    uint32_t _build_tia_amb_gain_led2();   // TIA_AMB_GAIN register value (LED2 fields + AMBDAC)
#endif

    // FreeRTOS task
#ifndef INCUNEST_OFFLINE
    static void _task_trampoline(void* pv);
    void _task_body();
#endif

    // Signal processing
    float _biquad_process(float x, BiquadFilter& filt);  // precharge on first call, then step
    void  _process_sample(int32_t led1, int32_t led2, int32_t aled1, int32_t aled2,
                          int32_t led1_aled1, int32_t led2_aled2);

    // Algorithms — synchronous (used by unit tests and SpO2/HR1)
    void _update_spo2(int32_t ir_corr, int32_t red_corr);
    void _update_hr1(int32_t led1_aled1);
    // HR2/HR3 synchronous entry points (unit-test only; production uses split paths below)
    void _update_hr2(int32_t led1_aled1);
    void _update_hr3(int32_t led1_aled1);
    void _reset_algorithms();

    // HR2 async split: fast per-sample path + linearise + compute
    bool _update_hr2_sample(int32_t led1_aled1); // filter+decimate+buffer; returns true when interval fires
    void _linearize_hr2();                        // copy _hr2_buf → _hr2_seg (call under _state_mutex)
    void _compute_hr2();                          // autocorr on _hr2_seg → _hr2_result/_hr2_sqi_result

    // HR3 async split
    bool _update_hr3_sample(int32_t led1_aled1); // filter+decimate+buffer; returns true when interval fires
    void _linearize_hr3();                        // DC+Hann into _hr3_fft (call under _state_mutex)
    void _compute_hr3();                          // FFT+HPS on _hr3_fft → _hr3_result/_hr3_sqi_result

    // HR2/HR3 async FreeRTOS tasks
#ifndef INCUNEST_OFFLINE
    static void _hr2_task_trampoline(void* pv);
    void _hr2_task_body();
    static void _hr3_task_trampoline(void* pv);
    void _hr3_task_body();
#endif

    // ── Hardware ──
    int _pin_cs;
    int _pin_drdy;

    // ── FreeRTOS ──
    SemaphoreHandle_t _drdy_sem;
    SemaphoreHandle_t _spi_mutex;    // protects SPI bus access (_write_reg / _read_spi_raw)
    SemaphoreHandle_t _state_mutex;  // protects internal processing state (_ppgdisp_channel, filter
                                     // buffers, SpO2/HR accumulators) shared between _process_sample()
                                     // and the config setters that do not access the SPI bus
    QueueHandle_t     _data_queue;
    TaskHandle_t      _task_handle;
    bool              _initialized;
    volatile bool     _diag_active;          // true while runDiagnostics() holds DIAG_EN; _task_body() skips reads
    volatile uint32_t _diag_holdoff_samples; // countdown after diagnostics end; _task_body() feeds frozen raw input
    int32_t _diag_last_led1, _diag_last_led2;          // last valid raw ADC values, saved on every normal cycle
    int32_t _diag_last_aled1, _diag_last_aled2;
    int32_t _diag_last_led1_aled1, _diag_last_led2_aled2;

    // ── HR2/HR3 async computation tasks ──────────────────────────────────────
    // Task A (_task_body) runs the fast per-sample path and signals Task B/C when
    // the computation window fires. Task B/C run the slow autocorr / FFT+HPS
    // outside the real-time loop, then write results under _state_mutex.
    SemaphoreHandle_t _hr2_calc_sem;    // given by Task A, taken by Task B
    SemaphoreHandle_t _hr3_calc_sem;    // given by Task A, taken by Task C
    TaskHandle_t      _hr2_task_handle;
    TaskHandle_t      _hr3_task_handle;
    volatile bool     _hr2_computing;   // true while Task B holds _hr2_seg; prevents Task A from overwriting
    volatile bool     _hr3_computing;   // true while Task C uses _hr3_fft; prevents Task A from overwriting
    float             _hr2_result;      // written by Task B, copied to _current_data under _state_mutex
    float             _hr2_sqi_result;
    float             _hr3_result;      // written by Task C
    float             _hr3_sqi_result;

#if INCUNEST_TIMING_STATS
    // ── Timing instrumentation ─────────────────────────────────────────────────
    struct TimingStat {
        uint64_t max_us = 0;
        uint64_t sum_us = 0;
        uint32_t count  = 0;
        void update(uint64_t dt) { if (dt > max_us) max_us = dt; sum_us += dt; count++; }
        uint64_t mean_us() const { return count ? sum_us / count : 0; }
        void reset() { max_us = sum_us = count = 0; }
    };
    TimingStat _ts_spo2, _ts_hr1, _ts_hr2, _ts_hr3, _ts_cycle;  // Task A fast-path timings
    TimingStat _ts_hr2_compute, _ts_hr3_compute;                 // Task B/C slow-path timings
    // Note: uxTaskGetStackHighWaterMark() returns bytes on ESP32 (portSTACK_TYPE = uint8_t)
    uint32_t   _ts_emit_counter = 0;
    static constexpr uint32_t ts_emit_interval = 2500;  // emit every 5 s at 500 Hz
    void _emit_timing();
    void _emit_tasks();   // emits $TASK frame per FreeRTOS task + $TASKS_END
#endif

    // ── Chip configuration ──
    uint16_t          _afe_sample_rate_hz;
    uint8_t           _afe_adc_averages;     // user-visible count (1 = no averaging)
    float             _afe_led1_current_mA;
    float             _afe_led2_current_mA;
    uint8_t           _afe_led_range_mA;     // 75 or 150
    // TIA gain — separate per-channel when _afe_ensepgain=true
    // When false: chip uses LED2 fields for both channels; LED1 fields written but ignored by hardware
    // When true:  TIAGAIN→LED1(IR), TIA_AMB_GAIN→LED2(RED)
    bool              _afe_ensepgain;
    AFE4490TIAGain    _afe_tia_gain_led1;    // RF_LED1 in TIAGAIN
    AFE4490TIACF      _afe_tia_cf_led1;      // CF_LED1 in TIAGAIN
    AFE4490Stage2Gain _afe_stage2_gain_led1; // STG2GAIN1[2:0] in TIAGAIN D[10:8]
    bool              _afe_stage2_en1;       // STAGE2EN1 (D14 of TIAGAIN)
    AFE4490TIAGain    _afe_tia_gain_led2;    // RF_LED2 in TIA_AMB_GAIN
    AFE4490TIACF      _afe_tia_cf_led2;      // CF_LED2 in TIA_AMB_GAIN
    AFE4490Stage2Gain _afe_stage2_gain_led2; // STG2GAIN2[2:0] in TIA_AMB_GAIN D[10:8]
    bool              _afe_stage2_en2;       // STAGE2EN2 (D14 of TIA_AMB_GAIN); also forced ON when _afe_ambdac_uA > 0
    uint8_t           _afe_ambdac_uA;        // AMBDAC[3:0] value (0–10), written to TIA_AMB_GAIN D19:D16

    // ── Signal processing configuration ──
    AFE4490Channel    _ppgdisp_channel;
    AFE4490Filter     _ppgdisp_filter_type;

    // ── PPG display filter (Butterworth bandpass or MA, configurable via setFilter()) ──
    BiquadFilter      _ppgdisp_bpf;          // default: 0.5–20 Hz

    // ── Moving average state (PPG display filter — used when _ppgdisp_filter_type == MOVING_AVERAGE) ──
    static constexpr int ma_len = 8;
    float    _ppgdisp_ma_buf[ma_len];
    int      _ppgdisp_ma_idx;
    float    _ppgdisp_ma_sum;

    // ── HR1 moving average state (independent of PPG display filter) ──
    static constexpr int hr1_ma_max_len = 64;  // supports up to 640 Hz @ 5 Hz cutoff
    float    _hr1_ma_buf[hr1_ma_max_len];
    uint32_t _hr1_ma_len;   // computed from sample_rate in _recalc_rate_params()
    int      _hr1_ma_idx;
    float    _hr1_ma_sum;

    // ── Rate-dependent algorithm parameters (derived from _afe_sample_rate_hz) ──
    uint32_t          _spo2_warmup_samples;
    uint32_t          _hr1_refractory_samples;
    float             _spo2_dc_iir_alpha;
    float             _spo2_ac_ema_beta;
    float             _hr1_dc_alpha;

    // ── SpO2 state ──
    float    _spo2_dc_ir;
    float    _spo2_dc_red;
    float    _spo2_ac2_ir;
    float    _spo2_ac2_red;
    uint32_t _spo2_sample_count;
    float    _spo2_a;
    float    _spo2_b;

    // ── HR1 state ──
    float    _hr1_dc;
    float    _hr1_running_max;
    bool     _hr1_ppg_above_thresh;
    uint32_t _hr1_last_peak_idx;
    uint32_t _hr1_sample_idx;
    int32_t  _hr1_intervals[5];
    uint8_t  _hr1_interval_count;

    // ── HR2 — autocorrelation-based HR algorithm ──────────────────────────────
    // Bandpass-filters led1_aled1 (0.5–5 Hz), decimates by hr2_decim_factor,
    // accumulates a circular buffer of hr2_buf_len samples, then periodically
    // computes normalised autocorrelation to find the fundamental RR period.
    static constexpr int hr2_buf_len         = 400;  // 8 s at 50 Hz (fs/hr2_decim_factor)
    static constexpr int hr2_acorr_max_lag   = 137;  // guard band lower bound 22 BPM at 50 Hz: 50*60/22 = 137 samples
    static constexpr int hr2_decim_factor    = 10;   // 500 Hz → 50 Hz

    BiquadFilter _hr2_bpf;                   // bandpass filter (default 0.5–5 Hz)
    float    _hr2_buf[hr2_buf_len];          // circular buffer of decimated filtered samples
    float    _hr2_seg[hr2_buf_len];          // linearized copy for autocorrelation (avoids stack pressure)
    int      _hr2_buf_idx;                   // next write position in _hr2_buf
    uint32_t _hr2_buf_count;                 // samples written (capped at hr2_buf_len)
    uint32_t _hr2_decim_counter;             // decimation phase counter
    uint32_t _hr2_update_counter;            // decimated samples since last autocorr computation

    // ── HR3 — FFT + Harmonic Product Spectrum HR algorithm ───────────────────
    // Low-pass-filters led1_aled1 (10 Hz anti-aliasing), decimates by hr3_decim_factor,
    // accumulates 512 samples, then every hr3_update_interval decimated samples applies
    // a Hann window, computes the real FFT, and finds the dominant peak via the
    // Harmonic Product Spectrum (fundamental × 2nd harmonic × 3rd harmonic).
    static constexpr int hr3_buf_len         = 512;  // 10.24 s at 50 Hz → freq resolution 0.098 Hz ≈ 5.9 BPM/bin
    static constexpr int hr3_decim_factor    = 10;   // 500 Hz → 50 Hz

    BiquadFilter _hr3_bpf;                     // bandpass filter (default 0.4–15 Hz)
    float    _hr3_buf[hr3_buf_len];            // circular buffer of decimated LP-filtered samples
    float    _hr3_hann[hr3_buf_len];           // precomputed Hann window coefficients (computed once in begin())
    float    _hr3_fft[hr3_buf_len * 2];        // complex FFT buffer (interleaved re/im), also scratch for windowed input
    int      _hr3_buf_idx;                     // next write position in _hr3_buf
    uint32_t _hr3_buf_count;                   // samples written (capped at hr3_buf_len)
    uint32_t _hr3_decim_counter;               // decimation phase counter
    uint32_t _hr3_update_counter;              // decimated samples since last FFT computation

    // ── Algorithm tuning parameters (runtime-configurable) ──────────────────
    float    _spo2_warmup_s;
    float    _spo2_dc_iir_tau_s;
    float    _spo2_ac_ema_tau_s;
    float    _spo2_min;
    float    _spo2_max;
    float    _spo2_min_dc;
    float    _spo2_pi_sqi_lo;
    float    _spo2_pi_sqi_hi;
    float    _hr1_dc_tau_s;
    float    _hr1_ma_cutoff_hz;
    float    _hr1_sqi_cv_max;
    float    _hr2_min_corr;
    uint32_t _hr2_update_interval;
    uint32_t _hr3_update_interval;
    float    _hr_min_bpm;
    float    _hr_max_bpm;

    // ── Output snapshot (written by task, pushed to queue) ──
    AFE4490Data _current_data;

    // ── Static ISR trampoline ──
    // _g_instance holds a pointer to the single active INCUNEST_AFE4490 object so that
    // _drdy_isr_static (a plain C-compatible function required by attachInterrupt)
    // can forward the interrupt to the correct instance.
    //
    // LIMITATION: only one INCUNEST_AFE4490 instance is supported at a time. A second
    // instance would overwrite _g_instance and its DRDY interrupts would be routed
    // to the wrong object. To support two AFE4490 chips, either:
    //   - add a second static ISR + pointer pair, or
    //   - switch to ESP-IDF gpio_isr_handler_add(), which passes a void* argument
    //     per handler, eliminating the need for a singleton pointer altogether.
#ifndef INCUNEST_OFFLINE
    static INCUNEST_AFE4490* _g_instance;
    static void IRAM_ATTR _drdy_isr_static();
#endif

#ifdef UNIT_TEST
public:
    // Expose internals for unit testing only — not part of the public API

    // Biquad filter
    using TestBiquadFilter = BiquadFilter;
    void  test_recalc_biquad(BiquadFilter& f)           { _recalc_biquad(f); }
    float test_biquad_process(float x, BiquadFilter& f) { return _biquad_process(x, f); }

    // HR1
    void  test_feed_hr1(int32_t led1_aled1) { _update_hr1(led1_aled1); }
    float test_hr1()                        { return _current_data.hr1; }
    float test_hr1_sqi()                    { return _current_data.hr1_sqi; }

    // HR2
    void  test_feed_hr2(int32_t led1_aled1) { _update_hr2(led1_aled1); }
    float test_hr2()                        { return _current_data.hr2; }
    float test_hr2_sqi()                    { return _current_data.hr2_sqi; }

    // HR3
    void  test_feed_hr3(int32_t led1_aled1) { _update_hr3(led1_aled1); }
    float test_hr3()                        { return _current_data.hr3; }
    float test_hr3_sqi()                    { return _current_data.hr3_sqi; }

    // SpO2
    void  test_feed_spo2(int32_t ir_corr, int32_t red_corr) { _update_spo2(ir_corr, red_corr); }
    float test_spo2()                       { return _current_data.spo2; }
    float test_spo2_r()                     { return _current_data.spo2_r; }
    float test_spo2_sqi()                   { return _current_data.spo2_sqi; }
#endif
};
