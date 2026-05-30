# Boot Time Reduction Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce boot time from ~13s to ~4s by fixing an I2C probe timeout, eliminating over-conservative delays, and parallelizing the three independent actuator current tests.

**Architecture:** Four tracks: (1) I2C probe timeout fix in `initRoomSensor()`; (2) remove gratuitous pre-wait `vTaskDelay` calls in `actuatorsTest()`; (3) run heater, phototherapy, and fan tests in a single interleaved measurement loop (no RTOS tasks needed); (4) optionally shorten the startup beep.

**Tech Stack:** ESP32-Arduino (FreeRTOS), C++, PlatformIO, INA3221 current sensors (dual: MAIN 0x41, SECUNDARY 0x40), STS3X/SHTC3 room sensors.

---

## Timing Profile (from boot log)

```
 330ms  System init (EEPROM, I2C, GPIO, PWM)          114ms
 444ms  initSensors() starts
        ├─ initCurrentSensor(MAIN)                    ~260ms
        ├─ initCurrentSensor(SECUNDARY)               ~260ms
        └─ initRoomSensor(): I2C probe timeout        ~3075ms ← ROOT CAUSE KNOWN
4039ms  initSensors() ends / BQ25730 + TFT + PID       ~53ms
4092ms  testStandByCurrent()                           621ms   ← already optimal (window=3, 200ms)
4713ms  testBuzzer() HW≥17 path                        700ms   ← fixed beep
5413ms  testSensors()                                  338ms
5751ms  actuatorsTest() starts
        ├─ heater stabilization                       1508ms   ← physical (SECUNDARY INA3221)
        ├─ photo pre-wait (vTaskDelay 700ms)           700ms   ← OVER-SPEC, remove
        ├─ photo stabilization                        1510ms   ← physical (MAIN INA3221 CH2)
        ├─ humidifier USB_FAULT delay (700ms)          700ms   ← OVER-SPEC, remove
        ├─ fan pre-wait (vTaskDelay 700ms)             700ms   ← OVER-SPEC, remove
        ├─ fan spin-up (220ms)                         220ms   ← physical (MAIN INA3221 CH3)
        └─ fan measurement                             625ms
11735ms actuatorsTest() ends
11735ms AFE4490 begin()                               1236ms   ← uninvestigated
12971ms Comm task ready
```

**Total: ~13s**

---

## INA3221 Channel Map (HW≥16) — Key for Understanding Parallelism

```
MAIN INA3221 (0x41)         SECUNDARY INA3221 (0x40)
  CH1: SYSTEM (standby)       CH1: HEATER
  CH2: PHOTOTHERAPY           CH2: USB / Humidifier (HW<16)
  CH3: FAN                    CH3: BATTERY
```

Both chips are on the same I2C bus. **Each channel is measured independently** by the INA3221 hardware even while other channels are active — the chip cycles CH1→CH2→CH3 continuously and each channel's shunt is a separate resistor. This is what makes parallel testing safe.

---

## File Map

- **Modify:** `src/initHardware.cpp`
  - `initRoomSensor()` — add `setTimeOut(10)` before probe loop (Task 1)
  - `actuatorsTest()` — replace sequential tests with interleaved parallel loop (Tasks 2 + 3)
  - `testBuzzer()` HW≥17 path — shorten beep (Task 4, optional)
- **New:** `src/initHardware.cpp` — `measureThreeActuatorsParallel()` helper (Task 3)
- **Modify:** `src/incunest_afe4490.cpp` — add timing log to `begin()` (Task 5)

---

## Task 1: Fix initRoomSensor() I2C probe timeout (~3000ms)

**Root cause (confirmed):** `initRoomSensor()` probes 3 I2C addresses (0x4A, 0x4B, 0x70) on `wire2` which is initialised at 10kHz with no explicit timeout. The ESP32 Arduino Wire library default timeout for a NACK (device not present) is ~1000ms per transaction at that clock speed. For 2–3 absent sensor addresses: 2–3 × ~1000ms = 2–3s.

The STS3X library delays (`stopMeasurement` = 1ms, `readStatusRegister` = 10ms) are negligible — they are not the cause.

**Fix: one line.** Call `setTimeOut(10)` on the I2C bus before the probe loop. 10ms is 10× the time needed for a genuine NACK at 10kHz (~900µs for 9 clock cycles).

**Files:**
- Modify: `src/initHardware.cpp:342`

- [ ] **Step 1: Add timeout before the probe loop in initRoomSensor()**

Open `src/initHardware.cpp`. Find `initRoomSensor()` at line 342. Add one line before the `for` loop:

```cpp
void initRoomSensor() {
  static int16_t room_sensor_error;
  static char errorMessage[64];

  // Without an explicit timeout the ESP32 Wire library blocks ~1s per probe
  // for an address that returns NACK. At 3 sensor possibilities this costs ~3s.
#if (HW_NUM >= 16)
  wire2->setTimeOut(10);
#else
  wire->setTimeOut(10);
#endif

  for (int i = 0; i < ROOM_SENSOR_POSIBILITIES; i++) {
    // ... rest of function unchanged
```

- [ ] **Step 2: Flash and verify**

Expected: the gap between secondary current sensor detection and ADS1110 config shrinks from ~3075ms to ~600ms (just the STS3X init delays for any sensors that are actually present).

New expected log:
```
[   753] Wire.cpp: Bus already started in Master Mode.
[  1360] [SKIN] ADS1110 configured   ← was 4039ms
[  1360] [HW] -> Initializing BQ25730 charger
```

If the gap is still large (>800ms), the timeout setting was not applied to the right bus. Confirm whether `wire` / `wire2` correspond to `Wire` / `Wire1` and adjust accordingly.

- [ ] **Step 3: Commit**

```bash
git add src/initHardware.cpp
git commit -m "fix(boot): set Wire timeout to 10ms before I2C sensor probing (-3000ms)"
```

---

## Task 2: Remove three over-conservative pre-waits in actuatorsTest()

**Why:** Three `vTaskDelay(700ms)` calls act as "let the INA3221 settle" guards, but:
- Photo pre-wait: heater (SECUNDARY) just turned off, photo uses MAIN — completely different chip, no settle needed beyond one conversion cycle (~107ms).
- Fan pre-wait: same MAIN INA3221 as phototherapy but different channel; after phototherapy OFF the INA3221 CH3 just needs one fresh conversion.
- Humidifier: just reads a GPIO fault pin (`USB_FAULT`) which responds within ~50ms — 100ms is sufficient.

This task applies whether or not Task 3 (parallelism) is implemented. If Task 3 IS done, Steps 2–4 here become superseded by the parallel loop, but the constants added in Step 1 are still used.

**Files:**
- Modify: `src/initHardware.cpp:151-160` (add constants)
- Modify: `src/initHardware.cpp:733,769,800` (replace waits)

- [ ] **Step 1: Add two constants near the existing timing #defines (line ~151)**

```cpp
// Time for one fresh INA3221 conversion (AVG128 × 140µs × 2 × 3ch ≈ 107ms), with margin.
#define INA3221_ONE_CYCLE_SETTLE_MS 200
// USB_FAULT GPIO latches within ~50ms of USB_EN assertion; 100ms gives 2× margin.
#define USB_FAULT_SETTLE_MS 100
```

- [ ] **Step 2: Replace phototherapy pre-wait (~line 733)**

```cpp
// Before:
  vTaskDelay(pdMS_TO_TICKS(CURRENT_STABILIZE_TIME_DEFAULT));
  offsetCurrent = measureMeanConsumption(MAIN, PHOTOTHERAPY_SHUNT_CHANNEL);

// After:
  vTaskDelay(pdMS_TO_TICKS(INA3221_ONE_CYCLE_SETTLE_MS));
  offsetCurrent = measureMeanConsumption(MAIN, PHOTOTHERAPY_SHUNT_CHANNEL);
```

- [ ] **Step 3: Replace humidifier delay (~line 769, HW≥16 path)**

```cpp
// Before:
  in3_hum.turn(ON);
  vTaskDelay(pdMS_TO_TICKS(CURRENT_STABILIZE_TIME_DEFAULT));
  bool usbFaultDetected = !GPIORead(USB_FAULT);

// After:
  in3_hum.turn(ON);
  vTaskDelay(pdMS_TO_TICKS(USB_FAULT_SETTLE_MS));
  bool usbFaultDetected = !GPIORead(USB_FAULT);
```

- [ ] **Step 4: Replace fan pre-wait (~line 800)**

```cpp
// Before:
  vTaskDelay(pdMS_TO_TICKS(CURRENT_STABILIZE_TIME_DEFAULT));
  offsetCurrent = measureMeanConsumption(MAIN, FAN_SHUNT_CHANNEL);

// After:
  vTaskDelay(pdMS_TO_TICKS(INA3221_ONE_CYCLE_SETTLE_MS));
  offsetCurrent = measureMeanConsumption(MAIN, FAN_SHUNT_CHANNEL);
```

- [ ] **Step 5: Flash and verify**

Expected savings vs original:
```
Photo pre-wait:   700ms → 200ms   (-500ms)
Humidifier:       700ms → 100ms   (-600ms)
Fan pre-wait:     700ms → 200ms   (-500ms)
Total:                            -1600ms
```

Boot log should show `FAN consumption` at approximately `[ 10135]` instead of `[ 11735]`.

If phototherapy `in3.phototherapy_intensity` varies between boots (noisy baseline), increase `INA3221_ONE_CYCLE_SETTLE_MS` to 300ms. If humidifier shows false USB_FAULT, increase `USB_FAULT_SETTLE_MS` to 150ms.

- [ ] **Step 6: Commit**

```bash
git add src/initHardware.cpp
git commit -m "perf(boot): reduce actuator pre-waits to INA3221 settle minimum (-1600ms)"
```

---

## Task 3: Parallelize heater + phototherapy + fan tests

**Why this is safe:** The three actuators measure from independent hardware channels:
- Heater → SECUNDARY INA3221 (0x40) CH1
- Phototherapy → MAIN INA3221 (0x41) CH2
- Fan → MAIN INA3221 (0x41) CH3

Fan and phototherapy share the same chip but different channels — the INA3221 measures all channels independently in its internal cycle. Neither can contaminate the other's reading. The I2C bus is shared (only one transaction at a time), but each I2C read is ~3ms at 10kHz; with 110ms sleep between reads the bus utilisation is ~8%.

Power supply load: heater ~2.88A + phototherapy ~0.1A + fan ~0.11A ≈ 3.1A simultaneously. This is less than the normal running load once the device is in service, so the supply handles it.

**Approach: single interleaved loop (no RTOS tasks needed)**

Instead of separate FreeRTOS tasks, a single loop ticks every 110ms and advances all three stability windows in lockstep. This avoids task synchronisation complexity while achieving the same wall-clock benefit.

```
Current sequential:  heater(1508) + photo(200+1510) + hum(100) + fan(200+220+625) = 4363ms
Parallel interleaved: max(heater, photo, fan) + humidifier_overhead ≈ 1508ms + 100ms = 1608ms
Savings: ~2755ms
```

**Files:**
- Modify: `src/initHardware.cpp:674-844`
- The helper `measureThreeActuatorsParallel()` is added as a `static` function above `actuatorsTest()`.

- [ ] **Step 1: Add the parallel helper function above actuatorsTest() (~line 673)**

```cpp
// Measures heater (SECUNDARY CH1), phototherapy (MAIN CH2), and fan (MAIN CH3)
// simultaneously by interleaving INA3221 reads in a single loop.
// All three actuators must already be ON when this is called.
// offsets[] = {heaterOffset, photoOffset, fanOffset}
// Returns true if any channel exceeded its maximum (critical error).
struct ActuatorResult {
  float heater;
  float photo;
  float fan;
  bool heaterStable;
  bool photoStable;
  bool fanStable;
};

static ActuatorResult measureThreeActuatorsParallel(
    float heaterOffset, float photoOffset, float fanOffset,
    float heaterMin, float heaterMax,
    float photoMin,  float photoMax,
    float fanMin,    float fanMax,
    int maxTimeMs = 8000, int intervalMs = 110, int windowSize = 10)
{
  const float RATIO = CURRENT_STABILIZE_THRESHOLD_RATIO;
  float hBuf[10] = {}, pBuf[10] = {}, fBuf[10] = {};
  float hThresh = (heaterMax - heaterMin) * RATIO;
  float pThresh = (photoMax  - photoMin)  * RATIO;
  float fThresh = (fanMax    - fanMin)    * RATIO;

  ActuatorResult r = {0, 0, 0, false, false, false};
  float hFirst = measureMeanConsumption(SECUNDARY, HEATER_SHUNT_CHANNEL)    - heaterOffset;
  float pFirst = measureMeanConsumption(MAIN,      PHOTOTHERAPY_SHUNT_CHANNEL) - photoOffset;
  float fFirst = measureMeanConsumption(MAIN,      FAN_SHUNT_CHANNEL)       - fanOffset;
  for (int i = 0; i < windowSize; i++) { hBuf[i] = hFirst; pBuf[i] = pFirst; fBuf[i] = fFirst; }

  int idx = 0, count = 0, elapsed = 0;
  while (elapsed < maxTimeMs && !(r.heaterStable && r.photoStable && r.fanStable)) {
    vTaskDelay(pdMS_TO_TICKS(intervalMs));
    elapsed += intervalMs;

    r.heater = measureMeanConsumption(SECUNDARY, HEATER_SHUNT_CHANNEL)      - heaterOffset;
    r.photo  = measureMeanConsumption(MAIN,      PHOTOTHERAPY_SHUNT_CHANNEL) - photoOffset;
    r.fan    = measureMeanConsumption(MAIN,      FAN_SHUNT_CHANNEL)          - fanOffset;

    hBuf[idx % windowSize] = r.heater;
    pBuf[idx % windowSize] = r.photo;
    fBuf[idx % windowSize] = r.fan;
    idx++; count++;

    if (count >= windowSize) {
      auto stable = [&](float* buf, float thresh, float mn_exp, float mx_exp) -> bool {
        float mn = buf[0], mx = buf[0];
        for (int i = 1; i < windowSize; i++) {
          if (buf[i] < mn) mn = buf[i]; if (buf[i] > mx) mx = buf[i];
        }
        float last = buf[(idx - 1) % windowSize];
        return (mx - mn) < thresh && last >= mn_exp && last <= mx_exp;
      };
      if (!r.heaterStable) r.heaterStable = stable(hBuf, hThresh, heaterMin, heaterMax);
      if (!r.photoStable)  r.photoStable  = stable(pBuf, pThresh, photoMin,  photoMax);
      if (!r.fanStable)    r.fanStable    = stable(fBuf, fThresh, fanMin,    fanMax);
    }
  }
  return r;
}
```

- [ ] **Step 2: Rewrite actuatorsTest() for HW≥16 to use the parallel helper**

Replace the body of `actuatorsTest()` from line 674. The structure below is HW≥16 only (inside `#if (HW_NUM >= 16)`). The existing HW<16 sequential path is kept unchanged inside `#else`.

```cpp
bool actuatorsTest() {
  long error = HW_error;
  logI("[HW] -> Checking actuators...");
  digitalWrite(ACTUATORS_EN, HIGH);

  logI("[HW] -> digitalCurrentSensorPresent MAIN=" +
       String(digitalCurrentSensorPresent[MAIN]) +
       " SECUNDARY=" + String(digitalCurrentSensorPresent[SECUNDARY]));

#if (HW_NUM >= 16)
  // ── Phototherapy constants (same as before) ──────────────────────────────
  const int   PHOTOTHERAPY_TEST_PWM = PWM_MAX_VALUE * 10 / 100;
  const float PHOTOTHERAPY_PWM_ZERO = 20.0f;
  const float photoScale = (PHOTOTHERAPY_TEST_PWM + PHOTOTHERAPY_PWM_ZERO) /
                           (PWM_MAX_VALUE          + PHOTOTHERAPY_PWM_ZERO);

  // ── Measure all baselines before any actuator is active ──────────────────
  float heaterOffset = measureMeanConsumption(SECUNDARY, HEATER_SHUNT_CHANNEL);
  float photoOffset  = measureMeanConsumption(MAIN,      PHOTOTHERAPY_SHUNT_CHANNEL);
  float fanOffset    = measureMeanConsumption(MAIN,      FAN_SHUNT_CHANNEL);
  logI("[HW] -> Baselines — heater: " + String(heaterOffset) +
       "  photo: " + String(photoOffset) +
       "  fan: "   + String(fanOffset) + " A");

  // ── Turn on all three actuators simultaneously ───────────────────────────
  ledcWrite(HEATER_PWM_CHANNEL,      PWM_MAX_VALUE);
  ledcWrite(PHOTOTHERAPY_PWM_CHANNEL, PHOTOTHERAPY_TEST_PWM);
  // Fan needs spin-up; start last to align thermal stabilisation windows
  vTaskDelay(pdMS_TO_TICKS(INA3221_ONE_CYCLE_SETTLE_MS));  // photo/heater first samples
  ledcWrite(FAN_PWM_CHANNEL, PWM_MAX_VALUE);
  vTaskDelay(pdMS_TO_TICKS(220));  // fan motor spin-up (datasheet: ~2 INA3221 cycles)
  logI("[HW] -> Heater + Phototherapy + Fan ON, measuring in parallel...");

  ActuatorResult res = measureThreeActuatorsParallel(
      heaterOffset, photoOffset, fanOffset,
      HEATER_CONSUMPTION_MIN,       HEATER_CONSUMPTION_MAX,
      PHOTOTHERAPY_CONSUMPTION_MIN * photoScale, PHOTOTHERAPY_CONSUMPTION_MAX * photoScale,
      FAN_CONSUMPTION_MIN,          FAN_CONSUMPTION_MAX);

  // ── Turn everything off ───────────────────────────────────────────────────
  ledcWrite(HEATER_PWM_CHANNEL,      0);
  ledcWrite(PHOTOTHERAPY_PWM_CHANNEL, 0);
  ledcWrite(FAN_PWM_CHANNEL,          0);

  // ── Log and check results ─────────────────────────────────────────────────
  logI("[HW] -> Heater: "       + String(res.heater, 3) + " A");
  logI("[HW] -> Phototherapy: " + String(res.photo, 3)  + " A");
  logI("[HW] -> FAN: "          + String(res.fan, 3)    + " A");

  in3.heater_current_test        = res.heater;
  in3.phototherapy_current_test  = res.photo;
  in3.fan_current_test           = res.fan;

  // Heater checks (critical — abort if failed)
  if (res.heater < HEATER_CONSUMPTION_MIN) {
    addErrorToVar(HW_error, HEATER_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> Heater current too low");
    in3.alarmToReport[HEATER_ISSUE_ALARM] = true;
    setAlarm(HEATER_ISSUE_ALARM);
    digitalWrite(ACTUATORS_EN, LOW);
    return true;
  }
  if (res.heater > HEATER_CONSUMPTION_MAX) {
    addErrorToVar(HW_error, HEATER_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> Heater current too high");
    in3.alarmToReport[HEATER_ISSUE_ALARM] = true;
    setAlarm(HEATER_ISSUE_ALARM);
    digitalWrite(ACTUATORS_EN, LOW);
    return true;
  }
  { Preferences p; p.begin(NS_CFG, false); p.putUChar(KEY_HEATER_TEST, 1); p.end(); }

  // Phototherapy checks
  if (res.photo < PHOTOTHERAPY_CONSUMPTION_MIN * photoScale) {
    addErrorToVar(HW_error, PHOTOTHERAPY_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> Phototherapy current too low at 10%");
  }
  if (res.photo > PHOTOTHERAPY_CONSUMPTION_MAX * photoScale) {
    addErrorToVar(HW_error, PHOTOTHERAPY_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> Phototherapy current too high at 10%");
    digitalWrite(ACTUATORS_EN, LOW);
    return true;
  }

  // Extrapolate phototherapy PWM for target current
  int pwmTarget = (int)roundf(
      PHOTOTHERAPY_CONSUMPTION_DEFAULT *
          (PHOTOTHERAPY_TEST_PWM + PHOTOTHERAPY_PWM_ZERO) / res.photo -
      PHOTOTHERAPY_PWM_ZERO);
  pwmTarget = constrain(pwmTarget, 0, PWM_MAX_VALUE);
  in3.phototherapy_intensity = pwmTarget;
  logI("[HW] -> Phototherapy extrapolated PWM=" + String(pwmTarget) +
       " (" + String(pwmTarget * 100 / PWM_MAX_VALUE) + "%) for " +
       String(PHOTOTHERAPY_CONSUMPTION_DEFAULT, 2) + " A");

  // Fan checks
  if (res.fan < FAN_CONSUMPTION_MIN) {
    addErrorToVar(HW_error, FAN_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> Fan current too low");
    digitalWrite(ACTUATORS_EN, LOW);
    return true;
  }
  if (res.fan > FAN_CONSUMPTION_MAX &&
      res.fan > FAN_MAX_CURRENT_OVERRIDE * FAN_CONSUMPTION_MAX * 2) {
    addErrorToVar(HW_error, FAN_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> Fan current too high");
    digitalWrite(ACTUATORS_EN, LOW);
    return true;
  }

  // ── Humidifier (GPIO fault check only on HW≥16) ──────────────────────────
  in3_hum.turn(ON);
  vTaskDelay(pdMS_TO_TICKS(USB_FAULT_SETTLE_MS));  // 100ms — USB_FAULT latches within ~50ms
  bool usbFaultDetected = !GPIORead(USB_FAULT);
  in3_hum.turn(OFF);
  if (usbFaultDetected) {
    addErrorToVar(HW_error, HUMIDIFIER_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> USB_FAULT on humidifier");
    digitalWrite(ACTUATORS_EN, LOW);
  }
  in3.humidifier_current_test = 1.0;
  logI("[HW] -> Humidifier USB_EN test passed, no fault");

#else
  // ── HW<16: original sequential test unchanged ────────────────────────────
  // ... (keep all existing code for HW<16 here, unmodified)
#endif

  if (error == HW_error) {
    logI("[HW] -> OK -> Actuators are working as expected");
  } else {
    logI("[HW] -> Fail -> Some actuators are not working as expected");
  }
  digitalWrite(ACTUATORS_EN, LOW);
  return false;
}
```

- [ ] **Step 3: Flash and verify the parallel test**

Expected boot log for actuatorsTest():
```
[  5751] Checking actuators...
[  5762] Baselines — heater: 0.00  photo: 0.00  fan: 0.00 A
[  5765] Heater + Phototherapy + Fan ON, measuring in parallel...
[  7373] Heater: 2.88 A            ← all three measured in ~1600ms total
[  7373] Phototherapy: 0.096 A
[  7373] FAN: 0.11 A
[  7373] Phototherapy extrapolated PWM=191 ...
[  7473] Humidifier USB_EN test passed
[  7473] OK -> Actuators are working as expected
          ↑ was 11735ms — saves ~4260ms
```

Check:
- `in3.phototherapy_intensity` should be consistent across multiple boots (same extrapolated PWM ±5).
- No spurious heater/fan/photo current errors on 5 consecutive boots.
- If fan stabilises faster than heater/photo, `r.fanStable` will be set early and the loop continues only for the remaining two — this is correct behaviour.

- [ ] **Step 4: If phototherapy reading is noisy**

Running phototherapy and fan simultaneously on the same MAIN INA3221 could theoretically cause inter-channel noise. If `in3.phototherapy_intensity` drifts >10 PWM counts between boots, separate the fan start:

```cpp
// Start fan AFTER phototherapy is stable (non-parallel fallback for photo only):
// measure phototherapy alone first (200ms settle + stabilize), then do fan.
// This gives up ~625ms savings but keeps phototherapy accuracy.
```

Only apply this fallback if actual measurements show the issue.

- [ ] **Step 5: Commit**

```bash
git add src/initHardware.cpp
git commit -m "perf(boot): run heater+photo+fan tests in parallel (-4260ms)"
```

---

## Task 4: Investigate AFE4490 init delay (1236ms)

**Why:** `incunest_afe4490.cpp:begin()` takes 1236ms (11735ms → 12971ms). This might be necessary PRF settling or a conservative `delay()`.

**Files:**
- Modify: `src/incunest_afe4490.cpp` (add/remove timing logs)

- [ ] **Step 1: Add timing markers to begin()**

```cpp
logI("[afe4490] begin() start");
// before each delay inside begin():
logI("[afe4490] pre-delay " + String(X) + "ms");
vTaskDelay(...);  // or delay(...)
logI("[afe4490] post-delay");
```

- [ ] **Step 2: Flash and identify which delay(s) sum to 1236ms**

- [ ] **Step 3: Reduce if over-conservative**

Any settle time longer than the datasheet minimum can be halved and tested. Any `delay()` (blocking) should be converted to `vTaskDelay()`.

- [ ] **Step 4: Remove diagnostic logs, commit**

```bash
git add src/incunest_afe4490.cpp
git commit -m "perf(boot): reduce AFE4490 begin() settle time"
```

---

## Task 5 (Optional): Shorten buzzer beep for HW≥17

The HW≥17 `testBuzzer()` path emits a 700ms beep with no current measurement. 300ms is clearly audible; 400ms saved. UX tradeoff — discuss before doing.

**Files:**
- Modify: `src/initHardware.cpp:656-659`

- [ ] **Step 1: Add constant near line 151**

```cpp
#define BUZZER_BEEP_DURATION_MS 300
```

- [ ] **Step 2: Replace delay in HW≥17 path**

```cpp
  #else
    ledcWrite(BUZZER_PWM_CHANNEL, BUZZER_HALF_PWM);
    vTaskDelay(pdMS_TO_TICKS(BUZZER_BEEP_DURATION_MS));
    ledcWrite(BUZZER_PWM_CHANNEL, false);
  #endif
```

- [ ] **Step 3: Flash, listen, confirm beep is satisfying**

- [ ] **Step 4: Commit**

```bash
git add src/initHardware.cpp
git commit -m "perf(boot): shorten startup beep to 300ms (-400ms)"
```

---

## Expected Total Savings

| Task | Savings | Confidence |
|------|---------|------------|
| Task 1: initRoomSensor() I2C timeout | ~3000ms | High (root cause confirmed) |
| Task 2: actuator pre-waits removal | ~1600ms | High |
| Task 3: parallel actuator tests | ~4260ms | High (supersedes most of Task 2) |
| Task 4: AFE4490 | unknown | Needs investigation |
| Task 5: buzzer beep | ~400ms | Optional/UX |

**Note:** Tasks 2 and 3 overlap. Task 3 removes the need for most of the pre-waits and subsumes their savings. The combined savings from Tasks 2+3 is ~4260ms (not ~5860ms).

```
Current:  13000ms
Task 1:   -3000ms  →  ~10000ms
Tasks 2+3: -4260ms  →  ~5740ms
Task 4:    -???ms   →  ~4500ms (estimated if AFE4490 has savings)
Task 5:    -400ms   →  ~4100ms

Target: ~4s boot time
```
