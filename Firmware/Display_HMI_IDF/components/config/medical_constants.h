/**
 * @file medical_constants.h
 * @brief Normative constants for the IncuNest neonatal incubator HMI.
 *
 * @details ALL values derived from IEC/clinical standards live here.
 *          Never write a bare numeric literal in application code when
 *          a normative value is involved. Each constant cites its clause.
 *          Changing any value requires a documented normative justification.
 *
 * Normativa aplicable:
 * - IEC 60601-2-19:2021 — Particular requirements for infant incubators
 * - IEC 60601-1-8:2007/A1:2013 — Alarm systems for medical electrical equipment
 * - IEC 62366-1:2015 — Usability engineering for medical devices
 *
 * @author IncuNest Team
 * @date   2026-04-28
 */
#pragma once

/* ==========================================================================
   IEC 60601-2-19 §201.15.4.2.2.101 — AIR CONTROLLED temperature range
   ========================================================================== */

/** Minimum setpoint in AIR CONTROLLED mode (inclusive) */
#define TEMP_AIR_CTRL_MIN_C          30.0f

/** Maximum setpoint in AIR CONTROLLED mode without override warning */
#define TEMP_AIR_CTRL_MAX_C          37.0f

/** Absolute maximum with operator override + confirmation — §201.15.4.2.2.101 */
#define TEMP_AIR_CTRL_OVERRIDE_C     39.0f

/* ==========================================================================
   IEC 60601-2-19 §201.15.4.2.2.102 — BABY CONTROLLED (skin) temperature range
   ========================================================================== */

/** Minimum setpoint in BABY CONTROLLED mode */
#define TEMP_BABY_CTRL_MIN_C         35.0f

/** Maximum setpoint in BABY CONTROLLED mode without override warning */
#define TEMP_BABY_CTRL_MAX_C         37.5f

/** Absolute maximum with operator override — §201.15.4.2.2.102 */
#define TEMP_BABY_CTRL_OVERRIDE_C    39.0f

/* ==========================================================================
   IEC 60601-2-19 §201.7.4.2 — Temperature adjustment resolution
   ========================================================================== */

/** Minimum increment for AIR CONTROLLED setpoint (≤0.5°C per standard) */
#define TEMP_AIR_INCREMENT_C         0.2f

/** Minimum increment for BABY CONTROLLED setpoint (≤0.25°C per standard) */
#define TEMP_BABY_INCREMENT_C        0.2f

/** Threshold below which label updates are suppressed (anti-jitter) */
#define TEMP_LABEL_UPDATE_THRESHOLD_C  0.05f

/* ==========================================================================
   IEC 60601-1-8 §6.3.2.2.2 — Visual alarm signal requirements
   ========================================================================== */

/** HIGH priority alarm flash rate minimum (Hz) — §6.3.2.2.2 Table 3 */
#define ALARM_HIGH_FLASH_HZ          2.0f

/** MEDIUM priority alarm flash rate (Hz) */
#define ALARM_MED_FLASH_HZ           0.5f

/** LOW priority alarm — static, no flashing */
#define ALARM_LOW_FLASH_HZ           0.0f

/* ==========================================================================
   IEC 60601-1-8 §6.8.5 — Alarm deactivation states
   ========================================================================== */

/** Maximum duration of AUDIO PAUSED state for HIGH priority alarms (ms)
 *  §6.8.5: "not more than 3 min for HIGH priority" — using conservative 2 min */
#define ALARM_AUDIO_PAUSE_HIGH_MS    (2 * 60 * 1000)

/** Maximum duration of AUDIO PAUSED state for MEDIUM priority alarms (ms) */
#define ALARM_AUDIO_PAUSE_MED_MS     (5 * 60 * 1000)

/** Maximum AUDIO PAUSED during system warm-up — §201.12.3.104 */
#define ALARM_AUDIO_PAUSE_WARMUP_MS  (30 * 60 * 1000)

/* ==========================================================================
   IEC 60601-1-8 §201.9.6.2.1.102 — Alarm audio level
   ========================================================================== */

/** Minimum alarm sound level in dB(A) at 1m — §201.9.6.2.1.102 */
#define ALARM_AUDIO_MIN_DB           50

/* ==========================================================================
   IEC 60601-2-19 §201.11.8 — Configuration persistence after power loss
   ========================================================================== */

/** Maximum power interruption after which CONTROL TEMPERATURE must persist (ms).
 *  §201.11.8: temperature setting must be retained for at least 10 minutes. */
#define CONFIG_PERSIST_CUTOFF_MS     (10 * 60 * 1000)

/* ==========================================================================
   IEC 60601-2-19 §201.9.6.2.1.101 — Display backlight minimum brightness
   ========================================================================== */

/** Minimum backlight brightness in %. Operator cannot go below this.
 *  §201.9.6.2.1.101: display must remain legible in all intended environments */
#define DISPLAY_BL_MIN_PCT           20

/* ==========================================================================
   IEC 62366-1 §5.1 / HUS-003 — Screen lock safety timeout
   ========================================================================== */

/** Duration of long-press needed to unlock the screen lock (ms) */
#define SCREEN_LOCK_PRESS_MS         1500

/** Inactivity timeout before screen auto-locks (ms) */
#define SCREEN_AUTOLOCK_MS           (5 * 60 * 1000)

/* ==========================================================================
   IEC 62366-1 §5.6 / UI-002 — Confirmation popup timeout
   ========================================================================== */

/** If operator does not respond to a confirmation popup, it auto-cancels (ms) */
#define CONFIRM_POPUP_TIMEOUT_MS     10000

/* ==========================================================================
   IEC 60601-2-19 §201.12.3.105 — Alarm test function duration
   ========================================================================== */

/** Duration of the alarm test signal when operator presses TEST (ms) */
#define ALARM_TEST_DURATION_MS       3000

/* ==========================================================================
   Humidity control ranges (derived from system requirements)
   ========================================================================== */

#define HUMIDITY_MIN_PCT             20
#define HUMIDITY_MAX_PCT             95
#define HUMIDITY_INCREMENT_PCT       5
