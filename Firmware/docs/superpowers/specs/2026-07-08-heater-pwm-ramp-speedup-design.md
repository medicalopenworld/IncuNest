# Heater PWM Ramp Speed-Up — Design Spec

**Date:** 2026-07-08
**Status:** Implemented

## Overview

Speed up and correct the heater PWM soft-start ramp (`heaterSafeMAXPWM` in `motherBoard/src/system/PID.cpp`) that runs when temperature control is activated (`startPID()`), while keeping the two safety properties it's meant to preserve: no short-circuit damage and no exceeding `heaterMaxPowerAmps`.

## Background

Short-circuit protection is already covered by `actuatorsTest()` at boot (`initHardware.cpp`): it drives the heater to full PWM and validates current with an early-exit at ~220ms; on failure it raises `HEATER_ISSUE_ALARM`, which `PIDHandler()` already gates to force heater PWM to 0 (`ongoingCriticalAlarm()`). This ramp is not re-validated on every `startPID()` call — that boot-time coverage is trusted for the session.

What the ramp actually did before this change: start `heaterSafeMAXPWM` at 5/255 and step ±5 every 2000ms wall-clock (`CURRENT_CHECK_PERIOD_MS`), reaching full PWM in ~100s regardless of real sensor cadence.

## Changes

| Constant | Before | After |
|---|---|---|
| `HEATER_START_PWM` (`board.h`) | 5 | 1 |
| `HEATER_POWER_FACTOR_INCREASE`/`DECREASE` (`main.h`) | 5 | 3 |
| `CURRENT_CHECK_PERIOD_MS` (`main.h`) | 2000ms wall-clock | removed → `HEATER_RAMP_SAMPLE_CYCLES = 3` (fresh-sample count) |
| `CURRENT_UPDATE_PERIOD_MS` (`main.h`) | 100ms | 110ms |

**`CURRENT_UPDATE_PERIOD_MS` → 110ms**: the INA3221 (AVG_128, 140µs CT) takes ~107.52ms per full conversion cycle. At 100ms, `currentMonitor()` could occasionally re-poll before a genuinely new conversion landed. 110ms guarantees a real new sample per call, matching the interval the boot self-test already uses (`measureThreeActuatorsParallel(..., 110)`).

**Ramp cadence tied to real samples, not wall-clock**: `heaterPowerConsumptionCheck()` now advances only when `heaterCurrentSampleSeq` (new counter in `legacy/sensors.cpp`, incremented only inside the block that actually reads the SECUNDARY/heater I2C chip) changes, counting 3 such changes before applying a ±3 step. Result: ~330ms per step, full ramp in ~28s (vs ~100s before).

**Fail-safe on sensor dropout**: `currentMonitor()` now probes I2C presence (`i2cDevicePresent()`) before trusting a reading, for both MAIN and SECUNDARY chips — the vendored `Beastdevices_INA3221` library does not check I2C transaction results and silently returns a stale/zero value on NACK. If the SECUNDARY (heater) chip stops answering: `in3.heater_current` keeps its last known value, `heaterCurrentSampleSeq` does not advance (so the ramp parks instead of climbing on invalid data), and after `HEATER_SENSOR_DROPOUT_ALARM_CYCLES` (10, ~1.1s) of continuous failure, `HEATER_ISSUE_ALARM` is raised — same "unrecoverable within session" semantics as the existing boot-time fault. MAIN-chip dropouts (system/fan/phototherapy) hold their last known value with no new alarm (out of scope for this change).

## Out of scope (deferred, flagged separately)

- Lowering `INA3221_REG_CONF_AVG_128` to `AVG_64`/`AVG_16` for faster hardware conversion: feasible in principle, but HW17's heater channel has a documented PCB defect (`HEATER_CURRENT_CORRECTION_FACTOR`, PWM-switching-node tap) empirically tuned under AVG_128 — needs bench validation before changing.
- `legacy/UI_actuatorsProgress()` calling `PIDHandler()` from a second FreeRTOS task (pre-existing concurrency hazard, not introduced by this change) — tracked as part of the legacy on-board UI removal proposal (`2026-07-08-legacy-onboard-ui-removal-proposal.md`), Phase 2.
- The `Beastdevices_INA3221` library's total lack of I2C-failure signalling at the `_read()` level — this change works around it with an external bus-presence probe rather than forking the vendored library.

## Verification

Compiled clean on `IncuNest_V16` and `IncuNest_V17` (`pio run`). `PID.cpp`/`sensors.cpp` are outside the `native` Unity test scope (`modules/control/` only) — manual verification on real hardware still pending.
