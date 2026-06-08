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
extern MAM_IncuNest_Humidifier in3_hum;
extern TFT_eSPI tft;
extern RotaryEncoder encoder;

extern bool WIFI_EN;
extern long lastDebugUpdate;
extern long loopCounts;
extern int page;

extern double errorTemperature[SENSOR_TEMP_QTY], temperatureCalibrationPoint;
extern double ReferenceTemperatureRange, ReferenceTemperatureLow;
extern double provisionalReferenceTemperatureLow;
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

// room variables
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

extern IncuNest_parameters in3;

#define CALIBRATION_ERROR 0.05
#define TIME_BETWEEN_SAMPLES 1 // minutes
#define SAMPLES_WITHIN_ERROR 3
#define DEFAULT_CALIBRATION_TEMPERATURE 36
#define MINIMUM_DIFFERENCE_TEMPERATURE 2

bool checkStableTemperatures(double *referenceSensorHistory,
                             double *sensorToCalibrateHistory,
                             int historyLength, double stabilityError, double targetReference)
{

  for (int i = 1; i < historyLength; i++)
  {
    // Check value for the sensor to calibrate
    if (sensorToCalibrateHistory[i - 1] == false)
    {
      return false;
    }
    // Check stability for the sensor to calibrate
    if (abs(sensorToCalibrateHistory[i] - sensorToCalibrateHistory[i - 1]) > stabilityError)
    {
      return false;
    }
    // Check stability for the reference sensor
    if (abs(referenceSensorHistory[i] - referenceSensorHistory[i - 1]) > stabilityError)
    {
      return false;
    }
    // Check error for the reference sensor and calibration range
    if (targetReference)
    {
      if (abs(referenceSensorHistory[i] - targetReference) > stabilityError)
      {
        return false;
      }
    }
    else
    {
      if (abs(referenceSensorHistory[i] - DEFAULT_CALIBRATION_TEMPERATURE) < MINIMUM_DIFFERENCE_TEMPERATURE)
      {
        return false;
      }
    }
  }
  return true;
}

void autoCalibration()
{
  bool exitCalibrationMenu = false;
  in3.alarmsEnabled = false;
  byte numWords = 1;
  long lastTemperatureMeasurement = millis();
  int historyLengthPosition = false;
  double referenceSensorHistory[SAMPLES_WITHIN_ERROR];
  double sensorToCalibrateHistory[SAMPLES_WITHIN_ERROR];
  referenceSensorHistory[0] = in3.temperature[ROOM_DIGITAL_TEMP_SENSOR];
  sensorToCalibrateHistory[0] = in3.temperature[SKIN_SENSOR];
  page = AUTO_CALIBRATION_PAGE;
  print_text = true;
  tft.setTextSize(1);
  setTextColor(COLOUR_MENU_TEXT);
  for (int i = false; i < numWords; i++)
  {
    pos_text[i] = LEFT_MARGIN;
  }
  pos_text[SET_CALIB_UI_ROW] = CENTER;
  switch (in3.language)
  {
  case ENGLISH:
    words[AUTO_CALIB_MESSAGE_UI_ROW] =
        (char *)("Calibrating...");
    break;
  case SPANISH:
    words[AUTO_CALIB_MESSAGE_UI_ROW] =
        (char *)("Calibrating...");
    break;
  case FRENCH:
    words[AUTO_CALIB_MESSAGE_UI_ROW] =
        (char *)("Calibrating...");
    break;
  case PORTUGUESE:
    words[AUTO_CALIB_MESSAGE_UI_ROW] =
        (char *)("Calibrating...");
    break;
  }
  menu_rows = numWords;
  graphics(page, in3.language, print_text, menu_rows, false, false);
  drawHeading(page, in3.serialNumber);
  bar_pos = true;
  ypos = graphicHeight(bar_pos - 1);
  while (!GPIORead(ENC_SWITCH))
  {
    vTaskDelay(pdMS_TO_TICKS(SWITCH_DEBOUNCE_TIME_MS));
  }
  autoCalibrationProcess = setupAutoCalibrationPoint;
  while (!exitCalibrationMenu)
  {
    vTaskDelay(pdMS_TO_TICKS(CALIBRATION_TASK_PERIOD_MS));
    switch (autoCalibrationProcess)
    {
    case setupAutoCalibrationPoint:
      logI(
          "=================================================point 0");
      clearCalibrationValues();
      autoCalibrationProcess = firstAutoCalibrationPoint;
      turnFans(ON);
      break;
    case firstAutoCalibrationPoint:
      if (!GPIORead(ENC_SWITCH) ||
          checkStableTemperatures(referenceSensorHistory,
                                  sensorToCalibrateHistory,
                                  SAMPLES_WITHIN_ERROR, CALIBRATION_ERROR, false))
      {
        provisionalReferenceTemperatureLow =
            in3.temperature[ROOM_DIGITAL_TEMP_SENSOR];
        provisionalRawTemperatureLow[SKIN_SENSOR] =
            in3.temperature[SKIN_SENSOR];
        while (!GPIORead(ENC_SWITCH))
        {
          vTaskDelay(pdMS_TO_TICKS(SWITCH_DEBOUNCE_TIME_MS));
          exitCalibrationMenu = back_mode();
          vTaskDelay(pdMS_TO_TICKS(SWITCH_DEBOUNCE_TIME_MS));
        }
        in3.desiredControlTemperature = DEFAULT_CALIBRATION_TEMPERATURE;
        startPID(airPID);
        autoCalibrationProcess = secondAutoCalibrationPoint;
        referenceSensorHistory[historyLengthPosition] = false;
        sensorToCalibrateHistory[historyLengthPosition] = false;
        logI(
            "=================================================point 1");
      }
      break;
    case secondAutoCalibrationPoint:
      PIDHandler();
      if (!GPIORead(ENC_SWITCH) ||
          checkStableTemperatures(referenceSensorHistory,
                                  sensorToCalibrateHistory,
                                  SAMPLES_WITHIN_ERROR, CALIBRATION_ERROR, DEFAULT_CALIBRATION_TEMPERATURE))
      {
        logI(
            "=================================================point 2");
        ReferenceTemperatureLow = provisionalReferenceTemperatureLow;
        RawTemperatureLow[SKIN_SENSOR] =
            provisionalRawTemperatureLow[SKIN_SENSOR];
        ReferenceTemperatureRange =
            in3.temperature[ROOM_DIGITAL_TEMP_SENSOR] -
            ReferenceTemperatureLow;
        RawTemperatureRange[SKIN_SENSOR] =
            (in3.temperature[SKIN_SENSOR] - RawTemperatureLow[SKIN_SENSOR] + SKIN_CALIBRATION_CORRECTION_FACTOR);
        logI("calibration factors: " + String(RawTemperatureLow[SKIN_SENSOR]) +
             "," + String(RawTemperatureRange[SKIN_SENSOR]) + "," +
             String(ReferenceTemperatureRange) + "," +
             String(ReferenceTemperatureLow));
        saveCalibrationToEEPROM();
        ledcWrite(HEATER_PWM_CHANNEL, false);
        turnFans(OFF);
        exitCalibrationMenu = true;
        stopPID(airPID);
      }
      break;
    }
    if (millis() - lastTemperatureMeasurement >
        minsToMillis(TIME_BETWEEN_SAMPLES))
    {
      lastTemperatureMeasurement = millis();
      if (historyLengthPosition == SAMPLES_WITHIN_ERROR)
      {
        historyLengthPosition = false;
      }
      referenceSensorHistory[historyLengthPosition] =
          in3.temperature[ROOM_DIGITAL_TEMP_SENSOR];
      sensorToCalibrateHistory[historyLengthPosition] =
          in3.temperature[SKIN_SENSOR];
      historyLengthPosition++;
      for (int i = 1; i < SAMPLES_WITHIN_ERROR; i++)
      {
        logI(String(abs(*referenceSensorHistory - *(referenceSensorHistory + i))));
        logI(String(abs(*sensorToCalibrateHistory - *(sensorToCalibrateHistory + i))));
      }
    }
  }
  in3.alarmsEnabled = true;
  UI_mainMenu();
}

void fineTuneCalibration()
{
  byte numWords = 2;
  in3.fineTuneSkinTemperature = false;
  in3.fineTuneAirTemperature = false;
  page = FINE_TUNE_CALIBRATION_PAGE;
  print_text = true;
  tft.setTextSize(1);
  setTextColor(COLOUR_MENU_TEXT);
  for (int i = false; i < numWords; i++)
  {
    pos_text[i] = LEFT_MARGIN;
  }
  pos_text[SET_CALIB_UI_ROW] = CENTER;
  words[TEMP_CALIB_UI_ROW] =
      (char *)("Temperature adjust");
  words[SET_CALIB_UI_ROW] = (char *)("SET");
  menu_rows = numWords;
  graphics(page, in3.language, print_text, menu_rows, false, false);
  drawHeading(page, in3.serialNumber);
  bar_pos = true;
  ypos = graphicHeight(bar_pos - 1);
  setTextColor(COLOUR_MENU_TEXT);
  drawFloat(in3.temperature[SKIN_SENSOR], 1, valuePosition, ypos, textFontSize);
  while (!GPIORead(ENC_SWITCH))
  {
    vTaskDelay(pdMS_TO_TICKS(SWITCH_DEBOUNCE_TIME_MS));
  }
}

void firstPointCalibration()
{
  byte numWords = 2;
  page = FIRST_POINT_CALIBRATION_PAGE;
  print_text = true;
  tft.setTextSize(1);
  setTextColor(COLOUR_MENU_TEXT);
  for (int i = false; i < numWords; i++)
  {
    pos_text[i] = LEFT_MARGIN;
  }
  pos_text[SET_CALIB_UI_ROW] = CENTER;
  switch (in3.language)
  {
  case ENGLISH:
    words[TEMP_CALIB_UI_ROW] =
        (char *)("First point");
    words[SET_CALIB_UI_ROW] = (char *)("SET");
    break;
  case SPANISH:
    words[TEMP_CALIB_UI_ROW] =
        (char *)("First point");
    words[SET_CALIB_UI_ROW] = (char *)("SET");
    break;
  case FRENCH:
    words[TEMP_CALIB_UI_ROW] =
        (char *)("First point");
    words[SET_CALIB_UI_ROW] = (char *)("SET");
    break;
  case PORTUGUESE:
    words[TEMP_CALIB_UI_ROW] =
        (char *)("First point");
    words[SET_CALIB_UI_ROW] = (char *)("SET");
    break;
  }
  menu_rows = numWords;
  graphics(page, in3.language, print_text, menu_rows, false, false);
  drawHeading(page, in3.serialNumber);
  bar_pos = true;
  ypos = graphicHeight(bar_pos - 1);
  setTextColor(COLOUR_MENU_TEXT);
  drawFloat(in3.temperature[SKIN_SENSOR], 1, valuePosition, ypos, textFontSize);
  while (!GPIORead(ENC_SWITCH))
  {
    vTaskDelay(pdMS_TO_TICKS(SWITCH_DEBOUNCE_TIME_MS));
  }
}

void secondPointCalibration()
{
  byte numWords = 2;
  page = SECOND_POINT_CALIBRATION_PAGE;
  print_text = true;
  tft.setTextSize(1);
  setTextColor(COLOUR_MENU_TEXT);
  for (int i = false; i < numWords; i++)
  {
    pos_text[i] = LEFT_MARGIN;
  }
  pos_text[SET_CALIB_UI_ROW] = CENTER;
  switch (in3.language)
  {
  case ENGLISH:
    words[TEMP_CALIB_UI_ROW] =
        (char *)("Second point");
    words[SET_CALIB_UI_ROW] = (char *)("SET");
    break;
  case SPANISH:
    words[TEMP_CALIB_UI_ROW] =
        (char *)("Second point");
    words[SET_CALIB_UI_ROW] = (char *)("SET");
    break;
  case FRENCH:
    words[TEMP_CALIB_UI_ROW] =
        (char *)("Second point");
    words[SET_CALIB_UI_ROW] = (char *)("SET");
    break;
  case PORTUGUESE:
    words[TEMP_CALIB_UI_ROW] =
        (char *)("Second point");
    words[SET_CALIB_UI_ROW] = (char *)("SET");
    break;
  }
  menu_rows = numWords;
  graphics(page, in3.language, print_text, menu_rows, false, false);
  drawHeading(page, in3.serialNumber);
  bar_pos = true;
  ypos = graphicHeight(bar_pos - 1);
  setTextColor(COLOUR_MENU_TEXT);
  drawFloat(in3.temperature[SKIN_SENSOR], 1, valuePosition, ypos, textFontSize);
  while (!GPIORead(ENC_SWITCH))
  {
    vTaskDelay(pdMS_TO_TICKS(SWITCH_DEBOUNCE_TIME_MS));
  }
}

bool checkStableCurrentConsumption(double *referenceSensorHistory,
                                   int historyLength, double stabilityError)
{
  for (int i = 0; i < historyLength; i++)
  {
    if (abs(*referenceSensorHistory - *(referenceSensorHistory + i)) >
        stabilityError)
    {
      return false;
    }
  }
  return true;
}

void clearCalibrationValues()
{
  RawTemperatureLow[SKIN_SENSOR] = false;
  RawTemperatureRange[SKIN_SENSOR] = false;
  RawTemperatureLow[ROOM_DIGITAL_TEMP_SENSOR] = false;
  RawTemperatureRange[ROOM_DIGITAL_TEMP_SENSOR] = false;
  ReferenceTemperatureRange = false;
  ReferenceTemperatureLow = false;
}
