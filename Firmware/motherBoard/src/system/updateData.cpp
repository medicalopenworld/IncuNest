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
#include <Arduino.h>

#include "main.h"

extern long lastDebugUpdate;
extern long loopCounts;

extern double errorTemperature[SENSOR_TEMP_QTY];

extern bool digitalCurrentSensorPresent[2];

extern double HeaterPIDOutput;
extern double humidityControlPIDOutput;
extern int humidifierTimeCycle;

extern PID airControlPID;
extern PID skinControlPID;
extern PID humidityControlPID;

long lastHumToggle;
bool humToggle;

bool activeStatus, lastActiveStatus;

extern IncuNest_parameters in3;

void timeTrackHandler() {
  if (in3.temperatureControl || in3.humidityControl || in3.phototherapy) {
    activeStatus = true;
    if (millis() - in3.last_check_time > TIME_TRACK_UPDATE_PERIOD) {
      in3.last_check_time = millis();
      in3.control_active_time += millisToHours(TIME_TRACK_UPDATE_PERIOD);
      if (in3.temperatureControl) {
        in3.heater_active_time += millisToHours(TIME_TRACK_UPDATE_PERIOD);
        in3.fan_active_time += millisToHours(TIME_TRACK_UPDATE_PERIOD);
      }
      if (in3.humidityControl) {
        in3.humidifier_active_time += millisToHours(TIME_TRACK_UPDATE_PERIOD);
        if (!in3.temperatureControl) {
          in3.fan_active_time += millisToHours(TIME_TRACK_UPDATE_PERIOD);
        }
      }
      if (in3.phototherapy) {
        in3.phototherapy_active_time += millisToHours(TIME_TRACK_UPDATE_PERIOD);
      }
      { Preferences p; p.begin(NS_RT, false);
        p.putFloat(KEY_RT_CTRL,   in3.control_active_time);
        p.putFloat(KEY_RT_HEATER, in3.heater_active_time);
        p.putFloat(KEY_RT_FAN,    in3.fan_active_time);
        p.putFloat(KEY_RT_HUM,    in3.humidifier_active_time);
        p.putFloat(KEY_RT_PHOTO,  in3.phototherapy_active_time);
        p.end(); }
    }
  } else {
    activeStatus = false;
    if (millis() - in3.last_check_time > TIME_TRACK_UPDATE_PERIOD) {
      in3.last_check_time = millis();
      in3.standby_time += millisToHours(TIME_TRACK_UPDATE_PERIOD);
      { Preferences p; p.begin(NS_RT, false); p.putFloat(KEY_RT_STANDBY, in3.standby_time); p.end(); }
    }
  }
  if (activeStatus != lastActiveStatus) {
    in3.last_check_time = millis();
  }
  lastActiveStatus = activeStatus;
}

void updateData() {
  if (LOG_INFORMATION) {
    loopCounts++;
    if ((millis() - lastDebugUpdate > DEBUG_LOOP_PRINT)) {
      if (airControlPID.GetMode() == AUTOMATIC) {
        logI("[PID] -> Heater PWM output is: " +
             String(100 * HeaterPIDOutput / HEATER_MAX_PWM) + "%");
        logI("[PID] -> Desired air temp is: " +
             String(in3.desiredControlTemperature) + "ºC");
      }
      if (skinControlPID.GetMode() == AUTOMATIC) {
        logI("[PID] -> Heater PWM output is: " +
             String(100 * HeaterPIDOutput / HEATER_MAX_PWM) + "%");
        logI("[PID] -> Desired skin temp is: " +
             String(in3.desiredControlTemperature) + "ºC");
      }
      if (humidityControlPID.GetMode() == AUTOMATIC) {
        logI("[PID] -> Humidifier output is: " +
             String(100 * humidityControlPIDOutput / humidifierTimeCycle) +
             "%");
        logI("[PID] -> Desired humditity is: " +
             String(in3.desiredControlHumidity) + "%");
      }

      logI("[SENSORS] -> Baby temperature: " +
           String(in3.temperature[SKIN_SENSOR]) + "ºC, correction error is " +
           String(errorTemperature[SKIN_SENSOR]));
      logI("[SENSORS] -> Air temperature: " +
           String(in3.temperature[ROOM_DIGITAL_TEMP_SENSOR]) +
           "ºC, correction error is " +
           String(errorTemperature[ROOM_DIGITAL_TEMP_SENSOR]));
      logI("[SENSORS] -> Humidity: " +
           String(in3.humidity[ROOM_DIGITAL_HUM_SENSOR]) + "%");
      logI("[SENSORS] -> fan speed: " + String(in3.fan_rpm) + " rpm");

      logI("[SENSORS] -> System current consumption is: " +
           String(in3.system_current, 2) + " Amps");
      if (digitalCurrentSensorPresent[MAIN]) {
        logI("[SENSORS] -> System voltage is: " +
             String(in3.system_voltage, 2) + " V");
        logI("[SENSORS] -> Phototherapy current consumption is: " +
             String(in3.phototherapy_current, 4) + " Amps");
        logI("[SENSORS] -> Fan current consumption is: " +
             String(in3.fan_current, 4) + " Amps");
      }
      if (digitalCurrentSensorPresent[SECUNDARY]) {
        logI("[SENSORS] -> USB current is: " + String(in3.USB_current, 4) +
             " Amps");
        logI("[SENSORS] -> USB voltage is: " + String(in3.USB_voltage, 2) +
             " V");
        logI("[SENSORS] -> BATTERY charge current is: " +
             String(in3.BATTERY_current, 4) + " Amps");
        logI("[SENSORS] -> BATTERY voltage is: " +
             String(in3.BATTERY_voltage, 2) + " V");
      }
      logI("[ALARMS] -> Alarms active: " + String(activeAlarm()));
      // logI("[SENSORS] -> ON_OFF: " + String(GPIORead(ON_OFF_SWITCH)));
      logI("[LATENCY] -> Looped " +
           String(loopCounts * 1000 / (millis() - lastDebugUpdate)) +
           " Times per second");
      loopCounts = 0;
      lastDebugUpdate = millis();
    }
  }
}