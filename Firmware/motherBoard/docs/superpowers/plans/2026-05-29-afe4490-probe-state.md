# AFE4490 Probe State Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `probe_state` (DISCONNECTED / NOT_APPLIED / APPLIED) to the AFE4490 library, expose it via the communication protocol on the motherboard, and make the Display_HMI receive and act on the new `CTRL,PROBE,<N>` messages.

**Architecture:** The `ProbeState` enum and field are added to `AFE4490Data` in the library. Detection logic is computed inside `_update_spo2()` using the already-available DC and PI metrics. Motherboard `CommTask.cpp` reads `g_spo2_data.probe_state`: sends `CTRL,PROBE,<N>` every 2 s while not APPLIED, and `CTRL,PROBE,2` immediately on the APPLIED transition; PPG/VIT are suppressed until APPLIED. Display `CommTask.cpp` parses the new message and `UITask.cpp` drives probe attachment state from it, replacing the old variance heuristic.

**Tech Stack:** C++17, ESP32-S3, Arduino + FreeRTOS, PlatformIO, incunest_afe4490 library (v0.25), LVGL.

> ⚠️ **Library path note:** Changes go into `.pio/libdeps/main/incunest_afe4490/`. This directory is managed by PlatformIO and can be overwritten by `pio update`. After validating, propagate these changes to the library's source repository.

---

## Files

| Action | Path |
|--------|------|
| Modify | `.pio/libdeps/main/incunest_afe4490/incunest_afe4490.h` |
| Modify | `.pio/libdeps/main/incunest_afe4490/incunest_afe4490.cpp` |
| Modify | `src/CommTask.cpp` (motherboard) |
| Modify | `../Display_HMI/include/CommTask.h` |
| Modify | `../Display_HMI/src/CommTask.cpp` |
| Modify | `../Display_HMI/src/UITask.cpp` |

All paths are relative to `Firmware/motherBoard/` unless noted. The display lives at `Firmware/Display_HMI/`.

---

### Task 1: Add `ProbeState` enum and field to the library header

**Files:**
- Modify: `.pio/libdeps/main/incunest_afe4490/incunest_afe4490.h`

Context: `AFE4490Data` struct closes at line 77 (`};`). The existing enums section begins at line 79. `ProbeState` is inserted between them and then referenced as a field inside `AFE4490Data`.

- [ ] **Step 1: Add `ProbeState` enum immediately after `AFE4490Data` closing brace**

Between line 77 (`};` of `AFE4490Data`) and line 79 (`// ── Enumerations`), insert:

```cpp
// Probe contact state — updated every sample inside _update_spo2().
// Initialises to DISCONNECTED (= 0) via zero-initialisation of AFE4490Data.
enum class ProbeState : uint8_t {
    DISCONNECTED = 0,   // DC level below spo2_min_dc — no optical path
    NOT_APPLIED  = 1,   // DC valid, but PI < spo2_pi_sqi_lo — probe not on skin
    APPLIED      = 2    // PI >= spo2_pi_sqi_lo — valid contact
};
```

- [ ] **Step 2: Add `probe_state` field at the end of `AFE4490Data`**

Inside `AFE4490Data`, after `hr3_sqi` and before the closing `};`:

```cpp
    float   hr3_sqi;     // HR3 Signal Quality Index [0–1]: HPS peak prominence in search range; 0=diffuse HPS, 1=dominant peak
    ProbeState probe_state; // contact state — DISCONNECTED / NOT_APPLIED / APPLIED
};
```

- [ ] **Step 3: Build check**

```
cd C:\Users\Pablo\Documents\IncuNest_dev\IncuNest\Firmware\motherBoard
pio run -e main 2>&1 | head -30
```

Expected: no errors related to `ProbeState` or `probe_state`.

- [ ] **Step 4: Commit**

```bash
git add ".pio/libdeps/main/incunest_afe4490/incunest_afe4490.h"
git commit -m "feat(afe4490-lib): add ProbeState enum and probe_state field to AFE4490Data"
```

---

### Task 2: Implement `probe_state` detection in `_update_spo2()`

**Files:**
- Modify: `.pio/libdeps/main/incunest_afe4490/incunest_afe4490.cpp`

Context: `_update_spo2(int32_t ir_corr, int32_t red_corr)` is at line 1489. `_current_data.pi` is computed at line 1504. Right after that, at line 1509, the algorithm already checks `_spo2_dc_ir < _spo2_min_dc` for "no finger" — the exact condition for `DISCONNECTED`. The three-way detection reuses `_spo2_min_dc` (hardware disconnect threshold) and `_cfg.spo2_pi_sqi_lo` (low-PI threshold) — both already in the config.

- [ ] **Step 1: Insert `probe_state` detection after `pi` computation**

After line 1504 (`_current_data.pi = ...`) and before line 1506 (`_spo2_sample_count++`), insert:

```cpp
    // Probe state — reuses thresholds already used by the SpO2 algorithm.
    if (_spo2_dc_ir < _spo2_min_dc || _spo2_dc_red < _spo2_min_dc) {
        _current_data.probe_state = ProbeState::DISCONNECTED;
    } else if (_current_data.pi < _cfg.spo2_pi_sqi_lo) {
        _current_data.probe_state = ProbeState::NOT_APPLIED;
    } else {
        _current_data.probe_state = ProbeState::APPLIED;
    }
```

The resulting block:

```cpp
    // Perfusion Index (always updated, independent of SpO2 warmup)
    _current_data.pi = (_spo2_dc_ir > 1.0f) ? (sqrtf(_spo2_ac2_ir) / _spo2_dc_ir) * 100.0f : -1.0f;

    // Probe state — reuses thresholds already used by the SpO2 algorithm.
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

Expected: clean build.

- [ ] **Step 3: Note — no changes needed in `SPO2.cpp`**

`SPO2_Task` does `memcpy((void*)&g_spo2_data, &data, sizeof(data))` which copies the full struct including `probe_state`. No changes needed.

- [ ] **Step 4: Commit**

```bash
git add ".pio/libdeps/main/incunest_afe4490/incunest_afe4490.cpp"
git commit -m "feat(afe4490-lib): compute probe_state in _update_spo2 using DC and PI thresholds"
```

---

### Task 3: Add probe status protocol and gate SPO2 data in motherboard `CommTask.cpp`

**Files:**
- Modify: `src/CommTask.cpp` (motherboard, at `Firmware/motherBoard/src/CommTask.cpp`)

**Protocol:**
```
CTRL,PROBE,<N>\n
```
- N=0 (DISCONNECTED): sent every 2 s
- N=1 (NOT_APPLIED): sent every 2 s
- N=2 (APPLIED): sent **once** immediately on the DISCONNECTED/NOT_APPLIED → APPLIED transition; then PPG+VIT resume

**Changes in both UART (`#if HW_NUM >= 16`) and USB (`#else`) paths:**
1. Add `static ProbeState prev_probe_state` for transition detection
2. Add `static unsigned long last_probe_status_time` for the 2 s timer
3. PPG block: skip sending if `probe_state != APPLIED`
4. 1 s vitals block: if not APPLIED send probe status every 2 s; if APPLIED send VIT
5. Transition detection: at the top of the loop body, send `CTRL,PROBE,2` when state becomes APPLIED

#### 3a — UART path (inside `#if HW_NUM >= 16`, around lines 674–771)

- [ ] **Step 1: Add static variables near the existing `last_ppg_time` / `last_tel_time` declarations**

```cpp
static unsigned long last_ppg_time          = 0;
static unsigned long last_tel_time          = 0;
static unsigned long last_probe_status_time = 0;
static ProbeState    prev_probe_state       = ProbeState::DISCONNECTED;
```

- [ ] **Step 2: Add transition detection at the top of the `while(true)` loop body** (before the STATE request block, around line 675)

```cpp
    // Detect APPLIED transition — notify display immediately
    {
      ProbeState cur = g_spo2_data.probe_state;
      if (cur == ProbeState::APPLIED && prev_probe_state != ProbeState::APPLIED) {
        hmiSerial.print("CTRL,PROBE,2\n");
      }
      prev_probe_state = cur;
    }
```

- [ ] **Step 3: Wrap the PPG block with a `probe_state == APPLIED` guard**

Old PPG block (lines ~680–703):
```cpp
    // --- PPG waveform (25 Hz = every 40 ms) ---
    if (millis() - last_ppg_time >= 40) {
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
      last_ppg_time = millis();
    }
```

New (add outer guard):
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

- [ ] **Step 4: Replace the vitals section inside the 1 s block**

Old (lines ~741–768, inside `if (millis() - last_tel_time > 1000)`):
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

New:
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

#### 3b — USB CDC path (inside `#else`, around lines 876–951) — mirror of 3a

- [ ] **Step 5: Add static variables (same as Step 1)**

- [ ] **Step 6: Add transition detection at the top of the USB `while(true)` loop body**

```cpp
    // Detect APPLIED transition — notify display immediately
    {
      ProbeState cur = g_spo2_data.probe_state;
      if (cur == ProbeState::APPLIED && prev_probe_state != ProbeState::APPLIED) {
        CommunicationHost_Send("CTRL,PROBE,2\n");
      }
      prev_probe_state = cur;
    }
```

- [ ] **Step 7: Wrap USB PPG block** (same guard as Step 3, using `CommunicationHost_Send`)

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

- [ ] **Step 8: Replace USB vitals section** (same logic as Step 4, using `CommunicationHost_Send`)

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

- [ ] **Step 9: Build**

```
pio run -e main 2>&1 | head -40
```

Expected: clean build.

- [ ] **Step 10: Commit**

```bash
git add src/CommTask.cpp
git commit -m "feat(comm): gate SPO2 data on PROBE_APPLIED; send CTRL,PROBE status every 2s; notify on transition"
```

---

### Task 4: Add `ProbeContactState` and `ControlBoard_Message_Probe` to Display `CommTask.h`

**Files:**
- Modify: `Firmware/Display_HMI/include/CommTask.h`

Context: The file already has `SKIN_PROBE_*` defines (lines 103–110) for a different sensor (skin temperature probe). The new type is for the SpO2/AFE4490 probe and goes after the `ControlBoard_Message_VIT` struct (line 79) and before the `ControlBoard_Message_State` struct (line 81).

- [ ] **Step 1: Add `ProbeContactState` enum and `ControlBoard_Message_Probe` struct**

After line 79 (`} ControlBoard_Message_VIT;`) and before line 81 (`typedef struct {` for `ControlBoard_Message_State`), insert:

```c
// SpO2 probe contact state (CTRL,PROBE — sent every 2 s when not APPLIED, once on APPLIED)
// Values must match ProbeState enum in motherboard incunest_afe4490.h
typedef enum {
  SPO2_PROBE_DISCONNECTED = 0,  // No optical path (DC below threshold)
  SPO2_PROBE_NOT_APPLIED  = 1,  // Probe present but not on skin (PI too low)
  SPO2_PROBE_APPLIED      = 2   // Valid contact
} ProbeContactState;

typedef struct {
  ProbeContactState state;
  bool              updated;
} ControlBoard_Message_Probe;
```

- [ ] **Step 2: Add `extern` declaration** at the bottom of the globals section (after `extern ControlBoard_Message_VIT ctrl_vit_msg;`, line 135):

```c
extern ControlBoard_Message_Probe ctrl_probe_msg;
```

- [ ] **Step 3: Build check (display firmware)**

```
cd C:\Users\Pablo\Documents\IncuNest_dev\IncuNest\Firmware\Display_HMI
pio run 2>&1 | head -30
```

Expected: no errors.

- [ ] **Step 4: Commit**

```bash
git add Firmware/Display_HMI/include/CommTask.h
git commit -m "feat(display-comm): add ProbeContactState enum and ControlBoard_Message_Probe"
```

---

### Task 5: Add `CTRL,PROBE` parser to Display `CommTask.cpp`

**Files:**
- Modify: `Firmware/Display_HMI/src/CommTask.cpp`

Context: Global instances are declared at lines 12–19. The `CTRL,VIT` parser is at lines 184–190. The new `CTRL,PROBE` parser goes immediately after it, before the `CTRL,ALM` branch (line 191).

- [ ] **Step 1: Add global instance** at line ~18 (after `ctrl_vit_msg`):

```c
ControlBoard_Message_Probe ctrl_probe_msg = {SPO2_PROBE_DISCONNECTED, false};
```

- [ ] **Step 2: Add `CTRL,PROBE` parser in `parse_message()`** after the `CTRL,VIT` block (line 190):

```c
  } else if (strncmp(line, "CTRL,PROBE", 10) == 0) {
    int state = 0;
    if (sscanf(line, "CTRL,PROBE,%d", &state) == 1 &&
        state >= SPO2_PROBE_DISCONNECTED && state <= SPO2_PROBE_APPLIED) {
      ctrl_probe_msg.state   = (ProbeContactState)state;
      ctrl_probe_msg.updated = true;
    }
```

The full surrounding block becomes:
```c
  } else if (strncmp(line, "CTRL,VIT", 8) == 0) {
    int hr = 0, spo2 = 0;
    if (sscanf(line, "CTRL,VIT,%d,%d", &hr, &spo2) == 2) {
      ctrl_vit_msg.hr      = (uint8_t)hr;
      ctrl_vit_msg.spo2    = (uint8_t)spo2;
      ctrl_vit_msg.updated = true;
    }
  } else if (strncmp(line, "CTRL,PROBE", 10) == 0) {
    int state = 0;
    if (sscanf(line, "CTRL,PROBE,%d", &state) == 1 &&
        state >= SPO2_PROBE_DISCONNECTED && state <= SPO2_PROBE_APPLIED) {
      ctrl_probe_msg.state   = (ProbeContactState)state;
      ctrl_probe_msg.updated = true;
    }
  } else if (strncmp(line, "CTRL,ALM", strlen("CTRL,ALM")) == 0) {
```

- [ ] **Step 3: Build check**

```
cd C:\Users\Pablo\Documents\IncuNest_dev\IncuNest\Firmware\Display_HMI
pio run 2>&1 | head -30
```

Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add Firmware/Display_HMI/src/CommTask.cpp
git commit -m "feat(display-comm): parse CTRL,PROBE messages into ctrl_probe_msg"
```

---

### Task 6: Drive probe attachment from `CTRL,PROBE` in Display `UITask.cpp`

**Files:**
- Modify: `Firmware/Display_HMI/src/UITask.cpp`

Context: The lock screen PPG block is at lines 3582–3613. The current probe detection is variance-based: `spo2ProbeAttached = (ppg_disp_max - ppg_disp_min) > 20.0f` (line 3593). This heuristic is replaced with the authoritative `ctrl_probe_msg`. The HR block is at lines 3615–3632 and reads `spo2ProbeAttached` — it does not need to change.

Static variables at lines 104–107:
```c
static float ppg_disp_min = 128.0f;
static float ppg_disp_max = 128.0f;
static bool spo2ProbeAttached = false;
static bool spo2ProbeAttachedPrev = false;
```

- [ ] **Step 1: Add a new `CTRL,PROBE` handler block before the PPG block (before line 3582)**

Insert:
```cpp
    // --- Lock screen: probe contact state from CTRL,PROBE ---
    if (ctrl_probe_msg.updated) {
      ctrl_probe_msg.updated = false;
      bool applied = (ctrl_probe_msg.state == SPO2_PROBE_APPLIED);
      // Falling edge: probe removed — hide chart and HR, reset normalisation
      if (!applied && spo2ProbeAttachedPrev) {
        ppg_disp_min = 128.0f;
        ppg_disp_max = 128.0f;
        if (ui_LockPPGChart)
          lv_obj_add_flag(ui_LockPPGChart, LV_OBJ_FLAG_HIDDEN);
        if (ui_LockHRCont)
          lv_obj_add_flag(ui_LockHRCont, LV_OBJ_FLAG_HIDDEN);
      }
      // Rising edge: probe applied — show chart, reset normalisation
      if (applied && !spo2ProbeAttachedPrev) {
        ppg_disp_min = 128.0f;
        ppg_disp_max = 128.0f;
        if (ui_LockPPGChart)
          lv_obj_clear_flag(ui_LockPPGChart, LV_OBJ_FLAG_HIDDEN);
      }
      spo2ProbeAttached     = applied;
      spo2ProbeAttachedPrev = applied;
    }
```

- [ ] **Step 2: Remove the variance-based probe detection from the PPG block**

Old PPG block (lines 3582–3613):
```cpp
    if (ui_LockPPGChart && lockPPGSeries && ctrl_ppg_msg.updated) {
      float ppg_val = (float)ctrl_ppg_msg.ppg;
      ctrl_ppg_msg.updated = false;
      if (ppg_val < ppg_disp_min)
        ppg_disp_min = ppg_val;
      else
        ppg_disp_min += (128.0f - ppg_disp_min) * 0.05f;
      if (ppg_val > ppg_disp_max)
        ppg_disp_max = ppg_val;
      else
        ppg_disp_max += (128.0f - ppg_disp_max) * 0.05f;
      spo2ProbeAttached = (ppg_disp_max - ppg_disp_min) > 20.0f;

      // On falling edge: hide chart and HR, reset normalisation
      if (!spo2ProbeAttached && spo2ProbeAttachedPrev) {
        ppg_disp_min = 128.0f;
        ppg_disp_max = 128.0f;
        lv_obj_add_flag(ui_LockPPGChart, LV_OBJ_FLAG_HIDDEN);
        if (ui_LockHRCont)
          lv_obj_add_flag(ui_LockHRCont, LV_OBJ_FLAG_HIDDEN);
      }
      // On rising edge: show chart
      if (spo2ProbeAttached && !spo2ProbeAttachedPrev) {
        lv_obj_clear_flag(ui_LockPPGChart, LV_OBJ_FLAG_HIDDEN);
      }
      spo2ProbeAttachedPrev = spo2ProbeAttached;

      if (locked && spo2ProbeAttached) {
        lv_chart_set_next_value(ui_LockPPGChart, lockPPGSeries,
                                (lv_coord_t)ppg_val);
      }
    }
```

New PPG block (retain min/max tracking for normalisation; remove variance detection and edge handling — those now live in the `CTRL,PROBE` block above):
```cpp
    if (ui_LockPPGChart && lockPPGSeries && ctrl_ppg_msg.updated) {
      float ppg_val = (float)ctrl_ppg_msg.ppg;
      ctrl_ppg_msg.updated = false;
      if (ppg_val < ppg_disp_min)
        ppg_disp_min = ppg_val;
      else
        ppg_disp_min += (128.0f - ppg_disp_min) * 0.05f;
      if (ppg_val > ppg_disp_max)
        ppg_disp_max = ppg_val;
      else
        ppg_disp_max += (128.0f - ppg_disp_max) * 0.05f;

      if (locked && spo2ProbeAttached) {
        lv_chart_set_next_value(ui_LockPPGChart, lockPPGSeries,
                                (lv_coord_t)ppg_val);
      }
    }
```

- [ ] **Step 3: Build check**

```
cd C:\Users\Pablo\Documents\IncuNest_dev\IncuNest\Firmware\Display_HMI
pio run 2>&1 | head -40
```

Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add Firmware/Display_HMI/src/UITask.cpp
git commit -m "feat(display-ui): drive spo2ProbeAttached from CTRL,PROBE messages, remove variance heuristic"
```

---

## Protocol summary (final)

| Message | When sent | Meaning |
|---------|-----------|---------|
| `CTRL,PROBE,0\n` | Every 2 s (from 1 s telemetry tick) | Probe disconnected — no optical path |
| `CTRL,PROBE,1\n` | Every 2 s (from 1 s telemetry tick) | Probe present, not on patient skin |
| `CTRL,PROBE,2\n` | Once, on DISCONNECTED/NOT_APPLIED → APPLIED transition | Probe contact established |
| `CTRL,PPG,<byte>\n` | 25 Hz | Only when `probe_state == APPLIED` |
| `CTRL,VIT,<hr>,0\n` | 1 Hz | Only when `probe_state == APPLIED` |
