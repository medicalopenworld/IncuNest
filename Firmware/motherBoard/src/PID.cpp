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
long lastHeaterPowerCheck = false;

extern in3ator_parameters in3;
extern MAM_in3ator_Humidifier in3_hum;
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

void PIDInit()
{
  airControlPID.SetMode(MANUAL);
  skinControlPID.SetMode(MANUAL);
  humidityControlPID.SetMode(MANUAL);
}

void heaterPowerConsumptionCheck()
{
  int heaterSafeMAXPWM_before = in3.heaterSafeMAXPWM;
  if (millis() - lastHeaterPowerCheck > CURRENT_CHECK_PERIOD_MS)
  {
    lastHeaterPowerCheck = millis();
    if (in3.heater_current > HEATER_MAX_POWER_AMPS || in3.system_current > HEATER_MAX_POWER_AMPS)
    {
      in3.heaterSafeMAXPWM -= HEATER_POWER_FACTOR_DECREASE;
      if (in3.heaterSafeMAXPWM < 0)
      {
        in3.heaterSafeMAXPWM = 0;
      }
    }
    else if (in3.heater_current < HEATER_SAFE_POWER_AMPS || in3.system_current < HEATER_SAFE_POWER_AMPS)
    {
      in3.heaterSafeMAXPWM += HEATER_POWER_FACTOR_INCREASE;
      if (in3.heaterSafeMAXPWM > HEATER_MAX_PWM)
      {
        in3.heaterSafeMAXPWM = HEATER_MAX_PWM;
      }
    }
    if (heaterSafeMAXPWM_before != in3.heaterSafeMAXPWM)
    {
      logI("[PID] -> Heater current is " + String(in3.heater_current) + ", changed max PWM to: " + String(in3.heaterSafeMAXPWM));
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
}

void PIDHandler()
{
  heaterPowerConsumptionCheck();
  if (airControlPID.GetMode() == AUTOMATIC)
  {
    if (abs(in3.temperature[ROOM_DIGITAL_TEMP_SENSOR] -
            in3.desiredControlTemperature) <
            anti_windup_offset[ROOM_DIGITAL_TEMP_SENSOR] &&
        airControlPID.GetKi() != Ki[airPID])
    {
      airControlPID.SetTunings(Kp[airPID], Ki[airPID], Kd[airPID]);
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
}

void startPID(byte var)
{
  in3.heaterSafeMAXPWM = HEATER_START_PWM;
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
    airControlPID.SetSampleTime(PID_HUMIDITY_SAMPLE_TIME);
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
