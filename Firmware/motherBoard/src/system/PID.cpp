/*
  MIT License

  Copyright (c) 2022 Medical Open World, Pablo Sánchez Bergasa

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

*/
#include "PID.h"

#include <Arduino.h>

#include "main.h"

double HeaterPIDOutput;
double skinControlPIDInput;
double airControlPIDInput;
double humidityControlPIDOutput;
int humidifierTimeCycle = 5000;
unsigned long windowStartTime;
double fanControlPIDOutput;
double fanTargetRPM = FAN_TARGET_RPM;
// Tracks heaterCurrentSampleSeq/systemCurrentSampleSeq (sensors_module.cpp)
// so heaterSafeMAXPWM only steps once HEATER_RAMP_SAMPLE_CYCLES genuinely new
// samples of the reference current (see HEATER_POWER_REFERENCE_IS_SYSTEM_CURRENT,
// board.h) have arrived, instead of on a wall-clock timer or a tick that
// advances whether or not the corresponding sensor actually responded. If
// that sensor is absent, its sample seq never moves and the ramp stays
// parked at HEATER_START_PWM instead of climbing blind.
static unsigned long lastSeenCurrentControlSampleSeq = 0;
static int heaterRampSampleCounter = 0;

extern IncuNest_parameters in3;
extern MAM_IncuNest_Humidifier in3_hum;
extern bool humidifierState, humidifierStateChange;

double Kp[numPID] = {KP_SKIN, KP_AIR, KP_HUMIDITY};
double Ki[numPID] = {KI_SKIN, KI_AIR, KI_HUMIDITY};
double Kd[numPID] = {KD_SKIN, KD_AIR, KD_HUMIDITY};
double anti_windup_offset[numPID] = {AWO_SKIN, AWO_AIR, AWO_HUMIDITY};

PID airControlPID(&in3.temperature[ROOM_DIGITAL_TEMP_SENSOR], &HeaterPIDOutput,
                  &in3.desiredControlTemperature, Kp[airPID], Ki[airPID],
                  Kd[airPID], P_ON_E, DIRECT);
PID skinControlPID(&in3.temperature[SKIN_SENSOR], &HeaterPIDOutput,
                   &in3.desiredControlTemperature, Kp[skinPID], Ki[skinPID],
                   Kd[skinPID], P_ON_E, DIRECT);
PID humidityControlPID(&in3.humidity[ROOM_DIGITAL_HUM_SENSOR],
                       &humidityControlPIDOutput, &in3.desiredControlHumidity,
                       Kp[humidityPID], Ki[humidityPID], Kd[humidityPID],
                       P_ON_E, DIRECT);
PID fanControlPID(&in3.fan_rpm, &fanControlPIDOutput, &fanTargetRPM, KP_FAN,
                  KI_FAN, KD_FAN, P_ON_E, DIRECT);

void PIDInit()
{
  airControlPID.SetMode(MANUAL);
  skinControlPID.SetMode(MANUAL);
  humidityControlPID.SetMode(MANUAL);
  fanControlPID.SetMode(MANUAL);
  fanControlPID.SetOutputLimits(0, PWM_MAX_VALUE);
  fanControlPID.SetSampleTime(PID_FAN_SAMPLE_TIME);
}

void heaterPowerConsumptionCheck()
{
#if !HEATER_CURRENT_LIMIT_ENABLED
  // Current-based throttling disabled (HEATER_CURRENT_LIMIT_ENABLED false):
  // pin heaterSafeMAXPWM at HEATER_MAX_PWM so the ramp startPID() kicks off at
  // HEATER_START_PWM doesn't leave the heater stuck at minimum power - this
  // function is otherwise the only place that raises heaterSafeMAXPWM.
  if (in3.heaterSafeMAXPWM != HEATER_MAX_PWM)
  {
    in3.heaterSafeMAXPWM = HEATER_MAX_PWM;
    if (airControlPID.GetMode() == AUTOMATIC)
    {
      airControlPID.SetOutputLimits(0, in3.heaterSafeMAXPWM);
    }
    if (skinControlPID.GetMode() == AUTOMATIC)
    {
      skinControlPID.SetOutputLimits(0, in3.heaterSafeMAXPWM);
    }
  }
  return;
#endif
  // Only advance once a genuinely new sample of the reference current has
  // landed (sample seq changed) - PIDHandler() runs every 1ms, much faster
  // than the ~110ms current sensor refresh, so without this guard the same
  // sample would be counted as "new" on every call. This also fails safe if
  // the corresponding sensor is absent/down: its sample seq then never
  // advances, so the ramp stays parked instead of climbing blind.
#if HEATER_POWER_REFERENCE_IS_SYSTEM_CURRENT
  unsigned long currentControlSampleSeq = systemCurrentSampleSeq;
#else
  unsigned long currentControlSampleSeq = heaterCurrentSampleSeq;
#endif
  if (currentControlSampleSeq == lastSeenCurrentControlSampleSeq)
  {
    return;
  }
  lastSeenCurrentControlSampleSeq = currentControlSampleSeq;

  if (++heaterRampSampleCounter < HEATER_RAMP_SAMPLE_CYCLES)
  {
    return;
  }
  heaterRampSampleCounter = 0;

  int heaterSafeMAXPWM_before = in3.heaterSafeMAXPWM;
#if HEATER_POWER_REFERENCE_IS_SYSTEM_CURRENT
  bool overLimit = in3.system_current > in3.heaterMaxPowerAmps;
  bool underMargin = in3.system_current < (in3.heaterMaxPowerAmps - HEATER_SAFE_MARGIN_AMPS);
#else
  bool overLimit = in3.heater_current > in3.heaterMaxPowerAmps || in3.system_current > in3.heaterMaxPowerAmps;
  bool underMargin = in3.heater_current < (in3.heaterMaxPowerAmps - HEATER_SAFE_MARGIN_AMPS) || in3.system_current < (in3.heaterMaxPowerAmps - HEATER_SAFE_MARGIN_AMPS);
#endif
  if (overLimit)
  {
    in3.heaterSafeMAXPWM -= HEATER_POWER_FACTOR_DECREASE;
    if (in3.heaterSafeMAXPWM < 0)
    {
      in3.heaterSafeMAXPWM = 0;
    }
  }
  else if (underMargin)
  {
    in3.heaterSafeMAXPWM += HEATER_POWER_FACTOR_INCREASE;
    if (in3.heaterSafeMAXPWM > HEATER_MAX_PWM)
    {
      in3.heaterSafeMAXPWM = HEATER_MAX_PWM;
    }
  }
  if (heaterSafeMAXPWM_before != in3.heaterSafeMAXPWM)
  {
#if HEATER_POWER_REFERENCE_IS_SYSTEM_CURRENT
    logI("[PID] -> System current is " + String(in3.system_current) + ", changed max PWM to: " + String(in3.heaterSafeMAXPWM));
#else
    logI("[PID] -> Heater current is " + String(in3.heater_current) + ", changed max PWM to: " + String(in3.heaterSafeMAXPWM));
#endif
    if (airControlPID.GetMode() == AUTOMATIC)
    {
      airControlPID.SetOutputLimits(0, in3.heaterSafeMAXPWM); // Set safe limits
    }
    if (skinControlPID.GetMode() == AUTOMATIC)
    {
      skinControlPID.SetOutputLimits(0, in3.heaterSafeMAXPWM); // Set safe limits
    }
  }
}

void PIDHandler()
{
  heaterPowerConsumptionCheck();
  {
    static bool prevHeaterGateBlocked = false;
    bool heaterGateBlocked = ongoingCriticalAlarm();
    if (heaterGateBlocked != prevHeaterGateBlocked &&
        (airControlPID.GetMode() == AUTOMATIC || skinControlPID.GetMode() == AUTOMATIC))
    {
      logI(String("[BOOT][DEBUG] PIDHandler: heater output ") +
           (heaterGateBlocked ? "BLOCKED by ongoingCriticalAlarm()" : "UNBLOCKED (ongoingCriticalAlarm cleared)"));
      prevHeaterGateBlocked = heaterGateBlocked;
    }
  }
  if (airControlPID.GetMode() == AUTOMATIC)
  {
    // Conditional integration: only integrate once the error is inside
    // anti_windup_offset, and drop back to Ki=0 if it grows past that again
    // (e.g. a large setpoint increase) - previously the only way Ki got
    // back to 0 was startPID() re-zeroing it as a side effect of being
    // called on every HMI heartbeat, which also wiped out integral progress
    // built up while legitimately closing in on the setpoint (see startPID()).
    bool airNearSetpoint = abs(in3.temperature[ROOM_DIGITAL_TEMP_SENSOR] -
                               in3.desiredControlTemperature) <
                           anti_windup_offset[ROOM_DIGITAL_TEMP_SENSOR];
    if (airNearSetpoint && airControlPID.GetKi() != Ki[airPID])
    {
      airControlPID.SetTunings(Kp[airPID], Ki[airPID], Kd[airPID]);
    }
    else if (!airNearSetpoint && airControlPID.GetKi() != 0)
    {
      airControlPID.SetTunings(Kp[airPID], false, Kd[airPID]);
    }
    airControlPID.Compute();
    ledcWrite(HEATER_PWM_CHANNEL, HeaterPIDOutput * !ongoingCriticalAlarm());
  }
  if (skinControlPID.GetMode() == AUTOMATIC)
  {
    if (abs(in3.temperature[SKIN_SENSOR] - in3.desiredControlTemperature) <
        anti_windup_offset[SKIN_SENSOR])
    {
      skinControlPID.SetTunings(Kp[skinPID], Ki[skinPID], Kd[skinPID]);
    }
    skinControlPID.Compute();
    ledcWrite(HEATER_PWM_CHANNEL, HeaterPIDOutput * !ongoingCriticalAlarm());
  }
  if (humidityControlPID.GetMode() == AUTOMATIC)
  {
    if (in3.humidity[ROOM_DIGITAL_HUM_SENSOR] - in3.desiredControlHumidity <
        anti_windup_offset[humidityPID])
    {
      humidityControlPID.SetTunings(Kp[humidityPID], Ki[humidityPID],
                                    Kd[humidityPID]);
    }
    humidityControlPID.Compute();
    if (millis() - windowStartTime >
        humidifierTimeCycle)
    { // time to shift the Relay Window
      windowStartTime += humidifierTimeCycle;
    }
    if (humidityControlPIDOutput < millis() - windowStartTime)
    {
      if (humidifierState || humidifierStateChange)
      {
        in3_hum.turn(OFF);
        humidifierStateChange = false;
      }
      humidifierState = false;
    }
    else
    {
      if (!humidifierState || humidifierStateChange)
      {
        in3_hum.turn(ON);
        humidifierStateChange = false;
      }
      humidifierState = true;
    }
  }
  // Fan closed-loop lifecycle: turnFans() holds the calibrated baseline duty
  // open-loop while the fan spins up; here we hand over to the PID bumplessly
  // once FAN_SPINUP_GRACE_MS has elapsed. Running the loop during the ~3s
  // spin-up made it chase the lagged, still-ramping RPM measurement and wind
  // the duty far past baseline (a ~6000rpm overshoot on a 4000 target).
  {
    static bool fanWasCommanded = false;
    static long fanCommandedAt = 0;
    bool fanActive = in3.fanHasSpeedFeedback && in3.fanPidEnabled &&
                     in3.fanCommandedOn && !ongoingFanCriticalAlarm();
    if (fanActive && !fanWasCommanded)
    {
      fanCommandedAt = millis();
    }
    fanWasCommanded = fanActive;
    if (fanActive)
    {
      if (fanControlPID.GetMode() == AUTOMATIC)
      {
        fanControlPID.Compute();
        ledcWrite(FAN_CTL_PWM_CHANNEL,
                  fanControlPIDOutput * !ongoingFanCriticalAlarm());
      }
      else if (millis() - fanCommandedAt >= FAN_SPINUP_GRACE_MS)
      {
        // Fan has reached ~target open-loop; seed the loop at the baseline
        // it's already running at so PID_v1 latches it into its integral on
        // the MANUAL->AUTOMATIC edge, then take over for slow trim.
        fanControlPIDOutput = in3.fanCtlPWM;
        fanControlPID.SetMode(AUTOMATIC);
      }
    }
  }
}

void startPID(byte var)
{
  logI("[BOOT][DEBUG] startPID called with var=" + String(var) +
       " (0=skinPID,1=airPID,2=humidityPID), in3.controlMode=" +
       String(in3.controlMode));

  // Communication_Receiver() (main.cpp) calls startPID() on every HMI
  // command while temperature/humidity control stays on, not only on a
  // genuine OFF->ON transition or setpoint change. Bail out if the
  // requested loop is already AUTOMATIC so we don't re-zero Ki (below) and
  // wipe out integral progress the anti-windup logic in PIDHandler() has
  // legitimately built up while closing in on the setpoint.
  switch (var)
  {
  case airPID:
    if (airControlPID.GetMode() == AUTOMATIC)
      return;
    break;
  case skinPID:
    if (skinControlPID.GetMode() == AUTOMATIC)
      return;
    break;
  case humidityPID:
    if (humidityControlPID.GetMode() == AUTOMATIC)
      return;
    break;
  }

  if (var != humidityPID &&
      airControlPID.GetMode() != AUTOMATIC &&
      skinControlPID.GetMode() != AUTOMATIC)
  {
    in3.heaterSafeMAXPWM = HEATER_START_PWM;
    // Restart the ramp cadence so each control activation behaves the same,
    // regardless of how heaterPowerConsumptionCheck() was mid-cycle before.
    heaterRampSampleCounter = 0;
  }
  switch (var)
  {
  case airPID:
    airControlPID.SetTunings(Kp[airPID], false, Kd[airPID]);
    airControlPID.SetControllerDirection(DIRECT);
    airControlPID.SetSampleTime(PID_TEMPERATURE_SAMPLE_TIME);
    airControlPID.SetMode(AUTOMATIC);
    airControlPID.SetOutputLimits(0, in3.heaterSafeMAXPWM); // reset safe limits
    break;
  case skinPID:
    skinControlPID.SetTunings(Kp[skinPID], false, Kd[skinPID]);
    skinControlPID.SetControllerDirection(DIRECT);
    skinControlPID.SetSampleTime(PID_TEMPERATURE_SAMPLE_TIME);
    skinControlPID.SetMode(AUTOMATIC);
    skinControlPID.SetOutputLimits(0, in3.heaterSafeMAXPWM); // reset safe limits

    break;
  case humidityPID:
    humidifierStateChange = true;
    windowStartTime = millis();
    humidityControlPID.SetTunings(Kp[humidityPID], false, Kd[humidityPID]);
    humidityControlPID.SetControllerDirection(DIRECT);
    humidityControlPID.SetOutputLimits(
        humidifierTimeCycle * HUMIDIFIER_DUTY_CYCLE_MIN / 100,
        humidifierTimeCycle * HUMIDIFIER_DUTY_CYCLE_MAX / 100);
    humidityControlPID.SetSampleTime(PID_HUMIDITY_SAMPLE_TIME);
    humidityControlPID.SetMode(AUTOMATIC);
    break;
  }
}

void stopPID(byte var)
{
  switch (var)
  {
  case airPID:
    airControlPID.SetMode(MANUAL);
    break;
  case skinPID:
    skinControlPID.SetMode(MANUAL);
    break;
  case humidityPID:
    humidityControlPID.SetMode(MANUAL);
  }
}
