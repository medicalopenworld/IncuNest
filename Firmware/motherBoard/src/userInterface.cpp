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

extern int tft_width, tft_height;

extern PID airControlPID;
extern PID skinControlPID;
extern PID humidityControlPID;

extern in3ator_parameters in3;

void updateDisplayHeader() {
  if (millis() - lastGraphicSensorsUpdate > UI_SENSOR_UPDATE_PERIOD_MS) {
    if (page == MAIN_MENU_PAGE) {
      UI_updateConnectivityEvents();
    }
    if (page == MAIN_MENU_PAGE || page == ACTUATORS_PROGRESS_PAGE) {
      updateDisplaySensors();
    }
    lastGraphicSensorsUpdate = millis();
  }
}

void checkAlarmsToDisplay() {
  byte alarmToDisplay = alarmPendingToDisplay();
  byte alarmToClear = alarmPendingToClear();
  if (alarmToDisplay) {
    drawAlarmMessage(alarmIDtoString(alarmToDisplay));
    clearDisplayedAlarm(alarmToDisplay);
  }
  if (alarmToClear) {
    clearAlarmPendingToClear(alarmToClear);
    drawHeading(page, in3.serialNumber);
    if (ongoingAlarms()) {
      drawAlarmMessage(alarmIDtoString(activeAlarm()));
    }
  }
}

void bar_pos_handler(int UI_page) {
  if (EncMove && !selected) {
    if (EncMove < 0) {
      EncMove++;
      if (UI_page == MAIN_MENU_PAGE) {
        enableSetProcess = enableSet;
      } else {
        enableSetProcess = true;
      }
      if (bar_pos < menu_rows - !enableSetProcess) {
        eraseBar(menu_rows, bar_pos);
        bar_pos++;
        updateBar(menu_rows, bar_pos);
      }
    } else {
      EncMove--;
      if (bar_pos > 1) {
        eraseBar(menu_rows, bar_pos);
        bar_pos--;
        updateBar(menu_rows, bar_pos);
      }
    }
    ypos = graphicHeight(bar_pos - 1);
  }
}

void bar_highlight() {
  if (menu_rows) {
    if (selected) {
      tft.fillRect(0,
                   (tft_height - TFT_HEIGHT_HEADING) * (bar_pos - 1) /
                           menu_rows +
                       TFT_HEIGHT_HEADING,
                   width_select, (tft_height - TFT_HEIGHT_HEADING) / menu_rows,
                   COLOUR_CHOSEN);
    } else {
      tft.fillRect(
          0,
          (tft_height - TFT_HEIGHT_HEADING) * (bar_pos - 1) / menu_rows +
              TFT_HEIGHT_HEADING,
          width_select, (tft_height - TFT_HEIGHT_HEADING) / menu_rows, WHITE);
    }
    for (int i = 2; i <= menu_rows; i++) {
      tft.fillRect(0,
                   (tft_height - TFT_HEIGHT_HEADING) * (i - 1) / menu_rows +
                       TFT_HEIGHT_HEADING - 1,
                   tft_height, TFT_SEPARATOR_HEIGHT, WHITE); // mejorable
    }
  }
}

void userInterfaceHandler(int UI_page) {
  // Detect external control mode change (e.g. skin probe auto-switch to AIR)
  static bool lastControlMode = DEFAULT_CONTROL_MODE;
  if (UI_page == MAIN_MENU_PAGE && in3.controlMode != lastControlMode) {
    lastControlMode = in3.controlMode;
    UI_mainMenu();
    return;
  }
  lastControlMode = in3.controlMode;

  updateDisplayHeader();
  checkSetMessage(UI_page, menu_rows);
  checkAlarmsToDisplay();
  bar_pos_handler(UI_page);

  if (!GPIORead(ENC_SWITCH)) {
    selected = !selected;
    bar_highlight();
    if (!encoderContinuousPress(UI_page)) {
      switch (UI_page) {
      case MAIN_MENU_PAGE:
        switch (bar_pos - graphicTextOffset) {
        case CONTROL_MODE_UI_ROW:
          if (in3.controlMode == CONTROL_AIR && !skinProbeIsValid()) {
            break; // Cannot switch to SKIN: probe not detected/validated
          }
          in3.controlMode = !in3.controlMode;
          EEPROM.write(EEPROM_CONTROL_MODE, in3.controlMode);
          EEPROM.commit();
          UI_mainMenu();
          break;
        case TEMPERATURE_UI_ROW:
          while (GPIORead(ENC_SWITCH)) {
            vTaskDelay(pdMS_TO_TICKS(WHILE_LOOP_DELAY));
            if (EncMove) {
              if (!in3.temperatureControl) {
                in3.temperatureControl = true;
                drawRightString((char *)(initialSensorsValue),
                                initialSensorPosition, temperatureY,
                                textFontSize);
                setTextColor(COLOUR_MENU_TEXT);
                drawFloat(in3.desiredControlTemperature, 1, temperatureX - 65,
                          temperatureY, textFontSize);
                enableSet = true;
              }
              if (EncMove > 0) {
                if (in3.desiredControlTemperature >
                    minDesiredTemp[in3.controlMode]) {
                  updateUIData = true;
                }
              } else {
                if (in3.desiredControlTemperature <
                    maxDesiredTemp[in3.controlMode]) {
                  updateUIData = true;
                }
              }
              if (updateUIData) {
                setTextColor(COLOUR_MENU);
                drawFloat(in3.desiredControlTemperature, 1, temperatureX - 65,
                          temperatureY, textFontSize);
                in3.desiredControlTemperature -=
                    float(EncMove) * stepTemperatureIncrement;
                setTextColor(COLOUR_MENU_TEXT);
                drawFloat(in3.desiredControlTemperature, 1, temperatureX - 65,
                          temperatureY, textFontSize);
              }
              EncMove = false;
              updateUIData = false;
            }
          }
          EEPROM.writeFloat(EEPROM_DESIRED_CONTROL_TEMPERATURE,
                            in3.desiredControlTemperature);
          EEPROM.commit();
          break;
        case HUMIDITY_UI_ROW:
          while (GPIORead(ENC_SWITCH)) {
            vTaskDelay(pdMS_TO_TICKS(WHILE_LOOP_DELAY));
            if (EncMove) {
              if (!in3.humidityControl) {
                in3.humidityControl = true;
                setTextColor(COLOUR_MENU);
                drawRightString((char *)(initialSensorsValue),
                                initialSensorPosition, humidityY, textFontSize);
                setTextColor(COLOUR_MENU_TEXT);
                drawCentreNumber(in3.desiredControlHumidity, humidityX - 65,
                                 humidityY);
                enableSet = true;
              }
              if (EncMove > 0) {
                if (in3.desiredControlHumidity > minHum) {
                  updateUIData = true;
                }
              } else {
                if (in3.desiredControlHumidity < maxHum) {
                  updateUIData = true;
                }
              }
              if (updateUIData) {
                setTextColor(COLOUR_MENU);
                drawCentreNumber(in3.desiredControlHumidity, humidityX - 65,
                                 humidityY);
                in3.desiredControlHumidity -= (EncMove)*stepHumidityIncrement;
                setTextColor(COLOUR_MENU_TEXT);
                drawCentreNumber(in3.desiredControlHumidity, humidityX - 65,
                                 humidityY);
              }
            }
            EncMove = false;
            updateUIData = false;
          }
          EEPROM.write(EEPROM_DESIRED_CONTROL_HUMIDITY,
                       in3.desiredControlHumidity);
          EEPROM.commit();
          break;
        case LED_UI_ROW:
          in3.phototherapy = !in3.phototherapy;
          EEPROM.write(EEPROM_PHOTOTHERAPY_ACTIVE, in3.phototherapy);
          EEPROM.commit();
          setTextColor(COLOUR_MENU);
          if (in3.phototherapy) {
            drawRightString((char *)("OFF"), unitPosition, ypos, textFontSize);
          } else {
            drawRightString((char *)("ON"), unitPosition, ypos, textFontSize);
          }
          setTextColor(COLOUR_MENU_TEXT);
          if (in3.phototherapy) {
            drawRightString((char *)("ON"), unitPosition, ypos, textFontSize);
          } else {
            drawRightString((char *)("OFF"), unitPosition, ypos, textFontSize);
          }
          ledcWrite(PHOTOTHERAPY_PWM_CHANNEL,
                    in3.phototherapy * in3.phototherapy_intensity);
          turnFans(in3.phototherapy);
          break;
        case SETTINGS_UI_ROW:
          UI_settings();
          break;
        case START_UI_ROW:
          if (in3.controlMode == CONTROL_SKIN && !skinProbeIsValid()) {
            in3.controlMode = CONTROL_AIR;
            EEPROM.write(EEPROM_CONTROL_MODE, in3.controlMode);
            EEPROM.commit();
            UI_mainMenu();
            break;
          }
          UI_actuatorsProgress();
          break;
        }
        break;
      case SETTINGS_PAGE:
        switch (bar_pos - graphicTextOffset) {
        case LANGUAGE_UI_ROW:
          while (GPIORead(ENC_SWITCH)) {
            vTaskDelay(pdMS_TO_TICKS(WHILE_LOOP_DELAY));
            if (EncMove) {
              setTextColor(COLOUR_MENU);
              switch (in3.language) {
              case SPANISH:
                textToWrite = (char *)("SPA");
                break;
              case ENGLISH:
                textToWrite = (char *)("ENG");
                break;
              case FRENCH:
                textToWrite = (char *)("FRA");
                break;
              case PORTUGUESE:
                textToWrite = (char *)("POR");
                break;
              }
              drawRightString(textToWrite, unitPosition, ypos, textFontSize);
              in3.language -= EncMove;
              if (in3.language < 0) {
                in3.language = NUM_LANGUAGES - 1;
              }
              if (in3.language >= NUM_LANGUAGES) {
                in3.language = false;
              }
              setTextColor(COLOUR_MENU_TEXT);
              switch (in3.language) {
              case SPANISH:
                textToWrite = (char *)("SPA");
                break;
              case ENGLISH:
                textToWrite = (char *)("ENG");
                break;
              case FRENCH:
                textToWrite = (char *)("FRA");
                break;
              case PORTUGUESE:
                textToWrite = (char *)("POR");
                break;
              }
              drawRightString(textToWrite, unitPosition, ypos, textFontSize);
              EncMove = false;
            }
          }
          EEPROM.write(EEPROM_LANGUAGE, in3.language);
          EEPROM.commit();
          UI_settings();
          break;
        case SERIAL_NUMBER_UI_ROW:
          while (GPIORead(ENC_SWITCH)) {
            vTaskDelay(pdMS_TO_TICKS(WHILE_LOOP_DELAY));
            if (EncMove) {
              setTextColor(COLOUR_MENU);
              drawRightNumber(in3.serialNumber, unitPosition, ypos);
              in3.serialNumber -= EncMove;
              EEPROM.writeInt(EEPROM_SERIAL_NUMBER, in3.serialNumber);
              setTextColor(COLOUR_MENU_TEXT);
              drawRightNumber(in3.serialNumber, unitPosition, ypos);
            }
            EncMove = false;
          }
          EEPROM.commit();
          break;
        case CCID_UI_ROW:
          break;
        case WIFI_EN_UI_ROW:
          WIFI_EN = !WIFI_EN;
          if (WIFI_EN) {
            wifiInit();
          } else {
            wifiDisable();
          }
          setTextColor(COLOUR_MENU);
          if (WIFI_EN) {
            drawRightString((char *)("OFF"), unitPosition, ypos, textFontSize);
          } else {
            drawRightString((char *)("ON"), unitPosition, ypos, textFontSize);
          }
          setTextColor(COLOUR_MENU_TEXT);
          if (WIFI_EN) {
            drawRightString((char *)("ON"), unitPosition, ypos, textFontSize);
          } else {
            drawRightString((char *)("OFF"), unitPosition, ypos, textFontSize);
          }
          break;
        case DEFAULT_VALUES_UI_ROW:
          loaddefaultValues();
          if (WIFI_EN) {
            wifiInit();
          } else {
            wifiDisable();
          }
          UI_settings();
          break;
        case HW_TEST_UI_ROW:
          initHardware(true);
          UI_settings();
          break;
        case CALIBRATION_UI_ROW:
          UI_calibration();
          break;
        }
        break;
      case CALIBRATION_SENSORS_PAGE:
        switch (bar_pos - graphicTextOffset) {
        case TWO_POINT_CALIB_UI_ROW:
          firstPointCalibration();
          break;
        case FINE_TUNE_UI_ROW:
          fineTuneCalibration();
          break;
        case AUTO_CALIB_UI_ROW:
          autoCalibration();
          break;
        case RESET_CALIB_UI_ROW:
          resetCalibration();
          UI_calibration();
          break;
        }
        break;
      case FINE_TUNE_CALIBRATION_PAGE:
        switch (bar_pos - graphicTextOffset) {
        case TEMP_CALIB_UI_ROW:
          errorTemperature[SKIN_SENSOR] = false;
          diffSkinTemperature = in3.temperature[SKIN_SENSOR];
          diffAirTemperature = in3.temperature[ROOM_DIGITAL_TEMP_SENSOR];
          while (GPIORead(ENC_SWITCH)) {
            vTaskDelay(pdMS_TO_TICKS(WHILE_LOOP_DELAY));
            if (EncMove) {
              setTextColor(COLOUR_MENU);
              drawFloat(diffSkinTemperature, 1, valuePosition, ypos,
                        textFontSize);
              setTextColor(COLOUR_MENU_TEXT);
              diffSkinTemperature += EncMove * (0.1);
              diffAirTemperature += EncMove * (0.1);
              drawFloat(diffSkinTemperature, 1, valuePosition, ypos,
                        textFontSize);
              EncMove = false;
            }
          }
          break;
        case SET_CALIB_UI_ROW:
          in3.fineTuneSkinTemperature =
              diffSkinTemperature - in3.temperature[SKIN_SENSOR];
          in3.fineTuneAirTemperature =
              diffAirTemperature - in3.temperature[ROOM_DIGITAL_TEMP_SENSOR];
          logI("[CALIBRATION] -> Fine tune Skin value is " +
               String(in3.fineTuneSkinTemperature));
          logI("[CALIBRATION] -> Fine tune Air value is " +
               String(in3.fineTuneAirTemperature));
          EEPROM.writeFloat(EEPROM_FINE_TUNE_TEMP_SKIN,
                            in3.fineTuneSkinTemperature);
          EEPROM.writeFloat(EEPROM_FINE_TUNE_TEMP_AIR, in3.fineTuneAirTemperature);
          EEPROM.commit();
          UI_mainMenu();
          break;
        }
        break;
      case FIRST_POINT_CALIBRATION_PAGE:
        clearCalibrationValues();
        switch (bar_pos - graphicTextOffset) {
        case TEMP_CALIB_UI_ROW:
          errorTemperature[SKIN_SENSOR] = false;
          diffSkinTemperature = in3.temperature[SKIN_SENSOR];
          diffAirTemperature = in3.temperature[ROOM_DIGITAL_TEMP_SENSOR];
          while (GPIORead(ENC_SWITCH)) {
            vTaskDelay(pdMS_TO_TICKS(WHILE_LOOP_DELAY));
            if (EncMove) {
              setTextColor(COLOUR_MENU);
              drawFloat(diffSkinTemperature, 1, valuePosition, ypos,
                        textFontSize);
              setTextColor(COLOUR_MENU_TEXT);
              diffSkinTemperature += EncMove * (0.1);
              diffAirTemperature += EncMove * (0.1);
              drawFloat(diffSkinTemperature, 1, valuePosition, ypos,
                        textFontSize);
              EncMove = false;
            }
          }
          break;
        case SET_CALIB_UI_ROW:
          provisionalReferenceTemperatureLow = diffSkinTemperature;
          provisionalRawTemperatureLow[SKIN_SENSOR] =
              in3.temperature[SKIN_SENSOR];
          logI("[CALIBRATION] -> Low reference point is " +
               String(provisionalReferenceTemperatureLow) +
               ", low raw skin point is " +
               String(provisionalRawTemperatureLow[SKIN_SENSOR]));
          secondPointCalibration();
          break;
        }
        break;
      case SECOND_POINT_CALIBRATION_PAGE:
        switch (bar_pos - graphicTextOffset) {
        case TEMP_CALIB_UI_ROW:
          diffSkinTemperature = in3.temperature[SKIN_SENSOR];
          while (GPIORead(ENC_SWITCH)) {
            vTaskDelay(pdMS_TO_TICKS(WHILE_LOOP_DELAY));
            if (EncMove) {
              setTextColor(COLOUR_MENU);
              drawFloat(diffSkinTemperature, 1, valuePosition, ypos,
                        textFontSize);
              setTextColor(COLOUR_MENU_TEXT);
              diffSkinTemperature += EncMove * (0.1);
              drawFloat(diffSkinTemperature, 1, valuePosition, ypos,
                        textFontSize);
              EncMove = false;
              logI("difTemp: " + String(diffSkinTemperature));
            }
          }
          break;
        case SET_CALIB_UI_ROW:
          ReferenceTemperatureLow = provisionalReferenceTemperatureLow;
          RawTemperatureLow[SKIN_SENSOR] =
              provisionalRawTemperatureLow[SKIN_SENSOR];
          ReferenceTemperatureRange =
              diffSkinTemperature - ReferenceTemperatureLow;
          if (RawTemperatureRange[SKIN_SENSOR]) {
            RawTemperatureRange[SKIN_SENSOR] =
                (in3.temperature[SKIN_SENSOR] - RawTemperatureLow[SKIN_SENSOR]);
            logI("calibration factors: " +
                 String(RawTemperatureLow[SKIN_SENSOR]) + "," +
                 String(RawTemperatureRange[SKIN_SENSOR]) + "," +
                 String(ReferenceTemperatureRange) + "," +
                 String(ReferenceTemperatureLow));
            saveCalibrationToEEPROM();
          } else {
            logI("[CALIBRATION] -> ERROR -> DIVIDE BY ZERO");
          }
          UI_settings();
          break;
        }
        break;
      case AUTO_CALIBRATION_PAGE:
        break;
      }
      selected = false;
      if (menu_rows) {
        tft.fillRect(
            0,
            (tft_height - TFT_HEIGHT_HEADING) * (bar_pos - 1) / menu_rows +
                TFT_HEIGHT_HEADING,
            width_select, (tft_height - TFT_HEIGHT_HEADING) / menu_rows, WHITE);
      }
      encoderContinuousPress(UI_page);
      vTaskDelay(pdMS_TO_TICKS(SWITCH_DEBOUNCE_TIME_MS));
    }
  }
}

bool encoderContinuousPress(int UI_page) {

  if (UI_page == MAIN_MENU_PAGE) {
    long timePressed = millis();
    while (!GPIORead(ENC_SWITCH)) {
      vTaskDelay(pdMS_TO_TICKS(WHILE_LOOP_DELAY));
      if (HOLD_PRESS_TO_GO_TO_SETTINGS &&
          millis() - timePressed > timePressToSettings) {
        UI_settings();
        return (true);
      }
    }
  } else {
    return (back_mode());
  }
  return false;
}

int getYpos(int UI_menu_rows, byte row) {
  row++; // because it starts at zero
  if (UI_menu_rows) {
    return ((tft_height - TFT_HEIGHT_HEADING) / (2 * UI_menu_rows) +
            (row - 1) * (tft_height - TFT_HEIGHT_HEADING) / (menu_rows) +
            letter_height);
  }
  return false;
}

void checkSetMessage(int UI_page, int UI_menu_rows) {
  if ((UI_page == MAIN_MENU_PAGE)) {
    int compareTime;
    if (blinkSetMessageState) {
      compareTime = blinkTimeON;
    } else {
      compareTime = blinkTimeOFF;
    }
    if (millis() - lastBlinkSetMessage > compareTime) {
      lastBlinkSetMessage = millis();
      blinkSetMessageState = !blinkSetMessageState;
      if (enableSet) {
        drawStartMessage(enableSet, UI_menu_rows);
      } else {
        drawHelpMessage(in3.language);
        drawCentreString(helpMessage,
                         width_select + (tft_width - width_select) / 2,
                         getYpos(menu_rows, START_UI_ROW), textFontSize);
      }
    }
  }
}

bool back_mode() {
  vTaskDelay(pdMS_TO_TICKS(SWITCH_DEBOUNCE_TIME_MS));
  last_encPulsed = millis();
  byte back_bar = false;
  while (!GPIORead(ENC_SWITCH)) {
    if (millis() - last_encPulsed > time_back_wait) {
      back_bar++;
      tft.drawLine(width_back - back_bar, 0, width_back - back_bar,
                   TFT_HEIGHT_HEADING, COLOUR_MENU);
    }
    if (back_bar == width_back) {
      if (page == CALIBRATION_SENSORS_PAGE) {
        UI_settings();
      } else {
        UI_mainMenu();
      }
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(((time_back_draw + time_back_wait) / width_back)));
  }
  if (millis() - last_encPulsed > time_back_wait) {
    drawBack();
  }
  vTaskDelay(pdMS_TO_TICKS(SWITCH_DEBOUNCE_TIME_MS));
  return (false);
}
