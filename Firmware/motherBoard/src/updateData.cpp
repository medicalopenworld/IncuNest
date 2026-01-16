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
extern TwoWire *wire;
extern MAM_in3ator_Humidifier in3_hum;
extern TFT_eSPI tft;
extern RotaryEncoder encoder;

extern bool WIFI_EN;
extern long lastDebugUpdate;
extern long loopCounts;
extern int page;

extern double errorTemperature[SENSOR_TEMP_QTY], temperatureCalibrationPoint;
extern double ReferenceTemperatureRange, ReferenceTemperatureLow;
extern double provisionalReferenceTemperatureLow;
extern double fineTuneSkinTemperature;
extern double RawTemperatureLow[SENSOR_TEMP_QTY],
    RawTemperatureRange[SENSOR_TEMP_QTY];
extern double provisionalRawTemperatureLow[SENSOR_TEMP_QTY];
extern int temperature_array_pos; // temperature sensor number turn to measure
extern float diffSkinTemperature,
    diffAirTemperature; // difference between measured temperature and user
                        // input real temperature
extern bool humidifierState, humidifierStateChange;
extern int previousHumidity; // previous sampled humidity
extern float diffHumidity;   // difference between measured humidity and user
                             // input real humidity

extern byte autoCalibrationProcess;

// Sensor check rate (in ms). Both sensors are checked in same interrupt and
// they have different check rates
extern byte encoderRate;
extern byte encoderCount;

extern volatile long lastEncPulse;
extern volatile bool statusEncSwitch;

// WIFI
extern bool WIFI_connection_status;

extern bool digitalCurrentSensorPresent[2];

// room variables
extern const float minDesiredTemp[2]; // minimum allowed temperature to be set
extern const float maxDesiredTemp[2]; // maximum allowed temperature to be set
extern const int presetTemp[2];       // preset baby skin temperature

extern boolean A_set;
extern boolean B_set;
extern int encoderpinA;                 // pin  encoder A
extern int encoderpinB;                 // pin  encoder B
extern bool encPulsed, encPulsedBefore; // encoder switch status
extern bool updateUIData;
extern volatile int EncMove;     // moved encoder
extern volatile int lastEncMove; // moved last encoder
extern volatile int
    EncMoveOrientation;            // set to -1 to increase values clockwise
extern int last_encoder_move;      // moved encoder
extern long encoder_debounce_time; // in milliseconds, debounce time in encoder
                                   // to filter signal bounces
extern long last_encPulsed;        // last time encoder was pulsed

// Text Graphic position variables
extern int humidityX;
extern int humidityY;
extern int temperatureX;
extern int temperatureY;
extern int ypos;
extern bool print_text;
extern int initialSensorPosition;
extern bool pos_text[8];

extern bool enableSet;
extern float temperaturePercentage, temperatureAtStart;
extern float humidityPercentage, humidityAtStart;
extern int barWidth, barHeight, tempBarPosX, tempBarPosY, humBarPosX,
    humBarPosY;
extern int screenTextColor, screenTextBackgroundColour;

// User Interface display variables
extern bool autoLock; // setting that enables backlight switch OFF after a
                      // given time of no user actions
extern long
    lastbacklightHandler; // last time there was a encoder movement or pulse

extern bool selected;
extern char cstring[128];
extern char *textToWrite;
extern char *words[12];
extern char *helpMessage;
extern byte bar_pos;
extern byte menu_rows;
extern byte length;
extern long lastGraphicSensorsUpdate;
extern long lastSensorsUpdate;
extern bool enableSetProcess;
extern long blinking;
extern bool state_blink;
extern bool blinkSetMessageState;
extern long lastBlinkSetMessage;

extern double HeaterPIDOutput;
extern double skinControlPIDInput;
extern double airControlPIDInput;
extern double humidityControlPIDOutput;
extern int humidifierTimeCycle;
extern unsigned long windowStartTime;

extern PID airControlPID;
extern PID skinControlPID;
extern PID humidityControlPID;

extern int ScreenBacklightMode;

long lastHumToggle;
bool humToggle;

bool activeStatus, lastActiveStatus;

float previousTemperature[2];

extern in3ator_parameters in3;

void updateDisplaySensors() {
  float temperatureToUpdate;
  if (page == MAIN_MENU_PAGE || (page == ACTUATORS_PROGRESS_PAGE)) {
    drawSelectedTemperature(in3.temperature[in3.controlMode],
                            previousTemperature[in3.controlMode]);
    previousTemperature[in3.controlMode] = in3.temperature[in3.controlMode];
    drawHumidity(in3.humidity[ROOM_DIGITAL_HUM_SENSOR], previousHumidity);
    previousHumidity = in3.humidity[ROOM_DIGITAL_HUM_SENSOR];
  }
  if (page == ACTUATORS_PROGRESS_PAGE) {
    drawUnselectedTemperature(in3.temperature[!in3.controlMode],
                              previousTemperature[!in3.controlMode]);
    previousTemperature[!in3.controlMode] = in3.temperature[!in3.controlMode];
    setTextColor(COLOUR_MENU_TEXT);
    if (in3.temperatureControl) {
      float previousTemperaturePercentage = temperaturePercentage;
      if (in3.controlMode) {
        temperatureToUpdate = in3.temperature[ROOM_DIGITAL_TEMP_SENSOR];
      } else {
        temperatureToUpdate = in3.temperature[SKIN_SENSOR];
      }
      if ((in3.desiredControlTemperature - temperatureAtStart)) {
        temperaturePercentage =
            100 - ((in3.desiredControlTemperature - temperatureToUpdate) * 100 /
                   (in3.desiredControlTemperature - temperatureAtStart));
      }
      if (temperaturePercentage > 99) {
        temperaturePercentage = 100;
      }
      if (temperaturePercentage < 0) {
        temperaturePercentage = false;
      }
      updateLoadingTemperatureBar(int(previousTemperaturePercentage),
                                  int(temperaturePercentage));
    }
    if (in3.humidityControl) {
      float previousHumidityPercentage = humidityPercentage;
      if ((in3.desiredControlHumidity - humidityAtStart)) {
        humidityPercentage =
            100 - ((in3.desiredControlHumidity -
                    in3.humidity[ROOM_DIGITAL_HUM_SENSOR]) *
                   100 / (in3.desiredControlHumidity - humidityAtStart));
      }
      if (humidityPercentage > 99) {
        humidityPercentage = 100;
      }
      if (humidityPercentage < 0) {
        humidityPercentage = false;
      }
      updateLoadingHumidityBar(int(previousHumidityPercentage),
                               int(humidityPercentage));
    }
  }
}

void logI(String dataString) {
  if (LOG_INFORMATION) {
    static const char *TAG_USER = "APP";
    if (log_mutex == NULL ||
        xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      // Formato: "123: mensaje"
      ESP_LOGI(TAG_USER, "%lu: %s", millis() / 1000, dataString.c_str());
      if (log_mutex)
        xSemaphoreGiveRecursive(log_mutex);
    }
  }
}

void logCon(String dataString) {
  if (!LOG_GPRS)
    return;

  static const char *TAG_USER = "APP";
  if (log_mutex == NULL ||
      xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    ESP_LOGI(TAG_USER, "%lu: %s", millis() / 1000, dataString.c_str());
    if (log_mutex)
      xSemaphoreGiveRecursive(log_mutex);
  }
}

void logModemData(String dataString) {
  if (!LOG_MODEM_DATA)
    return;

  static const char *TAG_USER = "APP";
  if (log_mutex == NULL ||
      xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    ESP_LOGI(TAG_USER, "%lu: %s", millis() / 1000, dataString.c_str());
    if (log_mutex)
      xSemaphoreGiveRecursive(log_mutex);
  }
}

void logE(String dataString) {
  if (!LOG_ERRORS)
    return;

  static const char *TAG_USER = "APP";
  if (log_mutex == NULL ||
      xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    ESP_LOGI(TAG_USER, "%lu: %s", millis() / 1000, dataString.c_str());
    if (log_mutex)
      xSemaphoreGiveRecursive(log_mutex);
  }
}

void logAlarm(String dataString) {
  if (!LOG_ALARMS)
    return;

  static const char *TAG_USER = "APP";
  if (log_mutex == NULL ||
      xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    ESP_LOGI(TAG_USER, "%lu: %s", millis() / 1000, dataString.c_str());
    if (log_mutex)
      xSemaphoreGiveRecursive(log_mutex);
  }
}

void backlightHandler() {
  if (autoLock) {
    if (millis() - lastbacklightHandler > BACKLIGHT_NO_INTERACTION_TIME &&
        !ongoingAlarms()) {
      if (ScreenBacklightMode != BL_POWERSAVE) {
        ledcWrite(SCREENBACKLIGHT_PWM_CHANNEL, BACKLIGHT_POWER_SAFE);
        ScreenBacklightMode = BL_POWERSAVE;
      }
    } else {
      if (ScreenBacklightMode != BL_NORMAL) {
        ledcWrite(SCREENBACKLIGHT_PWM_CHANNEL, BACKLIGHT_POWER_DEFAULT);
        ScreenBacklightMode = BL_NORMAL;
      }
    }
  } else {
    if (ScreenBacklightMode != BL_NORMAL) {
      ledcWrite(SCREENBACKLIGHT_PWM_CHANNEL, BACKLIGHT_POWER_DEFAULT);
      ScreenBacklightMode = BL_NORMAL;
    }
  }
}

void timeTrackHandler() {
  if (in3.temperatureControl || in3.humidityControl || in3.phototherapy) {
    activeStatus = true;
    if (millis() - in3.last_check_time > TIME_TRACK_UPDATE_PERIOD) {
      in3.last_check_time = millis();
      in3.control_active_time += millisToHours(TIME_TRACK_UPDATE_PERIOD);
      EEPROM.writeFloat(EEPROM_CONTROL_ACTIVE_TIME, in3.control_active_time);
      if (in3.temperatureControl) {
        in3.heater_active_time += millisToHours(TIME_TRACK_UPDATE_PERIOD);
        in3.fan_active_time += millisToHours(TIME_TRACK_UPDATE_PERIOD);
        EEPROM.writeFloat(EEPROM_HEATER_ACTIVE_TIME, in3.heater_active_time);
        EEPROM.writeFloat(EEPROM_FAN_ACTIVE_TIME, in3.fan_active_time);
      }
      if (in3.humidityControl) {
        in3.humidifier_active_time += millisToHours(TIME_TRACK_UPDATE_PERIOD);
        EEPROM.writeFloat(EEPROM_HUMIDIFIER_ACTIVE_TIME,
                          in3.humidifier_active_time);
        if (!in3.temperatureControl) {
          in3.fan_active_time += millisToHours(TIME_TRACK_UPDATE_PERIOD);
          EEPROM.writeFloat(EEPROM_FAN_ACTIVE_TIME, in3.fan_active_time);
        }
      }
      if (in3.phototherapy) {
        in3.phototherapy_active_time += millisToHours(TIME_TRACK_UPDATE_PERIOD);
        EEPROM.writeFloat(EEPROM_PHOTOTHERAPY_ACTIVE_TIME,
                          in3.phototherapy_active_time);
      }
      EEPROM.commit();
    }
  } else {
    activeStatus = false;
    if (millis() - in3.last_check_time > TIME_TRACK_UPDATE_PERIOD) {
      in3.last_check_time = millis();
      in3.standby_time += millisToHours(TIME_TRACK_UPDATE_PERIOD);
      EEPROM.writeFloat(EEPROM_STANDBY_TIME, in3.standby_time);
      EEPROM.commit();
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