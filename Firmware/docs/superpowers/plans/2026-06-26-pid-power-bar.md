# PID Power Bar Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add three vertical `lv_bar` widgets to the HMI main screen showing PID duty cycle (0–100%) for air temperature, skin temperature, and humidity controls, fed by a new `CTRL,DUTY` serial command from the motherboard.

**Architecture:** The motherboard computes duty % from its PID outputs every 1 s and appends `CTRL,DUTY,<temp_pct>,<hum_pct>` after the existing `CTRL,TEL` send. The HMI CommTask parses it into volatile globals; UITask consumes a pending flag and calls `UI_UpdatePowerBars()`. Three `lv_bar_t` widgets (hidden by default) are created at init time and shown/hidden by `Switch_cb`, `AirPanel_cb`, `SkinPanel_cb`, and `skin_mode_force_off()`.

**Tech Stack:** Arduino/ESP-IDF + LVGL 8.3.11 on ESP32-S3 (HMI); Arduino/ESP-IDF on ESP32-S3 (motherboard); FreeRTOS; ASCII newline-terminated serial protocol.

## Global Constraints

- No magic numbers — every numeric literal must be a named `#define` in its appropriate header file.
- Duty % range for temperature: 0–100 → `DUTY_TEMP_PCT_MAX 100` in `motherBoard/include/tasks/PID.h`.
- Duty % range for humidity: 0–95 → reuse `HUMIDIFIER_DUTY_CYCLE_MAX` (95) and `HUMIDIFIER_DUTY_CYCLE_MIN` (0) from `motherBoard/include/main.h`.
- Message buffer size → `DUTY_MSG_BUF_SIZE 24` in `motherBoard/include/protocol/CommTask.h`.
- HMI bar range max → `POWER_BAR_PCT_MAX 100` in `Display_HMI/include/config/display_config.h`.
- Bar color: orange `0xFF8C00` → `COLOR_POWER_BAR` in `Display_HMI/include/config/display_config.h`.
- Bar dimensions: width 8, height 30, x-offset 10 → `POWER_BAR_WIDTH`, `POWER_BAR_HEIGHT`, `POWER_BAR_X_OFFSET` in `Display_HMI/include/config/display_config.h`.
- Both UART (HW_NUM >= 16) and USB-CDC (HW_NUM < 16) paths in motherboard CommTask must send `CTRL,DUTY`.
- `skin_mode_force_off()` must also swap power bar visibility when it switches from SKIN to AIR panel while temp is ON.

---

## File Map

| File | Change |
|------|--------|
| `motherBoard/include/tasks/PID.h` | Add `DUTY_TEMP_PCT_MAX 100` |
| `motherBoard/include/protocol/CommTask.h` | Add `DUTY_MSG_BUF_SIZE 24` |
| `motherBoard/src/tasks/CommTask.cpp` | Add `#include "tasks/PID.h"` + duty send in both UART and USB-CDC paths |
| `Display_HMI/include/config/display_config.h` | Add 5 power bar defines |
| `Display_HMI/include/tasks/CommTask.h` | Add 3 extern globals |
| `Display_HMI/src/tasks/CommTask.cpp` | Add 3 global definitions + `CTRL,DUTY` parser branch |
| `Display_HMI/include/ui/ElementsCreation.h` | Add 3 extern widget declarations |
| `Display_HMI/src/ui/ElementsCreation.cpp` | Add 3 widget definitions + creation code |
| `Display_HMI/include/tasks/UITask.h` | Add `UI_UpdatePowerBars()` declaration |
| `Display_HMI/src/tasks/UITask.cpp` | Add function body + pending flag consumption + show/hide hooks in 5 locations |

---

### Task 1: Motherboard — Protocol constants and CTRL,DUTY send

**Files:**
- Modify: `motherBoard/include/tasks/PID.h`
- Modify: `motherBoard/include/protocol/CommTask.h`
- Modify: `motherBoard/src/tasks/CommTask.cpp`

**Interfaces:**
- Produces: `CTRL,DUTY,<temp_pct>,<hum_pct>\n` sent every 1 s on both UART and USB-CDC paths.

- [ ] **Step 1: Add `DUTY_TEMP_PCT_MAX` to PID.h**

  In `motherBoard/include/tasks/PID.h`, after the existing `#define AWO_HUMIDITY 5` at line 27, add:

  ```cpp
  #define DUTY_TEMP_PCT_MAX 100
  ```

- [ ] **Step 2: Add `DUTY_MSG_BUF_SIZE` to motherboard CommTask.h**

  In `motherBoard/include/protocol/CommTask.h`, after the existing `SKIN_PROBE_VALID` defines block (around line 17), add:

  ```cpp
  #define DUTY_MSG_BUF_SIZE 24
  ```

- [ ] **Step 3: Add PID include to motherboard CommTask.cpp**

  In `motherBoard/src/tasks/CommTask.cpp`, after line 2 (`#include "main.h"`), add:

  ```cpp
  #include "tasks/PID.h"
  ```

- [ ] **Step 4: Add duty send to the UART path (HW_NUM >= 16)**

  In `motherBoard/src/tasks/CommTask.cpp`, in the `#if HW_NUM >= 16` UART path, the `if (millis() - last_tel_time > 1000)` block sends `CTRL,TEL` at lines 757–763 with `hmiSerial.print(msg)`. After the `hmiSerial.print(msg);` line (line 763) and before the `if (g_spo2_data.probe_state` block (line 765), insert:

  ```cpp
      int temp_duty = (in3.heaterSafeMAXPWM > 0)
          ? (int)(HeaterPIDOutput / in3.heaterSafeMAXPWM * DUTY_TEMP_PCT_MAX)
          : 0;
      if (temp_duty < 0)                  temp_duty = 0;
      if (temp_duty > DUTY_TEMP_PCT_MAX)  temp_duty = DUTY_TEMP_PCT_MAX;

      int hum_duty = (humidifierTimeCycle > 0)
          ? (int)(humidityControlPIDOutput / humidifierTimeCycle * DUTY_TEMP_PCT_MAX)
          : 0;
      if (hum_duty < HUMIDIFIER_DUTY_CYCLE_MIN) hum_duty = HUMIDIFIER_DUTY_CYCLE_MIN;
      if (hum_duty > HUMIDIFIER_DUTY_CYCLE_MAX) hum_duty = HUMIDIFIER_DUTY_CYCLE_MAX;

      char duty_msg[DUTY_MSG_BUF_SIZE];
      snprintf(duty_msg, sizeof(duty_msg), "CTRL,DUTY,%d,%d\n", temp_duty, hum_duty);
      hmiSerial.print(duty_msg);
  ```

  Note: `HeaterPIDOutput`, `humidityControlPIDOutput`, and `humidifierTimeCycle` are declared in `PID.cpp` and accessible via the `PID.h` include added in Step 3. `in3` is already `extern IncuNest_parameters in3` at line 27 of CommTask.cpp.

- [ ] **Step 5: Add duty send to the USB-CDC path (HW_NUM < 16)**

  In the same file, in the `#else` USB-CDC path, the `if (millis() - last_tel_time > 1000)` block sends `CTRL,TEL` at lines 967–973 with `CommunicationHost_Send(msg)`. After the `CommunicationHost_Send(msg);` line (line 973) and before the `if (g_spo2_data.probe_state` block (line 975), insert:

  ```cpp
        int temp_duty = (in3.heaterSafeMAXPWM > 0)
            ? (int)(HeaterPIDOutput / in3.heaterSafeMAXPWM * DUTY_TEMP_PCT_MAX)
            : 0;
        if (temp_duty < 0)                  temp_duty = 0;
        if (temp_duty > DUTY_TEMP_PCT_MAX)  temp_duty = DUTY_TEMP_PCT_MAX;

        int hum_duty = (humidifierTimeCycle > 0)
            ? (int)(humidityControlPIDOutput / humidifierTimeCycle * DUTY_TEMP_PCT_MAX)
            : 0;
        if (hum_duty < HUMIDIFIER_DUTY_CYCLE_MIN) hum_duty = HUMIDIFIER_DUTY_CYCLE_MIN;
        if (hum_duty > HUMIDIFIER_DUTY_CYCLE_MAX) hum_duty = HUMIDIFIER_DUTY_CYCLE_MAX;

        char duty_msg[DUTY_MSG_BUF_SIZE];
        snprintf(duty_msg, sizeof(duty_msg), "CTRL,DUTY,%d,%d\n", temp_duty, hum_duty);
        CommunicationHost_Send(duty_msg);
  ```

- [ ] **Step 6: Build motherboard firmware**

  ```
  cd motherBoard && pio run -e IncuNest_V17
  ```

  Expected: PASS with no errors. Verify `CTRL,DUTY` appears in output of `pio device monitor` every ~1 s alongside `CTRL,TEL`.

- [ ] **Step 7: Commit**

  ```bash
  git add motherBoard/include/tasks/PID.h
  git add motherBoard/include/protocol/CommTask.h
  git add motherBoard/src/tasks/CommTask.cpp
  git commit -m "feat(mb): add CTRL,DUTY periodic send with heater/humidity duty cycle"
  ```

---

### Task 2: HMI — CTRL,DUTY parser and globals

**Files:**
- Modify: `Display_HMI/include/config/display_config.h`
- Modify: `Display_HMI/include/tasks/CommTask.h`
- Modify: `Display_HMI/src/tasks/CommTask.cpp`

**Interfaces:**
- Consumes: `CTRL,DUTY,<int>,<int>\n` from the serial stream.
- Produces: `g_tempDutyPct`, `g_humDutyPct`, `g_pendingDutyApply` for UITask to consume.

- [ ] **Step 1: Add power bar constants to display_config.h**

  In `Display_HMI/include/config/display_config.h`, after the `TOUCH_EXT_NARROW` define at the end of the file, add:

  ```cpp
  // -----------------------------------------------------------------------------
  // Power bar widget constants (PID duty cycle indicator)
  // -----------------------------------------------------------------------------
  #define COLOR_POWER_BAR     lv_color_hex(0xFF8C00)
  #define POWER_BAR_WIDTH     8
  #define POWER_BAR_HEIGHT    30
  #define POWER_BAR_X_OFFSET  10
  #define POWER_BAR_PCT_MAX   100
  ```

- [ ] **Step 2: Add extern globals to HMI CommTask.h**

  In `Display_HMI/include/tasks/CommTask.h`, after `extern volatile bool g_pendingTelemetryApply;` (line 132), add:

  ```cpp
  extern volatile int  g_tempDutyPct;
  extern volatile int  g_humDutyPct;
  extern volatile bool g_pendingDutyApply;
  ```

- [ ] **Step 3: Add global definitions to HMI CommTask.cpp**

  In `Display_HMI/src/tasks/CommTask.cpp`, after the line `volatile bool g_pendingTelemetryApply = false;` (line 28), add:

  ```cpp
  volatile int  g_tempDutyPct     = 0;
  volatile int  g_humDutyPct      = 0;
  volatile bool g_pendingDutyApply = false;
  ```

- [ ] **Step 4: Add CTRL,DUTY parser branch to parse_message()**

  In `Display_HMI/src/tasks/CommTask.cpp`, the `parse_message()` function ends its chain with the `CTRL,WIFI` `else if` at line 236 followed by its closing `}` at line 244, then `#endif` at line 245. After the `CTRL,WIFI` block closing brace (line 243) and before the `#endif` (line 245), insert a new `else if` branch:

  ```cpp
  } else if (strncmp(line, "CTRL,DUTY", 9) == 0) {
    int t = 0, h = 0;
    if (sscanf(line, "CTRL,DUTY,%d,%d", &t, &h) == 2) {
      g_tempDutyPct     = t;
      g_humDutyPct      = h;
      g_pendingDutyApply = true;
    }
  ```

  (The existing outer closing `}` of the CTRL,WIFI block becomes the closing brace for CTRL,WIFI, and a new `}` closes the new CTRL,DUTY branch. The chain then hits `#endif`.)

- [ ] **Step 5: Build HMI firmware**

  ```
  cd Display_HMI && pio run -e main
  ```

  Expected: PASS with no errors.

- [ ] **Step 6: Commit**

  ```bash
  git add Display_HMI/include/config/display_config.h
  git add Display_HMI/include/tasks/CommTask.h
  git add Display_HMI/src/tasks/CommTask.cpp
  git commit -m "feat(hmi): add CTRL,DUTY parser and power bar display constants"
  ```

---

### Task 3: HMI — Power bar widget creation

**Files:**
- Modify: `Display_HMI/include/ui/ElementsCreation.h`
- Modify: `Display_HMI/src/ui/ElementsCreation.cpp`

**Interfaces:**
- Consumes: `POWER_BAR_WIDTH`, `POWER_BAR_HEIGHT`, `POWER_BAR_X_OFFSET`, `POWER_BAR_PCT_MAX`, `COLOR_POWER_BAR` from `display_config.h`.
- Produces: `ui_AirPowerBar`, `ui_SkinPowerBar`, `ui_HumPowerBar` — hidden `lv_bar_t` widgets ready for use by UITask.

- [ ] **Step 1: Add extern declarations to ElementsCreation.h**

  In `Display_HMI/include/ui/ElementsCreation.h`, after `extern lv_obj_t * ui_HumDesired;` (line 173), add:

  ```cpp
  // PID power bar widgets (duty cycle indicator, hidden until control ON)
  extern lv_obj_t * ui_AirPowerBar;
  extern lv_obj_t * ui_SkinPowerBar;
  extern lv_obj_t * ui_HumPowerBar;
  ```

- [ ] **Step 2: Add widget definitions to ElementsCreation.cpp**

  In `Display_HMI/src/ui/ElementsCreation.cpp`, after `lv_obj_t *ui_HumDesired = NULL;` (line 84), add:

  ```cpp
  lv_obj_t *ui_AirPowerBar  = NULL;
  lv_obj_t *ui_SkinPowerBar = NULL;
  lv_obj_t *ui_HumPowerBar  = NULL;
  ```

- [ ] **Step 3: Create ui_AirPowerBar in ui_ScreenMain_screen_init**

  In `Display_HMI/src/ui/ElementsCreation.cpp`, after the `ui_TempAirDesired` creation block ends (after line 992, `LV_PART_MAIN | LV_STATE_DEFAULT);`), and before the existing `ui_AirTempBar` creation block (line 994), insert:

  ```cpp
  ui_AirPowerBar = lv_bar_create(ui_AirTempBarCont);
  lv_bar_set_range(ui_AirPowerBar, 0, POWER_BAR_PCT_MAX);
  lv_bar_set_value(ui_AirPowerBar, 0, LV_ANIM_OFF);
  lv_obj_set_width(ui_AirPowerBar, POWER_BAR_WIDTH);
  lv_obj_set_height(ui_AirPowerBar, POWER_BAR_HEIGHT);
  lv_obj_align_to(ui_AirPowerBar, ui_TempAirDesired,
                  LV_ALIGN_OUT_RIGHT_MID, POWER_BAR_X_OFFSET, 0);
  lv_obj_set_style_bg_color(ui_AirPowerBar, lv_color_hex(0x404040),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_AirPowerBar, LV_OPA_COVER,
                          LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(ui_AirPowerBar, COLOR_POWER_BAR,
                            LV_PART_INDICATOR | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_AirPowerBar, LV_OPA_COVER,
                          LV_PART_INDICATOR | LV_STATE_DEFAULT);
  lv_obj_add_flag(ui_AirPowerBar, LV_OBJ_FLAG_HIDDEN);
  ```

- [ ] **Step 4: Create ui_SkinPowerBar in ui_ScreenMain_screen_init**

  After the `ui_TempSkinDesired` creation block ends (after line 1168, `LV_PART_MAIN | LV_STATE_DEFAULT);`), and before the `ui_Label6` creation (line 1170), insert:

  ```cpp
  ui_SkinPowerBar = lv_bar_create(ui_SkinTempBarCont);
  lv_bar_set_range(ui_SkinPowerBar, 0, POWER_BAR_PCT_MAX);
  lv_bar_set_value(ui_SkinPowerBar, 0, LV_ANIM_OFF);
  lv_obj_set_width(ui_SkinPowerBar, POWER_BAR_WIDTH);
  lv_obj_set_height(ui_SkinPowerBar, POWER_BAR_HEIGHT);
  lv_obj_align_to(ui_SkinPowerBar, ui_TempSkinDesired,
                  LV_ALIGN_OUT_RIGHT_MID, POWER_BAR_X_OFFSET, 0);
  lv_obj_set_style_bg_color(ui_SkinPowerBar, lv_color_hex(0x404040),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_SkinPowerBar, LV_OPA_COVER,
                          LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(ui_SkinPowerBar, COLOR_POWER_BAR,
                            LV_PART_INDICATOR | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_SkinPowerBar, LV_OPA_COVER,
                          LV_PART_INDICATOR | LV_STATE_DEFAULT);
  lv_obj_add_flag(ui_SkinPowerBar, LV_OBJ_FLAG_HIDDEN);
  ```

- [ ] **Step 5: Create ui_HumPowerBar in ui_ScreenMain_screen_init**

  After the `ui_HumDesired` creation block ends (after line 1390, `LV_PART_MAIN | LV_STATE_DEFAULT);`), and before the `ui_ImgArrowUpHum` creation (line 1392), insert:

  ```cpp
  ui_HumPowerBar = lv_bar_create(ui_HumPanelCont);
  lv_bar_set_range(ui_HumPowerBar, 0, POWER_BAR_PCT_MAX);
  lv_bar_set_value(ui_HumPowerBar, 0, LV_ANIM_OFF);
  lv_obj_set_width(ui_HumPowerBar, POWER_BAR_WIDTH);
  lv_obj_set_height(ui_HumPowerBar, POWER_BAR_HEIGHT);
  lv_obj_align_to(ui_HumPowerBar, ui_HumDesired,
                  LV_ALIGN_OUT_RIGHT_MID, POWER_BAR_X_OFFSET, 0);
  lv_obj_set_style_bg_color(ui_HumPowerBar, lv_color_hex(0x404040),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_HumPowerBar, LV_OPA_COVER,
                          LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(ui_HumPowerBar, COLOR_POWER_BAR,
                            LV_PART_INDICATOR | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_HumPowerBar, LV_OPA_COVER,
                          LV_PART_INDICATOR | LV_STATE_DEFAULT);
  lv_obj_add_flag(ui_HumPowerBar, LV_OBJ_FLAG_HIDDEN);
  ```

- [ ] **Step 6: Build HMI firmware**

  ```
  cd Display_HMI && pio run -e main
  ```

  Expected: PASS with no errors.

- [ ] **Step 7: Commit**

  ```bash
  git add Display_HMI/include/ui/ElementsCreation.h
  git add Display_HMI/src/ui/ElementsCreation.cpp
  git commit -m "feat(hmi): create AirPowerBar, SkinPowerBar, HumPowerBar widgets (hidden)"
  ```

---

### Task 4: HMI — Update logic, pending flag, show/hide hooks

**Files:**
- Modify: `Display_HMI/include/tasks/UITask.h`
- Modify: `Display_HMI/src/tasks/UITask.cpp`

**Interfaces:**
- Consumes: `g_tempDutyPct`, `g_humDutyPct`, `g_pendingDutyApply` (from Task 2), `ui_AirPowerBar`, `ui_SkinPowerBar`, `ui_HumPowerBar` (from Task 3).
- Produces: bars animate when controls are on; hide completely when controls turn off.

- [ ] **Step 1: Add UI_UpdatePowerBars declaration to UITask.h**

  In `Display_HMI/include/tasks/UITask.h`, after `void temp_content_set_visible(bool visible);` (line 36), add:

  ```cpp
  void UI_UpdatePowerBars(int tempPct, int humPct);
  ```

- [ ] **Step 2: Add UI_UpdatePowerBars function body to UITask.cpp**

  In `Display_HMI/src/tasks/UITask.cpp`, just before the `skin_mode_force_off()` function (line 305), insert:

  ```cpp
  void UI_UpdatePowerBars(int tempPct, int humPct) {
    if (ui_AirPowerBar && !lv_obj_has_flag(ui_AirPowerBar, LV_OBJ_FLAG_HIDDEN))
      lv_bar_set_value(ui_AirPowerBar, tempPct, LV_ANIM_ON);
    if (ui_SkinPowerBar && !lv_obj_has_flag(ui_SkinPowerBar, LV_OBJ_FLAG_HIDDEN))
      lv_bar_set_value(ui_SkinPowerBar, tempPct, LV_ANIM_ON);
    if (ui_HumPowerBar && !lv_obj_has_flag(ui_HumPowerBar, LV_OBJ_FLAG_HIDDEN))
      lv_bar_set_value(ui_HumPowerBar, humPct, LV_ANIM_ON);
  }
  ```

- [ ] **Step 3: Add show/hide to skin_mode_force_off()**

  In `skin_mode_force_off()`, inside the `if (selectedPanel == SKIN_PANEL_SELECTED)` block, the `if (tempSwitched)` section is at lines 339–342:

  ```cpp
      if (tempSwitched) {
        hmi_msg.controlMode = CONTROL_AIR;
        temp_chart_show_for_selected_panel();
      }
  ```

  Change it to:

  ```cpp
      if (tempSwitched) {
        hmi_msg.controlMode = CONTROL_AIR;
        temp_chart_show_for_selected_panel();
        lv_obj_add_flag(ui_SkinPowerBar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_AirPowerBar, LV_OBJ_FLAG_HIDDEN);
      }
  ```

- [ ] **Step 4: Add show/hide to AirPanel_cb()**

  In `AirPanel_cb()` (line 1666), after:
  ```cpp
    lv_obj_add_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);
  ```
  (line 1675), add:

  ```cpp
    lv_obj_clear_flag(ui_AirPowerBar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_SkinPowerBar, LV_OBJ_FLAG_HIDDEN);
  ```

- [ ] **Step 5: Add show/hide to SkinPanel_cb()**

  In `SkinPanel_cb()` (line 1692), after:
  ```cpp
    lv_obj_add_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);
  ```
  (line 1703), add:

  ```cpp
    lv_obj_add_flag(ui_AirPowerBar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_SkinPowerBar, LV_OBJ_FLAG_HIDDEN);
  ```

- [ ] **Step 6: Add show/hide to Switch_cb() — temperature branch**

  In `Switch_cb()`, inside the `if (obj == ui_Switch1)` temperature branch:

  a) In the **temp ON** path (`if (checked)`), the panel-selection block at lines 1832–1844 sets `selectedPanel`. After the last `else` closes (line 1844, `hmi_msg.controlMode = CONTROL_AIR;`) and before `temp_chart_show_for_selected_panel();` (line 1846), insert:

  ```cpp
      if (selectedPanel == AIR_PANEL_SELECTED) {
        lv_obj_clear_flag(ui_AirPowerBar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_SkinPowerBar, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(ui_AirPowerBar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_SkinPowerBar, LV_OBJ_FLAG_HIDDEN);
      }
  ```

  b) In the **temp OFF** path (`else`, lines 1856–1888), after the `lv_obj_set_style_bg_color(ui_SkinPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);` block (line 1885) and before the closing `}` of the else (line 1888), insert:

  ```cpp
      lv_obj_add_flag(ui_AirPowerBar, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_SkinPowerBar, LV_OBJ_FLAG_HIDDEN);
  ```

- [ ] **Step 7: Add show/hide to Switch_cb() — humidity branch**

  In `Switch_cb()`, inside the `if (obj == ui_Switch2)` humidity branch:

  a) In the **hum ON** path (`if (checked)`), after `lv_obj_clear_flag(ui_HumPanelCont, LV_OBJ_FLAG_HIDDEN);` (line 1897), add:

  ```cpp
      lv_obj_clear_flag(ui_HumPowerBar, LV_OBJ_FLAG_HIDDEN);
  ```

  b) In the **hum OFF** path (`else`), after `lv_obj_add_flag(ui_HumPanelCont, LV_OBJ_FLAG_HIDDEN);` (line 1916), add:

  ```cpp
      lv_obj_add_flag(ui_HumPowerBar, LV_OBJ_FLAG_HIDDEN);
  ```

- [ ] **Step 8: Add pending flag consumption to UITask loop**

  In the UITask loop, after the `g_pendingTelemetryApply` block closes (after line 3970), and before `if (g_pendingAlarmUpdate)` (line 3972), insert:

  ```cpp
    if (g_pendingDutyApply) {
      g_pendingDutyApply = false;
      UI_UpdatePowerBars(g_tempDutyPct, g_humDutyPct);
    }
  ```

  (This executes inside the `LVGL_Lock()` context that wraps the whole loop body — no additional locking needed.)

- [ ] **Step 9: Build HMI firmware**

  ```
  cd Display_HMI && pio run -e main
  ```

  Expected: PASS with no errors.

- [ ] **Step 10: Commit**

  ```bash
  git add Display_HMI/include/tasks/UITask.h
  git add Display_HMI/src/tasks/UITask.cpp
  git commit -m "feat(hmi): animate power bars on CTRL,DUTY and show/hide on control state changes"
  ```
