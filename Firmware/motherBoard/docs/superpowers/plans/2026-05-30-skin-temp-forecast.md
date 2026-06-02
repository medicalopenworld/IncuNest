# Skin Temperature Forecast & Contact Quality — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `skinTemperatureForecast` (Prony T_ss estimator) and `skinContactQuality` (dT/dt classifier) as read-only fields in `in3`, logged alongside the existing `[SKIN_WARMUP]` trace.

**Architecture:** A single static function `updateSkinForecast(float tempRaw)` owns all state (warmup detection, circular sample buffer, Prony math, contact classifier). It is called from the existing `applyNTCResult()` after calibration is applied, and writes directly to `in3.skinTemperatureForecast` and `in3.skinContactQuality`. No other files are touched.

**Tech Stack:** C++17 / Arduino / FreeRTOS / PlatformIO ESP32-S3. No unit-test runner — correctness is verified by `pio run` (compilation) and serial log inspection after flash.

---

## File Map

| File | Change |
|---|---|
| `include/main.h` | Add 2 fields to `IncuNest_parameters` struct |
| `src/sensors.cpp` | Add 8 constants, add `updateSkinForecast()`, call it from `applyNTCResult()`, extend `[SKIN_WARMUP]` log |

---

## Task 1 — Add struct fields to `IncuNest_parameters`

**Files:**
- Modify: `include/main.h` (around line 666, after `fineTuneSkinTemperature`)

- [ ] **Step 1.1 — Add the two new fields**

Open `include/main.h`. After the line:
```cpp
  double fineTuneSkinTemperature = false;
```
Insert:
```cpp
  double skinTemperatureForecast = NAN;  // Prony T_ss estimate. NAN = not yet available
  uint8_t skinContactQuality = 0;        // 0=unknown, 1=poor, 2=good
```

- [ ] **Step 1.2 — Verify NAN is available in this TU**

`NAN` is defined via `<math.h>`. `main.h` already includes `esp_log.h` which pulls in
the C standard headers on ESP-IDF. If the build fails with "NAN undeclared", add
`#include <math.h>` at the top of `main.h`. Likely not needed.

- [ ] **Step 1.3 — Build**

```bash
pio run
```

Expected: `[SUCCESS]`. No new warnings beyond the pre-existing clang-analysis ones
visible in the IDE (those are not from GCC and do not affect the build).

- [ ] **Step 1.4 — Commit**

```bash
git add include/main.h
git commit -m "feat(sensors): add skinTemperatureForecast and skinContactQuality to in3"
```

---

## Task 2 — Add constants and implement `updateSkinForecast()`

**Files:**
- Modify: `src/sensors.cpp`

### Step 2.1 — Add configuration constants

Find the top of `src/sensors.cpp` (after the `#include` lines, before any function
definitions). Add these file-scope constants:

```cpp
// --- Skin forecast configuration ---
static constexpr float    SKIN_WARMUP_DETECT_DELTA_C         = 1.5f;   // °C/5s to detect placement
static constexpr float    SKIN_REMOVAL_DELTA_C               = 2.0f;   // °C/5s drop = removal
static constexpr float    SKIN_SETTLED_RATE_C_PER_S          = 0.005f; // dT/dt < this → settled
static constexpr uint32_t SKIN_SAMPLE_INTERVAL_MS            = 10000;  // Prony buffer cadence
static constexpr float    SKIN_PRONY_MIN_DENOM               = 0.05f;  // numerical guard
static constexpr float    SKIN_FORECAST_MAX_C                = 42.0f;
static constexpr float    SKIN_FORECAST_MIN_C                = 30.0f;
static constexpr float    SKIN_CONTACT_QUALITY_RATE_THRESHOLD = 0.20f; // °C/s at t=15s (tune later)
```

- [ ] **Step 2.2 — Add the warmup state enum**

Immediately after the constants block (still file scope, before any function):

```cpp
enum SkinWarmupState : uint8_t { SW_IDLE, SW_WARMING, SW_SETTLED };
```

- [ ] **Step 2.3 — Add `updateSkinForecast()` — full implementation**

Add this function immediately before `applyNTCResult()` (around line 380):

```cpp
// Estimates final skin temperature (Prony 3-point method) and classifies
// sensor contact quality (dT/dt at t=15s). Called after every valid NTC reading.
// All state is static — no heap allocation.
static void updateSkinForecast(float tempRaw) {
  static SkinWarmupState state      = SW_IDLE;
  static uint32_t warmupStartMs     = 0;
  static float T_at_placement       = 0.0f;
  static bool qualityClassified     = false;

  // 5-second sliding window for placement/removal/settled detection
  static float    tempBefore5s      = NAN;
  static uint32_t last5sMs         = 0;
  static uint8_t  settledWindows   = 0;  // consecutive 5s windows below settled threshold

  // Circular sample buffer: one entry per SKIN_SAMPLE_INTERVAL_MS from placement
  static float    pronyBuf[12]     = {};
  static uint16_t pronyTotal       = 0;  // total samples written this placement
  static uint32_t lastSampleMs     = 0;

  uint32_t now = millis();

  // Guard: if called after a long gap (sensor was absent), reset to IDLE.
  if (state != SW_IDLE && (now - last5sMs) > 30000) {
    state = SW_IDLE;
    in3.skinTemperatureForecast = NAN;
    in3.skinContactQuality      = 0;
    pronyTotal                  = 0;
    tempBefore5s                = NAN;
  }

  // ── 5-second delta check ──────────────────────────────────────────────────
  if (now - last5sMs >= 5000) {
    if (!isnan(tempBefore5s)) {
      float delta = tempRaw - tempBefore5s;

      if (state == SW_IDLE) {
        if (delta > SKIN_WARMUP_DETECT_DELTA_C) {
          // Placement detected
          state             = SW_WARMING;
          warmupStartMs     = now;
          T_at_placement    = tempBefore5s;  // temp just before the jump
          qualityClassified = false;
          pronyTotal        = 0;
          settledWindows    = 0;
          lastSampleMs      = now;
          in3.skinContactQuality      = 0;
          in3.skinTemperatureForecast = NAN;
          // First Prony sample at t≈0
          pronyBuf[0] = tempRaw;
          pronyTotal  = 1;
        }
      } else if (state == SW_WARMING) {
        if (delta < -SKIN_REMOVAL_DELTA_C) {
          // Removal detected
          state                       = SW_IDLE;
          in3.skinTemperatureForecast = NAN;
          in3.skinContactQuality      = 0;
          pronyTotal                  = 0;
        } else {
          // Check settled: |delta| < threshold × 5s
          if (fabsf(delta) < SKIN_SETTLED_RATE_C_PER_S * 5.0f) {
            settledWindows++;
            if (settledWindows >= 6) {  // 6 × 5s = 30s
              state = SW_SETTLED;
            }
          } else {
            settledWindows = 0;
          }
        }
      } else {  // SW_SETTLED
        if (delta < -SKIN_REMOVAL_DELTA_C) {
          state                       = SW_IDLE;
          in3.skinTemperatureForecast = NAN;
          in3.skinContactQuality      = 0;
          pronyTotal                  = 0;
        }
      }
    }
    tempBefore5s = tempRaw;
    last5sMs     = now;
  }

  // ── Contact quality at t=15s ──────────────────────────────────────────────
  if (state == SW_WARMING && !qualityClassified) {
    uint32_t elapsed = now - warmupStartMs;
    if (elapsed >= 14800 && elapsed <= 15200) {
      float dT_dt = (tempRaw - T_at_placement) / 15.0f;
      in3.skinContactQuality = (dT_dt >= SKIN_CONTACT_QUALITY_RATE_THRESHOLD) ? 2 : 1;
      qualityClassified = true;
    }
  }

  // ── Prony buffer: add one sample every SKIN_SAMPLE_INTERVAL_MS ───────────
  if (state == SW_WARMING && now - lastSampleMs >= SKIN_SAMPLE_INTERVAL_MS) {
    pronyBuf[pronyTotal % 12] = tempRaw;
    pronyTotal++;
    lastSampleMs = now;
  }

  // ── Forecast computation ──────────────────────────────────────────────────
  if (state == SW_SETTLED) {
    in3.skinTemperatureForecast = in3.temperature[SKIN_SENSOR];
    return;
  }

  uint8_t pronyValid = (pronyTotal >= 12) ? 12 : (uint8_t)pronyTotal;
  if (state != SW_WARMING || pronyValid < 3) {
    in3.skinTemperatureForecast = NAN;
    return;
  }

  // Select spacing k (in number of 10s samples) based on elapsed time.
  // Larger k → more spread → better numerical conditioning.
  uint32_t elapsed = now - warmupStartMs;
  uint8_t k;
  if      (elapsed < 40000)  k = 1;   // Δt=10s, need ≥3 samples
  else if (elapsed < 80000)  k = 2;   // Δt=20s, need ≥5 samples
  else if (elapsed < 120000) k = 3;   // Δt=30s, need ≥7 samples
  else                        k = 4;   // Δt=40s, need ≥9 samples

  if (pronyValid < (uint8_t)(2 * k + 1)) {
    in3.skinTemperatureForecast = NAN;
    return;
  }

  // Read T1 (oldest), T2 (middle), T3 (newest) from circular buffer.
  // pronyTotal-1 is the index of the most recent write.
  // Adding 1200 (=100×12) ensures the expression stays positive before % 12.
  int base  = (int)pronyTotal - 1;
  float T3  = pronyBuf[(base          + 1200) % 12];  // most recent
  float T2  = pronyBuf[(base - k      + 1200) % 12];  // middle
  float T1  = pronyBuf[(base - 2 * k  + 1200) % 12];  // oldest

  float denom = T1 + T3 - 2.0f * T2;

  if (fabsf(denom) < SKIN_PRONY_MIN_DENOM) {
    // Signal nearly flat → treat as settled
    in3.skinTemperatureForecast = in3.temperature[SKIN_SENSOR];
    return;
  }

  float T_ss = (T1 * T3 - T2 * T2) / denom;

  if (T_ss < SKIN_FORECAST_MIN_C || T_ss > SKIN_FORECAST_MAX_C || T_ss < (double)tempRaw) {
    in3.skinTemperatureForecast = NAN;
    return;
  }

  in3.skinTemperatureForecast = (double)T_ss;
}
```

- [ ] **Step 2.4 — Build**

```bash
pio run
```

Expected: `[SUCCESS]`. The function is defined but not yet called — no unused-variable
warnings because the statics are all written within the function.

- [ ] **Step 2.5 — Commit**

```bash
git add src/sensors.cpp
git commit -m "feat(sensors): add updateSkinForecast() — Prony estimator + contact quality"
```

---

## Task 3 — Wire `updateSkinForecast()` and extend the log

**Files:**
- Modify: `src/sensors.cpp` — inside `applyNTCResult()`

### Context

`applyNTCResult()` currently ends with this log block (after calibration):

```cpp
    // [SKIN_WARMUP_LOG] CSV para análisis de curva de calentamiento.
    // Formato: t_ms,raw_C,filtered_C,calibrated_C
    // Filtrar con tag "SKIN_W" en el monitor serie.
    // Borrar este bloque cuando el algoritmo de predicción esté validado.
    static uint32_t skinLogLastMs = 0;
    static uint32_t skinLogT0 = 0;
    static float skinLogLastRaw = 0;
    if (fabsf(tempRaw - skinLogLastRaw) > 2.0f || skinLogT0 == 0) {
      skinLogT0 = millis(); // reset si el sensor fue recolocado (salto >2°C)
    }
    skinLogLastRaw = tempRaw;
    if (millis() - skinLogLastMs >= 1000) {
      logI("[SKIN_WARMUP] " + String(millis() - skinLogT0) + "," +
           String(tempRaw, 3) + "," +
           String(filteredTemp, 3) + "," +
           String(in3.temperature[SKIN_SENSOR], 3));
      skinLogLastMs = millis();
    }

    return true;
```

- [ ] **Step 3.1 — Call `updateSkinForecast()` and extend the log**

Replace the entire log block above with:

```cpp
    updateSkinForecast(tempRaw);

    // [SKIN_WARMUP_LOG] CSV para análisis de curva de calentamiento.
    // Formato: t_ms,raw_C,filtered_C,calibrated_C,forecast_C,quality
    // Borrar este bloque cuando el algoritmo de predicción esté validado.
    static uint32_t skinLogLastMs = 0;
    static uint32_t skinLogT0 = 0;
    static float skinLogLastRaw = 0;
    if (fabsf(tempRaw - skinLogLastRaw) > 2.0f || skinLogT0 == 0) {
      skinLogT0 = millis();
    }
    skinLogLastRaw = tempRaw;
    if (millis() - skinLogLastMs >= 1000) {
      String forecastStr = isnan(in3.skinTemperatureForecast)
                               ? "nan"
                               : String((float)in3.skinTemperatureForecast, 3);
      logI("[SKIN_WARMUP] " + String(millis() - skinLogT0) + "," +
           String(tempRaw, 3) + "," +
           String(filteredTemp, 3) + "," +
           String(in3.temperature[SKIN_SENSOR], 3) + "," +
           forecastStr + "," +
           String(in3.skinContactQuality));
      skinLogLastMs = millis();
    }

    return true;
```

- [ ] **Step 3.2 — Build**

```bash
pio run
```

Expected: `[SUCCESS]`. Check that no new warnings appear.

- [ ] **Step 3.3 — Flash and verify via serial**

```bash
pio run -t upload && pio device monitor | Select-String "SKIN_WARMUP"
```

**Before placing the sensor**, verify the log is silent (no output) — `updateSkinForecast`
is in `SW_IDLE` and the 1s log timer doesn't fire until a valid reading is received.

Wait for boot to complete (you will see other log lines but no `SKIN_WARMUP`).

**Place the sensor in the axilla.** Verify the log starts printing within 1-2s.

Expected first ~20s of output (sensor warming up, no forecast yet):
```
[SKIN_WARMUP] 1001,27.450,27.123,27.123,nan,0
[SKIN_WARMUP] 2003,29.800,28.500,28.500,nan,0
...
[SKIN_WARMUP] 14900,35.200,34.800,34.800,nan,0
```

At t≈15s, `quality` changes from `0` to `1` or `2` (one-time classification).

At t≈20s, first Prony estimate appears (likely a large overestimate — that's expected and the point of data collection):
```
[SKIN_WARMUP] 20100,36.100,35.900,35.900,41.200,2
```

By t≈60-90s the forecast should narrow:
```
[SKIN_WARMUP] 90000,37.900,37.800,37.800,38.400,2
```

When sensor is removed: `forecast` returns to `nan`, `quality` to `0` within the next
5-second detection window.

**If forecast never appears (stays `nan` past t=30s):** check that `pronyTotal` is
incrementing — add a temporary `logI("prony total=" + String(pronyTotal))` inside
`updateSkinForecast` for diagnosis, then remove it.

- [ ] **Step 3.4 — Commit**

```bash
git add src/sensors.cpp
git commit -m "feat(sensors): wire updateSkinForecast, extend SKIN_WARMUP log with forecast+quality"
```

---

## Self-Review Checklist

**Spec coverage:**
- §3 new fields in `in3` → Task 1 ✓
- §4.1 placement/removal/settled detection → Task 2 Step 2.3 ✓
- §4.2 circular buffer, 12 entries, 10s cadence → Task 2 Step 2.3 ✓
- §4.3 Prony from t=20s, growing Δt, all sanity checks → Task 2 Step 2.3 ✓
- §4.4 contact quality at t=15s ±200ms → Task 2 Step 2.3 ✓
- §5 extended log with forecast+quality → Task 3 ✓
- §6 all 8 constants, file-scope → Task 2 Step 2.1 ✓
- §7 no PID/display/EEPROM/alarm changes → confirmed, only sensors.cpp + main.h touched ✓

**Placeholder scan:** no TBDs, no "implement later", all code blocks complete ✓

**Type consistency:**
- `updateSkinForecast(float tempRaw)` — defined Task 2 Step 2.3, called Task 3 Step 3.1 ✓
- `in3.skinTemperatureForecast` (double) — declared Task 1, written in Task 2, read in Task 3 ✓
- `in3.skinContactQuality` (uint8_t) — declared Task 1, written in Task 2, read in Task 3 ✓
- `SW_IDLE / SW_WARMING / SW_SETTLED` enum — defined Task 2 Step 2.2, used Task 2 Step 2.3 ✓
- All 8 constants defined Task 2 Step 2.1, used Task 2 Step 2.3 ✓
