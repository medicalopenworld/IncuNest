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

static const char *TAG __attribute__((unused)) = "SECURITY";
extern SemaphoreHandle_t log_mutex;

// Pending alarm queue for HMI connection
struct PendingAlarm
{
  char message[128];
  bool valid;
};

static PendingAlarm pending_alarms[10];
static int pending_alarm_count = 0;
static bool hmi_connected = false;

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

extern bool digitalCurrentSensorPresent[2];

// room variables;         // desired temperature in heater
extern float minDesiredTemp[2]; // minimum allowed temperature to be set
extern float maxDesiredTemp[2]; // maximum allowed temperature to be set
extern int presetTemp[2];       // preset baby skin temperature

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

extern long lastSuccesfullSensorUpdate[SENSOR_TEMP_QTY];
extern QueueHandle_t sharedSensorQueue;

extern double HeaterPIDOutput;
extern double skinControlPIDInput;
extern double airControlPIDInput;
extern double humidityControlPIDOutput;
extern int humidifierTimeCycle;
extern unsigned long windowStartTime;

extern PID airControlPID;
extern PID fanControlPID;
extern double fanControlPIDOutput;
extern PID skinControlPID;
extern PID humidityControlPID;

#define TEMPERATURE_ERROR 1 // 1 degrees difference to trigger alarm
#define HUMIDITY_ERROR 10   // 10 %RH to trigger alarm

#define TEMPERATURE_ERROR_HYSTERESIS \
  0.05                              // 0.05 degrees difference to disable alarm
#define HUMIDITY_ERROR_HYSTERESIS 5 // 5 %RH to disable alarm

// Set to 1 for testing, then change to 30 for production
#define ACTUATORS_ALARM_STABILIZATION_MINS 30

#define FAN_TEST_CURRENTDIF_MIN \
  0.2 // when the fan is spinning, heater cools down and consume less current
#define FAN_TEST_PREHEAT_TIME \
  30000 // when the fan is spinning, heater cools down and consume less current

#define ALARM_TIME_DELAY 30 // in mins, time to check alarm
// security config
#define AIR_THERMAL_CUTOUT AIR_TEMPERATURE_SET_MAX
#define SKIN_THERMAL_CUTOUT SKIN_TEMPERATURE_SET_MAX
#define AIR_THERMAL_CUTOUT_HYSTERESIS 0.2
#define SKIN_THERMAL_CUTOUT_HYSTERESIS 0.2
#define enableAlarms true

#define MINIMUM_SUCCESSFULL_SENSOR_UPDATE 20000 // in millis

bool alarmOnGoing[NUM_ALARMS];
bool displayAlarm[NUM_ALARMS];
bool clearedAlarm[NUM_ALARMS];
long lastAlarmTrigger[NUM_ALARMS];
float alarmSensedValue;
long lastPowerSupplyCheck;

extern IncuNest_parameters in3;

void initAlarms()
{
  lastAlarmTrigger[AIR_THERMAL_CUTOUT_ALARM] =
      -1 * minsToMillis(ALARM_TIME_DELAY);
  lastAlarmTrigger[SKIN_THERMAL_CUTOUT_ALARM] =
      -1 * minsToMillis(ALARM_TIME_DELAY);
}

bool evaluateAlarm(byte alarmID, float setPoint, float measuredValue,
                   float errorMargin, float hysteresisValue, long alarmTime,
                   bool bidirectional = false)
{
  bool alarmSound = DEFAULT_SOUND_ALARM;
  if (millis() - alarmTime < minsToMillis(ALARM_TIME_DELAY))
  {
    alarmSound = SILENCED_ALARM;
  }
  if (bidirectional)
  {
    float deviation = fabsf(measuredValue - setPoint);
    if (!alarmOnGoing[alarmID] && deviation > errorMargin)
    {
      in3.alarmToReport[alarmID] = true;
      setAlarm(alarmID, alarmSound);
      return true;
    }
    else if (alarmOnGoing[alarmID] &&
             deviation < errorMargin - hysteresisValue)
    {
      in3.alarmToReport[alarmID] = false;
      resetAlarm(alarmID);
    }
  }
  else if (errorMargin)
  {
    if (!alarmOnGoing[alarmID] &&
        (measuredValue > (setPoint + errorMargin + hysteresisValue)))
    {
      in3.alarmToReport[alarmID] = true;
      setAlarm(alarmID, alarmSound);
      return true;
    }
    else if (alarmOnGoing[alarmID] &&
             (measuredValue < (setPoint + errorMargin - hysteresisValue)))
    {
      in3.alarmToReport[alarmID] = false;
      resetAlarm(alarmID);
    }
  }
  else
  {
    if (!alarmOnGoing[alarmID] &&
        (measuredValue > (setPoint + hysteresisValue)))
    {
      in3.alarmToReport[alarmID] = true;
      setAlarm(alarmID, alarmSound);
      return true;
    }
    else if (alarmOnGoing[alarmID] &&
             (measuredValue < (setPoint - hysteresisValue)))
    {
      in3.alarmToReport[alarmID] = false;
      resetAlarm(alarmID);
    }
  }
  return false;
}

void checkThermalCutOuts()
{
  evaluateAlarm(AIR_THERMAL_CUTOUT_ALARM, in3.airTemperatureSetMax,
                in3.temperature[ROOM_DIGITAL_TEMP_SENSOR], false,
                AIR_THERMAL_CUTOUT_HYSTERESIS,
                lastAlarmTrigger[AIR_THERMAL_CUTOUT_ALARM]);
  evaluateAlarm(SKIN_THERMAL_CUTOUT_ALARM, in3.skinTemperatureSetMax,
                in3.temperature[SKIN_SENSOR], false,
                SKIN_THERMAL_CUTOUT_HYSTERESIS,
                lastAlarmTrigger[SKIN_THERMAL_CUTOUT_ALARM]);
}

void checkStatusOfSensor(byte sensor)
{
  byte alarmID = 0;
  switch (sensor)
  {
  case ROOM_DIGITAL_TEMP_SENSOR:
    alarmID = AIR_SENSOR_ISSUE_ALARM;
    break;
  case SKIN_SENSOR:
    alarmID = SKIN_SENSOR_ISSUE_ALARM;
    break;
  }
  if (alarmID)
  {
    if (millis() - lastSuccesfullSensorUpdate[sensor] >
        MINIMUM_SUCCESSFULL_SENSOR_UPDATE)
    {
      in3.alarmToReport[alarmID] = true;
      if (!alarmOnGoing[alarmID])
      {
        setAlarm(alarmID);
      }
    }
    else
    {
      in3.alarmToReport[alarmID] = false;
      if (alarmOnGoing[alarmID])
      {
        resetAlarm(alarmID);
      }
    }
    //    }
  }
}

void sensorHealthMonitor()
{
  checkStatusOfSensor(ROOM_DIGITAL_TEMP_SENSOR);
  if (in3.temperatureControl && in3.controlMode == CONTROL_SKIN)
  {
    checkStatusOfSensor(SKIN_SENSOR);
  }
  else if (alarmOnGoing[SKIN_SENSOR_ISSUE_ALARM])
  {
    resetAlarm(SKIN_SENSOR_ISSUE_ALARM);
  }
}

void powerMonitor()
{
  currentMonitor();
  voltageMonitor();
}

void alarmTimerStart(bool assumeStabilized)
{
  for (int i = 0; i < NUM_ALARMS; i++)
  {
    lastAlarmTrigger[i] = millis();
  }
  lastAlarmTrigger[AIR_THERMAL_CUTOUT_ALARM] =
      -1 * minsToMillis(ALARM_TIME_DELAY);
  lastAlarmTrigger[SKIN_THERMAL_CUTOUT_ALARM] =
      -1 * minsToMillis(ALARM_TIME_DELAY);
  if (assumeStabilized)
  {
    // Used when resuming an already-running control (restoreState boot):
    // the incubator didn't just go cold, so there is no need to make
    // temperature/humidity alarms wait out another full stabilization
    // window - treat it as already elapsed, same as the thermal cutouts.
    lastAlarmTrigger[TEMPERATURE_ALARM] =
        -1 * minsToMillis(ACTUATORS_ALARM_STABILIZATION_MINS);
    lastAlarmTrigger[HUMIDITY_ALARM] =
        -1 * minsToMillis(ACTUATORS_ALARM_STABILIZATION_MINS);
  }
  // Otherwise TEMPERATURE_ALARM and HUMIDITY_ALARM both keep their millis()
  // value from the loop above, so checkAlarms() enforces the same
  // ACTUATORS_ALARM_STABILIZATION_MINS grace period for both after a fresh
  // activation. The thermal cutouts stay immediately eligible either way -
  // they are a hard over-temperature safety limit, not a setpoint-tracking
  // alarm.
}

byte activeAlarm()
{
  for (int i = 0; i < NUM_ALARMS; i++)
  {
    if (alarmOnGoing[i])
    {
      return (i);
    }
  }
  return 0;
}

bool ongoingAlarms()
{
  return (alarmOnGoing[TEMPERATURE_ALARM] || alarmOnGoing[HUMIDITY_ALARM] ||
          alarmOnGoing[AIR_THERMAL_CUTOUT_ALARM] ||
          alarmOnGoing[SKIN_THERMAL_CUTOUT_ALARM] ||
          alarmOnGoing[AIR_SENSOR_ISSUE_ALARM] ||
          alarmOnGoing[SKIN_SENSOR_ISSUE_ALARM] ||
          alarmOnGoing[HEATER_ISSUE_ALARM] || alarmOnGoing[FAN_ISSUE_ALARM] ||
          alarmOnGoing[POWER_SUPPLY_ALARM] || alarmOnGoing[AIR_BLOCKED_ALARM]);
}

int getActiveAlarmCount()
{
  int count = 0;
  for (int i = 0; i < NUM_ALARMS; i++)
  {
    if (alarmOnGoing[i])
    {
      count++;
    }
  }
  return count;
}

bool ongoingCriticalAlarm()
{
  return (alarmOnGoing[AIR_THERMAL_CUTOUT_ALARM] ||
          alarmOnGoing[SKIN_THERMAL_CUTOUT_ALARM] ||
          alarmOnGoing[AIR_SENSOR_ISSUE_ALARM] ||
          (alarmOnGoing[SKIN_SENSOR_ISSUE_ALARM] &&
           in3.controlMode == CONTROL_SKIN) ||
          alarmOnGoing[HEATER_ISSUE_ALARM] ||
          alarmOnGoing[FAN_ISSUE_ALARM] ||
          alarmOnGoing[POWER_SUPPLY_ALARM]);
}

bool ongoingCriticalWiringAlarm()
{
  return (alarmOnGoing[HEATER_ISSUE_ALARM] || alarmOnGoing[FAN_ISSUE_ALARM] ||
          alarmOnGoing[POWER_SUPPLY_ALARM]);
}

// Unlike ongoingCriticalWiringAlarm() (used for heater/humidifier gating),
// a heater fault only has to take the fan down with it when this unit has
// no independent way (RPM feedback) to verify the fan is still spinning.
bool ongoingFanCriticalAlarm()
{
  return (alarmOnGoing[FAN_ISSUE_ALARM] || alarmOnGoing[POWER_SUPPLY_ALARM] ||
          (alarmOnGoing[HEATER_ISSUE_ALARM] && !in3.fanHasSpeedFeedback));
}

char *alarmIDtoString(byte alarmID)
{
  byte lang = in3.language; // or hmi_cmd_msg.language
  switch (alarmID)
  {
  case AIR_THERMAL_CUTOUT_ALARM:
  case SKIN_THERMAL_CUTOUT_ALARM:
    if (lang == SPANISH)
      return (char *)("CORTE TERMICO");
    if (lang == FRENCH)
      return (char *)("COUPURE THERMIQUE");
    return (char *)("THERMAL CUTOUT");
    break;
  case TEMPERATURE_ALARM:
    if (lang == SPANISH)
      return (char *)("ERROR TEMPERATURA");
    if (lang == FRENCH)
      return (char *)("ERREUR TEMPERATURE");
    return (char *)("TEMPERATURE ALARM");
    break;
  case HUMIDITY_ALARM:
    if (lang == SPANISH)
      return (char *)("ERROR HUMEDAD");
    if (lang == FRENCH)
      return (char *)("ERREUR HUMIDITE");
    return (char *)("HUMIDITY ALARM");
    break;
  case AIR_SENSOR_ISSUE_ALARM:
    if (lang == SPANISH)
      return (char *)("ALERTA SENSOR AIRE");
    if (lang == FRENCH)
      return (char *)("ALERTE CAPTEUR AIR");
    return (char *)("AIR SENSOR ALARM");
    break;
  case SKIN_SENSOR_ISSUE_ALARM:
    if (lang == SPANISH)
      return (char *)("ALERTA SENSOR PIEL");
    if (lang == FRENCH)
      return (char *)("ALERTE CAPTEUR PEAU");
    return (char *)("SKIN SENSOR ALARM");
    break;
  case FAN_ISSUE_ALARM:
    if (lang == SPANISH)
      return (char *)("ERROR VENTILADOR");
    if (lang == FRENCH)
      return (char *)("ERREUR VENTILATEUR");
    return (char *)("FAN ALARM");
    break;
  case AIR_BLOCKED_ALARM:
    if (lang == SPANISH)
      return (char *)("SALIDA DE AIRE OBSTRUIDA");
    if (lang == FRENCH)
      return (char *)("SORTIE D'AIR OBSTRUEE");
    return (char *)("AIR OUTLET BLOCKED");
    break;
  case HEATER_ISSUE_ALARM:
    if (lang == SPANISH)
      return (char *)("ERROR CALENTADOR");
    if (lang == FRENCH)
      return (char *)("ERREUR CHAUFFAGE");
    return (char *)("HEATER ALARM");
    break;
  case POWER_SUPPLY_ALARM:
    if (lang == SPANISH)
      return (char *)("ERROR ALIMENTACION");
    if (lang == FRENCH)
      return (char *)("ERREUR ALIMENTATION");
    return (char *)("POWER SUPPLY ALARM");
    break;
  default:
    return (char *)("ALARM");
    break;
  }
}

int alarmPendingToDisplay()
{
  for (int i = 0; i < NUM_ALARMS; i++)
  {
    if (displayAlarm[i])
      return i;
  }
  return 0;
}

void clearDisplayedAlarm(byte alarm) { displayAlarm[alarm] = false; }

void clearAlarmPendingToClear(byte alarm) { clearedAlarm[alarm] = false; }

int alarmPendingToClear()
{
  for (int i = 0; i < NUM_ALARMS; i++)
  {
    if (clearedAlarm[i])
      return i;
  }
  return 0;
}

void sendPendingAlarms()
{
  for (int i = 0; i < pending_alarm_count && i < 10; i++)
  {
    if (pending_alarms[i].valid)
    {
#if CONFIG_IDF_TARGET_ESP32S3
      CommunicationHost_Send(pending_alarms[i].message);
#endif
      if (log_mutex == NULL ||
          xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
      {
        ESP_LOGI(TAG, "Sent pending alarm: %s", pending_alarms[i].message);
        if (log_mutex)
          xSemaphoreGiveRecursive(log_mutex);
      }
      pending_alarms[i].valid = false;
    }
  }
  pending_alarm_count = 0;
}

void setHMIConnected(bool connected)
{
  if (connected && !hmi_connected)
  {
    if (log_mutex == NULL ||
        xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
      ESP_LOGI(TAG, "HMI newly connected, flushing %d pending alarms",
               pending_alarm_count);
      if (log_mutex)
        xSemaphoreGiveRecursive(log_mutex);
    }
    hmi_connected = true;
    sendPendingAlarms();
  }
  else
  {
    hmi_connected = connected;
  }
}

void sendAlarmUSB(byte alarmID, bool isActive)
{
  char msg[128];
  const char *title = "ALARM";
  const char *desc = "alarm";

  byte lang = in3.language;

  switch (alarmID)
  {
  case HUMIDITY_ALARM:
    if (lang == SPANISH)
    {
      title = "ERROR HUMEDAD";
      desc = "ERROR DE HUMEDAD";
    }
    else if (lang == FRENCH)
    {
      title = "ERREUR HUMIDITE";
      desc = "ERREUR D HUMIDITE";
    }
    else
    {
      title = "HUMIDITY ERROR";
      desc = "HUMIDITY ERROR";
    }
    break;
  case TEMPERATURE_ALARM:
    if (lang == SPANISH)
    {
      title = "ALARMA DE TEMPERATURA";
      desc = "ALARMA DE TEMPERATURA";
    }
    else if (lang == FRENCH)
    {
      title = "ALARME DE TEMPERATURE";
      desc = "ALARME DE TEMPERATURE";
    }
    else
    {
      title = "TEMPERATURE ALARM";
      desc = "TEMPERATURE ALARM";
    }
    break;
  case AIR_THERMAL_CUTOUT_ALARM:
    if (lang == SPANISH)
    {
      title = "CORTE TERMICO AIRE";
      desc = "TEMPERATURA AIRE ELEVADA";
    }
    else if (lang == FRENCH)
    {
      title = "COUPURE THERMIQUE AIR";
      desc = "TEMPERATURE AIR ELEVEE";
    }
    else
    {
      title = "AIR THERMAL CUTOUT";
      desc = "AIR TEMP TOO HIGH";
    }
    break;
  case SKIN_THERMAL_CUTOUT_ALARM:
    if (lang == SPANISH)
    {
      title = "CORTE TERMICO PIEL";
      desc = "TEMPERATURA PIEL ELEVADA";
    }
    else if (lang == FRENCH)
    {
      title = "COUPURE THERMIQUE PEAU";
      desc = "TEMPERATURE PEAU ELEVEE";
    }
    else
    {
      title = "SKIN THERMAL CUTOUT";
      desc = "SKIN TEMP TOO HIGH";
    }
    break;
  case AIR_SENSOR_ISSUE_ALARM:
    if (lang == SPANISH)
    {
      title = "ERROR SENSOR AIRE";
      desc = "FALLO SENSOR AIRE";
    }
    else if (lang == FRENCH)
    {
      title = "ERREUR CAPTEUR AIR";
      desc = "PANNE CAPTEUR AIR";
    }
    else
    {
      title = "AIR SENSOR ERROR";
      desc = "AIR SENSOR FAILURE";
    }
    break;
  case SKIN_SENSOR_ISSUE_ALARM:
    if (lang == SPANISH)
    {
      title = "ERROR SENSOR PIEL";
      desc = "FALLO SENSOR PIEL";
    }
    else if (lang == FRENCH)
    {
      title = "ERREUR CAPTEUR PEAU";
      desc = "PANNE CAPTEUR PEAU";
    }
    else
    {
      title = "SKIN SENSOR ERROR";
      desc = "SKIN SENSOR FAILURE";
    }
    break;
  case FAN_ISSUE_ALARM:
    if (lang == SPANISH)
    {
      title = "ERROR VENTILADOR";
      desc = "FALLO VENTILADOR";
    }
    else if (lang == FRENCH)
    {
      title = "ERREUR VENTILATEUR";
      desc = "PANNE VENTILATEUR";
    }
    else
    {
      title = "FAN ERROR";
      desc = "FAN FAILURE";
    }
    break;
  case AIR_BLOCKED_ALARM:
    if (lang == SPANISH)
    {
      title = "SALIDA DE AIRE OBSTRUIDA";
      desc = "POSIBLE OBSTRUCCION EN LA SALIDA DE AIRE";
    }
    else if (lang == FRENCH)
    {
      title = "SORTIE D'AIR OBSTRUEE";
      desc = "OBSTRUCTION POSSIBLE DE LA SORTIE D'AIR";
    }
    else
    {
      title = "AIR OUTLET BLOCKED";
      desc = "POSSIBLE AIR OUTLET BLOCKAGE";
    }
    break;
  case HEATER_ISSUE_ALARM:
    if (lang == SPANISH)
    {
      title = "ERROR CALENTADOR";
      desc = "FALLO CALENTADOR";
    }
    else if (lang == FRENCH)
    {
      title = "ERREUR CHAUFFAGE";
      desc = "PANNE CHAUFFAGE";
    }
    else
    {
      title = "HEATER ERROR";
      desc = "HEATER FAILURE";
    }
    break;
  case POWER_SUPPLY_ALARM:
    if (lang == SPANISH)
    {
      title = "ERROR ALIMENTACION";
      desc = "FALLO FUENTE";
    }
    else if (lang == FRENCH)
    {
      title = "ERREUR ALIMENTATION";
      desc = "PANNE ALIMENTATION";
    }
    else
    {
      title = "POWER SUPPLY ERROR";
      desc = "POWER SUPPLY FAILURE";
    }
    break;
  }

  snprintf(msg, sizeof(msg), "CTRL,ALM,%d,%s,%s,%d\n", alarmID, title,
           desc, isActive ? 1 : 0);
  if (log_mutex == NULL ||
      xSemaphoreTakeRecursive(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    ESP_LOGI(TAG, "%s", msg);
    if (log_mutex)
      xSemaphoreGiveRecursive(log_mutex);
  }

#if CONFIG_IDF_TARGET_ESP32S3
  if (!hmi_connected)
  {
    // Queue the alarm if HMI not connected
    if (pending_alarm_count < 10)
    {
      strncpy(pending_alarms[pending_alarm_count].message, msg,
              sizeof(pending_alarms[pending_alarm_count].message) - 1);
      pending_alarms[pending_alarm_count].message[127] = '\0';
      pending_alarms[pending_alarm_count].valid = true;
      pending_alarm_count++;
      ESP_LOGI(TAG, "Queued alarm: %s", msg);
    }
  }
  else
  {
    // Send immediately if HMI is connected
    CommunicationHost_Send(msg);
  }
#endif
}

void resendActiveAlarms()
{
  for (int i = 0; i < NUM_ALARMS; i++)
  {
    if (alarmOnGoing[i])
    {
      sendAlarmUSB(i, true);
    }
  }
}

void setAlarm(byte alarmID)
{
  logAlarm("[ALARM] ->" + String(alarmIDtoString(alarmID)) +
           " has been triggered");
  alarmOnGoing[alarmID] = true;
  displayAlarm[alarmID] = true;
  buzzerTone(buzzerAlarmBeepCount, buzzerAlarmBeepTime, buzzerAlarmTone);
  sendAlarmUSB(alarmID, true);
}

void setAlarm(byte alarmID, bool alarmSound)
{
  logAlarm("[ALARM] ->" + String(alarmIDtoString(alarmID)) +
           " has been triggered");
  alarmOnGoing[alarmID] = true;
  displayAlarm[alarmID] = true;
  if (alarmSound)
  {
    buzzerTone(buzzerAlarmBeepCount, buzzerAlarmBeepTime, buzzerAlarmTone);
  }
  sendAlarmUSB(alarmID, true);
}

void resetAlarm(byte alarmID)
{
  logAlarm("[ALARM] ->" + String(alarmIDtoString(alarmID)) +
           " has been disable");
  alarmOnGoing[alarmID] = false;
  clearedAlarm[alarmID] = true;
  if (!ongoingAlarms())
  {
    shutBuzzer();
  }
  sendAlarmUSB(alarmID, false);
}

void reestartOngoingAlarms()
{
  for (int i = 0; i < NUM_ALARMS; i++)
  {
    if (alarmOnGoing[i])
    {
      lastAlarmTrigger[i] = millis();
    }
  }
}

void checkAlarms()
{
  // Only evaluate after stabilization period has elapsed
  if (millis() - lastAlarmTrigger[TEMPERATURE_ALARM] >=
      minsToMillis(ACTUATORS_ALARM_STABILIZATION_MINS))
  {
    if (in3.temperatureControl)
    {
      if (in3.controlMode)
      {
        alarmSensedValue = in3.temperature[ROOM_DIGITAL_TEMP_SENSOR];
      }
      else
      {
        alarmSensedValue = in3.temperature[SKIN_SENSOR];
      }
      evaluateAlarm(TEMPERATURE_ALARM, in3.desiredControlTemperature,
                    alarmSensedValue, TEMPERATURE_ERROR,
                    TEMPERATURE_ERROR_HYSTERESIS,
                    lastAlarmTrigger[TEMPERATURE_ALARM], true);
    }
  }
  if (in3.humidityControl)
  {
    // Only evaluate after stabilization period has elapsed
    if (millis() - lastAlarmTrigger[HUMIDITY_ALARM] >=
        minsToMillis(ACTUATORS_ALARM_STABILIZATION_MINS))
    {
      evaluateAlarm(HUMIDITY_ALARM, in3.desiredControlHumidity,
                    in3.humidity[ROOM_DIGITAL_HUM_SENSOR], HUMIDITY_ERROR,
                    HUMIDITY_ERROR_HYSTERESIS,
                    lastAlarmTrigger[HUMIDITY_ALARM], true);
    }
  }
}

#if defined(FAN_SPEED_FEEDBACK)
// FAN_SPINUP_GRACE_MS now lives in board.h (shared with the PID handover in
// PID.cpp so both use one spin-up window).

void checkFanSpeed()
{
  static bool wasFanCommandedOn = false;
  static long fanCommandedOnSince = 0;

  if (!in3.fanHasSpeedFeedback)
  {
    return; // this unit's fan has no tachometer signal — nothing to check
  }

  if (in3.fanCommandedOn && !wasFanCommandedOn)
  {
    fanCommandedOnSince = millis();
  }
  wasFanCommandedOn = in3.fanCommandedOn;

  if (!in3.fanCommandedOn)
  {
    return; // fan intentionally off — no RPM expected
  }
  if (millis() - fanCommandedOnSince < FAN_SPINUP_GRACE_MS)
  {
    return; // still spinning up
  }

  if (!alarmOnGoing[FAN_ISSUE_ALARM] && in3.fan_rpm < FAN_MIN_RPM)
  {
    in3.alarmToReport[FAN_ISSUE_ALARM] = true;
    setAlarm(FAN_ISSUE_ALARM);
  }
  else if (alarmOnGoing[FAN_ISSUE_ALARM] &&
           in3.fan_rpm >= FAN_MIN_RPM + FAN_MIN_RPM_HYSTERESIS)
  {
    in3.alarmToReport[FAN_ISSUE_ALARM] = false;
    resetAlarm(FAN_ISSUE_ALARM);
  }
}

void checkAirBlockage()
{
#if AIR_BLOCKED_DETECTION_ENABLED
  static bool wasFanCommandedOn = false;
  static long fanCommandedOnSince = 0;
  static long dutyHighSince = 0;

  if (fanControlPID.GetMode() != AUTOMATIC)
  {
    dutyHighSince = 0;
    return; // not under closed-loop control — no duty signal to evaluate
  }
  if (alarmOnGoing[FAN_ISSUE_ALARM])
  {
    dutyHighSince = 0;
    return; // total fan failure already reported — don't also report this
  }

  if (in3.fanCommandedOn && !wasFanCommandedOn)
  {
    fanCommandedOnSince = millis();
  }
  wasFanCommandedOn = in3.fanCommandedOn;
  if (!in3.fanCommandedOn ||
      millis() - fanCommandedOnSince < FAN_SPINUP_GRACE_MS)
  {
    // During spin-up the PID output saturates at max duty by design (large
    // RPM error) — evaluating it here would false-alarm on every start.
    dutyHighSince = 0;
    return;
  }

  if (fanControlPIDOutput > FAN_DUTY_BLOCKED_THRESHOLD)
  {
    // Require the excess to be sustained: transients (heater kicking in and
    // sagging the supply, setpoint recovery) legitimately spike the duty
    // while the loop compensates.
    if (dutyHighSince == 0)
    {
      dutyHighSince = millis();
    }
    if (!alarmOnGoing[AIR_BLOCKED_ALARM] &&
        millis() - dutyHighSince >= AIR_BLOCKED_SUSTAIN_MS)
    {
      in3.alarmToReport[AIR_BLOCKED_ALARM] = true;
      setAlarm(AIR_BLOCKED_ALARM);
    }
  }
  else
  {
    dutyHighSince = 0;
    if (alarmOnGoing[AIR_BLOCKED_ALARM] &&
        fanControlPIDOutput <=
            FAN_DUTY_BLOCKED_THRESHOLD - FAN_DUTY_BLOCKED_HYSTERESIS)
    {
      in3.alarmToReport[AIR_BLOCKED_ALARM] = false;
      resetAlarm(AIR_BLOCKED_ALARM);
    }
  }
#endif
}
#endif

void powerSupplyCheck()
{
#if (HW_NUM >= 13)
  {
    if (millis() - lastPowerSupplyCheck > POWER_SUPPLY_CHECK_PERIOD)
    {
      lastPowerSupplyCheck = millis();
      if (digitalCurrentSensorPresent[MAIN] &&
          in3.system_voltage > MIN_SYSTEM_VOLTAGE_TRIGGER &&
          in3.system_voltage < MAX_SYSTEM_VOLTAGE_TRIGGER)
      {
        in3.alarmToReport[POWER_SUPPLY_ALARM] = true;
        if (!alarmOnGoing[POWER_SUPPLY_ALARM])
          setAlarm(POWER_SUPPLY_ALARM);
      }
      else
      {
        in3.alarmToReport[POWER_SUPPLY_ALARM] = false;
        if (alarmOnGoing[POWER_SUPPLY_ALARM])
          resetAlarm(POWER_SUPPLY_ALARM);
      }
    }
  }
#endif
}

#if (HW_NUM >= 16)
static void checkUsbFault()
{
  if (!GPIORead(USB_FAULT))
  { // active LOW: LOW = fault
    if (humidifierState)
    {
      logE("[PWR] USB_FAULT: humidifier short-circuit/overload, turning OFF");
      in3_hum.turn(OFF);
      humidifierState = false;
      in3.humidityControl = false;
      stopPID(humidityPID);
    }
  }
}
#endif

void securityCheck()
{
  checkThermalCutOuts();
  checkAlarms();
  sensorHealthMonitor();
  powerSupplyCheck();
#if defined(FAN_SPEED_FEEDBACK)
  checkFanSpeed();
  checkAirBlockage();
#endif
#if (HW_NUM >= 16)
  checkUsbFault();
#endif
}