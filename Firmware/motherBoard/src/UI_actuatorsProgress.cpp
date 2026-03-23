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

extern int tft_width, tft_height;

extern PID airControlPID;
extern PID skinControlPID;
extern PID humidityControlPID;

extern in3ator_parameters in3;


void turnActuators(bool mode) {
  ledcWrite(HEATER_PWM_CHANNEL,
            mode * HEATER_MAX_PWM * !ongoingCriticalAlarm());
  if (mode && !ongoingCriticalWiringAlarm()) {
    in3_hum.turn(ON);
  } else {
    in3_hum.turn(OFF);
  }
  turnFans(mode || in3.phototherapy);
}

void stopActuation() {
  stopPID(airPID);
  stopPID(skinPID);
  stopPID(humidityPID);
  turnActuators(OFF);
}

void turnFans(bool mode) {
  GPIOWrite(ACTUATORS_EN, mode || in3.phototherapy);
#if (HW_NUM >= 8)
  // ledcWrite(HEATER_PWM_CHANNEL, mode * HEATER_MAX_PWM);
  ledcWrite(FAN_PWM_CHANNEL, (mode && !ongoingCriticalWiringAlarm()) * FAN_PWM);
#else
  GPIOWrite(FAN, in3.phototherapy || mode && !ongoingCriticalWiringAlarm());
#endif
}

void UIDrawProgressPage() {
  tft.setTextSize(1);
  graphics(page, in3.language, false, false, false, false);
  drawHeading(page, in3.serialNumber);
  setTextColor(COLOUR_MENU_TEXT);
  setSensorsGraphicPosition(page);
  drawActuatorsSeparators();
  if (in3.controlMode) {
    switch (in3.language) {
    case SPANISH:
      textToWrite = (char *)("Temperatura aire");
      break;
    case PORTUGUESE:
      textToWrite = (char *)("Temperatura do ar");
      break;
    case ENGLISH:
      textToWrite = (char *)("Air temperature");
      break;
    case FRENCH:
      textToWrite = (char *)("Temperature de l'air");
      break;
    }
  } else {
    switch (in3.language) {
    case SPANISH:
      textToWrite = (char *)("Temperatura piel");
      break;
    case PORTUGUESE:
      textToWrite = (char *)("temperatura da pele");
      break;
    case ENGLISH:
      textToWrite = (char *)("Skin temperature");
      break;
    case FRENCH:
      textToWrite = (char *)("Temperature de la peau");
      break;
    }
  }
  drawCentreString(textToWrite, tft_width / 2,
                   tempBarPosY - 4 * letter_height / 3, textFontSize);
  if (!in3.controlMode) {
    switch (in3.language) {
    case SPANISH:
      textToWrite = (char *)("Temperatura aire");
      break;
    case PORTUGUESE:
      textToWrite = (char *)("Temperatura do ar");
      break;
    case ENGLISH:
      textToWrite = (char *)("Air temperature");
      break;
    case FRENCH:
      textToWrite = (char *)("Temperature de l'air");
      break;
    }
  } else {
    switch (in3.language) {
    case SPANISH:
      textToWrite = (char *)("Temperatura piel");
      break;
    case PORTUGUESE:
      textToWrite = (char *)("temperatura da pele");
      break;
    case ENGLISH:
      textToWrite = (char *)("Skin temperature");
      break;
    case FRENCH:
      textToWrite = (char *)("Temperature de la peau");
      break;
    }
  }
  drawCentreString(textToWrite, tft_width / 2, tft_height / 2 - letter_height,
                   textFontSize);

  switch (in3.language) {
  case SPANISH:
    textToWrite = (char *)("Humedad");
    break;
  case ENGLISH:
    textToWrite = (char *)("Humidity");
    break;
  case FRENCH:
    textToWrite = (char *)("Humidite");
    break;
  case PORTUGUESE:
    textToWrite = (char *)("Umidade");
    break;
  }
  drawCentreString(textToWrite, tft_width / 2,
                   humBarPosY - 4 * letter_height / 3, textFontSize);
  setTextColor(COLOUR_WARNING_TEXT);
  drawStop();
  setTextColor(COLOUR_MENU_TEXT);
  state_blink = true;
  while (!GPIORead(ENC_SWITCH)) {
    vTaskDelay(pdMS_TO_TICKS(WHILE_LOOP_DELAY));
  }
  if (in3.temperatureControl) {
    if (in3.controlMode == CONTROL_SKIN && !skinProbeIsValid()) {
      in3.controlMode = CONTROL_AIR;
      EEPROM.write(EEPROM_CONTROL_MODE, in3.controlMode);
      EEPROM.commit();
    }
    startPID(in3.controlMode);
  }
  if (in3.humidityControl) {
    startPID(humidityPID);
  }
  if (in3.temperatureControl) {
    printLoadingTemperatureBar(in3.desiredControlTemperature);
    temperatureAtStart = in3.temperature[in3.controlMode];
  }
  if (in3.humidityControl) {
    printLoadingHumidityBar(in3.desiredControlHumidity);
  }
}

void UI_actuatorsProgress() {
  bool exitActuation = false;
  in3.actuation = false;
  if (in3.temperatureControl && in3.humidityControl) {
    in3.actuation = ACTUATION_TEMP_AND_HUMIDITY;
  } else {
    if (in3.temperatureControl) {
      in3.actuation = ACTUATION_TEMPERATURE;
    }
    if (in3.humidityControl) {
      in3.actuation = ACTUATION_HUMIDITY;
    }
  }
  EEPROM.write(EEPROM_CONTROL_ACTIVE, in3.actuation);
  EEPROM.commit();
  alarmTimerStart();
  temperaturePercentage = false;
  page = ACTUATORS_PROGRESS_PAGE;
  UIDrawProgressPage();
  humidityAtStart = in3.humidity[ROOM_DIGITAL_HUM_SENSOR];
  turnFans(ON);
  while (!exitActuation) {
    vTaskDelay(pdMS_TO_TICKS(WHILE_LOOP_DELAY));
    if (in3.temperatureControl) {
      PIDHandler();
    }
    if (in3.humidityControl) {
      PIDHandler();
    }
    while (!GPIORead(ENC_SWITCH)) {
      vTaskDelay(pdMS_TO_TICKS(WHILE_LOOP_DELAY));
      exitActuation = back_mode();
    }
    blinkGoBackMessage();
    updateDisplayHeader();
    checkAlarmsToDisplay();
  }
  stopActuation();
  in3.actuation = false;
  EEPROM.write(EEPROM_CONTROL_ACTIVE, in3.actuation);
  EEPROM.commit();
}
