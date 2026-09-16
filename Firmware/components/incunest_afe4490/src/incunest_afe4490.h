#pragma once

// incunest_afe4490 — Medical Open World AFE4490 driver + PPG algorithms (HR, SpO2)
// Library version: v0.81 — ESP32-S3, Arduino + FreeRTOS
// Spec: incunest_afe4490_spec.md
// Chip datasheet: https://www.ti.com/lit/ds/symlink/afe4490.pdf
// Author: Medical Open World — http://medicalopenworld.org — <contact@medicalopenworld.org>

#define INCUNEST_AFE4490_VERSION "0.81"

#ifdef INCUNEST_OFFLINE
  #ifndef UNIT_TEST
    #define UNIT_TEST
  #endif
  #include "incunest_afe4490_platform_stub.h"
#else
  // PARCHE INCUNEST: sin Arduino. millis/delay, GPIO, String y los
// ayudantes numericos salen de la capa de plataforma propia.
#include "platform/plat_gpio.h"
#include "platform/plat_num.h"
#include "platform/plat_string.h"
#include "platform/plat_time.h"
  #include "platform/plat_spi.h"  // PARCHE INCUNEST: SPI sobre spi_master
  #include <freertos/FreeRTOS.h>
  #include <freertos/task.h>
  #include <freertos/semphr.h>
  #include <freertos/queue.h>
  #include <stdint.h>
  // Explicit — this header uses strcmp() (afeStrToRF/CF/RG) and expf() (EmaChannel::init())
  // directly. Real Arduino.h happens to pull these in transitively, but that must not be
  // relied upon: the plain UNIT_TEST/native path (pio test -e native, distinct from
  // INCUNEST_OFFLINE) uses a minimal Arduino.h stub that does not.
  #include <cstring>
  #include <cmath>
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

#ifndef INCUNEST_AFE4490_DIAG_TASK_STACK
#define INCUNEST_AFE4490_DIAG_TASK_STACK  2048
#endif

#ifndef INCUNEST_AFE4490_DIAG_TASK_PRIORITY
#define INCUNEST_AFE4490_DIAG_TASK_PRIORITY  (INCUNEST_AFE4490_TASK_PRIORITY - 2)
#endif

#ifndef INCUNEST_TIMING_STATS
#define INCUNEST_TIMING_STATS 0
#endif

// ── Probe state ───────────────────────────────────────────────────────────────
// Appended at the end (values 0-2 unchanged) to keep the $M2/$M3/$M4 wire encoding
// ((int)probe_state) stable for existing consumers/CSVs.
enum class ProbeState {
    PROBE_DISCONNECTED,  // cable or probe connector not plugged in
    PROBE_NOT_APPLIED,   // probe connected, OT indicates no tissue in the optical path
    PROBE_APPLIED,       // probe on patient, normal measurement
    PROBE_SATURATING     // Saturation from ABOVE where the AMBIENT (ALED) phase also clips → the
                         // saturation is driven by EXTERNAL light (lamp/phototherapy), not by the
                         // LED's own light reaching the PD (that case — LED phase clips, ambient
                         // clean — is finger-removed / probe-in-air → NOT_APPLIED; see v0.61
                         // classifier, §5.6.2). Presence is still UNKNOWN (external light saturates
                         // with or without a patient). SpO2/HR1/HR2/HR3 and RSQI treat it like any
                         // non-APPLIED state (invalid/reset); HGAC treats it like PROBE_APPLIED
                         // (acts to clear the saturation — the ambient alarm may then fire).
};

// DiagCode bitmask flags — returned in AFE4490Data::diag_code (uint32_t, 32 bits)
// Multiple bits may be set simultaneously.
//
// Bits 0–12: AFE4490 hardware DIAG register (REG_DIAG 0x30) — set by runAfeDiagnostics()
constexpr uint32_t AFE_DIAG_PD_ALM          = 0x00000001UL;  // bit  0 — photodiode out of range
constexpr uint32_t AFE_DIAG_LED_ALM         = 0x00000002UL;  // bit  1 — LED out of range
constexpr uint32_t AFE_DIAG_OUT             = 0x00000004UL;  // bit  2 — general diagnostic fault
constexpr uint32_t AFE_DIAG_LED2_ALM        = 0x00000008UL;  // bit  3 — LED2 driver fault
constexpr uint32_t AFE_DIAG_LED3_ALM        = 0x00000010UL;  // bit  4 — LED3 driver fault (if used)
constexpr uint32_t AFE_DIAG_LED1_ALM        = 0x00000020UL;  // bit  5 — LED1 driver fault
constexpr uint32_t AFE_DIAG_PDOC_ALM        = 0x00000040UL;  // bit  6 — photodiode open-circuit
constexpr uint32_t AFE_DIAG_PDSC_ALM        = 0x00000080UL;  // bit  7 — photodiode short-circuit
constexpr uint32_t AFE_DIAG_LED2OC_ALM      = 0x00000100UL;  // bit  8 — LED2 open-circuit
constexpr uint32_t AFE_DIAG_LED2SC_ALM      = 0x00000200UL;  // bit  9 — LED2 short-circuit
constexpr uint32_t AFE_DIAG_LED1OC_ALM      = 0x00000400UL;  // bit 10 — LED1 open-circuit
constexpr uint32_t AFE_DIAG_LED1SC_ALM      = 0x00000800UL;  // bit 11 — LED1 short-circuit
constexpr uint32_t AFE_DIAG_COMMON_MODE_ALM = 0x00001000UL;  // bit 12 — common-mode voltage out of range
constexpr uint32_t AFE_DIAG_MASK            = 0x00001FFFUL;  // mask for all 13 AFE DIAG bits
//
// Bits 13+: RSQM/HGAC software checks — set continuously at 500 Hz.
// AMB_SAT (bit 13, RSQM): ambient channel at the ADC positive rail (per-sample, hard). No
//   programmatic consumer beyond the aggregate rsqi gate and human inspection.
// AMBIENT_HIGH (bit 14, HGAC): produced by HGAC when RF is at the floor AND the ambient EMA is
//   still above the guard — i.e. ambient light alone saturates, so lowering RF/ILED can't help
//   → "TOO MUCH AMBIENT LIGHT" (actionable operator alarm). Preventive (EMA @ HIGH2), distinct
//   from AMB_SAT's per-sample hard rail. (Bit 14 previously held SIGNAL_WEAK, removed v0.56.)
constexpr uint32_t RSQM_DIAG_AMB_SAT      = 0x00002000UL;  // bit 13 — ambient channel near positive rail
constexpr uint32_t RSQM_DIAG_AMBIENT_HIGH = 0x00004000UL;  // bit 14 — ambient saturates at RF floor (HGAC alarm)
constexpr uint32_t RSQM_DIAG_SWITCHED_RC_SETTLING  = 0x00008000UL;  // bit 15 — hardware settling
// after ANY signal-chain change (RF, ILED, stage2 gain/RG, AMBDAC — not CF, which only shapes transient response, not the DC level). Named after the
// datasheet's own term for the affected circuitry (§7.7 t5 footnote (1): "four switched RC
// filters") — not the TIA's own RC pole (τ=RF·CF, settles within one sample) nor the LED/cable
// transient (_compute_led_on_to_sample_margin_counts()) — this one needs several PRP cycles
// (v0.65: derived from the datasheet's 3 ms minimum, §5.8.4), unlike those two.
// v0.72: enforced, not just labelled — _task_body() freezes inputs (last-valid raw values) for
// the whole window this bit is set, same mechanism as the diagnostic holdoff (§5.6.3).

// ── Public data struct ────────────────────────────────────────────────────────
struct AFE4490Data {
    // Field order mirrors the $M1/$P1 serial frame: raw signals first, then processed outputs
    // Raw ADC outputs (6 signals from AFE4490)
    int32_t led2;        // LED2VAL  — RED raw          (frame: LED2)
    int32_t led1;        // LED1VAL  — IR raw           (frame: LED1)
    int32_t aled2;       // ALED2VAL — ambient after LED2 (frame: ALED2)
    int32_t aled1;       // ALED1VAL — ambient after LED1 (frame: ALED1)
    int32_t led2_sub;  // LED2-ALED2 — RED ambient-corrected (frame: LED2_SUB)
    int32_t led1_sub;  // LED1-ALED1 — IR ambient-corrected  (frame: LED1_SUB)
    // Processed outputs
    float   ppg_disp;         // PPG display signal, OT domain (v0.69): BPF(ot_led1/ot_led2) + negated.
                              // A/A units, tiny magnitude (~1e-5..1e-6) — NOT normalized for display
                              // (see backlog: candidate ppg_disp_norm, PI-weighted [0..1]).
                              // Gain-invariant like the algorithms (§5.1/§5.2): no discontinuity when
                              // HGAC changes RF, unlike the raw-ADC-code led1_sub/led2_sub it replaced.
                              // Display only — not an input to algorithms.
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
    // RSQM outputs
    uint8_t    rsqi;         // Raw Signal Quality Index: 0=invalid, 1=valid
    uint32_t   diag_code;    // DiagCode bitmask: RSQM_DIAG_AMB_SAT | RSQM_DIAG_SWITCHED_RC_SETTLING | ...
    ProbeState probe_state;  // PROBE_DISCONNECTED, PROBE_NOT_APPLIED, PROBE_APPLIED, or PROBE_SATURATING
};

// ── Enumerations ──────────────────────────────────────────────────────────────
// PPG display source channel (setPPGChannel() / ppg_disp). v0.69: OT domain (see AFE4490Data::
// ppg_disp) — only IR/RED remain selectable. The four single-ended raw options (bare LED1/LED2/
// ALED1/ALED2, without ambient subtraction) had no OT equivalent (OT is structurally an
// ambient-corrected ratio, AFE4490AnalogState has no ot_aled1/ot_aled2) and were dropped rather
// than kept meaningless. Named LED1/LED2 (not IR/RED) for consistency with ot_led1/ot_led2.
enum class AFE4490Channel {
    LED1,  // ot_led1 (IR)  — default
    LED2   // ot_led2 (RED)
};

enum class AFE4490RF {
    RF_10K,
    RF_25K,
    RF_50K,
    RF_100K,
    RF_250K,
    RF_500K,  // default
    RF_1M
};

// TIA feedback capacitance — CF_LED[4:0] register code (TIAGAIN / TIA_AMB_GAIN, bits D[7:3]).
// The AFE4490 has five switchable capacitors in parallel over a fixed 5 pF base
// (datasheet Figure 114, p.36): bit0=5 pF, bit1=15 pF, bit2=25 pF, bit3=50 pF, bit4=150 pF.
//   CF(code) = 5 pF + Σ (selected weights)   → 32 distinct values, 5 pF … 250 pF.
// Datasheet example: code 01111 = 50+25+15+5 +5 = 100 pF.
// The weights are superincreasing (each > the sum of all smaller ones), so kAFE_CF_PF[] is
// strictly increasing in the code — code order == capacitance order (see static_assert below).
// Not an enum: all 32 combinations are legal hardware settings, so the code IS the value.
using AFE4490CFCode = uint8_t;              // valid range 0…31
constexpr AFE4490CFCode kAFE_CF_CODE_MAX = 31;
constexpr AFE4490CFCode kAFE_CF_CODE_DEFAULT = 0;   // 5 pF — chip reset value

enum class AFE4490RG {
    RG_100K,    // 0 dB   — ×1   — RG=100 kΩ  (Ri=100 kΩ fixed, datasheet Table 1)
    RG_150K,  // 3.5 dB — ×1.5 — RG=150 kΩ
    RG_200K,    // 6 dB   — ×2   — RG=200 kΩ
    RG_300K,  // 9.5 dB — ×3   — RG=300 kΩ
    RG_400K    // 12 dB  — ×4   — RG=400 kΩ
};

// ── Physical parameter tables (indexed by enum cast to int) ──────────────────
// Single source of truth for AFE4490 physical values.
// Do NOT duplicate these in application code — include this header and index by enum.
// Usage: float rf_ohm = kAFE_RF_OHM[(int)cfg.afe_tia_rf_led1];

// AFE4490RF → TIA feedback resistance [Ω]
constexpr float kAFE_RF_OHM[7] = {
    10e3f,   // RF_10K
    25e3f,   // RF_25K
    50e3f,   // RF_50K
   100e3f,   // RF_100K
   250e3f,   // RF_250K
   500e3f,   // RF_500K
     1e6f    // RF_1M
};

// AFE4490CFCode → TIA feedback capacitance [pF]
// Indexed by the raw CF_LED[4:0] register code: kAFE_CF_PF[code] = 5 + Σ selected weights.
// Bit weights: b0=5, b1=15, b2=25, b3=50, b4=150 pF (datasheet Figure 114).
constexpr float kAFE_CF_PF[32] = {
     5.0f,   // 00000 — 5 pF base only (chip reset default)
    10.0f,   // 00001 — 5
    20.0f,   // 00010 — 15
    25.0f,   // 00011 — 15+5
    30.0f,   // 00100 — 25
    35.0f,   // 00101 — 25+5
    45.0f,   // 00110 — 25+15
    50.0f,   // 00111 — 25+15+5
    55.0f,   // 01000 — 50
    60.0f,   // 01001 — 50+5
    70.0f,   // 01010 — 50+15
    75.0f,   // 01011 — 50+15+5
    80.0f,   // 01100 — 50+25
    85.0f,   // 01101 — 50+25+5
    95.0f,   // 01110 — 50+25+15
   100.0f,   // 01111 — 50+25+15+5   (datasheet worked example)
   155.0f,   // 10000 — 150
   160.0f,   // 10001 — 150+5
   170.0f,   // 10010 — 150+15
   175.0f,   // 10011 — 150+15+5
   180.0f,   // 10100 — 150+25
   185.0f,   // 10101 — 150+25+5
   195.0f,   // 10110 — 150+25+15
   200.0f,   // 10111 — 150+25+15+5
   205.0f,   // 11000 — 150+50
   210.0f,   // 11001 — 150+50+5
   220.0f,   // 11010 — 150+50+15
   225.0f,   // 11011 — 150+50+15+5
   230.0f,   // 11100 — 150+50+25
   235.0f,   // 11101 — 150+50+25+5
   245.0f,   // 11110 — 150+50+25+15
   250.0f    // 11111 — all caps (maximum)
};

// CF_LED[4:0] code → display string, same index as kAFE_CF_PF[] (used by $CFG / $SET).
constexpr const char* kAFE_CF_STR[32] = {
    "5p",   "10p",  "20p",  "25p",  "30p",  "35p",  "45p",  "50p",
    "55p",  "60p",  "70p",  "75p",  "80p",  "85p",  "95p",  "100p",
    "155p", "160p", "170p", "175p", "180p", "185p", "195p", "200p",
    "205p", "210p", "220p", "225p", "230p", "235p", "245p", "250p"
};

// AFE4490RG → Stage 2 feedback resistance RG [Ω]
constexpr float kAFE_RG_OHM[5] = {
   100e3f,   // RG_100K   (×1)
   150e3f,   // RG_150K (×1.5)
   200e3f,   // RG_200K   (×2)
   300e3f,   // RG_300K (×3)
   400e3f    // RG_400K  (×4)
};

// AFE4490RG → linear voltage gain [dimensionless] = RG / Ri
constexpr float kAFE_RG_GAIN[5] = {
    1.0f,    // RG_100K
    1.5f,    // RG_150K
    2.0f,    // RG_200K
    3.0f,    // RG_300K
    4.0f     // RG_400K
};

// Stage 2 input resistance Ri [Ω] — fixed by chip design (datasheet p.28)
constexpr float kAFE_RI_OHM = 100e3f;

// ── TIA CF helpers ──────────────────────────────────────────────────────────
// Invariant relied upon by afeCFPFToCode() and the auto-CF scan (§7.2): capacitance is
// strictly increasing in the code, because the bit weights are superincreasing.
// Recursive form (single return statement): the Arduino/ESP32 build compiles as C++11,
// where a constexpr function body cannot contain a loop.
constexpr bool afeCFTableIsMonotonic(int i = 1) {
    return (i > (int)kAFE_CF_CODE_MAX)
             ? true
             : (kAFE_CF_PF[i] > kAFE_CF_PF[i - 1] && afeCFTableIsMonotonic(i + 1));
}
static_assert(afeCFTableIsMonotonic(), "kAFE_CF_PF must be strictly increasing in the CF code");

// CF register code → capacitance [pF].
inline float afeCFCodeToPF(AFE4490CFCode code) {
    return kAFE_CF_PF[code & kAFE_CF_CODE_MAX];
}

// Relative tolerance used when matching a requested capacitance against the grid.
// Rationale: the auto-CF budget is computed as (settle_s / 5 / RF) × 1e12, which in float lands
// a few ULP BELOW the exact grid value — at 500 Hz / RF_100K it yields 99.9999924 instead of
// 100.0, which without this guard would drop a whole step (95 pF instead of 100 pF) and silently
// negate the gain. 0.01% is orders of magnitude below the on-chip capacitor tolerance, so
// accepting it can never cause a real settling violation.
constexpr float kAFE_CF_MATCH_TOL = 1e-4f;

// Capacitance [pF] → CF register code, quantised DOWN: returns the largest achievable CF ≤ pF
// (within kAFE_CF_MATCH_TOL). Never rounds up — overshooting CF would stretch the TIA time
// constant past the settling budget reserved before the ADC sample window (5τ ≤ settle_time, §7.2).
// Values below 5 pF clamp to code 0 (the 5 pF base cannot be switched out).
inline AFE4490CFCode afeCFPFToCode(float pF) {
    const float limit = pF * (1.0f + kAFE_CF_MATCH_TOL);
    for (int i = (int)kAFE_CF_CODE_MAX; i > 0; --i)
        if (kAFE_CF_PF[i] <= limit) return (AFE4490CFCode)i;
    return 0;
}

// ── ADC domain — SINGLE SOURCE OF TRUTH ─────────────────────────────────────
// Signed 22-bit twos-complement converter (datasheet p.11: ±1.2 V FS; p.43 §8.4.1).
// Code range [−2^21, 2^21−1] ↔ ±FSR. Two natures coexist, kept side by side:
//   NOMINAL — datasheet / theoretical (Table 7, p.88).
//   ACTUAL  — empirically MEASURED on IncuNest 16.A (AMBDAC sweep 2026-07-10);
//             the silicon rails sit ~230 codes short of nominal full scale.
// 1 LSB = FSR / 2^21 (bipolar twos-complement transfer function): Table 7 rows code=±1 ↔
// ±1.2/2^21 V and the negative rail −1.2 V ↔ −2^21 (exact). The positive full-scale code
// +2^21−1 is +1.2 V − 1 LSB, not +1.2 V — Table 7's "+1.2 V ↔ 2^21−1" is a nominal label,
// the one row inconsistent with the LSB, so it does NOT anchor SCALE.
namespace adc {
    // Voltage scale (datasheet)
    constexpr float   FSR     = 1.2f;            // V — full-scale range (±1.2 V differential)
    constexpr float   FS_CODE = 2097152.0f;      // 2^21 — full-scale divisor (twos-complement rail = −2^21)
    constexpr float   SCALE   = FSR / FS_CODE;   // V per code (1 LSB): V_ADC = code × SCALE

    // Code rails — NOMINAL (datasheet twos-complement limits)
    constexpr int32_t POS_RAIL_NOMINAL =  2097151;   // +2^21−1 (max positive code)
    constexpr int32_t NEG_RAIL_NOMINAL = -2097152;   // −2^21   (min negative code)

    // Code rails — ACTUAL (measured on 16.A, AMBDAC sweep 2026-07-10; NOT datasheet).
    // POS constant on all 4 channels; NEG constant on LED1/2, but ALED1/2 publish averaged
    // codes that oscillate down to ~−2096810 without pinning (ADC averaging mixes clipped and
    // non-clipped conversions) → NEG is approximate, absorbed by the guard band.
    constexpr int32_t POS_RAIL_ACTUAL  =  2096921;
    constexpr int32_t NEG_RAIL_ACTUAL  = -2096919;

    // Operative saturation thresholds — guard band inside the ACTUAL rail. Guard ≈ 2× worst
    // observed ALED-to-rail distance (~109 codes), sign-independent. False positives are
    // impossible in practice: a legit code within SAT_GUARD of the rail (0.01% of range) is
    // functionally saturated anyway.
    constexpr int32_t SAT_GUARD = 221;                          // codes
    constexpr int32_t SAT_POS   = POS_RAIL_ACTUAL - SAT_GUARD;  // 2096700
    constexpr int32_t SAT_NEG   = -SAT_POS;                     // −2096700 (symmetric by choice; NEG_RAIL_ACTUAL is doc-only)

    static_assert(POS_RAIL_ACTUAL < POS_RAIL_NOMINAL &&
                  NEG_RAIL_ACTUAL > NEG_RAIL_NOMINAL &&
                  SAT_POS < POS_RAIL_ACTUAL && SAT_NEG > NEG_RAIL_ACTUAL,
                  "ADC actual rails inside nominal FS; guarded sat inside the actual rail");
}

// Low-pass filter corner frequency selection (FLTRCNRSEL bit D15 of TIA_AMB_GAIN, datasheet p.75).
// 0 = 500 Hz (reset default); 1 = 1000 Hz.
// Written unconditionally to TIA_AMB_GAIN in _build_tia_amb_gain_led2().
constexpr uint8_t  kAFE_FLTRCNRSEL      = 0;
// Diagnostic holdoff: filter settling time after diagnostics mode (datasheet p.11).
// 500 Hz corner → 28 ms; 1000 Hz corner → 16 ms.
constexpr uint32_t kAFE_DIAG_HOLDOFF_MS = kAFE_FLTRCNRSEL ? 16U : 28U;

/// Physical quantities derived from ADC codes for one sample — purely the signal chain.
/// Computed in _process_sample() before RSQM and HGAC; not stored in AFE4490Data.
/// Datasheet Eq.2 (p.30):  V_DIFF = 2 × (I_PD × RF/Ri − I_CANCEL) × RG
/// Inversions:
///   V_ADC = code × adc::SCALE       [V]  (= code / adc::FS_CODE × adc::FSR)
///   V_TIA = 2 × (V_ADC / (2 × RG_ohm) + I_CANCEL_A) × Ri [V]  (= 2 × I_PD × RF)
///   I_PD  = V_TIA / (2 × RF_ohm)                         [A]
/// V_TIA is always the DIFFERENTIAL TIA output (datasheet §9.2.2 + Fig. 135: "Ideal Operating
/// Point" = 0.6 V, "TIA max (Differential)" = 1.0 V — the datasheet has no symbol named "V_OD";
/// verified against the rendered PDF 2026-07-21). The code has no single-ended/branch value
/// (each branch would be V_TIA / 2, TI linear FS 0.5 V, measured hard clip ~0.97 V/branch), so
/// the name carries no suffix — there is nothing to disambiguate it from.
/// ALED channels use the same RF and RG as the corresponding LED channel.
/// When STAGE2EN=0: RG_ohm = kAFE_RI_OHM (unity gain).

/// Channel identifier for AFE4490AnalogState per-channel queries.
/// The value is the bit index used in the validity masks (bit = 1 << (uint8_t)ch).
enum class AFE4490Ch : uint8_t { LED1 = 0, ALED1 = 1, LED2 = 2, ALED2 = 3 };

/// Per-channel measurement state, derived from the validity masks (see chState()).
/// Precedence: CH_CLIPPED_RANGE > CH_EXTENDED_RANGE > CH_VALID_RANGE.
enum class AFE4490ChState : uint8_t {
    // Naming: the three states are an increasing-range scale of v_tia, all suffixed _RANGE.
    // "VALID" here means TI's GUARANTEED-linear range (±1.0 V), NOT that EXTENDED is invalid —
    // an EXTENDED_RANGE value is still usable (empirically linear, "most likely correct").
    CH_VALID_RANGE    = 0,  // trustworthy measurement: inside TI's guaranteed-linear range (v_tia ≤ 1.0 V)
    CH_EXTENDED_RANGE = 1,  // v_tia in (tia_axis::FS_V, tia_axis::LIN_V] = (1.0, 1.8] V: TIA past its
                            // ±1.0 V TI guarantee but still empirically linear (sweep 2026-07-08) —
                            // value most likely correct. NOTE: (1.0, 1.8] is a TIA property; what's
                            // *observable* is gated by the ADC (±1.2 V FS). With default gain
                            // (RG=Ri → V_ADC=V_TIA) and AMBDAC=0 the ADC saturates first, so
                            // chState() returns CH_CLIPPED_RANGE (precedence) above ~1.2 V and this
                            // state only occurs in (1.0, 1.2] V. Reaching the (1.2, 1.8] tail needs
                            // AMBDAC to shift the ADC window up (see AMBDAC off-spec-zone task, v2).
    CH_CLIPPED_RANGE  = 2,  // ADC saturated or TIA beyond empirical linearity: the stored value is
                            // a BOUND, not a measurement (sat pos: real ≥ stored; sat neg: real ≤ stored)
};

/// ── v_tia operating axis — SINGLE SOURCE OF TRUTH ───────────────────────
/// Every threshold on the differential TIA output, in ascending order. Edit the values HERE
/// only; the strict ordering below is an enforced compile-time invariant. Two natures coexist:
/// PHYSICS (fixed: FS/ADC/LIN — datasheet/silicon/board) and POLICY (HGAC default HIGH2,
/// runtime-tunable). NOTE: the ADC full-scale (adc::FSR, mapped onto this axis at unity gain) is
/// NOMINAL; the operative saturation flag is adc::SAT_POS, an EMPIRICAL rail in adc_code, NOT this axis.
namespace tia_axis {
    constexpr float HIGH2_V = 0.9f;  // HGAC guard: reduce RF urgently above this  — POLICY default, runtime-tunable
    constexpr float FS_V    = 1.0f;  // TI full-scale guarantee                   — PHYSICS; above → CH_EXTENDED_RANGE
    constexpr float LIN_V   = 1.8f;  // empirical TIA linearity limit             — PHYSICS; above → CH_CLIPPED_RANGE
    // ADC saturation (adc::FSR = 1.2 V) is an ADC property, not a v_tia threshold; at unity gain
    // (V_ADC = V_TIA) it falls on this axis between FS_V and LIN_V, hence it enters the ordering.
    static_assert(HIGH2_V < FS_V && FS_V < adc::FSR && adc::FSR < LIN_V,
                  "v_tia axis strictly ascending (adc::FSR mapped at unity gain)");
}

/// Invalidity policy: VALUE-ALWAYS. Every field is always computed from the raw codes,
/// even when part of the chain is saturated — the validity masks (one bit per channel,
/// bit index = AFE4490Ch) tell the consumer whether each value is a measurement or a bound.
/// Consumers must query through the helpers (chState/chValidRange/otLedxValid/...), never the
/// masks directly, so the packing (per-stage masks) can change without touching consumers.
/// The thresholds below are PHYSICS (datasheet / silicon / per-board characterization),
/// not policy — HGAC actuation thresholds (hgac_v_tia_high2/high1/low1) live in the controller.
struct AFE4490AnalogState {
    // ── Detection thresholds — live in namespaces, not here ──────────────────
    // v_tia thresholds live in tia_axis; ADC saturation rails (SAT_POS/SAT_NEG) live in
    // namespace adc — both single sources of truth, no aliases in this struct. The masks
    // below are computed against tia_axis:: and adc:: directly.

    // ── Values (always computed — see validity masks below) ─────────────────
    float v_adc_led1,      v_adc_led2,      v_adc_aled1,      v_adc_aled2;       // V
    float v_tia_led1, v_tia_led2, v_tia_aled1, v_tia_aled2;  // V  (= 2 × I_PD × RF)
    float i_pd_led1,       i_pd_led2,       i_pd_aled1,       i_pd_aled2;        // A
    float ot_led1,         ot_led2;  // A/A — optical transmittance: (i_pd_led − i_pd_aled) / ILEDx_A.
                                     // Full electro-optical system ratio (LED efficiency × tissue ×
                                     // PD responsivity). Valid iff both source channels chValidRange().

    // ── Validity masks — one uint8_t per failure mode, packing 4 bits (upper 4 unused) ──
    // Each mask has one bit per physical channel, same bit position as AFE4490Ch:
    // LED1 = bit 0, ALED1 = bit 1, LED2 = bit 2, ALED2 = bit 3 (1 << (uint8_t)AFE4490Ch).
    // A set bit means that channel currently fails this specific check; use the
    // adcSatPos()/adcSatNeg()/tiaOverFs()/tiaOverLin() accessors below rather than
    // testing these masks directly (see the "mandatory access path" note further down).
    uint8_t adc_sat_pos  = 0;  // ADC code ≥ adc::SAT_POS
    uint8_t adc_sat_neg  = 0;  // ADC code ≤ adc::SAT_NEG
    uint8_t tia_over_fs  = 0;  // v_tia > tia_axis::FS_V (photocurrent unipolar → no lower bound)
    uint8_t tia_over_lin = 0;  // v_tia > tia_axis::LIN_V

    // ── Helpers (mandatory access path — packing may change) ────────────────
    bool adcSatPos(AFE4490Ch ch)  const { return (adc_sat_pos  & _bit(ch)) != 0; }
    bool adcSatNeg(AFE4490Ch ch)  const { return (adc_sat_neg  & _bit(ch)) != 0; }
    bool adcSat(AFE4490Ch ch)     const { return adcSatPos(ch) || adcSatNeg(ch); }
    bool tiaOverFs(AFE4490Ch ch)  const { return (tia_over_fs  & _bit(ch)) != 0; }
    bool tiaOverLin(AFE4490Ch ch) const { return (tia_over_lin & _bit(ch)) != 0; }

    AFE4490ChState chState(AFE4490Ch ch) const {
        if (adcSat(ch) || tiaOverLin(ch)) return AFE4490ChState::CH_CLIPPED_RANGE;
        if (tiaOverFs(ch))                return AFE4490ChState::CH_EXTENDED_RANGE;
        return AFE4490ChState::CH_VALID_RANGE;
    }
    bool chValidRange(AFE4490Ch ch) const { return chState(ch) == AFE4490ChState::CH_VALID_RANGE; }

    bool anyAdcSat()    const { return (adc_sat_pos | adc_sat_neg) != 0; }              // HGAC O1
    bool anyTiaOverFs() const { return tia_over_fs != 0; }                              // HGAC O2
    // Saturation from ABOVE (the only kind the probe produces — photocurrent is unipolar): ADC at
    // the positive rail OR v_tia past FS. Excludes adc_sat_neg on purpose: the ADC can only
    // rail negative via AMBDAC (unused in v1), and that wouldn't be cleared by reducing RF/ILED.
    // (tia_over_lin is subsumed by tia_over_fs — FS_V < LIN_V per the tia_axis static_assert.)
    bool anyPositiveSaturation() const { return (adc_sat_pos | tia_over_fs) != 0; }

private:
    static uint8_t _bit(AFE4490Ch ch) { return (uint8_t)(1u << (uint8_t)ch); }
};

/// Debug/test snapshot — aggregates internal signals not published via AFE4490Data.
/// Not in the production data path (not in queue). Intended for test tools (e.g. PulseNest).
/// Retrieved via getData(data, dbg); updated every sample under _state_mutex.
struct AFE4490DebugData {
    AFE4490AnalogState analog;  // physical reconstruction: V_ADC, V_TIA, I_PD for all 4 channels
    // Current RF in use per domain — whatever set it (manual $SET or HGAC Phase 1 descent).
    // Named without an "hgac_" prefix on purpose (v0.81): it's just the live config value,
    // not something HGAC computes or owns; HGAC is only the usual (not the only) mutator.
    AFE4490RF rf_led1;         // current RF for LED1 (IR) domain
    AFE4490RF rf_led2;         // current RF for LED2 (RED) domain
};

// ── String codecs for enum ↔ $CFG/$SET protocol labels ───────────────────────
// These functions convert between the enum values and the abbreviated string labels
// used in the $CFG serial frame and $SET command protocol.
// String labels: RF → "10K"/"25K"/"50K"/"100K"/"250K"/"500K"/"1M"
//                CF → "5p"/"10p"/"20p"/"30p"/"55p"/"155p"
//                STG2 → "0dB"/"3.5dB"/"6dB"/"9.5dB"/"12dB"

inline const char* afeRFToStr(AFE4490RF g) {
    switch (g) {
        case AFE4490RF::RF_10K:  return "10K";
        case AFE4490RF::RF_25K:  return "25K";
        case AFE4490RF::RF_50K:  return "50K";
        case AFE4490RF::RF_100K: return "100K";
        case AFE4490RF::RF_250K: return "250K";
        case AFE4490RF::RF_500K: return "500K";
        case AFE4490RF::RF_1M:   return "1M";
        default:                      return "?";
    }
}

inline bool afeStrToRF(const char* s, AFE4490RF& out) {
    if      (strcmp(s, "10K")  == 0) { out = AFE4490RF::RF_10K;  return true; }
    else if (strcmp(s, "25K")  == 0) { out = AFE4490RF::RF_25K;  return true; }
    else if (strcmp(s, "50K")  == 0) { out = AFE4490RF::RF_50K;  return true; }
    else if (strcmp(s, "100K") == 0) { out = AFE4490RF::RF_100K; return true; }
    else if (strcmp(s, "250K") == 0) { out = AFE4490RF::RF_250K; return true; }
    else if (strcmp(s, "500K") == 0) { out = AFE4490RF::RF_500K; return true; }
    else if (strcmp(s, "1M")   == 0) { out = AFE4490RF::RF_1M;   return true; }
    return false;
}

inline const char* afeCFToStr(AFE4490CFCode code) {
    return (code <= kAFE_CF_CODE_MAX) ? kAFE_CF_STR[code] : "?";
}

// Accepts any of the 32 achievable values ("5p" … "250p"); the six strings used before v0.62
// ("5p","10p","20p","30p","55p","155p") remain valid and map to the same hardware setting.
inline bool afeStrToCF(const char* s, AFE4490CFCode& out) {
    for (int i = 0; i <= (int)kAFE_CF_CODE_MAX; ++i) {
        if (strcmp(s, kAFE_CF_STR[i]) == 0) { out = (AFE4490CFCode)i; return true; }
    }
    return false;
}

inline const char* afeRGToStr(AFE4490RG g) {
    switch (g) {
        case AFE4490RG::RG_100K:   return "0dB";
        case AFE4490RG::RG_150K: return "3.5dB";
        case AFE4490RG::RG_200K:   return "6dB";
        case AFE4490RG::RG_300K: return "9.5dB";
        case AFE4490RG::RG_400K:  return "12dB";
        default:                            return "?";
    }
}

inline bool afeStrToRG(const char* s, AFE4490RG& out) {
    if      (strcmp(s, "0dB")   == 0) { out = AFE4490RG::RG_100K;   return true; }
    else if (strcmp(s, "3.5dB") == 0) { out = AFE4490RG::RG_150K; return true; }
    else if (strcmp(s, "6dB")   == 0) { out = AFE4490RG::RG_200K;   return true; }
    else if (strcmp(s, "9.5dB") == 0) { out = AFE4490RG::RG_300K; return true; }
    else if (strcmp(s, "12dB")  == 0) { out = AFE4490RG::RG_400K;  return true; }
    return false;
}

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
    // When afe_sep_tia_en=false: hardware uses LED2 fields for both channels (TIAGAIN LED1 fields ignored by chip)
    // When afe_sep_tia_en=true:  TIAGAIN register controls LED1 (IR); TIA_AMB_GAIN controls LED2 (RED)
    bool              afe_sep_tia_en;         // ENSEPGAIN bit D15 in TIAGAIN register
    AFE4490RF    afe_tia_rf_led1;     // RF for LED1 (IR)  — active when afe_sep_tia_en=true
    AFE4490CFCode     afe_tia_cf_led1_code;  // CF_LED1[4:0] register code (0–31) — active when afe_sep_tia_en=true
    float             afe_tia_cf_led1_pF;    // same setting in pF (= kAFE_CF_PF[afe_tia_cf_led1_code])
    AFE4490RG afe_stg2_rg_led1;  // Stage 2 STG2GAIN1 for LED1 (IR) — gain only, no enable
    bool              afe_stg2_en_led1;        // STAGE2EN1 (D14 of TIAGAIN) — explicit Stage 2 enable for LED1
    AFE4490RF    afe_tia_rf_led2;     // RF for LED2 (RED) — always active (both channels when ENSEPGAIN=0)
    AFE4490CFCode     afe_tia_cf_led2_code;  // CF_LED2[4:0] register code (0–31) — always active
    float             afe_tia_cf_led2_pF;    // same setting in pF (= kAFE_CF_PF[afe_tia_cf_led2_code])
    AFE4490RG afe_stg2_rg_led2;  // Stage 2 STG2GAIN2 for LED2 (RED) — gain only, no enable
    bool              afe_stg2_en_led2;        // STAGE2EN2 (D14 of TIA_AMB_GAIN) — explicit Stage 2 enable for LED2
    uint8_t           afe_ambdac_uA;         // Ambient cancellation DAC [0–10 µA]; AMBDAC[3:0] in TIA_AMB_GAIN D19:D16
    AFE4490Channel    ppgdisp_channel;       // PPG display channel selection
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
    float             spo2_ema_mean_tau_s;         // EMA mean (DC) time constant [s]
    float             spo2_ema_var_tau_s;          // EMA variance (AC²) time constant [s]
    float             spo2_min;                  // valid output lower bound [%]
    float             spo2_max;                  // valid output upper bound [%]
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
    // RSQM algorithm parameters
    float             rsqm_ot_thr;               // OT threshold: NOT_APPLIED vs APPLIED [A/A]
    float             rsqm_disconn_led_sub_thr;  // |led_sub| min for DISCONNECTED detection [adc_code]
    float             rsqm_disconn_i_pd_thr;     // |i_pd| min for DISCONNECTED detection [A]
    float             rsqm_probe_state_min_s;    // debounce time to change probe state [s]
    // HGAC parameters (RF-only, two EMA per gain domain — see incunest_afe4490_spec.md §5.8)
    bool              hgac_enable;                    // master enable; false = HGAC never actuates
    float             hgac_v_tia_high2;          // V — guard: reduce RF urgently above this (fast EMA)
    float             hgac_v_tia_high1;          // V — leveling: reduce RF above this (slow EMA)
    float             hgac_v_tia_low1;           // V — leveling: raise RF below this (slow EMA)
    float             hgac_ema_fast_tau_s;       // s — guard EMA time constant (HIGH2)
    float             hgac_ema_slow_tau_s;       // s — leveling EMA time constant (HIGH1/LOW1)
    float             hgac_ema_ambient_tau_s;    // s — ambient (ALED) EMA time constant (AMBIENT_HIGH alarm)
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
    // debug=true: queue items carry AFE4490DebugData alongside AFE4490Data — enables
    // getData(data, dbg) with true sample-level atomicity. Zero overhead when false (default).
    void begin(int pin_cs, int pin_drdy, bool debug = false);
#endif

    // Chip configuration setters (callable before or after begin())
    void setSampleRate(uint16_t hz);        // 63–5000 Hz; recalculates NUMAV_max
    void setAdcAverages(uint8_t num);       // 1=no averaging; clamped to floor(5000/PRF)
    void setLED1Current(float mA);
    void setLED2Current(float mA);
    void setLEDRange(uint8_t mA);           // 75 or 150 mA
    // Joint setters — apply the same value to both LED1 and LED2 channels; also sets ENSEPGAIN=0
    void setTIAGain(AFE4490RF gain);   // sets afe_tia_rf_led1 = afe_tia_rf_led2 = gain
    // Datasheet Equation 1 (§8.3.1.1): RF × CF ≤ Rx Sample Time / 10 — the chip's own upper
    // bound on CF. Returns that limit in pF for the given RF at the current sample rate.
    // Auto-CF is stricter and can never exceed it; a manual setTIACF*() can, so callers that
    // expose CF to a user should check against this and warn. Does not block anything.
    float getCFMaxEq1PF(AFE4490RF rf) const;

    void setTIACF(float pF);                // sets both channels; quantised DOWN to the largest CF ≤ pF
    void setStage2Gain(AFE4490RG gain); // sets afe_stg2_rg_led1 = afe_stg2_rg_led2 = gain
    // Separate gain mode — enable independent LED1/LED2 TIA settings (ENSEPGAIN bit D15 in TIAGAIN)
    void setEnSepGain(bool enable);
    // Per-channel setters — only meaningful when ENSEPGAIN=1 (setEnSepGain(true))
    void setTIAGainLED1(AFE4490RF gain);   // RF for LED1 (IR)  — writes TIAGAIN RF_LED1[2:0]
    void setTIACFLED1(float pF);            // CF for LED1 (IR)  — writes TIAGAIN CF_LED1[4:0]; quantised DOWN
    void setStage2GainLED1(AFE4490RG gain); // STG2GAIN1[2:0] for LED1 (IR) — gain only, does not touch STAGE2EN1
    void setStage2En1(bool en);             // STAGE2EN1 (D14 of TIAGAIN) — explicit Stage 2 enable for LED1
    void setTIAGainLED2(AFE4490RF gain);   // RF for LED2 (RED) — writes TIA_AMB_GAIN RF_LED2[2:0]
    void setTIACFLED2(float pF);            // CF for LED2 (RED) — writes TIA_AMB_GAIN CF_LED2[4:0]; quantised DOWN
    void setStage2GainLED2(AFE4490RG gain); // STG2GAIN2[2:0] for LED2 (RED) — gain only, does not touch STAGE2EN2
    void setStage2En2(bool en);             // STAGE2EN2 (D14 of TIA_AMB_GAIN) — explicit Stage 2 enable for LED2
    void setAmbDac(uint8_t uA);             // Ambient cancellation DAC current [0–10 µA]; AMBDAC[3:0] in TIA_AMB_GAIN D19:D16

    // Signal and filter configuration
    void setPPGChannel(AFE4490Channel channel);
    void setPPGDispFilter(float f_low_hz = 0.5f, float f_high_hz = 20.0f);

    // HR2 bandpass filter cutoffs (default 0.5–5 Hz); callable before or after begin()
    void setHR2Filter(float f_low_hz = 0.5f, float f_high_hz = 5.0f);

    // HR3 bandpass filter cutoffs (default 0.4–15 Hz); callable before or after begin()
    void setHR3Filter(float f_low_hz = 0.4f, float f_high_hz = 15.0f);

    // SpO2 algorithm setters (callable before or after begin())
    void setSpO2WarmupS(float s);                              // warmup period before reporting SpO2
    void setSpO2EmaMeanTauS(float tau_s);                      // EMA mean (DC) time constant; triggers recalc
    void setSpO2EmaVarTauS(float tau_s);                       // EMA variance (AC²) time constant; triggers recalc
    void setSpO2Range(float min_pct, float max_pct);           // valid SpO2 output range [%]
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

    // RSQM algorithm setters (callable before or after begin())
    void setRsqmOtThr(float v);                     // OT threshold: NOT_APPLIED vs APPLIED [A/A]
    void setRsqmDisconnLedSubThr(float v);          // |led_sub| min for DISCONNECTED detection [adc_code]
    void setRsqmDisconnIPdThr(float v);             // |i_pd| min for DISCONNECTED detection [A]
    void setRsqmProbeStateMinS(float s);            // debounce time to change probe state [s]; triggers recalc

    // HGAC setters (callable before or after begin()) — RF-only, two EMA per domain
    void setHgacEnable(bool en);                    // master enable; false = HGAC never actuates
    void setHgacVTiaHigh2(float v);                 // V — guard threshold (fast EMA); clamped ≤ FS_V
    void setHgacVTiaHigh1(float v);                 // V — leveling upper threshold (slow EMA)
    void setHgacVTiaLow1(float v);                  // V — leveling lower threshold (slow EMA)
    void setHgacEmaFastTauS(float s);               // s — guard EMA time constant; triggers recalc
    void setHgacEmaSlowTauS(float s);               // s — leveling EMA time constant; triggers recalc
    void setHgacEmaAmbientTauS(float s);            // s — ambient EMA time constant; triggers recalc

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
    // dbg: if non-null and begin(debug=true) was used, filled atomically with the debug
    // snapshot for the same sample. Ignored (not filled) when debug mode is inactive.
    bool getData(AFE4490Data& data, AFE4490DebugData* dbg = nullptr);
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
    // Default: kAFE_DIAG_HOLDOFF_MS — derived from kAFE_FLTRCNRSEL (datasheet p.11):
    //   500 Hz corner → 28 ms; 1000 Hz corner → 16 ms.
#ifndef INCUNEST_OFFLINE
    uint32_t runAfeDiagnostics(uint32_t diag_holdoff_ms = kAFE_DIAG_HOLDOFF_MS);
#endif

    // ISR entry point (must be public for static trampoline)
#ifndef INCUNEST_OFFLINE
    void _drdy_isr();
#endif

private:
    // ── Private types ─────────────────────────────────────────────────────────

    // 2nd-order Butterworth filter (bandpass or lowpass) using DF-II transposed form.
    // init_bp() / init_lp() compute coefficients via bilinear transform; process() runs
    // one sample; reset() clears state and schedules precharge on the next process() call.
    struct BiquadFilter {
        float f_low  = 0.0f;   // bandpass lower cutoff [Hz] (unused for LP)
        float f_high = 0.0f;   // upper cutoff / LP cutoff [Hz]

        // Design a 2nd-order Butterworth bandpass. Recalculates coefficients; does not reset state.
        void init_bp(float f_low_hz, float f_high_hz, float fs);

        // Design a 2nd-order Butterworth lowpass. Recalculates coefficients; does not reset state.
        void init_lp(float f_high_hz, float fs);

        // Clear state and schedule precharge on next process() call.
        void reset() { _v1 = 0.0f; _v2 = 0.0f; _needs_precharge = true; }

        // Process one sample (DF-II transposed). Precharges to steady-state on first call after reset().
        inline float process(float x) {
            if (_needs_precharge) {
                float denom = 1.0f + _a1 + _a2;
                float y_ss  = (denom != 0.0f) ? x * (_b0 + _b1 + _b2) / denom : 0.0f;
                _v1 = y_ss - _b0 * x;
                _v2 = _b2 * x - _a2 * y_ss;
                _needs_precharge = false;
            }
            float y = _b0 * x + _v1;
            _v1     = _b1 * x - _a1 * y + _v2;
            _v2     = _b2 * x - _a2 * y;
            return y;
        }

    private:
        float _b0 = 0.0f, _b1 = 0.0f, _b2 = 0.0f;   // DF-II transposed coefficients
        float _a1 = 0.0f, _a2 = 0.0f;
        float _v1 = 0.0f, _v2 = 0.0f;                 // filter state
        bool  _needs_precharge = true;
    };

    // EMA (Exponential Moving Average) estimator for one signal channel (mean + variance).
    // EMA low-pass: y += α·(x − y)  ≡  y = (1−α)·y + α·x   (α small = slow, α→1 = fast)
    // Configured by time constants τ (seconds): α = 1 − exp(−1/(τ·fs))
    struct EmaChannel {
        float    mean  = 0.0f;
        float    var   = 0.0f;
        uint32_t count = 0;

        // Set time constants and sample rate; computes α internally.
        // Must be called before the first update(). Safe to call again to reconfigure.
        // warmup_s (optional): samples until valid() turns true after construction/reset.
        // Default 0 → always valid (SpO2 manages its own warmup externally). HGAC passes 3·τ.
        void init(float mean_tau_s, float var_tau_s, float fs, float warmup_s = 0.0f) {
            _mean_alpha   = 1.0f - expf(-1.0f / (mean_tau_s * fs));
            _var_alpha    = 1.0f - expf(-1.0f / (var_tau_s  * fs));
            _warmup_count = (uint32_t)roundf(warmup_s * fs);
        }

        // Zero mean, var and count; keeps α and warmup unchanged. count=0 re-arms valid().
        void reset() { mean = 0.0f; var = 0.0f; count = 0; }

        // False until warmup_count samples have been seen since the last reset (see init()).
        bool valid() const { return count >= _warmup_count; }

        inline void update(float x) {
            // Seed on the first sample (after construction or reset): start mean AT x with
            // var = 0, instead of ramping mean up from 0. Ramping from 0 would leave
            // d = x − mean ≈ x for the first ~3·τ, spuriously INFLATING var (the AC-power
            // estimate → PI). Seeding removes that start-up bias. It does NOT shorten
            // the ~3·τ_var that var still needs to become representative (see spo2_warmup_s).
            if (count == 0) { mean = x; var = 0.0f; count = 1; return; }
            mean += _mean_alpha * (x - mean);
            float d = x - mean;
            var  += _var_alpha  * (d * d - var);
            count++;
        }

    private:
        float    _mean_alpha   = 0.0f;
        float    _var_alpha    = 0.0f;
        uint32_t _warmup_count = 0;   // 0 = always valid (default)
    };

    // HGAC gain domain — ENSEPGAIN=1 splits RF into two independent domains (IR=LED1+ALED1,
    // RED=LED2+ALED2; see A4/O10 in incunest_afe4490_spec.md §5.8). Distinct from AFE4490Ch
    // (per-channel validity masks, 4 values) since the RF actuator is per-color, not per-channel.
    enum class HgacColor : uint8_t { IR = 0, RED = 1 };

    // ── RSQM constants ─────────────────────────────────────────────────────────
    float             rsqm_disconn_led_sub_thr = 5000.0f;  // |led_sub| < this on both ch → disconnected; measured worst case: LED2_Sub=4013 (×1.25 margin)
    float             rsqm_disconn_i_pd_thr = 150e-9f; // |i_pd| < this on all 4 ch → disconnected; worst case 52 nA (ambdac=2 µA, RF=50K) → ×2.9 margin
                                                                 // Min i_pd LED ch in PROBE_APPLIED: 218 nA → ×1.45 below; sub criterion guards against false positives
    float             rsqm_ot_thr           = 1.0e-4f;  // valid channel, OT > this → PROBE_NOT_APPLIED; ≤ this → PROBE_APPLIED
                                                                 // (invalid/saturated channel → PROBE_SATURATING instead, checked first)
                                                                 // Widened from 8.5e-5 (2026-07-19): CONTEC MS100 simulator is highly sensitive
                                                                 // to probe placement, frequently reading OT above 8.5e-5 with probe applied.
                                                                 // Still needs empirical calibration with a real neonatal probe/patient.
    // ADC saturation thresholds live in namespace adc (SAT_POS/SAT_NEG).
    float             rsqm_probe_state_min_s = 0.2f;    // debounce time to change probe state [s] (= 100 samples @ 500 Hz)
    static constexpr uint32_t rsqm_diag_period_ms        = 5000; // periodic runAfeDiagnostics() interval in DISCONNECTED/NOT_APPLIED

    // ── HGAC constants (RF-only, two EMA per gain domain) ────────────────────
    // Thresholds on the fast/slow EMA of v_tia_led (per color). high2 sourced from tia_axis
    // (the guard, below full-scale); high1/low1 are pure policy (leveling dead-band). See §5.8.
    bool              hgac_enable          = false; // OFF by default until bench-validated
    float             hgac_v_tia_high2     = tia_axis::HIGH2_V; // V — guard (fast EMA); clamped ≤ FS_V by setter
    float             hgac_v_tia_high1     = 0.75f;             // V — leveling upper (slow EMA)
    float             hgac_v_tia_low1      = 0.20f;             // V — leveling lower (slow EMA)
    float             hgac_ema_fast_tau_s  = 0.1f;              // s — guard EMA: integrates severity×time
    float             hgac_ema_slow_tau_s  = 2.0f;              // s — leveling EMA: clean DC (f_c << HR_min)
    float             hgac_ema_ambient_tau_s = 2.0f;            // s — ambient (ALED) EMA: sustained, avoids flicker
    // Settling window after an RF change (arms RSQM_DIAG_SWITCHED_RC_SETTLING) is no longer a fixed
    // guess — see _compute_switched_rc_settling_samples() below, derived from datasheet §7.7 t5.

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
    // Dead time between LED turn-on and the opening of the Rx sample window, in AFECLK counts:
    // max(tia_settle_min, 10% of LED-on window). Datasheet purpose is LED/cable settling
    // (§8.3.1.3); the library also reuses it as the auto-CF budget, a separate and stricter
    // constraint than Eq. 1 — see spec §7.2. Named without asserting "TIA" or "LED/cable" alone
    // since it serves both roles.
    uint32_t _compute_led_on_to_sample_margin_counts() const;
    // Shared clamp for a phase window: quarter-period − 2 (ADC reset) − the given margin.
    uint32_t _window_counts(uint32_t margin) const;
    // LED-phase Rx sample window in AFECLK counts (LEDxENDC − LEDxSTC) = q − 2 − tia_margin.
    // LED1 == LED2 (symmetric). This is the datasheet's "Rx Sample Time" (Eq. 1, Fig. 58).
    uint32_t _compute_led_sample_window_counts() const;
    // Ambient-phase Rx sample window in AFECLK counts (ALEDxENDC − ALEDxSTC) = q − 2 − afe_ambient_margin_counts.
    // ALED1 == ALED2 (symmetric). NOT part of datasheet Eq. 1 — library-only margin, see §7.2.
    uint32_t _compute_ambient_sample_window_counts() const;
    // Samples (PRP cycles) to hold RSQM_DIAG_SWITCHED_RC_SETTLING after ANY signal-chain change
    // (RF, ILED, stage2 gain/RG, AMBDAC — not CF, which only shapes transient response, not the DC
    // level) — datasheet §7.7 t5 (p.17) + footnote (1): "> 3 ms of cumulative sampling time in
    // each phase", explicitly generalised there to "LED current setting, TIA gain, and so forth".
    // Uses the shorter of the LED/ambient sample windows (both symmetric between LED1 and LED2,
    // so one figure covers either color domain) — see spec §5.8.4/§7.2.
    uint32_t _compute_switched_rc_settling_samples() const;
    // Arms _switched_rc_settling_countdown (RSQM_DIAG_SWITCHED_RC_SETTLING) after ANY signal-chain
    // change — called from setTIAGain()/setTIAGainLED1/2(), setLED1/2Current(), setAmbDac(),
    // setStage2Gain()/setStage2GainLED1/2(), and setStage2En1/2() (a manual $SET included) as well
    // as from _hgac_change_rf(), so a manual change gets the same input-freeze protection
    // (§2.6b/§5.8.4) as an HGAC one.
    // CONTRACT (v0.80): every caller above MUST call this BEFORE mutating its own circuit-parameter
    // member (e.g. _afe_tia_rf_led1) — never after. A manual setter runs on Cmd_Task, a different
    // FreeRTOS task from _process_sample()'s; arming first guarantees that by the time the new
    // parameter value becomes visible to _process_sample(), the countdown is already non-zero, so
    // _should_freeze_input() can only ever see "old parameter, not yet armed" or "new parameter,
    // already armed" — never "new parameter, not yet armed", which is what let a fresh
    // _compute_analog_state() combine a stale raw ADC code with the already-new parameter and
    // latch the wrong value into _last_valid_analog_state for the whole settling window (found by
    // Alex, 2026-08-22, from a $SET-triggered RF change; the HGAC path was never affected, since it
    // mutates and arms sequentially within the same _process_sample() call, under _state_mutex).
    // Safe regardless of order for the countdown VALUE itself: _compute_switched_rc_settling_samples()
    // depends only on _afe_sample_rate_hz, never on the parameter being changed. See spec §5.8.4.
    void _arm_switched_rc_settling() { _switched_rc_settling_countdown = _compute_switched_rc_settling_samples(); }
    // Selects the largest CF code (of the 32) that settles within _compute_led_on_to_sample_margin_counts() for the given RF.
    // Called automatically by _recalc_rate_params() and setTIAGain*(); setTIACF*() overrides.
    void _recalc_afe_tia_cf_led1();   // updates _afe_tia_cf_led1 from _afe_tia_rf_led1
    void _recalc_afe_tia_cf_led2();   // updates _afe_tia_cf_led2 from _afe_tia_rf_led2
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

    // True while the analog front-end is known to be re-settling (diagnostics or an HGAC RF
    // change, §5.6.3/§5.8.4) — _task_body() freezes its input (feeds _last_valid_* instead of a
    // real SPI read) for as long as this holds. Not gated by INCUNEST_OFFLINE — it's pure member
    // reads, no SPI — so the condition itself is unit-testable without a running task.
    bool _should_freeze_input() const {
        return _diag_active || _diag_holdoff_samples > 0 || _switched_rc_settling_countdown > 0;
    }

    // FreeRTOS task
#ifndef INCUNEST_OFFLINE
    static void _task_trampoline(void* pv);
    static void _diag_task_trampoline(void* pv);
    void        _diag_task_body();
    void _task_body();
#endif

    // Signal processing
    void  _process_sample(int32_t led1, int32_t led2, int32_t aled1, int32_t aled2,
                          int32_t led1_sub, int32_t led2_sub);

    // Analog reconstruction — runs first in _process_sample(); provides V_ADC, V_TIA, I_PD, OT
    AFE4490AnalogState _compute_analog_state(int32_t led1, int32_t led2,
                                             int32_t aled1, int32_t aled2) const;

    // RSQM — runs after _compute_analog_state(); fills rsqi, diag_code, probe_state
    void _rsqm_update(int32_t led1, int32_t led2, int32_t aled1, int32_t aled2,
                      int32_t led1_sub, int32_t led2_sub, const AFE4490AnalogState& as);
    void _rsqm_request_probe_state(ProbeState requested); // debounce gate: commits state only after _rsqm_probe_state_min_samples consecutive requests

    // HGAC — runs after _rsqm_update(); RF-only, two EMA per gain domain (see spec §5.8).
    // Called from _process_sample(), which already holds _state_mutex — these methods take no
    // lock of their own; _hgac_change_rf() nests _spi_mutex (via the existing setTIAGainLED1/2()
    // setters) inside the caller's _state_mutex, which is safe because no code path acquires
    // _spi_mutex first and then blocks on _state_mutex.
    void _hgac_update(const AFE4490AnalogState& as);
    // One gain domain (color): fast EMA drives the HIGH2 guard, slow EMA the HIGH1/LOW1 leveling
    // dead-band. valid() gates each check (no action during warmup). Pure actuation — no diagnostic
    // output (the ambient alarm is computed separately, see _hgac_ambient_high). See spec §5.8.
    void _hgac_track(HgacColor color, EmaChannel& fast, EmaChannel& slow);
    // Ambient-light alarm condition for one domain (diagnostic, not actuation): RF at the floor AND
    // the ambient (ALED) EMA ≥ HIGH2 → ambient light alone saturates, so RF/ILED can't help. Kept
    // out of _hgac_track so actuation and diagnostic stay separate; produced by HGAC (not RSQM)
    // because it depends on the RF actuator state. See spec §5.8.
    bool _hgac_ambient_high(HgacColor color, const EmaChannel& ambient) const;
    // Atomic "change RF + reset this domain's EMAs + arm settling" primitive. CONTRACT: only ever
    // called from _hgac_update() (i.e. from inside _process_sample(), under _state_mutex) — never
    // call this directly from application code or another task. The reset() is required: a gain
    // change makes prior v_tia measurements meaningless (the EMAs re-warm at the new RF).
    void _hgac_change_rf(HgacColor color, AFE4490RF new_rf);

    // Algorithms — synchronous (used by unit tests and SpO2/HR1)
    // EXPERIMENT (OT-domain input): ot_ir/ot_red replace led1_sub/led2_sub as the
    // gain-invariant signal. probe_state is consumed (RSQM's classification), never computed
    // here — see incunest_afe4490.cpp for the reset-on-not-applied / division-safety design.
    void _spo2_update(float ot_ir, float ot_red, ProbeState probe_state);
    // EXPERIMENT (OT-domain input): ir is now as.ot_led1 (float, gain-invariant) instead of
    // led1_sub (int32_t raw ambient-corrected adc_code). HR1/HR2/HR3 are periodicity/timing-based
    // (peak timing, autocorrelation lag, FFT dominant frequency) — a uniform gain scale does
    // not change their result in steady state (unlike SpO2's amplitude ratio R), so this is a
    // pure type/domain change, no threshold recalibration needed for HR1/HR3. HR2's acorr0
    // near-zero guard was recalibrated for OT magnitudes (see hr2_ot_energy_eps).
    // probe_state (RSQM's classification, consumed only — see spec §5.1/§5.2 for the design,
    // mirrored here from SpO2 v0.41): while != PROBE_APPLIED, internal state resets every
    // sample (idempotent, no stored "previous state") and hr/hr_sqi become NaN/0.
    void _hr1_update(float ir, ProbeState probe_state);
    // HR2/HR3 synchronous entry points (unit-test only; production uses split paths below)
    void _hr2_update_for_test(float ir, ProbeState probe_state);
    void _hr3_update_for_test(float ir, ProbeState probe_state);
    void _reset_algorithms();

    // HR2 async split: fast per-sample path + linearise + compute. probe_state gate lives in
    // the fast path (shared by both the production call site and the sync test wrapper above):
    // while not applied, resets fast-path state and never triggers the slow path (Task B).
    bool _hr2_update_sample(float ir, ProbeState probe_state); // filter+decimate+buffer; returns true when interval fires
    void _hr2_linearize();                        // copy _hr2_buf → _hr2_seg (call under _state_mutex)
    void _hr2_compute();                          // autocorr on _hr2_seg → _hr2_result/_hr2_result_sqi

    // HR3 async split (same probe_state gate placement as HR2 above)
    bool _hr3_update_sample(float ir, ProbeState probe_state); // filter+decimate+buffer; returns true when interval fires
    void _hr3_linearize();                        // DC+Hann into _hr3_fft (call under _state_mutex)
    void _hr3_compute();                          // FFT+HPS on _hr3_fft → _hr3_result/_hr3_result_sqi

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
    uint32_t          _diag_code         { 0 };  // combined diag state: bits 0-12 = AFE DIAG reg, bits 13+ = RSQM flags
    volatile bool     _diag_active;          // true while runAfeDiagnostics() holds DIAG_EN; _task_body() skips reads
    volatile uint32_t _diag_holdoff_samples; // countdown after diagnostics end; _task_body() feeds frozen raw input
    // Last valid raw ADC values, saved on every normal (non-frozen) cycle. Fed back into
    // _process_sample() by _task_body() whenever the analog front-end is known to be
    // re-settling — after runAfeDiagnostics() (_diag_holdoff_samples) or after an HGAC RF
    // change (_switched_rc_settling_countdown, RSQM_DIAG_SWITCHED_RC_SETTLING) — instead of the real transient
    // values, so SpO2/HR1/HR2/HR3/ppg_disp absorb a constant input rather than an outlier.
    int32_t _last_valid_led1, _last_valid_led2;
    int32_t _last_valid_aled1, _last_valid_aled2;
    int32_t _last_valid_led1_sub, _last_valid_led2_sub;
    // Last valid FULLY-COMPUTED analog state (not just raw codes), saved by _process_sample() on
    // every normal cycle and reused as-is (never recomputed) while _should_freeze_input() is true.
    // Reviewed 2026-08-22 (Alex): recomputing from the frozen raw codes above via
    // _compute_analog_state() would combine an OLD raw ADC code with the NEW (already-changed)
    // circuit parameters (RF/RG/AMBDAC/ILED live in _afe_* members, updated by the very setter
    // that armed the freeze) — producing an artificially STEPPED v_tia/i_pd/ot, not a flat one, and
    // exciting ppg_disp's bandpass filter with a sharp pulse instead of a benign constant. Reusing
    // the whole struct sidesteps the mismatch by construction: there is no partial-freeze state
    // where some parameters are stale and others live.
    AFE4490AnalogState _last_valid_analog_state;

    // ── HR2/HR3 async computation tasks ──────────────────────────────────────
    // Task A (_task_body) runs the fast per-sample path and signals Task B/C when
    // the computation window fires. Task B/C run the slow autocorr / FFT+HPS
    // outside the real-time loop, then write results under _state_mutex.
    SemaphoreHandle_t _hr2_compute_sem;    // given by Task A, taken by Task B
    SemaphoreHandle_t _hr3_compute_sem;    // given by Task A, taken by Task C
    TaskHandle_t      _hr2_task_handle;
    TaskHandle_t      _hr3_task_handle;
    TaskHandle_t      _diag_task_handle;
    volatile bool     _hr2_computing;   // true while Task B holds _hr2_seg; prevents Task A from overwriting
    volatile bool     _hr3_computing;   // true while Task C uses _hr3_fft; prevents Task A from overwriting
    float             _hr2_result;      // written by Task B, copied to _current_data under _state_mutex
    float             _hr2_result_sqi;
    float             _hr3_result;      // written by Task C
    float             _hr3_result_sqi;

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
    // TIA gain — separate per-channel when _afe_sep_tia_en=true
    // When false: chip uses LED2 fields for both channels; LED1 fields written but ignored by hardware
    // When true:  TIAGAIN→LED1(IR), TIA_AMB_GAIN→LED2(RED)
    bool              _afe_sep_tia_en;
    AFE4490RF    _afe_tia_rf_led1;    // RF_LED1 in TIAGAIN
    AFE4490CFCode     _afe_tia_cf_led1;      // CF_LED1[4:0] code in TIAGAIN
    AFE4490RG _afe_stg2_rg_led1; // STG2GAIN1[2:0] in TIAGAIN D[10:8]
    bool              _afe_stg2_en_led1;       // STAGE2EN1 (D14 of TIAGAIN)
    AFE4490RF    _afe_tia_rf_led2;    // RF_LED2 in TIA_AMB_GAIN
    AFE4490CFCode     _afe_tia_cf_led2;      // CF_LED2[4:0] code in TIA_AMB_GAIN
    AFE4490RG _afe_stg2_rg_led2; // STG2GAIN2[2:0] in TIA_AMB_GAIN D[10:8]
    bool              _afe_stg2_en_led2;       // STAGE2EN2 (D14 of TIA_AMB_GAIN); also forced ON when _afe_ambdac_uA > 0
    uint8_t           _afe_ambdac_uA;        // AMBDAC[3:0] value (0–10), written to TIA_AMB_GAIN D19:D16

    // ── Signal processing configuration ──
    AFE4490Channel    _ppgdisp_channel;

    // ── PPG display filter (Butterworth bandpass, configurable via setPPGDispFilter()) ──
    BiquadFilter      _ppgdisp_bpf;          // default: 0.5–20 Hz

    // ── HR1 moving average state (independent of PPG display filter) ──
    static constexpr int hr1_ma_max_len = 64;  // supports up to 640 Hz @ 5 Hz cutoff
    float    _hr1_ma_buf[hr1_ma_max_len];
    uint32_t _hr1_ma_len;   // computed from sample_rate in _recalc_rate_params()
    int      _hr1_ma_idx;
    float    _hr1_ma_sum;

    // ── Rate-dependent algorithm parameters (derived from _afe_sample_rate_hz) ──
    uint32_t          _spo2_warmup_samples;
    uint32_t          _hr1_refractory_samples;
    float             _hr1_dc_alpha;

    // ── SpO2 state ──
    // DC and AC² power for IR and RED channels, tracked via EmaChannel. EXPERIMENT (OT-domain
    // input, branch experiment/ot-domain-inputs): x is now OT (as.ot_led1/ot_led2), gain-
    // invariant by construction, instead of the raw ambient-corrected adc_code (led1_sub/led2_sub)
    // used before this experiment. See incunest_afe4490_spec.md §5.1/§5.8.
    //   mean  = DC component (slow IIR low-pass of x)
    //           α_mean = 1 − exp(−1/(τ_mean·fs)),  τ_mean = spo2_ema_mean_tau_s (default 1.6 s)
    //           NOTE: future alternative — bandpass filter for DC tracking
    //   var   = AC power estimate = E[(x − mean)²] = EMA of AC²
    //           α_var  = 1 − exp(−1/(τ_var·fs)),   τ_var  = spo2_ema_var_tau_s  (default 1.0 s)
    //           NOTE: future alternative — bandpass-filtered AC² instead of DC-subtraction AC²
    //   count = sample count used directly for warmup (ir_ema/red_ema always updated
    //           together, so either's count works — no separate _spo2_sample_count needed).
    //           Naturally restarts warmup after a reset (see _spo2_update() in the .cpp).
    //   PI    = sqrt(var) / mean × 100  [%]  (Perfusion Index = RMS_AC / DC)
    EmaChannel _spo2_ch_ir_ema;
    EmaChannel _spo2_ch_red_ema;
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
    // Bandpass-filters led1_sub (0.5–5 Hz), decimates by hr2_decim_factor,
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
    // Low-pass-filters led1_sub (10 Hz anti-aliasing), decimates by hr3_decim_factor,
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
    float    _spo2_ema_mean_tau_s;
    float    _spo2_ema_var_tau_s;
    float    _spo2_min;
    float    _spo2_max;
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

    // ── RSQM state ────────────────────────────────────────────────────────────
    ProbeState     _rsqm_probe_state         { ProbeState::PROBE_DISCONNECTED }; // current probe state (output of _rsqm_request_probe_state)
    ProbeState     _rsqm_probe_state_pending { ProbeState::PROBE_DISCONNECTED }; // last requested state (debounce accumulator)
    uint32_t       _rsqm_probe_state_count   { 0 };                              // consecutive samples with same requested state
    uint32_t       _rsqm_probe_state_min_samples { 100 };                        // placeholder; recomputed as roundf(rsqm_probe_state_min_s × fs) in _recalc_rate_params() (ctor + any rate/param change)
    // Samples remaining in the HW_SETTLING window (0 = inactive). volatile: armed by
    // _arm_switched_rc_settling(), callable both from _task_body()'s own thread (via _hgac_change_rf()) and
    // from ANY other task calling setTIAGain*() directly (e.g. Cmd_Task on a manual $SET) — and
    // read/decremented in _process_sample() (_task_body()'s thread). A plain word store/decrement
    // race on THIS counter is benign (worst case +-1 sample of settle duration, never a corrupted
    // state), matching the existing _diag_holdoff_samples pattern — no mutex needed. That benignity
    // depends on _arm_switched_rc_settling() always running BEFORE its caller mutates the circuit
    // parameter (v0.80 — see the CONTRACT note on _arm_switched_rc_settling() above); reversed, the
    // same cross-thread race would corrupt _last_valid_analog_state itself, not just the countdown.
    // See spec §5.8.4.
    volatile uint32_t _switched_rc_settling_countdown;

    // ── HGAC state (RF-only, two EMA per gain domain) ────────────────────────
    // Fast EMA → HIGH2 guard; slow EMA → HIGH1/LOW1 leveling. Fed v_tia_led per sample while the
    // gate is open; reset on gain change and while gated off (clean warmup on re-enable).
    EmaChannel _hgac_ema_fast_led1, _hgac_ema_fast_led2;         // IR / RED guard estimators (v_tia_led)
    EmaChannel _hgac_ema_slow_led1, _hgac_ema_slow_led2;         // IR / RED leveling estimators (v_tia_led)
    EmaChannel _hgac_ema_ambient_led1, _hgac_ema_ambient_led2;   // IR / RED ambient estimators (v_tia_aled)

    bool             _debug_enabled; // set by begin(debug=true); enables combined queue items
    // ── Output snapshot (written by task, pushed to queue) ──
    AFE4490Data      _current_data;
    AFE4490DebugData _debug_data;   // debug snapshot; written under _state_mutex each sample

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

    // HR1
    void  test_feed_hr1(float ot_ir, ProbeState probe_state) { _hr1_update(ot_ir, probe_state); }
    float test_hr1()                        { return _current_data.hr1; }
    float test_hr1_sqi()                    { return _current_data.hr1_sqi; }

    // HR2
    void  test_feed_hr2(float ot_ir, ProbeState probe_state) { _hr2_update_for_test(ot_ir, probe_state); }
    float test_hr2()                        { return _current_data.hr2; }
    float test_hr2_sqi()                    { return _current_data.hr2_sqi; }

    // HR3
    void  test_feed_hr3(float ot_ir, ProbeState probe_state) { _hr3_update_for_test(ot_ir, probe_state); }
    float test_hr3()                        { return _current_data.hr3; }
    float test_hr3_sqi()                    { return _current_data.hr3_sqi; }

    // SpO2
    void  test_feed_spo2(float ot_ir, float ot_red, ProbeState probe_state) {
        _spo2_update(ot_ir, ot_red, probe_state);
    }
    float test_spo2()                       { return _current_data.spo2; }
    float test_spo2_r()                     { return _current_data.spo2_r; }
    float test_spo2_sqi()                   { return _current_data.spo2_sqi; }
    float test_spo2_ir_ema_mean()  const { return _spo2_ch_ir_ema.mean; }
    float test_spo2_ir_ema_var()   const { return _spo2_ch_ir_ema.var; }
    float test_spo2_red_ema_mean() const { return _spo2_ch_red_ema.mean; }
    float test_spo2_red_ema_var()  const { return _spo2_ch_red_ema.var; }
    void  test_set_spo2_ir_ema(float mean, float var)  { _spo2_ch_ir_ema.mean = mean; _spo2_ch_ir_ema.var = var; }
    void  test_set_spo2_red_ema(float mean, float var) { _spo2_ch_red_ema.mean = mean; _spo2_ch_red_ema.var = var; }

    // HGAC — full-pipeline entry point (HGAC hooks inside _process_sample(), not a narrower path)
    void  test_feed_sample(int32_t led1, int32_t led2, int32_t aled1, int32_t aled2) {
        _process_sample(led1, led2, aled1, aled2, led1 - aled1, led2 - aled2);
    }
    AFE4490RF test_hgac_rf_led1() const { return _afe_tia_rf_led1; }
    AFE4490RF test_hgac_rf_led2() const { return _afe_tia_rf_led2; }
    // Isolated access to the RF-change primitive, bypassing the full sample pipeline.
    void test_hgac_change_rf_led1(AFE4490RF new_rf) { _hgac_change_rf(HgacColor::IR, new_rf); }
    void test_hgac_change_rf_led2(AFE4490RF new_rf) { _hgac_change_rf(HgacColor::RED, new_rf); }
    uint32_t test_diag_code() const { return _current_data.diag_code; }
    // Settle-window sample count (datasheet t5) at the current sample rate — see spec §5.8.4.
    uint32_t test_compute_switched_rc_settling_samples() const { return _compute_switched_rc_settling_samples(); }
    // Whether _task_body() would freeze its input this cycle (§5.6.3/§5.8.4) — see v0.72.
    bool test_should_freeze_input() const { return _should_freeze_input(); }
    // OT_LED1 as of the most recently processed sample — equals the `as.ot_led1` just fed to
    // RSQM/SpO2/HR1-3/ppg_disp, whether frozen or freshly computed (v0.79, §5.8.4).
    float test_last_ot_led1() const { return _last_valid_analog_state.ot_led1; }
#endif
};
