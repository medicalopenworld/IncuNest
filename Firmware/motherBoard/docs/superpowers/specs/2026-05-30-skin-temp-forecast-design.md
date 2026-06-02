# Skin Temperature Forecast & Contact Quality — Design Spec

**Date:** 2026-05-30  
**Status:** Approved  
**Scope:** `motherBoard` firmware only. No PID changes. No display changes.

---

## 1. Problem

The skin NTC sensor (YSI 400, ADS1110 ADC, 5 Hz) takes 2–4 minutes to reach thermal
equilibrium when placed in the axilla. The convergence time varies with placement quality.
During this warmup the displayed temperature is clinically misleading and the PID
runs on a cold reading (safety concern to address separately in a future task).

**Goal of this spec:** Add two new read-only variables to `in3` that:
1. Estimate the final steady-state skin temperature as early as possible (`skinTemperatureForecast`)
2. Classify sensor contact quality at t=15s from placement (`skinContactQuality`)

Both variables are for **data collection and future validation only**. The existing
`in3.temperature[SKIN_SENSOR]` and PID are untouched.

---

## 2. Background — Data Analysis

Two warmup curves were captured from the same person:

| t from placement | Dataset 1 (raw °C) | Dataset 2 (raw °C) |
|---|---|---|
| 0s | 26.6 | 26.9 |
| 30s | 37.07 | 36.70 |
| 60s | 37.70 | 37.36 |
| 90s | 37.90 | 37.61 |
| ~265s | — | 38.03 (settled) |

Key findings:
- **T_ss ≈ 38.03°C** for both placements (same person, consistent)
- **τ varies ~2× between placements** (28–31s vs 44–63s) due to contact quality
- T_ss and τ are **both unknown** at placement time → prediction is fundamentally
  underdetermined; ±0.2°C is not reliably achievable
- **Realistic accuracy:** ±0.3–0.8°C depending on contact and time elapsed
- **Prony estimator** (3 equally-spaced points) gives useful estimates from t=20s,
  improves as more data accumulates
- **Contact quality** is detectable from dT/dt in the first 15s

The variables are introduced now to accumulate clinical data from real babies and
determine empirically when and how accurate the forecast becomes in practice.

---

## 3. New Fields in `IncuNest_parameters` (main.h)

```cpp
double skinTemperatureForecast;  // Prony T_ss estimate (°C). NAN = not yet available
uint8_t skinContactQuality;      // 0=unknown, 1=poor, 2=good
```

Initialized to `NAN` and `0` respectively on reset/startup.

---

## 4. Algorithm

All logic lives in a new static function `updateSkinForecast(float tempRaw)` called
from `applyNTCResult()` after every successful reading.

### 4.1 Placement Detection

A placement is detected when the temperature rise over the last 5 seconds exceeds
**1.5°C**. On detection:
- `warmupStartMs` = `millis()`
- `T_at_placement` = current `tempRaw`
- All Prony buffer and quality state reset

A removal is detected when temperature drops >2°C in 5s, or the reading returns 0.
On removal: reset to IDLE, `skinTemperatureForecast = NAN`, `skinContactQuality = 0`.

Internal warmup states: `IDLE`, `WARMING`, `SETTLED`.  
Transition WARMING→SETTLED: dT/dt < 0.005°C/s for 30 consecutive seconds.

### 4.2 Sample Buffer

A circular buffer of **12 entries** stores `(t_relative_ms, T_raw)` sampled every
**10 seconds** from placement. This covers 120s of history at constant cost.

### 4.3 Prony Estimator (T_ss)

Requires 3 equally-spaced samples. Runs from **t=20s** onwards.

The spacing Δt grows with elapsed time to improve numerical stability:

| Elapsed | Δt used | Points at |
|---|---|---|
| 20–39s | 10s | t-20, t-10, t |
| 40–79s | 20s | t-40, t-20, t |
| 80–119s | 30s | t-60, t-30, t |
| ≥120s | 40s | t-80, t-40, t |

Formula:
```
T_ss = (T1·T3 - T2²) / (T1 + T3 - 2·T2)
```

**Sanity checks** (any failure → `skinTemperatureForecast = NAN`):
- `|denominator| < 0.05` → signal is flat → use `in3.temperature[SKIN_SENSOR]` directly
- `T_ss < tempRaw` → physically impossible for rising signal
- `T_ss > 42.0°C` → outside physiological range
- `T_ss < 30.0°C` → below plausible baby temperature

When state is SETTLED, `skinTemperatureForecast` is set to `in3.temperature[SKIN_SENSOR]`
(the reading is already converged, no prediction needed).

### 4.4 Contact Quality Classifier

Computed **once** at `t_elapsed == 15s` (±200ms tolerance):

```
dT_dt_15s = (T_raw_now - T_at_placement) / 15.0  [°C/s]
```

- `dT_dt_15s >= SKIN_CONTACT_QUALITY_RATE_THRESHOLD` → `skinContactQuality = 2` (good)
- `dT_dt_15s <  SKIN_CONTACT_QUALITY_RATE_THRESHOLD` → `skinContactQuality = 1` (poor)

```cpp
#define SKIN_CONTACT_QUALITY_RATE_THRESHOLD 0.20f  // °C/s — to be tuned with baby data
```

Quality is frozen after classification until the next placement detection.

---

## 5. Logging

The existing `[SKIN_WARMUP]` log line is extended to include the two new fields:

```
[SKIN_WARMUP] t_ms,raw_C,filtered_C,cal_C,forecast_C,quality
```

- `forecast_C` = `skinTemperatureForecast` formatted to 3 decimal places,
  or the literal string `nan` when NAN
- `quality` = `skinContactQuality` (0, 1, or 2)

Log fires every 1000ms as before. This gives a complete trace for offline analysis.

---

## 6. Configuration Constants

Defined in `sensors.cpp` (file-scope, not in headers — these are internal tuning knobs):

```cpp
static constexpr float SKIN_WARMUP_DETECT_DELTA_C   = 1.5f;  // °C/5s to detect placement
static constexpr float SKIN_REMOVAL_DELTA_C          = 2.0f;  // °C/5s drop to detect removal
static constexpr float SKIN_SETTLED_RATE_C_PER_S     = 0.005f;// dT/dt threshold for SETTLED
static constexpr uint32_t SKIN_SAMPLE_INTERVAL_MS    = 10000; // Prony buffer sample rate
static constexpr float SKIN_PRONY_MIN_DENOM          = 0.05f; // numerical stability guard
static constexpr float SKIN_FORECAST_MAX_C           = 42.0f;
static constexpr float SKIN_FORECAST_MIN_C           = 30.0f;
static constexpr float SKIN_CONTACT_QUALITY_RATE_THRESHOLD = 0.20f; // °C/s at t=15s
```

---

## 7. What Is NOT Changed

- `in3.temperature[SKIN_SENSOR]` — unchanged
- `filter_1` (Butterworth) — unchanged
- Calibration logic — unchanged
- PID — untouched
- Display — untouched
- EEPROM — untouched
- Alarm logic — untouched

---

## 8. Files Modified

| File | Change |
|---|---|
| `include/main.h` | Add `skinTemperatureForecast` and `skinContactQuality` to struct |
| `src/sensors.cpp` | Add `updateSkinForecast()`, call from `applyNTCResult()`, extend log |

---

## 9. Future Work (out of scope)

- Tune `SKIN_CONTACT_QUALITY_RATE_THRESHOLD` with real baby data
- Evaluate if Prony accuracy justifies showing forecast on HMI display
- Evaluate PID clamping during warmup (separate safety task)
- Consider zero-heat-flux sensor hardware for faster equilibration
