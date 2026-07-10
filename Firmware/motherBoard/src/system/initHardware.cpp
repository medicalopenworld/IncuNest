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
extern TwoWire *wire2;
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
extern PID fanControlPID;
extern double fanControlPIDOutput;
extern PID skinControlPID;
extern PID humidityControlPID;

#define testMode false
#define operativeMode true

#define CURRENT_STABILIZE_TIME_DEFAULT 700
#define CURRENT_STABILIZE_TIME_HEATER 1000
#define CURRENT_STABILIZE_MAX_TIME 2000
// Cargas térmicas (heater, fotosterapia): necesitan más tiempo para que la
// ventana deslizante de measureStabilizedCurrent (WINDOW=10 × 200ms = 2 s)
// detecte estabilización real y no una deriva lenta.
#define CURRENT_STABILIZE_MAX_TIME_THERMAL 8000

// filter_0 (fan RPM Butterworth filter, src/legacy/sensors.cpp) is stateful
// and cold at boot — it needs several real samples to settle before its
// output is trustworthy. See the fanSpeedHandler() settle loop below.
#define FAN_RPM_SETTLE_INTERVAL_MS 20
#define FAN_RPM_SETTLE_ITERATIONS 100 // ~2 s total

#define INA3221_RESET_DELAY_MS 100
#define INA3221_FIRST_CONVERSION_DELAY_MS 150
// One fresh INA3221 conversion cycle: AVG128 x 140us x 2 (bus+shunt) x 3ch ~= 107ms, rounded up.
#define INA3221_ONE_CYCLE_SETTLE_MS 200
// USB_FAULT GPIO latches within ~50ms of USB_EN assertion; 100ms gives 2x margin.
#define USB_FAULT_SETTLE_MS 100
// Startup beep duration: clearly audible but not drawn out.
#define BUZZER_BEEP_DURATION_MS 300

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

#define HEATER_CONSUMPTION_MAX 13
#define FAN_CONSUMPTION_MAX 0.8
#define PHOTOTHERAPY_CONSUMPTION_DEFAULT 0.45
#define PHOTOTHERAPY_CONSUMPTION_MAX 3
#define PHOTOTHERAPY_INITIAL_PWM_PCT 40
#define HUMIDIFIER_CONSUMPTION_MAX 0.8

#define STANDBY_CONSUMPTION_MIN 0
#define STANDBY_CONSUMPTION_MAX 1

#define BUZZER_CONSUMPTION_MIN 0
#define BUZZER_CONSUMPTION_MAX 1
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

long HW_error = 0;
long lastTFTCheck;
int tft_width, tft_height;

extern IncuNest_parameters in3;
TCA9535 TCA(0x20);

bool initI2C() {
  int clkSpeed = 0;
  for (int i = 0; i < INIT_I2C_RETRIES; i++) {
    logI("[HW] -> Initializing I2C1 (SDA=" + String(I2C_SDA) +
         " SCL=" + String(I2C_SCL) + ")");
    Wire.begin(I2C_SDA, I2C_SCL, DEFAULT_I2C_SPEED);
    wire = &Wire;
    clkSpeed = Wire.getClock();
    if (clkSpeed) {
      logI("[HW] -> I2C1 initialized, clock: " + String(clkSpeed));
      break;
    }
  }
  if (!clkSpeed) {
    logI("[HW] -> I2C1 init error");
    return false;
  }

#if (HW_NUM >= 16)
  logI("[HW] -> Initializing I2C2 (SDA=" + String(I2C2_SDA) +
       " SCL=" + String(I2C2_SCL) + ")");
  Wire1.begin(I2C2_SDA, I2C2_SCL, DEFAULT_I2C_SPEED);
  wire2 = &Wire1;
  logI("[HW] -> I2C2 initialized (SHTC3 + STS35 bus)");
#endif

  return true;
}

void initPWMGPIO() {
  logI("[HW] -> Initialiting PWM GPIOs");
  ledcSetup(HEATER_PWM_CHANNEL, HEATER_PWM_FREQUENCY, DEFAULT_PWM_RESOLUTION);
  ledcAttachPin(HEATER, HEATER_PWM_CHANNEL);
  ledcSetup(BUZZER_PWM_CHANNEL, BUZZER_PWM_FREQUENCY, DEFAULT_PWM_RESOLUTION);
  ledcSetup(SCREENBACKLIGHT_PWM_CHANNEL, BUZZER_PWM_FREQUENCY,
            DEFAULT_PWM_RESOLUTION);
  ledcSetup(PHOTOTHERAPY_PWM_CHANNEL, PHOTOTHERAPY_PWM_FREQUENCY,
            DEFAULT_PWM_RESOLUTION);
  ledcAttachPin(SCREENBACKLIGHT, SCREENBACKLIGHT_PWM_CHANNEL);
  ledcAttachPin(BUZZER, BUZZER_PWM_CHANNEL);
  ledcAttachPin(PHOTOTHERAPY, PHOTOTHERAPY_PWM_CHANNEL);
  ledcWrite(SCREENBACKLIGHT_PWM_CHANNEL, 0);
  ledcWrite(HEATER_PWM_CHANNEL, 0);
  ledcWrite(BUZZER_PWM_CHANNEL, 0);
  ledcWrite(PHOTOTHERAPY_PWM_CHANNEL, 0);
#if (HW_NUM >= 6)
  ledcSetup(FAN_PWM_CHANNEL, FAN_PWM_FREQUENCY, DEFAULT_PWM_RESOLUTION);
  ledcAttachPin(FAN, FAN_PWM_CHANNEL);
  ledcWrite(FAN_PWM_CHANNEL, 0);
#endif
#if (HW_NUM >= 16)
  ledcSetup(FAN_CTL_PWM_CHANNEL, FAN_PWM_FREQUENCY, DEFAULT_PWM_RESOLUTION);
  ledcAttachPin(FAN_CTL, FAN_CTL_PWM_CHANNEL);
  ledcWrite(FAN_CTL_PWM_CHANNEL, in3.fanCtlPWM);
#endif

#if (HW_NUM == 8)
  ledcSetup(HUMIDIFIER_PWM_CHANNEL, HUMIDIFIER_PWM_FREQUENCY,
            DEFAULT_PWM_RESOLUTION);
  ledcAttachPin(HUMIDIFIER_CTL, HUMIDIFIER_PWM_CHANNEL);
  ledcWrite(HUMIDIFIER_CTL, 0);
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
  pinMode(UNUSED_GPIO_EXP0, OUTPUT);
  pinMode(UNUSED_GPIO_EXP1, OUTPUT);
  pinMode(UNUSED_GPIO_EXP2, OUTPUT);
  pinMode(UNUSED_GPIO_EXP3, OUTPUT);
  digitalWrite(UNUSED_GPIO_EXP0, HIGH);
  digitalWrite(UNUSED_GPIO_EXP1, HIGH);
  digitalWrite(UNUSED_GPIO_EXP2, HIGH);
  digitalWrite(UNUSED_GPIO_EXP3, HIGH);
  pinMode(GPRS_EN, OUTPUT);
  digitalWrite(GPRS_EN, HIGH);
  pinMode(HUMIDIFIER_CTL, OUTPUT);
  digitalWrite(HUMIDIFIER_CTL, LOW);
  digitalWrite(TFT_CS_EXP, LOW);
#elif (HW_NUM == 8)
  pinMode(HUMIDIFIER_PWM, OUTPUT);
#endif
#if (HW_NUM >= 9)
  pinMode(FAN_SPEED_FEEDBACK, INPUT_PULLUP);
#endif
#if (GPRS_PWRKEY)
  pinMode(GPRS_PWRKEY, OUTPUT);
#endif
  pinMode(encoderpinA, INPUT_PULLUP);
  pinMode(encoderpinB, INPUT_PULLUP);
  pinMode(ENC_SWITCH, INPUT_PULLUP);
  pinMode(TFT_CS, OUTPUT);
  pinMode(PHOTOTHERAPY, OUTPUT);
  pinMode(FAN, OUTPUT);
  pinMode(HEATER, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(SCREENBACKLIGHT, OUTPUT);
  pinMode(ACTUATORS_EN, OUTPUT);
#if (HW_NUM >= 16)
  pinMode(PWR_EN, OUTPUT);
  digitalWrite(PWR_EN, HIGH);    // keep LOW until power-latch check in setup()
  pinMode(ON_OFF_SWITCH, INPUT); // active HIGH: pressed=HIGH, released=LOW
  pinMode(USB_EN, OUTPUT);
  digitalWrite(USB_EN, LOW); // humidifier OFF by default
  pinMode(USB_FAULT, INPUT_PULLUP); // open-drain activo bajo: pull-up necesario
#endif
  initPWMGPIO();
  logI("[HW] -> GPIOs initilialized");
}

void initInterrupts() {
#if (HW_NUM <= 13)
  attachInterrupt(ENC_SWITCH, encSwitchHandler, CHANGE);
  attachInterrupt(ENC_A, encoderISR, CHANGE);
  attachInterrupt(ENC_B, encoderISR, CHANGE);
#endif

#if (HW_NUM >= 9)
  attachInterrupt(FAN_SPEED_FEEDBACK, fanEncoderISR, CHANGE);
#endif
}

void initRoomSensor() {
  static int16_t room_sensor_error;
  static char errorMessage[64];

  // Without an explicit timeout, the ESP32 Wire library blocks ~1s per probe
  // for any address that returns NACK. At 3 sensor possibilities this costs ~3s.
#if (HW_NUM >= 16)
  wire2->setTimeOut(10);
#else
  wire->setTimeOut(10);
#endif

  for (int i = 0; i < ROOM_SENSOR_POSIBILITIES; i++) {
    roomSensorPresent[i] = false;

    uint8_t addr = 0;
    switch (i) {
    case ROOM_SENSOR_SHTC3:
      addr = ROOM_SENSOR_SHTC3_I2C_ADDRESS;
      break;
    case ROOM_SENSOR_STS3X_MAIN:
      addr = ROOM_SENSOR_STS35_I2C_ADDRESS_MAIN;
      break;
    case ROOM_SENSOR_STS3X_REDUNDANT:
      addr = ROOM_SENSOR_STS35_I2C_ADDRESS_REDUNDANT;
      break;
    default:
      continue;
    }

#if (HW_NUM >= 16)
    wire2->beginTransmission(addr);
    roomSensorPresent[i] = (wire2->endTransmission() == 0);
#else
    wire->beginTransmission(addr);
    roomSensorPresent[i] = (wire->endTransmission() == 0);
#endif
    if (!roomSensorPresent[i])
      continue;

    logI(String("[HW] -> Room sensor found at 0x") + String(addr, HEX) +
         ", initializing...");

    switch (i) {
    case ROOM_SENSOR_STS3X_MAIN:
    case ROOM_SENSOR_STS3X_REDUNDANT: {
#if (HW_NUM >= 16)
      mySTS35[i].begin(Wire1, addr);
#else
      mySTS35[i].begin(Wire, addr);
#endif
      mySTS35[i].stopMeasurement(); // <-- salir de periódico por si acaso
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
#if (HW_NUM >= 16)
      mySHTC3.begin(Wire1);
#else
      mySHTC3.begin(Wire);
#endif
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
        vTaskDelay(pdMS_TO_TICKS(100));
        // Set shunt resistors
        mainDigitalCurrentSensor.setShuntRes(SYSTEM_SHUNT, PHOTOTHERAPY_SHUNT,
                                             FAN_SHUNT);
        mainDigitalCurrentSensor.setShuntConversionTime(
            INA3221_REG_CONF_CT_140US);
        mainDigitalCurrentSensor.setAveragingMode(INA3221_REG_CONF_AVG_128);
      } else {
        digitalCurrentSensorPresent[currentSensor] = true;
        secundaryDigitalCurrentSensor.begin();
        secundaryDigitalCurrentSensor.reset();
        vTaskDelay(pdMS_TO_TICKS(INA3221_RESET_DELAY_MS));
        secundaryDigitalCurrentSensor.setShuntRes(HEATER_SHUNT, USB_SHUNT,
                                                  BATTERY_SHUNT);
        secundaryDigitalCurrentSensor.setShuntConversionTime(
            INA3221_REG_CONF_CT_140US);
        secundaryDigitalCurrentSensor.setAveragingMode(
            INA3221_REG_CONF_AVG_128);
      }
      // Wait for first full conversion cycle: 128 avg × 140µs × 2 (bus+shunt) ×
      // 3 ch ≈ 108ms
      vTaskDelay(pdMS_TO_TICKS(INA3221_FIRST_CONVERSION_DELAY_MS));
      return (true);
    } else {
      logE("[HW] -> no digital sensor detected");
    }
    vTaskDelay(pdMS_TO_TICKS(INIT_CURRENT_SENSOR_DELAY));
  }
  return (false);
}

void addErrorToVar(long &errorVar, int error) { errorVar |= (1 << error); }

// Without RPM feedback there is no independent way to confirm the fan is
// still spinning once the heater has failed, so the safest option is to
// disable both actuators. This does NOT force a restart: the operator must
// power-cycle the unit manually, same as the existing (unrecoverable within
// the session) HEATER_ISSUE_ALARM behavior today.
static void disableFanOnUnverifiedHeaterFault() {
  if (!in3.fanHasSpeedFeedback) {
    in3.alarmToReport[FAN_ISSUE_ALARM] = true;
    setAlarm(FAN_ISSUE_ALARM);
  }
}

void initSkinSensor() {
#if (HW_NUM >= 16)
  // BABY_TEMP_EN excita el divisor resistivo; se mantiene LOW hasta la medida.
  pinMode(BABY_TEMP_EN, OUTPUT);
#if SKIN_NTC_PULSED_EXCITATION
  digitalWrite(BABY_TEMP_EN, LOW);
#else
  digitalWrite(BABY_TEMP_EN, HIGH);
#endif
  // Configura el ADS1110: single-shot, 14-bit (60 SPS), PGA=1.
  // Bit map: [ST/RDY][SC][PGA1][PGA0][DR1][DR0][0][0]
  //          [  0  ][ 1][  0 ][  0 ][  0][  1][0][0] = 0x44
  // Single-shot evita leer una conversión obsoleta al arranque y reduce
  // el tiempo de excitación de la NTC (~22 ms vs ~80 ms), minimizando
  // el autocalentamiento. 14-bit da 0.008°C de resolución, suficiente.
  // Los datos siguen left-justificados en 16 bits → fórmula raw*0.0625 válida.
  wire->beginTransmission(ADS1110_I2C_ADDRESS);
  wire->write(0x44);
  uint8_t err = wire->endTransmission();
  if (err) {
    logE("[SKIN] ADS1110 init I2C error: " + String(err));
  } else {
    logI("[SKIN] ADS1110 configured: single-shot, 14-bit/60SPS, PGA=1");
  }
#endif
}

void initSensors() {
  initCurrentSensor(MAIN);
  initCurrentSensor(SECUNDARY);
  initRoomSensor();
  initAmbientSensor();
  initSkinSensor();
}

void testSensors() {
  long error = HW_error;
  logI("[HW] -> Initialiting sensors");
  // sensors verification
  measureSkinSensor();
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

  testCurrent = measureStabilizedCurrent(
      MAIN, SYSTEM_SHUNT_CHANNEL, 0, STANDBY_CONSUMPTION_MIN,
      STANDBY_CONSUMPTION_MAX, CURRENT_STABILIZE_MAX_TIME);
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
  digitalWrite(TFT_CS_EXP, HIGH);
  vTaskDelay(pdMS_TO_TICKS(5));
  digitalWrite(TFT_CS_EXP, LOW);
#endif
  tft.setRotation(DISPLAY_DEFAULT_ROTATION);
  tft.fillScreen(TFT_BLACK);
  tft_width = tft.width();
  tft_height = tft.height();
#endif
}

void testDisplay() {
#if (HW_NUM < 15)
  long error = HW_error;
  float testCurrent = 0.0f, offsetCurrent = 0.0f;
  int backlight_start_value, backlight_end_value;
  offsetCurrent = measureMeanConsumption(MAIN, SYSTEM_SHUNT_CHANNEL);
#if (HW_NUM == 6)
  pinMode(TOUCH_CS, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  pinMode(TFT_RST, OUTPUT);
  pinMode(TFT_CS_EXP, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
  digitalWrite(TFT_CS_EXP, HIGH);
  // digitalWrite(TFT_RST, LOW); // alternating HIGH/LOW
  // delay(5);
  // digitalWrite(TFT_RST, HIGH); // alternating HIGH/LOW
  // delay(5);
#endif
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
  testCurrent = measureStabilizedCurrent(
      MAIN, SYSTEM_SHUNT_CHANNEL, offsetCurrent, SCREEN_CONSUMPTION_MIN,
      SCREEN_CONSUMPTION_MAX, CURRENT_STABILIZE_MAX_TIME);
  if (testCurrent < SCREEN_CONSUMPTION_MIN) {
    logE("[HW] -> WARNING -> Screen current is not high enough");
  }
  if (testCurrent > SCREEN_CONSUMPTION_MAX) {
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
  float testCurrent = 0.0f;
    #if(HW_NUM <= 16)
  float offsetCurrent = measureMeanConsumption(MAIN, SYSTEM_SHUNT_CHANNEL);
  ledcWrite(BUZZER_PWM_CHANNEL, BUZZER_HALF_PWM);
  testCurrent = measureStabilizedCurrent(
      MAIN, SYSTEM_SHUNT_CHANNEL, offsetCurrent, BUZZER_CONSUMPTION_MIN,
      BUZZER_CONSUMPTION_MAX, CURRENT_STABILIZE_MAX_TIME);
  ledcWrite(BUZZER_PWM_CHANNEL, 0);
  vTaskDelay(pdMS_TO_TICKS(CURRENT_STABILIZE_TIME_DEFAULT));
  #else
    ledcWrite(BUZZER_PWM_CHANNEL, BUZZER_HALF_PWM);
    vTaskDelay(pdMS_TO_TICKS(BUZZER_BEEP_DURATION_MS));
    ledcWrite(BUZZER_PWM_CHANNEL, 0);
  #endif
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

struct ActuatorResult {
  float heater;
  float photo;
  float fan;
};

// Turns heater (SECUNDARY CH1), phototherapy (MAIN CH2), and fan (MAIN CH3) ON
// simultaneously and measures all three in a single interleaved loop, avoiding
// the ~4s of sequential stabilisation waits. Channels are hardware-independent
// even when on the same INA3221 chip.
static ActuatorResult measureThreeActuatorsParallel(
    float heaterOffset, float photoOffset, float fanOffset,
    float heaterMin, float heaterMax,
    float photoMin,  float photoMax,
    float fanMin,    float fanMax,
    int maxTimeMs, int intervalMs) {
  const int W = 10;
  const float hThresh = (heaterMax - heaterMin) * CURRENT_STABILIZE_THRESHOLD_RATIO;
  const float pThresh = (photoMax  - photoMin)  * CURRENT_STABILIZE_THRESHOLD_RATIO;
  const float fThresh = (fanMax    - fanMin)    * CURRENT_STABILIZE_THRESHOLD_RATIO;

  float hBuf[W], pBuf[W], fBuf[W];
  float h0 = measureMeanConsumption(SECUNDARY, HEATER_SHUNT_CHANNEL)       - heaterOffset;
  float p0 = measureMeanConsumption(MAIN,      PHOTOTHERAPY_SHUNT_CHANNEL) - photoOffset;
  float f0 = measureMeanConsumption(MAIN,      FAN_SHUNT_CHANNEL)          - fanOffset;
  for (int i = 0; i < W; i++) { hBuf[i] = h0; pBuf[i] = p0; fBuf[i] = f0; }

  ActuatorResult r = {h0, p0, f0};
  bool hStable = false, pStable = false, fStable = false;
  int idx = 0, count = 0, elapsed = 0;
  int hOverMax = 0, pOverMax = 0, fOverMax = 0;

  while (elapsed < maxTimeMs && !(hStable && pStable && fStable)) {
    vTaskDelay(pdMS_TO_TICKS(intervalMs));
    elapsed += intervalMs;

    r.heater = measureMeanConsumption(SECUNDARY, HEATER_SHUNT_CHANNEL)       - heaterOffset;
    r.photo  = measureMeanConsumption(MAIN,      PHOTOTHERAPY_SHUNT_CHANNEL) - photoOffset;
    r.fan    = measureMeanConsumption(MAIN,      FAN_SHUNT_CHANNEL)          - fanOffset;

    // Early exit on confirmed overcurrent (2 consecutive readings above max)
    if (r.heater > heaterMax) { if (++hOverMax >= 2) break; } else { hOverMax = 0; }
    if (r.photo  > photoMax)  { if (++pOverMax >= 2) break; } else { pOverMax = 0; }
    if (r.fan    > fanMax)    { if (++fOverMax >= 2) break; } else { fOverMax = 0; }

    hBuf[idx % W] = r.heater;
    pBuf[idx % W] = r.photo;
    fBuf[idx % W] = r.fan;
    idx++; count++;

    if (count < W) continue;

    if (!hStable) {
      float mn = hBuf[0], mx = hBuf[0];
      for (int i = 1; i < W; i++) { if (hBuf[i] < mn) mn = hBuf[i]; if (hBuf[i] > mx) mx = hBuf[i]; }
      hStable = ((mx - mn) < hThresh && r.heater >= heaterMin && r.heater <= heaterMax);
    }
    if (!pStable) {
      float mn = pBuf[0], mx = pBuf[0];
      for (int i = 1; i < W; i++) { if (pBuf[i] < mn) mn = pBuf[i]; if (pBuf[i] > mx) mx = pBuf[i]; }
      pStable = ((mx - mn) < pThresh && r.photo >= photoMin && r.photo <= photoMax);
    }
    if (!fStable) {
      float mn = fBuf[0], mx = fBuf[0];
      for (int i = 1; i < W; i++) { if (fBuf[i] < mn) mn = fBuf[i]; if (fBuf[i] > mx) mx = fBuf[i]; }
      fStable = ((mx - mn) < fThresh && r.fan >= fanMin && r.fan <= fanMax);
    }
  }
  return r;
}

bool actuatorsTest() {
  long error = HW_error;
  logI("[HW] -> Checking actuators...");
  digitalWrite(ACTUATORS_EN, HIGH);

  logI("[HW] -> digitalCurrentSensorPresent MAIN=" +
       String(digitalCurrentSensorPresent[MAIN]) +
       " SECUNDARY=" + String(digitalCurrentSensorPresent[SECUNDARY]));
#if (HW_NUM >= 16)
  // Phototherapy test constants (same formula as original sequential code)
  const int   PHOTOTHERAPY_TEST_PWM = PWM_MAX_VALUE * 10 / 100;
  const float PHOTOTHERAPY_PWM_ZERO = 20.0f;
  const float photoScale = (PHOTOTHERAPY_TEST_PWM + PHOTOTHERAPY_PWM_ZERO) /
                           (PWM_MAX_VALUE          + PHOTOTHERAPY_PWM_ZERO);

  // Measure all baselines while all actuators are OFF
  float heaterOffset = measureMeanConsumption(SECUNDARY, HEATER_SHUNT_CHANNEL);
  float photoOffset  = measureMeanConsumption(MAIN,      PHOTOTHERAPY_SHUNT_CHANNEL);
  float fanOffset    = measureMeanConsumption(MAIN,      FAN_SHUNT_CHANNEL);
  logI("[HW] -> Heater offset: " + String(heaterOffset, 3) +
       "  Photo offset: " + String(photoOffset, 3) +
       "  Fan offset: " + String(fanOffset, 3) + " A");

  // Turn on heater and phototherapy; delay to let first INA3221 samples arrive,
  // then turn on fan so its spin-up overlaps with heater/photo thermal ramp.
  // The fan runs at its configured default operating speed (not full PWM) so
  // the current and RPM measured below reflect real operating conditions.
  ledcWrite(HEATER_PWM_CHANNEL,       PWM_MAX_VALUE);
  ledcWrite(PHOTOTHERAPY_PWM_CHANNEL, PHOTOTHERAPY_TEST_PWM);
  vTaskDelay(pdMS_TO_TICKS(INA3221_ONE_CYCLE_SETTLE_MS));
  ledcWrite(FAN_CTL_PWM_CHANNEL, in3.fanCtlPWM);
  ledcWrite(FAN_PWM_CHANNEL, in3.fanPwrSupplyPWM);
  vTaskDelay(pdMS_TO_TICKS(220));
  logI("[HW] -> Heater + Phototherapy + Fan ON, measuring in parallel...");

  ActuatorResult res = measureThreeActuatorsParallel(
      heaterOffset, photoOffset, fanOffset,
      HEATER_CONSUMPTION_MIN,                        HEATER_CONSUMPTION_MAX,
      PHOTOTHERAPY_CONSUMPTION_MIN * photoScale,     PHOTOTHERAPY_CONSUMPTION_MAX * photoScale,
      FAN_CONSUMPTION_MIN,                           FAN_CONSUMPTION_MAX,
      CURRENT_STABILIZE_MAX_TIME_THERMAL,            110);

  ledcWrite(HEATER_PWM_CHANNEL,       0);
  ledcWrite(PHOTOTHERAPY_PWM_CHANNEL, 0);
  ledcWrite(FAN_PWM_CHANNEL,          0);

  in3.heater_current_test       = res.heater;
  in3.phototherapy_current_test = res.photo;
  in3.fan_current_test          = res.fan;

  logI("[HW] -> Heater: "       + String(res.heater, 3) + " A  (min=" +
       String(HEATER_CONSUMPTION_MIN) + " max=" + String(HEATER_CONSUMPTION_MAX) + ")");
  logI("[HW] -> Phototherapy: " + String(res.photo, 3)  + " A  @10% PWM");
  logI("[HW] -> FAN: "          + String(res.fan, 3)    + " A");

  // Heater checks (critical — abort on failure)
  if (res.heater < HEATER_CONSUMPTION_MIN) {
    addErrorToVar(HW_error, HEATER_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> Heater current too low");
    in3.alarmToReport[HEATER_ISSUE_ALARM] = true;
    setAlarm(HEATER_ISSUE_ALARM);
    disableFanOnUnverifiedHeaterFault();
    // Fan measured in parallel — report it too if also bad
    if (res.fan < FAN_CONSUMPTION_MIN) {
      addErrorToVar(HW_error, FAN_CONSUMPTION_MIN_ERROR);
      logE("[HW] -> Fail -> Fan current also too low (wiring error)");
      in3.alarmToReport[FAN_ISSUE_ALARM] = true;
      setAlarm(FAN_ISSUE_ALARM);
    }
    digitalWrite(ACTUATORS_EN, LOW);
    return true;
  }
  if (res.heater > HEATER_CONSUMPTION_MAX) {
    addErrorToVar(HW_error, HEATER_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> Heater current too high");
    in3.alarmToReport[HEATER_ISSUE_ALARM] = true;
    setAlarm(HEATER_ISSUE_ALARM);
    disableFanOnUnverifiedHeaterFault();
    digitalWrite(ACTUATORS_EN, LOW);
    return true;
  }
  { Preferences p; p.begin(NS_CFG, false); p.putUChar(KEY_HEATER_TEST, 1); p.end(); }

  // Phototherapy checks
  if (res.photo < PHOTOTHERAPY_CONSUMPTION_MIN * photoScale) {
    addErrorToVar(HW_error, PHOTOTHERAPY_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> Phototherapy current too low at 10%");
  }
  if (res.photo > PHOTOTHERAPY_CONSUMPTION_MAX * photoScale) {
    addErrorToVar(HW_error, PHOTOTHERAPY_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> Phototherapy current too high at 10%");
    digitalWrite(ACTUATORS_EN, LOW);
    return true;
  }

  // Extrapolate phototherapy PWM for the target operating current. This is a
  // 1/x extrapolation from a 10%-PWM reading: a res.photo close to zero
  // (noise, or a reading that only barely cleared the MIN check above) blows
  // the result up towards/beyond PWM_MAX_VALUE. Only commit it — and only
  // clear photoFirstRun — when the *raw*, unclamped result actually lands in
  // range; otherwise leave phototherapy_intensity/photoFirstRun untouched so
  // the safe PHOTOTHERAPY_INITIAL_PWM_PCT fallback (main.cpp/initHardware.cpp
  // photoFirstRun checks) applies instead of silently maxing out the light.
  int pwmTargetRaw = (int)roundf(
      PHOTOTHERAPY_CONSUMPTION_DEFAULT *
          (PHOTOTHERAPY_TEST_PWM + PHOTOTHERAPY_PWM_ZERO) / res.photo -
      PHOTOTHERAPY_PWM_ZERO);
  if (pwmTargetRaw < 0 || pwmTargetRaw > PWM_MAX_VALUE) {
    addErrorToVar(HW_error, PHOTOTHERAPY_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> Phototherapy current too low/high to extrapolate a "
         "reliable intensity (raw PWM=" + String(pwmTargetRaw) + ")");
  } else {
    in3.phototherapy_intensity = pwmTargetRaw;
    in3.photoFirstRun = false;
    logI("[HW] -> Phototherapy extrapolated PWM=" + String(pwmTargetRaw) +
         " (" + String(pwmTargetRaw * 100 / PWM_MAX_VALUE) + "%) for " +
         String(PHOTOTHERAPY_CONSUMPTION_DEFAULT, 2) + " A");
  }

  // Fan checks
  if (res.fan < FAN_CONSUMPTION_MIN) {
    addErrorToVar(HW_error, FAN_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> Fan current too low");
    in3.alarmToReport[FAN_ISSUE_ALARM] = true;
    setAlarm(FAN_ISSUE_ALARM);
    digitalWrite(ACTUATORS_EN, LOW);
    return true;
  }
  if (res.fan > FAN_CONSUMPTION_MAX &&
      res.fan > FAN_MAX_CURRENT_OVERRIDE * FAN_CONSUMPTION_MAX * 2) {
    addErrorToVar(HW_error, FAN_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> Fan current too high");
    in3.alarmToReport[FAN_ISSUE_ALARM] = true;
    setAlarm(FAN_ISSUE_ALARM);
    digitalWrite(ACTUATORS_EN, LOW);
    return true;
  }

  // Fan type detection: does this unit's assembled fan report RPM pulses?
  // Persisted so restoreState boots (which skip this whole test) still know.
#if defined(FAN_SPEED_FEEDBACK)
  // Fan was cut along with heater/phototherapy above (this file, a few lines
  // up) — re-power it here so the RPM/duty checks below measure a genuinely
  // driven fan, not one coasting to a stop for the ~4s these checks take.
  ledcWrite(FAN_CTL_PWM_CHANNEL, in3.fanCtlPWM);
  ledcWrite(FAN_PWM_CHANNEL, in3.fanPwrSupplyPWM);

  // fanSpeedHandler() reads the last ISR-latched pulse period, not a live
  // poll. This relies on being the very first fanSpeedHandler() call at
  // boot: sensors_Task/security_Task (the only other callers) start after
  // initHardware() returns — see the fan-speed-feedback design doc.
  //
  // filter_0 (used inside fanSpeedHandler()) is a stateful 6th-order
  // Butterworth filter that has never been fed a sample before this point.
  // Its very first output from a cold/zero state is close to zero, which
  // sends FAN_RPM_CONVERSION/period to an absurd value (observed: RPM in
  // the trillions in the boot log) instead of a real reading. Feed it real
  // pulses for a couple of seconds so it settles before trusting fan_rpm.
  for (int i = 0; i < FAN_RPM_SETTLE_ITERATIONS; i++) {
    vTaskDelay(pdMS_TO_TICKS(FAN_RPM_SETTLE_INTERVAL_MS));
    fanSpeedHandler();
  }
  in3.fanHasSpeedFeedback = (in3.fan_rpm > 0);
  { Preferences p; p.begin(NS_CFG, false);
    p.putUChar(KEY_FAN_RPM_FEEDBACK, in3.fanHasSpeedFeedback); p.end(); }
  logI("[HW] -> Fan type: " +
       String(in3.fanHasSpeedFeedback ? "RPM feedback" : "no RPM feedback") +
       " (" + String(in3.fan_rpm) + " rpm)");
  if (in3.fanHasSpeedFeedback && in3.fan_rpm < FAN_MIN_RPM) {
    addErrorToVar(HW_error, FAN_RPM_MIN_ERROR);
    logE("[HW] -> Fail -> Fan RPM too low (" + String(in3.fan_rpm) +
         " < " + String(FAN_MIN_RPM) + ")");
    in3.alarmToReport[FAN_ISSUE_ALARM] = true;
    setAlarm(FAN_ISSUE_ALARM);
  }
  if (in3.fanHasSpeedFeedback && in3.fanPidEnabled &&
      in3.fan_rpm >= FAN_MIN_RPM) {
    // Engage closed-loop control and let it settle at the real target
    // before checking how much duty it took to get there. Seed the loop at
    // the duty the fan is already running at (bumpless: PID_v1 latches
    // *myOutput into its integral sum on the MANUAL->AUTOMATIC edge) so the
    // 2s settle window measures a converged trim, not a wind-up from zero.
    fanControlPIDOutput = in3.fanCtlPWM;
    fanControlPID.SetMode(AUTOMATIC);
    for (int i = 0; i < FAN_RPM_SETTLE_ITERATIONS; i++) {
      vTaskDelay(pdMS_TO_TICKS(FAN_RPM_SETTLE_INTERVAL_MS));
      fanSpeedHandler();
      fanControlPID.Compute();
      ledcWrite(FAN_CTL_PWM_CHANNEL, fanControlPIDOutput);
    }
    // Always logged (even with detection disabled) — this is the bench data
    // FAN_DUTY_BLOCKED_THRESHOLD must be calibrated from.
    logI("[HW] -> Fan duty to hold " + String(FAN_TARGET_RPM) + " rpm: " +
         String(fanControlPIDOutput) + " (rpm=" + String(in3.fan_rpm) + ")");
#if AIR_BLOCKED_DETECTION_ENABLED
    if (fanControlPIDOutput > FAN_DUTY_BLOCKED_THRESHOLD) {
      logE("[HW] -> Warning -> Fan duty too high, possible air outlet blockage");
      in3.alarmToReport[AIR_BLOCKED_ALARM] = true;
      setAlarm(AIR_BLOCKED_ALARM);
    }
#endif
    // Don't leave the PID AUTOMATIC relying on a later turnFans() call to
    // fix it — the fan is about to be cut below, matching heater/photo.
    fanControlPID.SetMode(MANUAL);
  }
  // Cut the fan back off, matching heater/phototherapy already being off —
  // normal operation re-enables it via turnFans() once boot completes.
  ledcWrite(FAN_CTL_PWM_CHANNEL, 0);
  ledcWrite(FAN_PWM_CHANNEL, 0);
#else
  in3.fanHasSpeedFeedback = false;
#endif

  // Humidifier: GPIO fault-pin check only (HW>=16 has no INA3221 on USB channel)
  in3_hum.turn(ON);
  vTaskDelay(pdMS_TO_TICKS(USB_FAULT_SETTLE_MS));
  bool usbFaultDetected = !GPIORead(USB_FAULT);
  in3_hum.turn(OFF);
  if (usbFaultDetected) {
    addErrorToVar(HW_error, HUMIDIFIER_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> USB_FAULT on humidifier (short-circuit/overload)");
    digitalWrite(ACTUATORS_EN, LOW);
  }
  in3.humidifier_current_test = 1.0f;
  logI("[HW] -> Humidifier USB_EN test passed, no fault");
#else
  float testCurrent = 0.0f, offsetCurrent = 0.0f;
  offsetCurrent = measureMeanConsumption(MAIN, SYSTEM_SHUNT_CHANNEL);
  logI("[HW] -> Heater offset (MAIN): " + String(offsetCurrent) + " Amps");
  ledcWrite(HEATER_PWM_CHANNEL, PWM_MAX_VALUE);
  logI("[HW] -> Heater PWM ON, stabilizing...");
  testCurrent = measureStabilizedCurrent(
      MAIN, SYSTEM_SHUNT_CHANNEL, offsetCurrent, HEATER_CONSUMPTION_MIN,
      HEATER_CONSUMPTION_MAX, CURRENT_STABILIZE_MAX_TIME_THERMAL, 110, 10);
  logI("[HW] -> Heater delta=" + String(testCurrent) + " Amps");
  logI("[HW] -> Heater current consumption: " + String(testCurrent) + " Amps");
  in3.heater_current_test = testCurrent;
  ledcWrite(HEATER_PWM_CHANNEL, 0);
  if (testCurrent < HEATER_CONSUMPTION_MIN) {
    addErrorToVar(HW_error, HEATER_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> Heater current consumption is too low");
    in3.alarmToReport[HEATER_ISSUE_ALARM] = true;
    setAlarm(HEATER_ISSUE_ALARM);
    disableFanOnUnverifiedHeaterFault();
    digitalWrite(ACTUATORS_EN, LOW);
    return (true);
  }
  if (testCurrent > HEATER_CONSUMPTION_MAX) {
    addErrorToVar(HW_error, HEATER_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> Heater current consumption is too high");
    in3.alarmToReport[HEATER_ISSUE_ALARM] = true;
    setAlarm(HEATER_ISSUE_ALARM);
    disableFanOnUnverifiedHeaterFault();
    digitalWrite(ACTUATORS_EN, LOW);
    return (true);
  }
  { Preferences p; p.begin(NS_CFG, false); p.putUChar(KEY_HEATER_TEST, 1); p.end(); }
  // Test a 10 % PWM: no deslumbra, estabilización térmica rápida.
  // La recta I(PWM) = m·PWM + b cruza el eje en PWM = -PHOTOTHERAPY_PWM_ZERO,
  // calibrado con el barrido inicial. La extrapolación es:
  //   PWM_target = I_target·(PWM_test + Z) / I_test − Z
  // Los límites de pass/fail se escalan por (PWM_test+Z)/(PWM_MAX+Z).
  const int   PHOTOTHERAPY_TEST_PWM  = PWM_MAX_VALUE * 10 / 100; // ~25
  const float PHOTOTHERAPY_PWM_ZERO  = 20.0f; // cruce de cero calibrado
  const float photoScale = (PHOTOTHERAPY_TEST_PWM + PHOTOTHERAPY_PWM_ZERO) /
                           (PWM_MAX_VALUE         + PHOTOTHERAPY_PWM_ZERO);

  vTaskDelay(pdMS_TO_TICKS(CURRENT_STABILIZE_TIME_DEFAULT));
  offsetCurrent = measureMeanConsumption(MAIN, PHOTOTHERAPY_SHUNT_CHANNEL);
  ledcWrite(PHOTOTHERAPY_PWM_CHANNEL, PHOTOTHERAPY_TEST_PWM);
  testCurrent = measureStabilizedCurrent(
      MAIN, PHOTOTHERAPY_SHUNT_CHANNEL, offsetCurrent,
      PHOTOTHERAPY_CONSUMPTION_MIN * photoScale,
      PHOTOTHERAPY_CONSUMPTION_MAX * photoScale,
      CURRENT_STABILIZE_MAX_TIME_THERMAL, 110, 10);
  ledcWrite(PHOTOTHERAPY_PWM_CHANNEL, false);

  logI("[HW] -> Phototherapy @10% (PWM=" + String(PHOTOTHERAPY_TEST_PWM) +
       "): " + String(testCurrent, 3) + " A");
  in3.phototherapy_current_test = testCurrent;;
  if (testCurrent < PHOTOTHERAPY_CONSUMPTION_MIN * photoScale) {
    addErrorToVar(HW_error, PHOTOTHERAPY_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> PHOTOTHERAPY current too low at 10%");
  }
  if (testCurrent > PHOTOTHERAPY_CONSUMPTION_MAX * photoScale) {
    addErrorToVar(HW_error, PHOTOTHERAPY_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> PHOTOTHERAPY current too high at 10%");
    digitalWrite(ACTUATORS_EN, LOW);
    return (true);
  }

  // Extrapolación: PWM para PHOTOTHERAPY_CONSUMPTION_DEFAULT amperios. Es una
  // extrapolación 1/x de una lectura al 10% de PWM: un testCurrent cercano a
  // cero (ruido, o una lectura que apenas superó el check MIN de arriba) hace
  // que el resultado se dispare hacia/más allá de PWM_MAX_VALUE. Solo se
  // aplica -y solo se limpia photoFirstRun- cuando el resultado bruto (sin
  // recortar) cae realmente dentro de rango; si no, se deja
  // phototherapy_intensity/photoFirstRun intactos para que el fallback seguro
  // PHOTOTHERAPY_INITIAL_PWM_PCT (checks de photoFirstRun en main.cpp /
  // initHardware.cpp) se aplique en vez de encender la luz a máxima potencia.
  int pwmTargetRaw = (int)roundf(
      PHOTOTHERAPY_CONSUMPTION_DEFAULT *
          (PHOTOTHERAPY_TEST_PWM + PHOTOTHERAPY_PWM_ZERO) / testCurrent -
      PHOTOTHERAPY_PWM_ZERO);
  if (pwmTargetRaw < 0 || pwmTargetRaw > PWM_MAX_VALUE) {
    addErrorToVar(HW_error, PHOTOTHERAPY_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> Phototherapy current too low/high to extrapolate a "
         "reliable intensity (raw PWM=" + String(pwmTargetRaw) + ")");
  } else {
    in3.phototherapy_intensity = pwmTargetRaw;
    in3.photoFirstRun = false;
    logI("[HW] -> Phototherapy extrapolated PWM=" + String(pwmTargetRaw) +
         " (" + String(pwmTargetRaw * 100 / PWM_MAX_VALUE) + "%) for " +
         String(PHOTOTHERAPY_CONSUMPTION_DEFAULT, 2) + " A");
  }
  offsetCurrent = measureMeanConsumption(SECUNDARY, USB_SHUNT_CHANNEL);
  in3_hum.turn(ON);
  vTaskDelay(pdMS_TO_TICKS(CURRENT_STABILIZE_TIME_DEFAULT));
  testCurrent =
      measureMeanConsumption(SECUNDARY, USB_SHUNT_CHANNEL) - offsetCurrent;
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
    digitalWrite(ACTUATORS_EN, LOW);
    return (true);
  }
  vTaskDelay(pdMS_TO_TICKS(CURRENT_STABILIZE_TIME_DEFAULT));
  offsetCurrent = measureMeanConsumption(MAIN, FAN_SHUNT_CHANNEL);
#if (HW_NUM >= 8)
  ledcWrite(FAN_PWM_CHANNEL, in3.fanPwrSupplyPWM);
#else
  digitalWrite(FAN, HIGH);
#endif
  // Wait for motor to spin up and INA3221 to fill its 128-sample buffer
  // before measureStabilizedCurrent takes its first reading (~2 full cycles)
  vTaskDelay(pdMS_TO_TICKS(220));

  testCurrent = measureStabilizedCurrent(
      MAIN, FAN_SHUNT_CHANNEL, offsetCurrent, FAN_CONSUMPTION_MIN,
      FAN_CONSUMPTION_MAX, CURRENT_STABILIZE_MAX_TIME);
  logI("[HW] -> FAN consumption: " + String(testCurrent) + " Amps");
  in3.fan_current_test = testCurrent;
#if (HW_NUM >= 8)
  ledcWrite(FAN_PWM_CHANNEL, 0);
#else
  digitalWrite(FAN, LOW);
#endif

  if (testCurrent < FAN_CONSUMPTION_MIN) {
    addErrorToVar(HW_error, FAN_CONSUMPTION_MIN_ERROR);
    logE("[HW] -> Fail -> Fan current consumption is too low");
    in3.alarmToReport[FAN_ISSUE_ALARM] = true;
    setAlarm(FAN_ISSUE_ALARM);
    digitalWrite(ACTUATORS_EN, LOW);
    return (true);
  }
  if (testCurrent > FAN_CONSUMPTION_MAX &&
      testCurrent > FAN_MAX_CURRENT_OVERRIDE * FAN_CONSUMPTION_MAX * 2) {
    addErrorToVar(HW_error, FAN_CONSUMPTION_MAX_ERROR);
    logE("[HW] -> Fail -> Fan current consumption is too high");
    in3.alarmToReport[FAN_ISSUE_ALARM] = true;
    setAlarm(FAN_ISSUE_ALARM);
    digitalWrite(ACTUATORS_EN, LOW);
    return (true);
  }

  // Fan type detection: does this unit's assembled fan report RPM pulses?
#if defined(FAN_SPEED_FEEDBACK)
  // Fan was just cut above (this file, a few lines up) — re-power it so the
  // settle loop below measures a genuinely driven fan, not one coasting to
  // a stop. FAN_SPEED_FEEDBACK is only ever defined where HW_NUM>=8's
  // ledcWrite branch above applies, so this mirrors that branch.
  ledcWrite(FAN_PWM_CHANNEL, in3.fanPwrSupplyPWM);

  // fanSpeedHandler() reads the last ISR-latched pulse period, not a live
  // poll. This relies on being the very first fanSpeedHandler() call at
  // boot: sensors_Task/security_Task (the only other callers) start after
  // initHardware() returns — see the fan-speed-feedback design doc.
  //
  // filter_0 (used inside fanSpeedHandler()) is a stateful 6th-order
  // Butterworth filter that has never been fed a sample before this point.
  // Its very first output from a cold/zero state is close to zero, which
  // sends FAN_RPM_CONVERSION/period to an absurd value (observed: RPM in
  // the trillions in the boot log) instead of a real reading. Feed it real
  // pulses for a couple of seconds so it settles before trusting fan_rpm.
  for (int i = 0; i < FAN_RPM_SETTLE_ITERATIONS; i++) {
    vTaskDelay(pdMS_TO_TICKS(FAN_RPM_SETTLE_INTERVAL_MS));
    fanSpeedHandler();
  }
  in3.fanHasSpeedFeedback = (in3.fan_rpm > 0);
  { Preferences p; p.begin(NS_CFG, false);
    p.putUChar(KEY_FAN_RPM_FEEDBACK, in3.fanHasSpeedFeedback); p.end(); }
  logI("[HW] -> Fan type: " +
       String(in3.fanHasSpeedFeedback ? "RPM feedback" : "no RPM feedback") +
       " (" + String(in3.fan_rpm) + " rpm)");
  if (in3.fanHasSpeedFeedback && in3.fan_rpm < FAN_MIN_RPM) {
    addErrorToVar(HW_error, FAN_RPM_MIN_ERROR);
    logE("[HW] -> Fail -> Fan RPM too low (" + String(in3.fan_rpm) +
         " < " + String(FAN_MIN_RPM) + ")");
    in3.alarmToReport[FAN_ISSUE_ALARM] = true;
    setAlarm(FAN_ISSUE_ALARM);
  }
  // Cut the fan back off — normal operation re-enables it via turnFans().
  ledcWrite(FAN_PWM_CHANNEL, 0);
#else
  in3.fanHasSpeedFeedback = false;
#endif
#endif
  if (error == HW_error) {
    logI("[HW] -> OK -> Actuators are working as expected");
  } else {
    logI("[HW] -> Fail -> Some actuators are not working as expected");
  }
  digitalWrite(ACTUATORS_EN, LOW);
  return (false);
}

bool initActuators() {
#if (HW_NUM <= 6)
  in3_hum.begin(HUMIDIFIER_BINARY, HUMIDIFIER_CTL);
#elif (HW_NUM <= 8)
  in3_hum.begin(HUMIDIFIER_PWM, HUMIDIFIER_CTL);
#elif (HW_NUM >= 16)
  in3_hum.begin(HUMIDIFIER_BINARY, USB_EN);
#else
  in3_hum.begin();
#endif
  { Preferences _p; _p.begin(NS_CFG, true);
    bool _heaterTest = _p.getUChar(KEY_HEATER_TEST, 0);
    _p.end();
    if (!digitalCurrentSensorPresent[MAIN] && _heaterTest &&
        USE_SYSTEM_WITHOUT_ACTUATORS_TEST) {
      logI("[HW] -> Fail -> No current sensor present, but still giving "
           "possibility to use incubator");
      return false;
    }
    if (!digitalCurrentSensorPresent[SECUNDARY] &&
        _heaterTest && USE_SYSTEM_WITHOUT_ACTUATORS_TEST) {
      logI("[HW] -> Fail -> No secondary current sensor, skipping heater test");
      return false;
    }
  }
  return (actuatorsTest());
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
#if (HW_NUM >= 16)
  logI("[HW] -> Initializing BQ25730 charger");
  if (!init_BQ25730(wire)) {
    logE("[HW] -> BQ25730 not found or init failed");
  }
#endif
  initTFT();
  initInterrupts();
  PIDInit();
  if (!in3.restoreState) {
    testStandByCurrent();
    testDisplay();
    testBuzzer();
  }
  ledcWrite(SCREENBACKLIGHT_PWM_CHANNEL, BACKLIGHT_POWER_DEFAULT);
  testSensors();
  if (!in3.restoreState) {
    in3.HW_critical_error = initActuators();
  } else {
    logI("[HW] -> restoreState: skipping actuatorsTest");
    in3.HW_critical_error = false;
  }
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
    while (GPIORead(ENC_SWITCH))
      ;
#endif
  }
  if (!in3.restoreState) {
    buzzerTone(2, buzzerStandbyToneDuration, buzzerStandbyTone);
  }
  if (in3.phototherapy) {
    // in3.phototherapy was just restored from NVS (EEPROM.cpp restoreState())
    // regardless of whether actuatorsTest() ran. On a restoreState boot
    // (crash/WDT) actuatorsTest() is skipped entirely, so phototherapy_intensity
    // is still its raw struct default (PWM_MAX_VALUE) — apply the same
    // photoFirstRun safe-default fallback main.cpp uses instead of driving
    // the light at full intensity.
    if (in3.photoFirstRun) {
      in3.phototherapy_intensity = PWM_MAX_VALUE * PHOTOTHERAPY_INITIAL_PWM_PCT / 100;
      in3.photoFirstRun = false;
    }
    ledcWrite(PHOTOTHERAPY_PWM_CHANNEL,
              in3.phototherapy * in3.phototherapy_intensity);
    turnFans(in3.phototherapy);
  }
  if (in3.restoreState) {
    // Resuming a control session that was already running, not starting
    // cold - assume the stabilization window already elapsed instead of
    // making temperature/humidity alarms wait out another full one.
    alarmTimerStart(true);
    if (in3.temperatureControl) {
      startPID(in3.controlMode);
      turnFans(ON);
      logI("[HW] -> restoreState: temperature PID restarted, fan ON");
    }
    if (in3.humidityControl) {
      startPID(humidityPID);
      logI("[HW] -> restoreState: humidity PID restarted");
    }
  }
  watchdogInit(WDT_TIMEOUT);
  initAlarms();
}