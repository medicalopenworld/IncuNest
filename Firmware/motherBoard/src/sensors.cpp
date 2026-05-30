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
extern SHTC3 mySHTC3; // Declare an instance of the SHTC3 class
extern SensirionI2cSts3x mySTS35[STS3X_NUM];
extern Adafruit_SHT4x sht4;
extern RotaryEncoder encoder;
extern Beastdevices_INA3221 mainDigitalCurrentSensor;
extern Beastdevices_INA3221 secundaryDigitalCurrentSensor;

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

extern bool roomSensorPresent[ROOM_SENSOR_POSIBILITIES];
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

// filter_1 (NTC piel) se llama a 1000/SKIN_SENSOR_UPDATE_PERIOD_MS ≈ 5 Hz.
// Con ω_n=0.02 (diseñado a 1000 Hz) el corte efectivo cae a 0.05 Hz → τ≈3 s.
// Con ω_n=0.2 (f_c=0.5 Hz a 5 Hz) → τ≈0.32 s → 5τ≈1.5 s de asentamiento.
const double SKIN_FILTER_NORMALIZED_CUT_OFF_FREQUENCY =
    2.0 * 0.5 / (1000.0 / SKIN_SENSOR_UPDATE_PERIOD_MS);

// Sample timer for filter
Timer<micros> timer = std::round(1e6 / FILTER_SAMPLE_FREQUENCY);
// Sixth-order Butterworth filter
auto filter_0 = butter<6>(FILTER_NORMALIZED_CUT_OFF_FREQUENCY);
auto filter_1 = butter<6>(SKIN_FILTER_NORMALIZED_CUT_OFF_FREQUENCY);
auto filter_2 = butter<6>(FILTER_NORMALIZED_CUT_OFF_FREQUENCY);

extern IncuNest_parameters in3;

long lastCurrentMeasurement, lastVoltageMeasurement;
long lastEncoderUpdate;
static long lastPhotoControl = 0;

void currentMonitor() {
  if (millis() - lastCurrentMeasurement > CURRENT_UPDATE_PERIOD_MS) {
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

  if (in3.phototherapy &&
      (millis() - in3.photoTurnOnTime  > PHOTO_SETTLE_MS) &&
      (millis() - lastPhotoControl     > PHOTO_CONTROL_PERIOD_MS) &&
      in3.phototherapy_current > 0.0f) {

    float error = in3.phototherapy_current - PHOTO_TARGET_CURRENT;
    if (error > PHOTO_TOLERANCE_A || error < -PHOTO_TOLERANCE_A) {
      int newIntensity = (int)(in3.phototherapy_intensity *
                               PHOTO_TARGET_CURRENT / in3.phototherapy_current + 0.5f);
      int delta = newIntensity - (int)in3.phototherapy_intensity;
      if (delta >  PHOTO_MAX_STEP) delta =  PHOTO_MAX_STEP;
      if (delta < -PHOTO_MAX_STEP) delta = -PHOTO_MAX_STEP;
      int next = (int)in3.phototherapy_intensity + delta;
      if (next < PHOTO_MIN_PWM)   next = PHOTO_MIN_PWM;
      if (next > PWM_MAX_VALUE)   next = PWM_MAX_VALUE;
      in3.phototherapy_intensity = (byte)next;
      ledcWrite(PHOTOTHERAPY_PWM_CHANNEL, in3.phototherapy_intensity);
    }
    lastPhotoControl = millis();
  }
}

void voltageMonitor() {
  if (millis() - lastVoltageMeasurement > VOLTAGE_UPDATE_PERIOD_MS) {
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

double measureStabilizedCurrent(bool sensor, int shunt, float offsetCurrent,
                                 float minExpected, float maxExpected,
                                 int maxTimeMs, int intervalMs, int window) {
  // Ventana deslizante de `window` lecturas: compara rango (max-min) contra
  // threshold. Evita salidas prematuras por deriva térmica lenta (heater,
  // fotosterapia), donde |curr-prev| en un solo intervalo es menor que el
  // threshold aunque la corriente siga subiendo.
  // Cargas rápidas: window=3 (~330 ms a 110 ms/intervalo).
  // Cargas térmicas: window=10 (~1.1 s a 110 ms/intervalo).
  const int WINDOW = window;
  float threshold = (maxExpected - minExpected) * CURRENT_STABILIZE_THRESHOLD_RATIO;

  float first = measureMeanConsumption(sensor, shunt) - offsetCurrent;
  float buf[WINDOW];
  for (int i = 0; i < WINDOW; i++) buf[i] = first;

  int idx = 0, count = 0, elapsed = 0, overMaxCount = 0;
  float curr = first;

  while (elapsed < maxTimeMs) {
    vTaskDelay(pdMS_TO_TICKS(intervalMs));
    elapsed += intervalMs;
    curr = measureMeanConsumption(sensor, shunt) - offsetCurrent;
    if (curr > maxExpected) {
      if (++overMaxCount >= 2) return curr;  // confirmed overcurrent, not a transient spike
    } else {
      overMaxCount = 0;
    }
    buf[idx % WINDOW] = curr;
    idx++;
    count++;
    if (count >= WINDOW) {
      float mn = buf[0], mx = buf[0];
      for (int i = 1; i < WINDOW; i++) {
        if (buf[i] < mn) mn = buf[i];
        if (buf[i] > mx) mx = buf[i];
      }
      if ((mx - mn) < threshold && curr >= minExpected && curr <= maxExpected) {
        return curr;
      }
    }
  }
  return curr;
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
    if (sensor == SECUNDARY) {
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

// Valores calibrados: base teórica S-H YSI 44006 (2252 Ω a 25 °C,
// A=1.4733e-3, B=2.3730e-4, C=1.0540e-7) con corrección de +0.37 °C
// aplicada sobre un punto de calibración real (36 °C → leía 36.6 °C).
// Método: R_corr(T) = R_teorica(T - 0.37 °C) para cada entrada.
// Resistencias en orden DESCENDENTE (NTC: más temperatura → menos resistencia).
static const YsiPoint ysi400Table[] = {
    {10.0f, 4574.0f}, {11.0f, 4361.0f}, {12.0f, 4156.0f}, {13.0f, 3964.0f},
    {14.0f, 3781.0f}, {15.0f, 3607.0f}, {16.0f, 3443.0f}, {17.0f, 3288.0f},
    {18.0f, 3141.0f}, {19.0f, 3000.0f}, {20.0f, 2867.0f}, {21.0f, 2741.0f},
    {22.0f, 2621.0f}, {23.0f, 2506.0f}, {24.0f, 2397.0f}, {25.0f, 2293.0f},
    {26.0f, 2194.0f}, {27.0f, 2100.0f}, {28.0f, 2011.0f}, {29.0f, 1927.0f},
    {30.0f, 1846.0f}, {31.0f, 1768.0f}, {32.0f, 1695.0f}, {33.0f, 1626.0f},
    {34.0f, 1559.0f}, {35.0f, 1495.0f}, {36.0f, 1435.0f}, {37.0f, 1377.0f},
    {38.0f, 1322.0f}, {39.0f, 1270.0f}, {40.0f, 1221.0f}, {41.0f, 1173.0f},
    {42.0f, 1127.0f},
};

static float resistanceToTempYSI400(float rntc) {
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
  const float rTop = 2260.0f; // Resistencia del divisor (Ω, 0.1%)
#if (HW_NUM >= 17)
  const float vExc = 2.048f;  // Tensión de excitación (LM4040EIM7-2.0, 2.048 V)
#else
  const float vExc = 3.3f;    // Tensión de excitación (GPIO, 3.3 V)
#endif

  if (adcReading_mV <= 0.0f || adcReading_mV >= vExc * 1000.0f)
    return NAN;

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

// Shared post-processing: calibration, filter, clamp
static bool applyNTCResult(float millivolts) {
  if (millivolts > ADC_TO_DISCARD_MIN && millivolts < ADC_TO_DISCARD_MAX) {
    float tempRaw = adcToCelsius(millivolts);
    if (isnan(tempRaw)) {
      in3.temperature[SKIN_SENSOR] = 0;
      return false;
    }
    // filter_1 tiene ω_n=0.2 a 5 Hz → τ≈1.6 muestras. 200 iteraciones
    // garantizan convergencia total en la primera lectura válida.
    if (lastSuccesfullSensorUpdate[SKIN_SENSOR] == 0) {
      for (int i = 0; i < 200; i++)
        filter_1(tempRaw);
    }
    lastSuccesfullSensorUpdate[SKIN_SENSOR] = millis();
    in3.temperature[SKIN_SENSOR] = filter_1(tempRaw);
    errorTemperature[SKIN_SENSOR] = in3.temperature[SKIN_SENSOR];
    if (RawTemperatureRange[SKIN_SENSOR]) {
      in3.temperature[SKIN_SENSOR] =
          (((in3.temperature[SKIN_SENSOR] - RawTemperatureLow[SKIN_SENSOR]) *
            ReferenceTemperatureRange) /
           RawTemperatureRange[SKIN_SENSOR]) +
          ReferenceTemperatureLow;
    }
    in3.temperature[SKIN_SENSOR] += in3.fineTuneSkinTemperature;
    errorTemperature[SKIN_SENSOR] -= in3.temperature[SKIN_SENSOR];
    if (in3.temperature[SKIN_SENSOR] < 0) {
      in3.temperature[SKIN_SENSOR] = 0;
    }
    return true;
  }
  in3.temperature[SKIN_SENSOR] = 0;
  return false;
}

bool measureSkinSensor() {
#if (HW_NUM >= 16)
  // Alimenta el divisor resistivo, espera estabilización y dispara una
  // conversión single-shot en el ADS1110 (14-bit/60 SPS, PGA=1).
  // [ST=1][SC=1][PGA=00][DR=01][00] = 0xC4
  // Con single-shot no hay riesgo de leer una conversión obsoleta y el
  // tiempo de excitación de la NTC queda en ~22 ms (5 ms settle + ~17 ms
  // conversión), minimizando el autocalentamiento.
  digitalWrite(BABY_TEMP_EN, HIGH);
  vTaskDelay(pdMS_TO_TICKS(22)); // espera estabilización del divisor

  wire->beginTransmission(ADS1110_I2C_ADDRESS);
  wire->write(0xC4); // dispara conversión single-shot
  if (wire->endTransmission() != 0) {
#if SKIN_NTC_PULSED_EXCITATION
    digitalWrite(BABY_TEMP_EN, LOW);
#endif
    in3.temperature[SKIN_SENSOR] = 0;
    return false;
  }

  // Espera a que el ADS1110 complete la conversión (/RDY=0).
  // Timeout: 100 ms (14-bit/60 SPS tarda ~17 ms).
  int16_t raw = 0;
  uint8_t msb = 0, lsb = 0, cfg = 0;
  bool convReady = false;
  uint32_t deadline = millis() + 100;
  while (millis() < deadline) {
    uint8_t n = wire->requestFrom((uint8_t)ADS1110_I2C_ADDRESS, (uint8_t)3);
    if (n < 3) {
#if SKIN_NTC_PULSED_EXCITATION
      digitalWrite(BABY_TEMP_EN, LOW);
#endif
      static uint32_t lastI2cErrLog = 0;
      if (millis() - lastI2cErrLog >= 1000) {
        logI("[SKIN] ADS1110 I2C error: only " + String(n) + " bytes received");
        lastI2cErrLog = millis();
      }
      in3.temperature[SKIN_SENSOR] = 0;
      return false;
    }
    msb = wire->read();
    lsb = wire->read();
    cfg = wire->read();
    if (!(cfg & 0x80)) { // /RDY=0: conversion complete, data fresh
      raw = (int16_t)((msb << 8) | lsb);
      convReady = true;
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
#if SKIN_NTC_PULSED_EXCITATION
  digitalWrite(BABY_TEMP_EN, LOW); // power off NTC divider after read
#endif

  if (!convReady) {
    static uint32_t lastTimeoutLog = 0;
    if (millis() - lastTimeoutLog >= 1000) {
      logI("[SKIN] ADS1110 timeout waiting for /RDY (cfg=0x" +
           String(cfg, HEX) + ")");
      lastTimeoutLog = millis();
    }
    return false;
  }

  // En 14-bit, raw=8191 indica saturación del ADS1110 (vm >= 2.048 V).
  // Esto ocurre cuando la sonda está desconectada (circuito abierto) o
  // cuando la temperatura es tan baja que rNTC > 3697 Ω (~< 16 °C).
  // En ambos casos reportar temperatura=0.
  if (raw >= 8191) {
    in3.temperature[SKIN_SENSOR] = 0;
    return false;
  }

  // ADS1110 right-justifica el dato. En 14-bit (DR=01): rango ±8191,
  // LSB = 4096 mV / 16384 = 0.25 mV/bit.
  float millivolts = raw * 0.25f;
  float tempC = adcToCelsius(millivolts);

  static uint32_t lastAds1110Log = 0;
  if (millis() - lastAds1110Log >= 1000) {
    logI("[SKIN] ADS1110 raw=" + String(raw) + " mV=" + String(millivolts, 1) +
         " T=" + String(tempC, 2) + "C" + " cfg=0x" + String(cfg, HEX));
    lastAds1110Log = millis();
  }

  return applyNTCResult(millivolts);

#else
  int NTCmeasurement;
  if (ADC_READ_FUNCTION == MILLIVOTSREAD_ADC) {
    NTCmeasurement = analogReadMilliVolts(BABY_NTC_PIN);
  } else {
    NTCmeasurement = analogRead(BABY_NTC_PIN);
  }
  return applyNTCResult((float)NTCmeasurement);
#endif
}

bool updateRoomSensor() {
  static char errorMessage[64];

  // Lecturas aisladas para decidir primario/redundante tras el loop.
  float sts_sum = 0.0f;
  int sts_count = 0;
  bool shtc3_ok = false;
  float shtc3_T = 0.0f;
  float shtc3_H = 0.0f;

  in3.airTemperatureRedundantSensor = 0;

  for (int i = 0; i < ROOM_SENSOR_POSIBILITIES; i++) {
    if (!roomSensorPresent[i])
      continue;

    switch (i) {
    case ROOM_SENSOR_STS3X_MAIN:
    case ROOM_SENSOR_STS3X_REDUNDANT: {
      float aTemperature = 0.0f;
      uint16_t err =
          mySTS35[i].measureSingleShot(REPEATABILITY_HIGH, false, aTemperature);
      if (err != NO_ERROR) {
        errorToString(err, errorMessage, sizeof(errorMessage));
        logI(String("Error in measureSingleShot() STS35: ") +
             String(errorMessage));
        break;
      }
      if (aTemperature > DIG_TEMP_TO_DISCARD_MIN &&
          aTemperature < DIG_TEMP_TO_DISCARD_MAX) {
        sts_sum += aTemperature;
        sts_count++;
        logI(String("STS35 OK: ") + String(aTemperature, 2) + " °C");
      } else {
        logI(String("STS35 out of range: ") + String(aTemperature, 2) + " °C");
      }
      break;
    }

    case ROOM_SENSOR_SHTC3: {
      SHTC3_Status_TypeDef shtc3_sensor_status = mySHTC3.update();
      if (!shtc3_sensor_status) {
        float sensedTemperature = mySHTC3.toDegC();
        if (sensedTemperature > DIG_TEMP_TO_DISCARD_MIN &&
            sensedTemperature < DIG_TEMP_TO_DISCARD_MAX) {
          shtc3_ok = true;
          shtc3_T = sensedTemperature;
          shtc3_H = mySHTC3.toPercent();
          logI(String("SHTC3 OK: ") + String(sensedTemperature, 2) + " °C, " +
               String(shtc3_H, 2) + " %RH");
        }
      }
      break;
    }
    default:
      break;
    }
  }

  // SHTC3 es la única fuente de humedad: actualiza siempre que haya lectura.
  if (shtc3_ok) {
    in3.humidity[ROOM_DIGITAL_HUM_SENSOR] = shtc3_H;
  }

  // Primario: STS35 si lee; si no, fallback al SHTC3. Redundante: SHTC3 solo
  // cuando el STS35 provee el primario (de lo contrario no hay redundancia).
  if (sts_count > 0) {
    lastSuccesfullSensorUpdate[ROOM_DIGITAL_TEMP_SENSOR] = millis();
    in3.temperature[ROOM_DIGITAL_TEMP_SENSOR] = sts_sum / sts_count;
    if (shtc3_ok) {
      in3.airTemperatureRedundantSensor = shtc3_T;
    }
    return true;
  }

  if (shtc3_ok) {
    lastSuccesfullSensorUpdate[ROOM_DIGITAL_TEMP_SENSOR] = millis();
    in3.temperature[ROOM_DIGITAL_TEMP_SENSOR] = shtc3_T;
    return true;
  }

  in3.temperature[ROOM_DIGITAL_TEMP_SENSOR] = 0;
  in3.humidity[ROOM_DIGITAL_HUM_SENSOR] = 0;
  initRoomSensor();
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
