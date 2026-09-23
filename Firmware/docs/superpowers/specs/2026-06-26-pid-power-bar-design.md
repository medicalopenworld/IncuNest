# PID Power Bar — Design Spec

**Date:** 2026-06-26  
**Status:** Approved

## Overview

Add a vertical "power bar" indicator next to each desired-value label on the main screen (air temperature, skin temperature, humidity). The bar represents the duty cycle (0–100%) of the PID that is currently controlling that actuator, received via a new `CTRL,DUTY` serial command from the motherboard.

---

## Section 1 — Communication Protocol

**New message (motherboard → HMI):**
```
CTRL,DUTY,<temp_pct>,<hum_pct>\n
```

| Field | Type | Range | Description |
|---|---|---|---|
| `temp_pct` | `int` | 0–100 | `HeaterPIDOutput / in3.heaterSafeMAXPWM * 100`. 0 when PID is MANUAL. |
| `hum_pct` | `int` | 0–95 | `humidityControlPIDOutput / humidifierTimeCycle * 100`. 0 when PID is MANUAL. |

- Sent every 1 s in the same periodic block as `CTRL,TEL` (both UART and USB-CDC paths).
- Max message length: `CTRL,DUTY,100,95\n` = 16 chars — well within the 256-char RX buffer.
- Backward compatible: HMI ignores the message if `sscanf` returns < 2.

---

## Section 2 — Motherboard Changes

**File:** `motherBoard/src/tasks/CommTask.cpp`

New defines:
- `DUTY_TEMP_PCT_MAX 100` → `motherBoard/include/tasks/PID.h`
- `DUTY_MSG_BUF_SIZE 24` → `motherBoard/include/tasks/CommTask.h`

Reused defines (already in `motherBoard/include/main.h`):
- `HUMIDIFIER_DUTY_CYCLE_MAX` (95)
- `HUMIDIFIER_DUTY_CYCLE_MIN` (0)

Calculation logic (added inside the `millis() - last_tel_time > 1000` block, after the TEL send, in both UART and USB-CDC paths):

```cpp
extern double HeaterPIDOutput;
int temp_duty = (in3.heaterSafeMAXPWM > 0)
    ? (int)(HeaterPIDOutput / in3.heaterSafeMAXPWM * DUTY_TEMP_PCT_MAX)
    : 0;
if (temp_duty < 0)                 temp_duty = 0;
if (temp_duty > DUTY_TEMP_PCT_MAX) temp_duty = DUTY_TEMP_PCT_MAX;

extern double humidityControlPIDOutput;
extern int humidifierTimeCycle;
int hum_duty = (humidifierTimeCycle > 0)
    ? (int)(humidityControlPIDOutput / humidifierTimeCycle * DUTY_TEMP_PCT_MAX)
    : 0;
if (hum_duty < HUMIDIFIER_DUTY_CYCLE_MIN) hum_duty = HUMIDIFIER_DUTY_CYCLE_MIN;
if (hum_duty > HUMIDIFIER_DUTY_CYCLE_MAX) hum_duty = HUMIDIFIER_DUTY_CYCLE_MAX;

char duty_msg[DUTY_MSG_BUF_SIZE];
snprintf(duty_msg, sizeof(duty_msg), "CTRL,DUTY,%d,%d\n", temp_duty, hum_duty);
// UART path:    hmiSerial.print(duty_msg);
// USB-CDC path: CommunicationHost_Send(duty_msg);
```

---

## Section 3 — HMI Parser & Globals

**File:** `Display_HMI/src/tasks/CommTask.cpp`

New globals:
```cpp
volatile int  g_tempDutyPct    = 0;
volatile int  g_humDutyPct     = 0;
volatile bool g_pendingDutyApply = false;
```
Declared as `extern` in `Display_HMI/include/tasks/CommTask.h`.

New parser branch in `parse_message()` (same level as `CTRL,TEL`, `CTRL,VIT`, etc.):
```cpp
} else if (strncmp(line, "CTRL,DUTY", 9) == 0) {
    int t = 0, h = 0;
    if (sscanf(line, "CTRL,DUTY,%d,%d", &t, &h) == 2) {
        g_tempDutyPct    = t;
        g_humDutyPct     = h;
        g_pendingDutyApply = true;
    }
}
```

---

## Section 4 — HMI Widget Creation

**File:** `Display_HMI/src/ui/ElementsCreation.cpp`

New global widget objects (declared at top of file and in header):
```cpp
lv_obj_t *ui_AirPowerBar  = NULL;
lv_obj_t *ui_SkinPowerBar = NULL;
lv_obj_t *ui_HumPowerBar  = NULL;
```

New defines in `Display_HMI/include/config/display_config.h`:
```cpp
#define COLOR_POWER_BAR      lv_color_hex(0xFF8C00)
#define POWER_BAR_WIDTH      8
#define POWER_BAR_HEIGHT     30
#define POWER_BAR_X_OFFSET   10
```

Widget creation pattern (identical for all three bars, called in `ui_ScreenMain_screen_init()`):
```cpp
// Shift desired label slightly left to make room
lv_obj_set_x(ui_TempAirDesired, -8);

ui_AirPowerBar = lv_bar_create(ui_AirTempBarCont);
lv_bar_set_range(ui_AirPowerBar, 0, DUTY_TEMP_PCT_MAX);
lv_bar_set_value(ui_AirPowerBar, 0, LV_ANIM_OFF);
lv_obj_set_width(ui_AirPowerBar, POWER_BAR_WIDTH);
lv_obj_set_height(ui_AirPowerBar, POWER_BAR_HEIGHT);
lv_obj_align_to(ui_AirPowerBar, ui_TempAirDesired,
                LV_ALIGN_OUT_RIGHT_MID, POWER_BAR_X_OFFSET, 0);
lv_obj_set_style_bg_color(ui_AirPowerBar, lv_color_hex(0x404040),
                           LV_PART_MAIN | LV_STATE_DEFAULT);
lv_obj_set_style_bg_color(ui_AirPowerBar, COLOR_POWER_BAR,
                           LV_PART_INDICATOR | LV_STATE_DEFAULT);
lv_obj_add_flag(ui_AirPowerBar, LV_OBJ_FLAG_HIDDEN);
```

Same pattern applied to `ui_SkinPowerBar` (parent: `ui_SkinTempBarCont`, label: `ui_TempSkinDesired`) and `ui_HumPowerBar` (parent: `ui_HumPanelCont`, label: `ui_HumDesired`).

---

## Section 5 — HMI Update Logic

**File:** `Display_HMI/src/tasks/UITask.cpp`

### Pending flag consumption (UITask loop):
```cpp
if (g_pendingDutyApply) {
    g_pendingDutyApply = false;
    LVGL_Lock();
    UI_UpdatePowerBars(g_tempDutyPct, g_humDutyPct);
    LVGL_Unlock();
}
```

### `UI_UpdatePowerBars()` function:
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

### Show/hide hooks (within existing `Switch_cb()` and `Display_ApplyCtrlState()`):

| Event | Bar action |
|---|---|
| Temp switch ON + air panel active | `lv_obj_clear_flag(ui_AirPowerBar, LV_OBJ_FLAG_HIDDEN)` |
| Temp switch ON + skin panel active | `lv_obj_clear_flag(ui_SkinPowerBar, LV_OBJ_FLAG_HIDDEN)` |
| Temp switch OFF | hide both `ui_AirPowerBar` + `ui_SkinPowerBar` |
| Panel switch air→skin | hide `ui_AirPowerBar`, show `ui_SkinPowerBar` |
| Panel switch skin→air | hide `ui_SkinPowerBar`, show `ui_AirPowerBar` |
| Hum switch ON | `lv_obj_clear_flag(ui_HumPowerBar, LV_OBJ_FLAG_HIDDEN)` |
| Hum switch OFF | `lv_obj_add_flag(ui_HumPowerBar, LV_OBJ_FLAG_HIDDEN)` |

Show/hide is integrated into existing `Switch_cb()` and `Display_ApplyCtrlState()` — no logic duplication.
