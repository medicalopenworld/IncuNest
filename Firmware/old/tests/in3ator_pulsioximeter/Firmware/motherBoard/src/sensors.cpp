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
extern Adafruit_ILI9341 tft;
extern SHTC3 mySHTC3; // Declare an instance of the SHTC3 class
extern Adafruit_SHT4x sht4;
extern RotaryEncoder encoder;
extern Beastdevices_INA3221 mainDigitalCurrentSensor;
extern Beastdevices_INA3221 secundaryDigitalCurrentSensor;

extern bool WIFI_EN;
extern long lastDebugUpdate;
extern long loopCounts;
extern int page;
extern long lastNTCmeasurement;
extern double errorTemperature[SENSOR_TEMP_QTY], temperatureCalibrationPoint;
extern double ReferenceTemperatureRange, ReferenceTemperatureLow;
extern double provisionalReferenceTemperatureLow;
extern double fineTuneSkinTemperature, fineTuneAirTemperature;
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

extern bool roomSensorPresent;
extern bool ambientSensorPresent;
extern bool digitalCurrentSensorPresent[2];

// room variables;
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
extern int screenTextColor, screenTextBackgroundColor;

// User Interface display variables
extern bool autoLock; // setting that enables backlight switch OFF after a
                      // given time of no user actions
extern long
    lastbacklightHandler; // last time there was a encoder movement or pulse
extern long sensorsUpdatePeriod;

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

extern double Kp[numPID], Ki[numPID], Kd[numPID];
extern PID airControlPID;
extern PID skinControlPID;
extern PID humidityControlPID;

// Sampling frequency
const double FILTER_SAMPLE_FREQUENCY = 1000; // Hz
// Cut-off frequency (-3 dB)
const double FILTER_CUTOFF_FREQUENCY = 10; // Hz
// Normalized cut-off frequency
const double FILTER_NORMALIZED_CUT_OFF_FREQUENCY =
    (2 * FILTER_CUTOFF_FREQUENCY / FILTER_SAMPLE_FREQUENCY);

// Sample timer for filter
Timer<micros> timer = std::round(1e6 / FILTER_SAMPLE_FREQUENCY);
// Sixth-order Butterworth filter
auto filter_0 = butter<6>(FILTER_NORMALIZED_CUT_OFF_FREQUENCY);
auto filter_1 = butter<6>(FILTER_NORMALIZED_CUT_OFF_FREQUENCY);
auto filter_2 = butter<6>(FILTER_NORMALIZED_CUT_OFF_FREQUENCY);

extern in3ator_parameters in3;

long lastCurrentMeasurement, lastVoltageMeasurement;
long lastEncoderUpdate;

void currentMonitor() {
  if (millis() - lastCurrentMeasurement > CURRENT_UPDATE_PERIOD) {
    if (digitalCurrentSensorPresent[MAIN]) {
      in3.system_current = measureMeanConsumption(MAIN, SYSTEM_SHUNT_CHANNEL);
      in3.fan_current = measureMeanConsumption(MAIN, FAN_SHUNT_CHANNEL);
      in3.phototherapy_current =
          measureMeanConsumption(MAIN, PHOTOTHERAPY_SHUNT_CHANNEL);
    }
    if (digitalCurrentSensorPresent[SECUNDARY]) {
      in3.heater_current =
          measureMeanConsumption(SECUNDARY, HEATER_SHUNT_CHANNEL);
      in3.USB_current = measureMeanConsumption(SECUNDARY, USB_SHUNT_CHANNEL);
      in3.BATTERY_current =
          measureMeanConsumption(SECUNDARY, BATTERY_SHUNT_CHANNEL);
    }
    lastCurrentMeasurement = millis();
  }
}

void voltageMonitor() {
  if (millis() - lastVoltageMeasurement > VOLTAGE_UPDATE_PERIOD) {
    if (digitalCurrentSensorPresent[MAIN]) {
      in3.system_voltage = measureMeanVoltage(MAIN, SYSTEM_SHUNT_CHANNEL);
    }
    if (digitalCurrentSensorPresent[SECUNDARY]) {
      in3.USB_voltage = measureMeanVoltage(SECUNDARY, USB_SHUNT_CHANNEL);
      in3.BATTERY_voltage =
          measureMeanVoltage(SECUNDARY, BATTERY_SHUNT_CHANNEL);
    }
    lastVoltageMeasurement = millis();
  }
}

double measureMeanConsumption(bool sensor, int shunt) {
#if (HW_NUM >= 6 && HW_NUM <= 8)
  for (int i = 0; i < CURRENT_MEASURES_AMOUNT; i++) {
    in3.system_current = filter_2(analogReadMilliVolts(SYSTEM_CURRENT_SENSOR) *
                                  ANALOG_TO_AMP_FACTOR);
  }
  return (in3.system_current);
#else
  if (digitalCurrentSensorPresent[sensor]) {
    if (sensor) {
      return (secundaryDigitalCurrentSensor.getCurrent(
          ina3221_ch_t(shunt))); // Amperes
    }
    return (
        mainDigitalCurrentSensor.getCurrent(ina3221_ch_t(shunt))); // Amperes
  }
#endif
  return (false);
}

float measureMeanVoltage(bool sensor, int shunt) {
  if (digitalCurrentSensorPresent[sensor]) {
    if (sensor) {
      return (secundaryDigitalCurrentSensor.getVoltage(
          ina3221_ch_t(shunt))); // Volts
    }
    return (mainDigitalCurrentSensor.getVoltage(ina3221_ch_t(shunt))); // Volts
  }
  return (false);
}

struct YsiPoint {
  float tempC;
  float resistance;
};

// Sustituye estos valores por la tabla real de TU sonda/fabricante.
// Te dejo puntos de ejemplo para que pegues los definitivos.
static const YsiPoint ysi400Table[] = {
    {32.0f, 0.0f}, {33.0f, 0.0f}, {34.0f, 0.0f}, {35.0f, 0.0f},
    {36.0f, 0.0f}, {37.0f, 0.0f}, {38.0f, 0.0f}, {39.0f, 0.0f},
    {40.0f, 0.0f}, {41.0f, 0.0f}, {42.0f, 0.0f},
};

float resistanceToTempYSI400(float rntc) {
  const int n = sizeof(ysi400Table) / sizeof(ysi400Table[0]);

  if (rntc <= 0.0f)
    return NAN;

  // La NTC baja su resistencia al subir temperatura
  for (int i = 0; i < n - 1; ++i) {
    float r1 = ysi400Table[i].resistance;
    float r2 = ysi400Table[i + 1].resistance;

    if ((rntc <= r1 && rntc >= r2) || (rntc >= r1 && rntc <= r2)) {
      float t1 = ysi400Table[i].tempC;
      float t2 = ysi400Table[i + 1].tempC;

      float frac = (rntc - r1) / (r2 - r1);
      return t1 + frac * (t2 - t1);
    }
  }

  return NAN;
}

float adcToCelsius(float adcReading_mV) {
  const float rTop = 2260.0f;
  float vExc = 3.3f;
  if (adcReading_mV <= 0.0f || adcReading_mV >= vExc * 1000.0f) {
    return NAN;
  }

  float vm = adcReading_mV / 1000.0f;
  float rntc = rTop * vm / (vExc - vm);

  return resistanceToTempYSI400(rntc);
}

void fanSpeedHandler() {
  double fanEncoderPeriodFiltered;
  if (in3.fanEncoderUpdate) {
    in3.fanEncoderUpdate = false;
    lastEncoderUpdate = millis();
    fanEncoderPeriodFiltered =
        filter_0(in3.fanEncoderPeriod[1] - in3.fanEncoderPeriod[0]);
    if (fanEncoderPeriodFiltered) {
      in3.fan_rpm = FAN_RPM_CONVERSION / fanEncoderPeriodFiltered;
    }
  } else if (millis() - lastEncoderUpdate > FAN_UPDATE_TIME_MIN) {
    in3.fan_rpm = false;
  }
}

bool measureNTCTemperature() {
  int NTCmeasurement;
  if (millis() - lastNTCmeasurement > NTC_MEASUREMENT_PERIOD) {
    if (ADC_READ_FUNCTION == MILLIVOTSREAD_ADC) {
      NTCmeasurement = analogReadMilliVolts(BABY_NTC_PIN);
    } else if (ADC_READ_FUNCTION == ANALOGREAD_ADC) {
      NTCmeasurement = analogRead(BABY_NTC_PIN);
    }
    if (NTCmeasurement > ADC_TO_DISCARD_MIN &&
        NTCmeasurement < ADC_TO_DISCARD_MAX) {
      lastSuccesfullSensorUpdate[SKIN_SENSOR] = millis();
      xQueueSend(sharedSensorQueue, &lastSuccesfullSensorUpdate[SKIN_SENSOR],
                 portMAX_DELAY);
      in3.temperature[SKIN_SENSOR] = filter_1(adcToCelsius(NTCmeasurement));
      errorTemperature[SKIN_SENSOR] = in3.temperature[SKIN_SENSOR];
      if (RawTemperatureRange[SKIN_SENSOR]) {
        in3.temperature[SKIN_SENSOR] =
            (((in3.temperature[SKIN_SENSOR] - RawTemperatureLow[SKIN_SENSOR]) *
              ReferenceTemperatureRange) /
             RawTemperatureRange[SKIN_SENSOR]) +
            ReferenceTemperatureLow;
      }
      in3.temperature[SKIN_SENSOR] += fineTuneSkinTemperature;
      errorTemperature[SKIN_SENSOR] -= in3.temperature[SKIN_SENSOR];
      if (in3.temperature < 0) {
        in3.temperature[SKIN_SENSOR] = 0;
      }
      lastNTCmeasurement = millis();
    } else {
      // logAlarm("[ALARM] -> NTC read is: " + String(NTCmeasurement));
    }
    return true;
  }
  return false;
}

bool updateRoomSensor() {
  if (roomSensorPresent) {
    SHTC3_Status_TypeDef sensorState = mySHTC3.update();
    float sensedTemperature;
    // logI("[SENSORS] -> Updating room humidity: state is " +
    // String(sensorState));
    if (!sensorState) {
      sensedTemperature = mySHTC3.toDegC();
      if (sensedTemperature > DIG_TEMP_TO_DISCARD_MIN &&
          sensedTemperature < DIG_TEMP_TO_DISCARD_MAX) {
        lastSuccesfullSensorUpdate[ROOM_DIGITAL_TEMP_SENSOR] = millis();
        xQueueSend(sharedSensorQueue,
                   &lastSuccesfullSensorUpdate[ROOM_DIGITAL_TEMP_SENSOR],
                   portMAX_DELAY);
        in3.temperature[ROOM_DIGITAL_TEMP_SENSOR] =
            sensedTemperature; // Add here measurement to temp array
        in3.humidity[ROOM_DIGITAL_HUM_SENSOR] = mySHTC3.toPercent();
        return true;
      }
    } else {
      initRoomSensor();
    }
  } else {
    initRoomSensor();
  }
  return false;
}

bool updateAmbientSensor() {
  if (ambientSensorPresent) {
    sensors_event_t humidity, temp;
    sht4.getEvent(&humidity,
                  &temp); // populate temp and humidity objects with fresh data
    in3.temperature[AMBIENT_DIGITAL_TEMP_SENSOR] = temp.temperature;
    in3.humidity[AMBIENT_DIGITAL_HUM_SENSOR] = humidity.relative_humidity;
    return true;
  } else {
    initAmbientSensor();
  }
  return false;
}
