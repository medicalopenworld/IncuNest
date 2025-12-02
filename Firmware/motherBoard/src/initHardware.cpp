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
extern double fineTuneSkinTemperature;
extern double RawTemperatureLow[SENSOR_TEMP_QTY],
    RawTemperatureRange[SENSOR_TEMP_QTY];
extern double provisionalRawTemperatureLow[SENSOR_TEMP_QTY];
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

// room variables
extern bool controlAlgorithm;

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

extern double Kp[numPID], Ki[numPID], Kd[numPID];
extern PID airControlPID;
extern PID skinControlPID;
extern PID humidityControlPID;

extern int ScreenBacklightMode;

#define testMode false
#define operativeMode true

#define CURRENT_STABILIZE_TIME_DEFAULT 700
#define CURRENT_STABILIZE_TIME_HEATER 1000

#define NTC_BABY_MIN 1
#define NTC_BABY_MAX 60
#define DIG_TEMP_ROOM_MIN 1
#define DIG_TEMP_ROOM_MAX 60
#define DIG_HUM_ROOM_MIN 1
#define DIG_HUM_ROOM_MAX 100

#if (HW_NUM != 6)
#define HEATER_CONSUMPTION_MIN 1.5
#define FAN_CONSUMPTION_MIN 0.03
#define PHOTOTHERAPY_CONSUMPTION_MIN 0.3
#define HUMIDIFIER_CONSUMPTION_MIN 0.07

#define HEATER_CONSUMPTION_MAX 20
#define FAN_CONSUMPTION_MAX 0.8
#define PHOTOTHERAPY_CONSUMPTION_DEFAULT 0.6
#define PHOTOTHERAPY_CONSUMPTION_MAX 3
#define HUMIDIFIER_CONSUMPTION_MAX 0.8

#define STANDBY_CONSUMPTION_MIN 0
#define STANDBY_CONSUMPTION_MAX 1

#define SCREEN_CONSUMPTION_MIN 0.005
#define SCREEN_CONSUMPTION_MAX 1

#define BUZZER_CONSUMPTION_MIN 0
#else
#define HEATER_CONSUMPTION_MIN 0
#define FAN_CONSUMPTION_MIN 0
#define PHOTOTHERAPY_CONSUMPTION_MIN 0
#define HUMIDIFIER_CONSUMPTION_MIN 0

#define HEATER_CONSUMPTION_MAX 10000
#define FAN_CONSUMPTION_MAX 10000
#define PHOTOTHERAPY_CONSUMPTION_MAX 10000
#define HUMIDIFIER_CONSUMPTION_MAX 10000

#define STANDBY_CONSUMPTION_MIN 0
#define STANDBY_CONSUMPTION_MAX 10

#define SCREEN_CONSUMPTION_MIN 0
#define SCREEN_CONSUMPTION_MAX 10

#define BUZZER_CONSUMPTION_MIN 0
#endif

long HW_error = false;
long lastTFTCheck;
int tft_width, tft_height;

extern in3ator_parameters in3;
TCA9535 TCA(0x20);

bool initI2C() {
  int clkSpeed = false;
  for (int i = 0; i < INIT_I2C_RETRIES; i++) {
    logI("[HW] -> Initializing i2c port");
    Wire.begin(I2C_SDA, I2C_SCL, DEFAULT_I2C_SPEED);
    wire = &Wire;
    clkSpeed = Wire.getClock();
    if (clkSpeed) {
      logI("[HW] -> I2c port initialized with clock speed: " +
           String(clkSpeed));
      return true;
    }
  }
  logI("[HW] -> I2c init error");
  return false;
}

void initPWMGPIO() {
  logI("[HW] -> Initialiting PWM GPIOs");
  ledcSetup(HEATER_PWM_CHANNEL, DEFAULT_PWM_FREQUENCY, DEFAULT_PWM_RESOLUTION);
  ledcSetup(BUZZER_PWM_CHANNEL, DEFAULT_PWM_FREQUENCY, DEFAULT_PWM_RESOLUTION);
  ledcSetup(SCREENBACKLIGHT_PWM_CHANNEL, DEFAULT_PWM_FREQUENCY,
            DEFAULT_PWM_RESOLUTION);
  ledcSetup(PHOTOTHERAPY_PWM_CHANNEL, DEFAULT_PWM_FREQUENCY,
            DEFAULT_PWM_RESOLUTION);
  ledcAttachPin(SCREENBACKLIGHT, SCREENBACKLIGHT_PWM_CHANNEL);
  ledcAttachPin(HEATER, HEATER_PWM_CHANNEL);
  ledcAttachPin(BUZZER, BUZZER_PWM_CHANNEL);
  ledcAttachPin(PHOTOTHERAPY, PHOTOTHERAPY_PWM_CHANNEL);
  ledcWrite(SCREENBACKLIGHT_PWM_CHANNEL, false);
  ledcWrite(HEATER_PWM_CHANNEL, false);
  ledcWrite(BUZZER_PWM_CHANNEL, false);
  ledcWrite(PHOTOTHERAPY_PWM_CHANNEL, false);
#if (HW_NUM >= 6)
  ledcSetup(FAN_PWM_CHANNEL, LOW_PWM_FREQUENCY, DEFAULT_PWM_RESOLUTION);
  ledcAttachPin(FAN, FAN_PWM_CHANNEL);
  ledcWrite(FAN_PWM_CHANNEL, false);
#endif

#if (HW_NUM == 8)
  ledcSetup(HUMIDIFIER_PWM_CHANNEL, HUMIDIFIER_PWM_FREQUENCY,
            DEFAULT_PWM_RESOLUTION);
  ledcAttachPin(HUMIDIFIER_CTL, HUMIDIFIER_PWM_CHANNEL);
  ledcWrite(HUMIDIFIER_CTL, false);
#endif
  logI("[HW] -> PWM GPIOs initialized");
}

void initGPIO() {
  initI2C();
  logI("[HW] -> Initializing GPIOs");
#if (HW_NUM == 6)
  TCA.begin();
  for (int pin = 0; pin < 16; pin++) {
    TCA.setPolarity(pin, false);
  }
  initPin(UNUSED_GPIO_EXP0, OUTPUT);
  initPin(UNUSED_GPIO_EXP1, OUTPUT);
  initPin(UNUSED_GPIO_EXP2, OUTPUT);
  initPin(UNUSED_GPIO_EXP3, OUTPUT);
  GPIOWrite(UNUSED_GPIO_EXP0, HIGH);
  GPIOWrite(UNUSED_GPIO_EXP1, HIGH);
  GPIOWrite(UNUSED_GPIO_EXP2, HIGH);
  GPIOWrite(UNUSED_GPIO_EXP3, HIGH);
  initPin(GPRS_EN, OUTPUT);
  GPIOWrite(GPRS_EN, HIGH);
  initPin(HUMIDIFIER_CTL, OUTPUT);
  GPIOWrite(HUMIDIFIER_CTL, LOW);
  GPIOWrite(TFT_CS_EXP, LOW);
#elif (HW_NUM == 8)
  initPin(HUMIDIFIER_PWM, OUTPUT);
#endif
#if (HW_NUM >= 10)
  initPin(FAN_SPEED_FEEDBACK, INPUT_PULLUP);
#endif
#if (HW_NUM >= 14)
  initPin(TOUCH_SENSOR, OUTPUT);
  GPIOWrite(TOUCH_SENSOR, LOW);
  initPin(TOUCH_SENSOR_SEL, OUTPUT);
  GPIOWrite(TOUCH_SENSOR_SEL, LOW);
#endif
#if (GPRS_PWRKEY)
  initPin(GPRS_PWRKEY, OUTPUT);
#endif
  initPin(encoderpinA, INPUT_PULLUP);
  initPin(encoderpinB, INPUT_PULLUP);
  initPin(ENC_SWITCH, INPUT_PULLUP);
  initPin(TFT_CS, OUTPUT);
  initPin(PHOTOTHERAPY, OUTPUT);
  initPin(FAN, OUTPUT);
  initPin(HEATER, OUTPUT);
  initPin(BUZZER, OUTPUT);
  initPin(SCREENBACKLIGHT, OUTPUT);
  initPin(ACTUATORS_EN, OUTPUT);
  // GPIOWrite(FAN, LOW);
  //  initPin(ON_OFF_SWITCH, INPUT);
  initPWMGPIO();
  logI("[HW] -> GPIOs initilialized");
}

void initInterrupts() {
  attachInterrupt(ENC_SWITCH, encSwitchHandler, CHANGE);
  attachInterrupt(ENC_A, encoderISR, CHANGE);
  attachInterrupt(ENC_B, encoderISR, CHANGE);

#if (HW_NUM >= 10)
  attachInterrupt(FAN_SPEED_FEEDBACK, fanEncoderISR, CHANGE);
#endif
}

void initRoomSensor() {
  static int16_t room_sensor_error;
  static char errorMessage[64];

  for (int i = 0; i < ROOM_SENSOR_POSIBILITIES; i++) {
    roomSensorPresent[i] = false;

    uint8_t addr = 0;
    switch (i) {
      case ROOM_SENSOR_SHTC3:           addr = ROOM_SENSOR_SHTC3_I2C_ADDRESS; break;
      case ROOM_SENSOR_STS3X_MAIN:      addr = ROOM_SENSOR_STS35_I2C_ADDRESS_MAIN; break;
      case ROOM_SENSOR_STS3X_REDUNDANT: addr = ROOM_SENSOR_STS35_I2C_ADDRESS_REDUNDANT; break;
      default: continue;
    }

    wire->beginTransmission(addr);
    roomSensorPresent[i] = (wire->endTransmission() == 0);
    if (!roomSensorPresent[i]) continue;

    logI(String("[HW] -> Room sensor found at 0x") + String(addr, HEX) + ", initializing...");

    switch (i) {
      case ROOM_SENSOR_STS3X_MAIN:
      case ROOM_SENSOR_STS3X_REDUNDANT: {
        mySTS35[i].begin(Wire, addr);
        mySTS35[i].stopMeasurement();                        // <-- salir de periódico por si acaso
        vTaskDelay(pdMS_TO_TICKS(INIT_ROOM_SENSOR_STS3X_DELAY));
        mySTS35[i].softReset();
        vTaskDelay(pdMS_TO_TICKS(INIT_ROOM_SENSOR_STS3X_DELAY));

        uint16_t aStatusRegister = 0u;
        room_sensor_error = mySTS35[i].readStatusRegister(aStatusRegister);
        if (room_sensor_error != NO_ERROR) {
          errorToString(room_sensor_error, errorMessage, sizeof(errorMessage));
          logI(String("Error in readStatusRegister(): ") + String(errorMessage));
          return;
        }
        logI(String("aStatusRegister: ") + String(aStatusRegister));

        // NO startPeriodicMeasurement aquí (usaremos single-shot en update)
        break;
      }

      case ROOM_SENSOR_SHTC3: {
        mySHTC3.begin(Wire);
        logI("SHTC3 initialized");
        break;
      }
    }
  }
}



void initAmbientSensor() {
  ambientSensorPresent = false;
  wire->beginTransmission(AMBIENT_SENSOR_I2C_ADDRESS);
  ambientSensorPresent = !(wire->endTransmission());
  if (ambientSensorPresent == true) {
    logI("[HW] -> Ambient sensor succesfully found, initializing...");
    sht4.begin(&Wire);
    sht4.setPrecision(SHT4X_HIGH_PRECISION);
  }
}

bool initCurrentSensor(bool currentSensor) {
  for (int i = 0; i < INIT_CURRENT_SENSOR_RETRIES; i++) {
    if (currentSensor == MAIN) {
      logI("[HW] -> Initialiting MAIN current sensor");
      wire->beginTransmission(MAIN_DIGITAL_CURRENT_SENSOR_I2C_ADDRESS);
    } else {
      logI("[HW] -> Initialiting SECUNDARY current sensor");
      wire->beginTransmission(SECUNDARY_DIGITAL_CURRENT_SENSOR_I2C_ADDRESS);
    }
    if (!(wire->endTransmission())) {
      digitalCurrentSensorPresent[currentSensor] = true;
      logI("[HW] ->digital sensor detected");
      if (currentSensor == MAIN) {
        mainDigitalCurrentSensor.begin();
        mainDigitalCurrentSensor.reset();
        // Set shunt resistors to 10 mOhm for all channels
        mainDigitalCurrentSensor.setShuntRes(SYSTEM_SHUNT, PHOTOTHERAPY_SHUNT,
                                             FAN_SHUNT);
        mainDigitalCurrentSensor.setShuntConversionTime(
            INA3221_REG_CONF_CT_140US);
        mainDigitalCurrentSensor.setAveragingMode(INA3221_REG_CONF_AVG_128);
      } else {
        digitalCurrentSensorPresent[currentSensor] = true;
        secundaryDigitalCurrentSensor.begin();
        secundaryDigitalCurrentSensor.reset();
        // Set shunt resistors to 10 mOhm for all channels
        secundaryDigitalCurrentSensor.setShuntRes(HEATER_SHUNT, USB_SHUNT,
                                                  BATTERY_SHUNT);
        secundaryDigitalCurrentSensor.setShuntConversionTime(
            INA3221_REG_CONF_CT_140US);
        secundaryDigitalCurrentSensor.setAveragingMode(
            INA3221_REG_CONF_AVG_128);
      }
      return (true);
    } else {
      logE("[HW] -> no digital sensor detected");
    }
    vTaskDelay(pdMS_TO_TICKS(INIT_CURRENT_SENSOR_DELAY));
  }
  return (false);
}

void addErrorToVar(long &errorVar, int error) { errorVar |= (1 << error); }

void initSensors() {
  initCurrentSensor(MAIN);
  initCurrentSensor(SECUNDARY);
  initRoomSensor();
  initAmbientSensor();
}

void testSensors() {
  long error = HW_error;
  logI("[HW] -> Initialiting sensors");
  // sensors verification
  for (int i = 0; i <= NTC_SAMPLES_TEST; i++) {
    measureNTCTemperature();
  }

  if (in3.temperature[SKIN_SENSOR] < NTC_BABY_MIN) {
    logE("[HW] -> Fail -> NTC temperature is lower than expected");
    addErrorToVar(HW_error, NTC_BABY_MIN_ERROR);
  }
  if (in3.temperature[SKIN_SENSOR] > NTC_BABY_MAX) {
    logE("[HW] -> Fail -> NTC temperature is higher than expected");
    addErrorToVar(HW_error, NTC_BABY_MAX_ERROR);
  }
  if (updateRoomSensor()) {
    if (in3.temperature[ROOM_DIGITAL_TEMP_SENSOR] < DIG_TEMP_ROOM_MIN) {
      logE("[HW] -> Fail -> Room temperature is lower than expected");
      addErrorToVar(HW_error, DIG_TEMP_ROOM_MIN_ERROR);
    }
    if (in3.temperature[ROOM_DIGITAL_TEMP_SENSOR] > DIG_TEMP_ROOM_MAX) {
      logE("[HW] -> Fail -> Room temperature is higher than expected");
      addErrorToVar(HW_error, DIG_TEMP_ROOM_MAX_ERROR);
    }
    if (in3.humidity[ROOM_DIGITAL_HUM_SENSOR] < DIG_HUM_ROOM_MIN) {
      logE("[HW] -> Fail -> Room humidity is lower than expected");
      addErrorToVar(HW_error, DIG_HUM_ROOM_MIN_ERROR);
    }
    if (in3.humidity[ROOM_DIGITAL_HUM_SENSOR] > DIG_HUM_ROOM_MAX) {
      logE("[HW] -> Fail -> Room humidity is higher than expected");
      addErrorToVar(HW_error, DIG_HUM_ROOM_MAX_ERROR);
    }
  } else {
    addErrorToVar(HW_error, DIGITAL_SENSOR_NOTFOUND);
    logE("[HW] -> Fail -> No room sensor found");
  }
  if (error == HW_error) {
    logI("[HW] -> OK -> Sensors are working as expected");
  }
}

void testStandByCurrent() {
  long error = HW_error;
  float testCurrent;
  logI("[HW] -> Measuring standby current...");

  testCurrent = measureMeanConsumption(MAIN, SYSTEM_SHUNT_CHANNEL);
  if (testCurrent < STANDBY_CONSUMPTION_MIN) {
    addErrorToVar(HW_error, DEFECTIVE_CURRENT_SENSOR);
    logE("[HW] -> Fail -> Defective current sensor");
  }
  if (testCurrent > STANDBY_CONSUMPTION_MAX) {
    addErrorToVar(HW_error, STANDBY_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> Maximum stanby current exceeded");
  }
  if (error == HW_error) {
    logI("[HW] -> OK -> Current sensor is working as expected: " +
         String(testCurrent) + " Amps");
  } else {
    logE("[HW] -> Fail -> test current is " + String(testCurrent) + " Amps");
  }
  in3.system_current_standby_test = testCurrent;
}

void initTFT() {

#if (HW_NUM < 15)
  tft.init();
#if (HW_NUM == 6)
  GPIOWrite(TFT_CS_EXP, HIGH);
  vTaskDelay(pdMS_TO_TICKS(5));
  GPIOWrite(TFT_CS_EXP, LOW);
#endif
  tft.setRotation(DISPLAY_DEFAULT_ROTATION);
  tft.fillScreen(TFT_BLACK);
  tft_width = tft.width();
  tft_height = tft.height();
#endif
}

void testTFT() {
#if (HW_NUM < 15)
  long error = HW_error;
  float testCurrent, offsetCurrent;
  int backlight_start_value, backlight_end_value;
  offsetCurrent = measureMeanConsumption(MAIN, SYSTEM_SHUNT_CHANNEL);
#if (HW_NUM == 6)
  initPin(TOUCH_CS, OUTPUT);
  initPin(SD_CS, OUTPUT);
  initPin(TFT_RST, OUTPUT);
  initPin(TFT_CS_EXP, OUTPUT);
  initPin(TFT_DC, OUTPUT);
  GPIOWrite(TOUCH_CS, HIGH);
  GPIOWrite(SD_CS, HIGH);
  GPIOWrite(TFT_CS_EXP, HIGH);
  // GPIOWrite(TFT_RST, LOW); // alternating HIGH/LOW
  // delay(5);
  // GPIOWrite(TFT_RST, HIGH); // alternating HIGH/LOW
  // delay(5);
#endif
  loadlogo();
  if (BACKLIGHT_CONTROL == DIRECT_BACKLIGHT_CONTROL) {
    backlight_start_value = false;
    backlight_end_value = BACKLIGHT_POWER_DEFAULT;
  } else {
    backlight_start_value = BACKLIGHT_POWER_DEFAULT;
    backlight_end_value = false;
  }
  for (int i = backlight_start_value; i < backlight_end_value; i++) {
    ledcWrite(SCREENBACKLIGHT_PWM_CHANNEL, i);
    vTaskDelay(pdMS_TO_TICKS(BACKLIGHT_DELAY));
    if (BACKLIGHT_CONTROL == INVERTED_BACKLIGHT_CONTROL) {
      i -= 2;
    }
  }
  vTaskDelay(pdMS_TO_TICKS(INIT_TFT_DELAY));
  testCurrent =
      measureMeanConsumption(MAIN, SYSTEM_SHUNT_CHANNEL) - offsetCurrent;
  if (testCurrent < SCREEN_CONSUMPTION_MIN) {
    // addErrorToVar(HW_error, DEFECTIVE_SCREEN;
    logE("[HW] -> WARNING -> Screen current is not high enough");
  }
  if (testCurrent > SCREEN_CONSUMPTION_MAX) {
    // addErrorToVar(HW_error, DEFECTIVE_SCREEN;
    logE("[HW] -> WARNING -> Screen current exceeded");
  }
  if (error == HW_error) {
    logI("[HW] -> OK -> Screen is working as expected: " + String(testCurrent) +
         " Amps");
  } else {
    logE("[HW] -> Fail -> test current is " + String(testCurrent) + " Amps");
  }
  in3.display_current_test = testCurrent;
#endif
}

void testBuzzer() {
  long error = HW_error;
  float testCurrent, offsetCurrent;

  offsetCurrent = measureMeanConsumption(MAIN, SYSTEM_SHUNT_CHANNEL);
  ledcWrite(BUZZER_PWM_CHANNEL, BUZZER_HALF_PWM);
  vTaskDelay(pdMS_TO_TICKS(CURRENT_STABILIZE_TIME_DEFAULT));
  testCurrent =
      measureMeanConsumption(MAIN, SYSTEM_SHUNT_CHANNEL) - offsetCurrent;
  ledcWrite(BUZZER_PWM_CHANNEL, false);
  vTaskDelay(pdMS_TO_TICKS(CURRENT_STABILIZE_TIME_DEFAULT));
  if (testCurrent < BUZZER_CONSUMPTION_MIN) {
    addErrorToVar(HW_error, DEFECTIVE_BUZZER);
    logE("[HW] -> Fail -> Buzzer current is not high enough");
  }
  if (error == HW_error) {
    logI("[HW] -> OK -> Buzzer is working as expected: " + String(testCurrent) +
         " Amps");
  } else {
    logE("[HW] -> Fail -> test current is " + String(testCurrent) + " Amps");
  }
  in3.buzzer_current_test = testCurrent;
}

bool actuatorsTest() {
  long error = HW_error;
  logI("[HW] -> Checking actuators...");
  GPIOWrite(ACTUATORS_EN, HIGH);

  float testCurrent, offsetCurrent;
  offsetCurrent = measureMeanConsumption(MAIN, SYSTEM_SHUNT_CHANNEL);
  ledcWrite(HEATER_PWM_CHANNEL, PWM_MAX_VALUE);
  vTaskDelay(pdMS_TO_TICKS(CURRENT_STABILIZE_TIME_HEATER));
  testCurrent =
      measureMeanConsumption(MAIN, SYSTEM_SHUNT_CHANNEL) - offsetCurrent;
  logI("[HW] -> Heater current consumption: " + String(testCurrent) + " Amps");
  in3.heater_current_test = testCurrent;
  ledcWrite(HEATER_PWM_CHANNEL, 0);
  if (testCurrent < HEATER_CONSUMPTION_MIN) {
    addErrorToVar(HW_error, HEATER_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> Heater current consumption is too low");
    in3.alarmToReport[HEATER_ISSUE_ALARM] = true;
    setAlarm(HEATER_ISSUE_ALARM);
    GPIOWrite(ACTUATORS_EN, LOW);
    return (true);
  }
  EEPROM.write(EEPROM_HEATER_TEST, true);
  EEPROM.commit();
  // if (testCurrent > HEATER_CONSUMPTION_MAX)
  // {
  //   addErrorToVar(HW_error, HEATER_CONSUMPTION_MAX_ERROR);
  //   logE("[HW] -> Fail -> Heater current consumption is too high");
  //   in3.alarmToReport[HEATER_ISSUE_ALARM] = true;
  //   setAlarm(HEATER_ISSUE_ALARM);
  //   return (true);
  // }
  vTaskDelay(pdMS_TO_TICKS(CURRENT_STABILIZE_TIME_DEFAULT));
  offsetCurrent = measureMeanConsumption(MAIN, PHOTOTHERAPY_SHUNT_CHANNEL);
  ledcWrite(PHOTOTHERAPY_PWM_CHANNEL, PWM_MAX_VALUE);
  vTaskDelay(pdMS_TO_TICKS(CURRENT_STABILIZE_TIME_DEFAULT));
  testCurrent =
      measureMeanConsumption(MAIN, PHOTOTHERAPY_SHUNT_CHANNEL) - offsetCurrent;
  ledcWrite(PHOTOTHERAPY_PWM_CHANNEL, false);
  logI("[HW] -> Phototherapy current consumption: " + String(testCurrent) +
       " Amps");
  in3.phototherapy_current_test = testCurrent;
  if (testCurrent < PHOTOTHERAPY_CONSUMPTION_MIN) {
    addErrorToVar(HW_error, PHOTOTHERAPY_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> PHOTOTHERAPY current consumption is too low");
  }
  if (testCurrent > PHOTOTHERAPY_CONSUMPTION_MAX) {
    addErrorToVar(HW_error, PHOTOTHERAPY_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> PHOTOTHERAPY current consumption is too high");
    GPIOWrite(ACTUATORS_EN, LOW);
    return (true);
  } else if (testCurrent > PHOTOTHERAPY_CONSUMPTION_DEFAULT) {
    in3.phototherapy_intensity =
        PHOTOTHERAPY_CONSUMPTION_DEFAULT * PWM_MAX_VALUE / testCurrent;
    logI("[HW] -> Phototherapy regulated to: " +
         String(float(in3.phototherapy_intensity) * 100 / PWM_MAX_VALUE) +
         " %");
  }
  offsetCurrent = measureMeanConsumption(
      SECUNDARY, USB_SHUNT_CHANNEL); // <- UPDATE THIS CODE TO ASK I2C DATA
  in3_hum.turn(ON);
  vTaskDelay(pdMS_TO_TICKS(CURRENT_STABILIZE_TIME_DEFAULT));
  testCurrent = measureMeanConsumption(SECUNDARY, USB_SHUNT_CHANNEL) -
                offsetCurrent; // <- UPDATE THIS CODE TO ASK I2C DATA
  logI("[HW] -> Humidifier current consumption: " + String(testCurrent) +
       " Amps");
  in3.humidifier_current_test = testCurrent;
  in3_hum.turn(OFF);
  if (testCurrent < HUMIDIFIER_CONSUMPTION_MIN) {
    addErrorToVar(HW_error, HUMIDIFIER_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> HUMIDIFIER current consumption is too low");
  }
  if (testCurrent > HUMIDIFIER_CONSUMPTION_MAX) {
    addErrorToVar(HW_error, HUMIDIFIER_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> HUMIDIFIER current consumption is too high");
    GPIOWrite(ACTUATORS_EN, LOW);
    return (true);
  }
  vTaskDelay(pdMS_TO_TICKS(CURRENT_STABILIZE_TIME_DEFAULT));
  offsetCurrent = measureMeanConsumption(MAIN, FAN_SHUNT_CHANNEL);
// GPIOWrite(FAN, HIGH);
#if (HW_NUM >= 8)
  ledcWrite(FAN_PWM_CHANNEL, PWM_MAX_VALUE);
#else
  GPIOWrite(FAN, HIGH);
#endif

  vTaskDelay(pdMS_TO_TICKS(CURRENT_STABILIZE_TIME_DEFAULT));
  testCurrent = measureMeanConsumption(MAIN, FAN_SHUNT_CHANNEL) - offsetCurrent;
  logI("[HW] -> FAN consumption: " + String(testCurrent) + " Amps");
  in3.fan_current_test = testCurrent;
  // GPIOWrite(FAN, LOW);
#if (HW_NUM >= 8)
  ledcWrite(FAN_PWM_CHANNEL, false);
#else
  GPIOWrite(FAN, LOW);
#endif

  if (testCurrent < FAN_CONSUMPTION_MIN) {
    addErrorToVar(HW_error, FAN_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> Fan current consumption is too low");
    GPIOWrite(ACTUATORS_EN, LOW);
    return (true);
  }
  if (testCurrent > FAN_CONSUMPTION_MAX &&
      testCurrent > FAN_MAX_CURRENT_OVERRIDE * FAN_CONSUMPTION_MAX * 2) {
    addErrorToVar(HW_error, FAN_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> Fan current consumption is too high");
    GPIOWrite(ACTUATORS_EN, LOW);
    return (true);
  }
  if (error == HW_error) {
    logI("[HW] -> OK -> Actuators are working as expected");
  } else {
    logI("[HW] -> Fail -> Some actuators are not working as expected");
  }
  GPIOWrite(ACTUATORS_EN, LOW);
  return (false);
}

bool initActuators() {
#if (HW_NUM <= 6)
  in3_hum.begin(HUMIDIFIER_BINARY, HUMIDIFIER_CTL);
#elif (HW_NUM <= 8)
  in3_hum.begin(HUMIDIFIER_PWM, HUMIDIFIER_CTL);
#else
  in3_hum.begin();
#endif
  if (!digitalCurrentSensorPresent[MAIN] && EEPROM.read(EEPROM_HEATER_TEST) &&
      USE_SYSTEM_WITHOUT_ACTUATORS_TEST) {
    logI("[HW] -> Fail -> No current sensor present, but still giving "
         "possibility to use incubator");
    return false;
  }
  return (actuatorsTest());
}

void initPin(uint8_t GPIO, uint8_t Mode) {
  if (GPIO < GPIO_EXP_BASE) {
    pinMode(GPIO, Mode);
  } else {
    TCA.pinMode1(GPIO - GPIO_EXP_BASE, Mode);
  }
}

void GPIOWrite(uint8_t GPIO, uint8_t Mode) {
  if (GPIO < GPIO_EXP_BASE) {
    digitalWrite(GPIO, Mode);
  } else {
    logI("[HW] -> TCA9355 writing pin" + String(GPIO - GPIO_EXP_BASE) + " -> " +
         String(Mode));
    if (!TCA.write1(GPIO - GPIO_EXP_BASE, Mode)) {
      logE("[HW] -> TCA9355 WRITE ERROR");
    }
  }
}

bool GPIORead(uint8_t GPIO) {
  if (GPIO < GPIO_EXP_BASE) {
    return (digitalRead(GPIO));
  } else {
    return (TCA.read1(GPIO - GPIO_EXP_BASE));
  }
}

void security_check_reboot_cause() {
  in3.resetReason = esp_reset_reason();
  switch (in3.resetReason) {
  case ESP_RST_BROWNOUT: // Brownout reset (voltage too low)
    logI("[HW] -> Brownout reset (voltage too low)");
    break;
  case ESP_RST_POWERON: // Power-on reset
    logI("[HW] -> Power-on reset");
    break;
  case ESP_RST_EXT: // Reset by external pin
    logI("[HW] -> Reset by external pin");
    break;
  case ESP_RST_SW: // Software reset via esp_restart
    logI("[HW] -> Software reset");
    break;
  case ESP_RST_DEEPSLEEP: // Reset after exiting deep sleep mode
    logI("[HW] -> Reset after exiting deep sleep mode");
    break;
  case ESP_RST_PANIC:    // Software reset due to exception/panic
  case ESP_RST_INT_WDT:  // Reset (software or hardware) due to interrupt
                         // watchdog
  case ESP_RST_TASK_WDT: // Reset due to task watchdog
  case ESP_RST_WDT:      // Reset due to other watchdogs
    logI("[HW] -> Reset due to error");
    in3.restoreState = true;
    break;

  // Add any other reset reasons you are interested in
  default:
    logI("Reset for another reason");
  }
}

void initHardware(bool printOutputTest) {
  logI("[HW] -> Initialiting hardware");
  initSensors();
  initTFT();
  initInterrupts();
  PIDInit();
  if (!in3.restoreState) {
    testStandByCurrent();
    testTFT();
    testBuzzer();
  }
  ledcWrite(SCREENBACKLIGHT_PWM_CHANNEL, BACKLIGHT_POWER_DEFAULT);
  testSensors();
  in3.HW_critical_error = initActuators();
  if (!HW_error) {
    logI("[HW] -> HARDWARE OK");
  } else {
    logE("[HW] -> HARDWARE TEST FAIL");
    logE("[HW] -> HARDWARE ERROR CODE:" + String(HW_error, HEX));
  }
  in3.HW_test_error_code = HW_error;
  if (printOutputTest || in3.HW_critical_error || in3.calibrationError) {
    logE("[HW] -> PRINTING ERROR TO USER");
#if (HW_NUM < 15)
    drawHardwareErrorMessage(HW_error, in3.HW_critical_error,
                             in3.calibrationError);
#endif
    while (GPIORead(ENC_SWITCH))
      ;
  }
  if (!in3.restoreState) {
    buzzerTone(2, buzzerStandbyToneDuration, buzzerStandbyTone);
  }
  if (in3.phototherapy) {
    ledcWrite(PHOTOTHERAPY_PWM_CHANNEL,
              in3.phototherapy * in3.phototherapy_intensity);
    turnFans(in3.phototherapy);
  }
  watchdogInit(WDT_TIMEOUT);
  initAlarms();
}