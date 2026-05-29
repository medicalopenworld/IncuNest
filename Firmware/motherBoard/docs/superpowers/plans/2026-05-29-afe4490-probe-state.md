# AFE4490 Probe State Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `probe_state` (DISCONNECTED / NOT_APPLIED / APPLIED) to the AFE4490 library, expose it via the communication protocol, and gate SpO2 display data behind `PROBE_APPLIED`.

**Architecture:** The `ProbeState` enum and field are added to `AFE4490Data` in the library. Detection logic is computed inside `_update_spo2()` using the already-available DC and PI metrics. `CommTask.cpp` reads `g_spo2_data.probe_state` and sends `CTRL,PROBE,<N>` every 2 s while not APPLIED, suppressing PPG/VIT in that same state.

**Tech Stack:** C++17, ESP32-S3, Arduino + FreeRTOS, PlatformIO, incunest_afe4490 library (v0.25).

> ⚠️ **Library path note:** Changes go into `.pio/libdeps/main/incunest_afe4490/`. This directory is managed by PlatformIO and can be overwritten by `pio update`. After validating, propagate these changes to the library's source repository.

---

## Files

| Action | Path |
|--------|------|
| Modify | `.pio/libdeps/main/incunest_afe4490/incunest_afe4490.h` |
| Modify | `.pio/libdeps/main/incunest_afe4490/incunest_afe4490.cpp` |
| Modify | `src/CommTask.cpp` |

---

### Task 1: Add `ProbeState` enum and field to the library header

**Files:**
- Modify: `.pio/libdeps/main/incunest_afe4490/incunest_afe4490.h` (lines 77–79)

Context: `AFE4490Data` is defined at lines 56–77. The `ProbeState` enum goes right after the struct closing brace, before the existing `AFE4490Channel` enum (line 80).

- [ ] **Step 1: Add the `ProbeState` enum after `AFE4490Data`**

Insert between line 77 (`};` closing `AFE4490Data`) and line 79 (`// ── Enumerations`):

```cpp
// Probe contact state — updated every sample inside _update_spo2().
// Initialises to DISCONNECTED (= 0) via zero-initialisation of AFE4490Data.
enum class ProbeState : uint8_t {
    DISCONNECTED = 0,   // DC level below spo2_min_dc — no optical path
    NOT_APPLIED  = 1,   // DC valid, but PI < spo2_pi_sqi_lo — probe not on skin
    APPLIED      = 2    // PI >= spo2_pi_sqi_lo — valid contact
};
```

- [ ] **Step 2: Add `probe_state` field to `AFE4490Data`**

Append inside `AFE4490Data` (after `hr3_sqi`, before the closing `};`):

```cpp
    ProbeState probe_state; // contact state — DISCONNECTED / NOT_APPLIED / APPLIED
```

The full tail of the struct becomes:
```cpp
    float   hr3;         // HR3 (FFT + HPS) in bpm
    float   hr3_sqi;     // HR3 Signal Quality Index [0–1]: HPS peak prominence in search range; 0=diffuse HPS, 1=dominant peak
    ProbeState probe_state; // contact state — DISCONNECTED / NOT_APPLIED / APPLIED
};
```

- [ ] **Step 3: Verify the header compiles (build check)**

```
pio run -e main 2>&1 | head -30
```

Expected: no errors about `ProbeState` or `probe_state`.

- [ ] **Step 4: Commit**

```bash
git add ".pio/libdeps/main/incunest_afe4490/incunest_afe4490.h"
git commit -m "feat(afe4490-lib): add ProbeState enum and probe_state field to AFE4490Data"
```

---

### Task 2: Implement probe_state detection in `_update_spo2()`

**Files:**
- Modify: `.pio/libdeps/main/incunest_afe4490/incunest_afe4490.cpp` (around line 1504)

Context: `_update_spo2(int32_t ir_corr, int32_t red_corr)` computes `pi` at line 1504, then immediately checks `_spo2_dc_ir < _spo2_min_dc` at line 1509 (which is the natural DISCONNECTED threshold). `probe_state` is computed from the same signals.

```cpp
// Line 1504 (existing):
_current_data.pi = (_spo2_dc_ir > 1.0f) ? (sqrtf(_spo2_ac2_ir) / _spo2_dc_ir) * 100.0f : -1.0f;
```

- [ ] **Step 1: Add probe_state detection immediately after `pi` is computed**

After line 1504 and before line 1506 (`_spo2_sample_count++`), insert:

```cpp
    // Probe state: reuses the same thresholds already used by the SpO2 algorithm.
    if (_spo2_dc_ir < _spo2_min_dc || _spo2_dc_red < _spo2_min_dc) {
        _current_data.probe_state = ProbeState::DISCONNECTED;
    } else if (_current_data.pi < _cfg.spo2_pi_sqi_lo) {
        _current_data.probe_state = ProbeState::NOT_APPLIED;
    } else {
        _current_data.probe_state = ProbeState::APPLIED;
    }
```

The resulting block (lines 1503–1510):
```cpp
    // Perfusion Index (always updated, independent of SpO2 warmup)
    _current_data.pi = (_spo2_dc_ir > 1.0f) ? (sqrtf(_spo2_ac2_ir) / _spo2_dc_ir) * 100.0f : -1.0f;

    // Probe state: reuses the same thresholds already used by the SpO2 algorithm.
    if (_spo2_dc_ir < _spo2_min_dc || _spo2_dc_red < _spo2_min_dc) {
        _current_data.probe_state = ProbeState::DISCONNECTED;
    } else if (_current_data.pi < _cfg.spo2_pi_sqi_lo) {
        _current_data.probe_state = ProbeState::NOT_APPLIED;
    } else {
        _current_data.probe_state = ProbeState::APPLIED;
    }

    _spo2_sample_count++;

    // Skip during warmup or if DC is too low (no finger)
    if (_spo2_sample_count < _spo2_warmup_samples ||
        _spo2_dc_ir < _spo2_min_dc || _spo2_dc_red < _spo2_min_dc) {
```

- [ ] **Step 2: Build and verify**

```
pio run -e main 2>&1 | head -30
```

Expected: clean build (no errors).

- [ ] **Step 3: Sanity check — `probe_state` is reachable via `g_spo2_data`**

In `SPO2.cpp`, `memcpy((void*)&g_spo2_data, &data, sizeof(data))` copies the entire struct including `probe_state`. No changes needed to `SPO2.cpp`.

- [ ] **Step 4: Commit**

```bash
git add ".pio/libdeps/main/incunest_afe4490/incunest_afe4490.cpp"
git commit -m "feat(afe4490-lib): compute probe_state in _update_spo2 using DC and PI thresholds"
```

---

### Task 3: Add probe status protocol and gate SPO2 data in `CommTask.cpp`

**Files:**
- Modify: `src/CommTask.cpp`

**Protocol addition:**
```
CTRL,PROBE,<N>\n
```
Where N = 0 (DISCONNECTED), 1 (NOT_APPLIED), 2 (APPLIED).  
Sent every 2 s while `probe_state != APPLIED`. When `APPLIED`, normal PPG + VIT flow resumes and `CTRL,PROBE` is not sent.

**What changes in each path:**
- Add a `static unsigned long last_probe_status_time = 0;` alongside existing timing statics.
- PPG block: skip (continue) if not APPLIED.
- Vitals block: skip VIT if not APPLIED; instead, if 2 s elapsed send `CTRL,PROBE,<N>`.

#### 3a — UART path (lines ~679–768, inside `#if HW_NUM >= 16`)

- [ ] **Step 1: Add `last_probe_status_time` static alongside the existing statics**

Find the block that declares `last_ppg_time` and `last_tel_time` in the UART path and add the new variable. It will look like:

```cpp
static unsigned long last_ppg_time         = 0;
static unsigned long last_tel_time         = 0;
static unsigned long last_probe_status_time = 0;  // add this line
```

- [ ] **Step 2: Replace the PPG block to skip when not APPLIED**

Old (line ~680–703):
```cpp
    // --- PPG waveform (25 Hz = every 40 ms) ---
    if (millis() - last_ppg_time >= 40) {
      // No valid signal: reset normalisation and send flat midpoint so the
      // display collapses its amplitude window immediately.
      if (g_spo2_data.spo2_sqi < 0.05f) {
        ppg_min = -1.0f;
        ppg_max =  1.0f;
        hmiSerial.print("CTRL,PPG,128\n");
      } else {
        float ppg_raw = g_spo2_data.ppg;
        // Expand range immediately, decay slowly toward 0 (bandpass signal is zero-mean)
        if (ppg_raw < ppg_min) ppg_min = ppg_raw;
        else ppg_min += (0.0f - ppg_min) * 0.005f;
        if (ppg_raw > ppg_max) ppg_max = ppg_raw;
        else ppg_max += (0.0f - ppg_max) * 0.005f;
        float range = ppg_max - ppg_min;
        uint8_t ppg_byte = (range > 1e-3f)
            ? (uint8_t)constrain((ppg_raw - ppg_min) / range * 255.0f, 0.0f, 255.0f)
            : 128;
        char ppg_msg[16];
        snprintf(ppg_msg, sizeof(ppg_msg), "CTRL,PPG,%u\n", ppg_byte);
        hmiSerial.print(ppg_msg);
      }
      last_ppg_time = millis();
    }
```

New (add `probe_state` guard at the top of the block):
```cpp
    // --- PPG waveform (25 Hz = every 40 ms) — only when probe is applied ---
    if (millis() - last_ppg_time >= 40) {
      if (g_spo2_data.probe_state == ProbeState::APPLIED) {
        if (g_spo2_data.spo2_sqi < 0.05f) {
          ppg_min = -1.0f;
          ppg_max =  1.0f;
          hmiSerial.print("CTRL,PPG,128\n");
        } else {
          float ppg_raw = g_spo2_data.ppg;
          if (ppg_raw < ppg_min) ppg_min = ppg_raw;
          else ppg_min += (0.0f - ppg_min) * 0.005f;
          if (ppg_raw > ppg_max) ppg_max = ppg_raw;
          else ppg_max += (0.0f - ppg_max) * 0.005f;
          float range = ppg_max - ppg_min;
          uint8_t ppg_byte = (range > 1e-3f)
              ? (uint8_t)constrain((ppg_raw - ppg_min) / range * 255.0f, 0.0f, 255.0f)
              : 128;
          char ppg_msg[16];
          snprintf(ppg_msg, sizeof(ppg_msg), "CTRL,PPG,%u\n", ppg_byte);
          hmiSerial.print(ppg_msg);
        }
      }
      last_ppg_time = millis();
    }
```

- [ ] **Step 3: Replace the vitals block to add probe status and gate VIT**

Old (line ~741–768):
```cpp
      // HR fusion: weighted average of HR2+HR3 with agreement check and hysteresis
      uint8_t hr_byte = 0;
      {
        float h2 = g_spo2_data.hr2, h3 = g_spo2_data.hr3;
        float s2 = g_spo2_data.hr2_sqi, s3 = g_spo2_data.hr3_sqi;
        bool valid = (h2 > 0.0f) && (h3 > 0.0f) &&
                     (fabsf(h2 - h3) < 8.0f) &&
                     (fmaxf(s2, s3) >= 0.8f) &&
                     (fminf(s2, s3) >= 0.5f);
        if (valid) {
          hr_bad_streak = 0;
          if (++hr_valid_streak >= 2) hr_displaying = true;
        } else {
          hr_valid_streak = 0;
          if (++hr_bad_streak >= 3) hr_displaying = false;
        }
        if (hr_displaying && valid) {
          float hr_fused = (h2 * s2 + h3 * s3) / (s2 + s3);
          if (hr_fused >= 40.0f && hr_fused <= 240.0f)
            hr_byte = (uint8_t)(hr_fused + 0.5f);
        }
      }
      char vit_msg[20];
      snprintf(vit_msg, sizeof(vit_msg), "CTRL,VIT,%u,0\n", hr_byte);
      hmiSerial.print(vit_msg);

      last_tel_time = millis();
```

New (insert probe status block; keep VIT conditional on APPLIED):
```cpp
      if (g_spo2_data.probe_state != ProbeState::APPLIED) {
        // Probe not on patient — send status every 2 s, suppress vitals
        if (millis() - last_probe_status_time >= 2000) {
          char probe_msg[20];
          snprintf(probe_msg, sizeof(probe_msg), "CTRL,PROBE,%u\n",
                   (uint8_t)g_spo2_data.probe_state);
          hmiSerial.print(probe_msg);
          last_probe_status_time = millis();
        }
      } else {
        // Probe applied — send fused HR vitals
        uint8_t hr_byte = 0;
        {
          float h2 = g_spo2_data.hr2, h3 = g_spo2_data.hr3;
          float s2 = g_spo2_data.hr2_sqi, s3 = g_spo2_data.hr3_sqi;
          bool valid = (h2 > 0.0f) && (h3 > 0.0f) &&
                       (fabsf(h2 - h3) < 8.0f) &&
                       (fmaxf(s2, s3) >= 0.8f) &&
                       (fminf(s2, s3) >= 0.5f);
          if (valid) {
            hr_bad_streak = 0;
            if (++hr_valid_streak >= 2) hr_displaying = true;
          } else {
            hr_valid_streak = 0;
            if (++hr_bad_streak >= 3) hr_displaying = false;
          }
          if (hr_displaying && valid) {
            float hr_fused = (h2 * s2 + h3 * s3) / (s2 + s3);
            if (hr_fused >= 40.0f && hr_fused <= 240.0f)
              hr_byte = (uint8_t)(hr_fused + 0.5f);
          }
        }
        char vit_msg[20];
        snprintf(vit_msg, sizeof(vit_msg), "CTRL,VIT,%u,0\n", hr_byte);
        hmiSerial.print(vit_msg);
      }

      last_tel_time = millis();
```

#### 3b — USB CDC path (lines ~881–950, inside `#else` branch)

The USB path has structurally identical PPG and vitals blocks. Apply the exact same changes, replacing `hmiSerial.print(...)` with `CommunicationHost_Send(...)`.

- [ ] **Step 4: Add `last_probe_status_time` static in the USB path** (same as Step 1, inside the `#else` scope).

- [ ] **Step 5: Replace USB PPG block** — same guard as Step 2, using `CommunicationHost_Send`:

```cpp
    // PPG waveform at 25 Hz — only when probe is applied
    if (millis() - last_ppg_time >= 40) {
      if (g_spo2_data.probe_state == ProbeState::APPLIED) {
        char ppg_msg[16];
        if (g_spo2_data.spo2_sqi < 0.05f) {
          ppg_min = -1.0f;
          ppg_max =  1.0f;
          snprintf(ppg_msg, sizeof(ppg_msg), "CTRL,PPG,128\n");
        } else {
          float ppg_raw = g_spo2_data.ppg;
          if (ppg_raw < ppg_min) ppg_min = ppg_raw;
          else ppg_min += (0.0f - ppg_min) * 0.005f;
          if (ppg_raw > ppg_max) ppg_max = ppg_raw;
          else ppg_max += (0.0f - ppg_max) * 0.005f;
          float range = ppg_max - ppg_min;
          uint8_t ppg_byte = (range > 1e-3f)
              ? (uint8_t)constrain((ppg_raw - ppg_min) / range * 255.0f, 0.0f, 255.0f)
              : 128;
          snprintf(ppg_msg, sizeof(ppg_msg), "CTRL,PPG,%u\n", ppg_byte);
        }
        CommunicationHost_Send(ppg_msg);
      }
      last_ppg_time = millis();
    }
```

- [ ] **Step 6: Replace USB vitals block** — same logic as Step 3, using `CommunicationHost_Send`:

```cpp
      if (g_spo2_data.probe_state != ProbeState::APPLIED) {
        if (millis() - last_probe_status_time >= 2000) {
          char probe_msg[20];
          snprintf(probe_msg, sizeof(probe_msg), "CTRL,PROBE,%u\n",
                   (uint8_t)g_spo2_data.probe_state);
          CommunicationHost_Send(probe_msg);
          last_probe_status_time = millis();
        }
      } else {
        uint8_t hr_byte = 0;
        {
          float h2 = g_spo2_data.hr2, h3 = g_spo2_data.hr3;
          float s2 = g_spo2_data.hr2_sqi, s3 = g_spo2_data.hr3_sqi;
          bool valid = (h2 > 0.0f) && (h3 > 0.0f) &&
                       (fabsf(h2 - h3) < 8.0f) &&
                       (fmaxf(s2, s3) >= 0.8f) &&
                       (fminf(s2, s3) >= 0.5f);
          if (valid) {
            hr_bad_streak = 0;
            if (++hr_valid_streak >= 2) hr_displaying = true;
          } else {
            hr_valid_streak = 0;
            if (++hr_bad_streak >= 3) hr_displaying = false;
          }
          if (hr_displaying && valid) {
            float hr_fused = (h2 * s2 + h3 * s3) / (s2 + s3);
            if (hr_fused >= 40.0f && hr_fused <= 240.0f)
              hr_byte = (uint8_t)(hr_fused + 0.5f);
          }
        }
        char vit_msg[20];
        snprintf(vit_msg, sizeof(vit_msg), "CTRL,VIT,%u,0\n", hr_byte);
        CommunicationHost_Send(vit_msg);
      }

      last_tel_time = millis();
```

- [ ] **Step 7: Build**

```
pio run -e main 2>&1 | head -40
```

Expected: clean build, no errors.

- [ ] **Step 8: Commit**

```bash
git add src/CommTask.cpp
git commit -m "feat(comm): gate SPO2 data on PROBE_APPLIED; send CTRL,PROBE every 2s otherwise"
```

---

## Protocol summary (for display-side reference)

| Message | Direction | When sent | Meaning |
|---------|-----------|-----------|---------|
| `CTRL,PROBE,0\n` | MB → Display | Every 2 s | Probe disconnected (no optical path) |
| `CTRL,PROBE,1\n` | MB → Display | Every 2 s | Probe present but not applied to patient |
| `CTRL,PROBE,2\n` | MB → Display | — | (Never sent — APPLIED transitions to normal PPG/VIT flow) |
| `CTRL,PPG,<byte>\n` | MB → Display | 25 Hz | Only when `probe_state == APPLIED` |
| `CTRL,VIT,<hr>,0\n` | MB → Display | 1 Hz | Only when `probe_state == APPLIED` |
