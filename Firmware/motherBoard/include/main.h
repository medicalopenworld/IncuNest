#ifndef _MAIN_H
#define _MAIN_H

#define TINY_GSM_MODEM_SIM800
#define modemSerial Serial2
#define THINGSBOARD_ENABLE_PSRAM 0
#define THINGSBOARD_ENABLE_DYNAMIC 1
#define THINGSBOARD_ENABLE_STREAM_UTILS 1
#include "ThingsBoard.h"
#include "config/transport_policy.h" // tabla única GPRS/WiFi
#include <Arduino.h>
#include <TinyGsmClient.h>

#include <ESPmDNS.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
// include libraries
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/semphr.h"
#include <Beastdevices_INA3221.h>
#include <Preferences.h>
#include <Filters.h>
#include <RotaryEncoder.h>
#include <Wire.h>

#include <AH/Timing/MillisMicrosTimer.hpp>
#undef DEBUG
#include <Filters/Butterworth.hpp>

#include "Adafruit_GFX.h"
#include "Adafruit_SHT4x.h"
#include "BluetoothSerial.h"
#include "CommTask.h"
#include "control_types.h"
#include "alarm_ids.h"
#include "Credentials_public.h"
#include "ESP32_config.h"
#include "GPRS.h"
#include "PID.h"
#include "SPI.h"
#include "SPO2.h"
#include "SparkFun_SHTC3.h"
#include "TCA9555.h"
#include "Wifi_OTA.h"
#include "board.h"
#include "driver/rtc_io.h"
#include "esp32/ulp.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "IncuNest_humidifier.h"
#include "nvs_flash.h"
#if CONFIG_IDF_TARGET_ESP32S3
#include "usb/cdc_acm_host.h"
#include "usb/usb_host.h"
#include "usb/vcp.hpp"
#include "usb/vcp_ch34x.hpp"
#endif
#include "BQ25730.h"
#include <SensirionI2cSts3x.h>
#include <TFT_eSPI.h> // Hardware-specific library

#include <Arduino_MQTT_Client.h>
#include <Espressif_MQTT_Client.h>
#include <Espressif_Updater.h>

#define HW_REVISION 'A'
#define HWversion String(HW_NUM) + "." + String(HW_REVISION)
#define FWversion "18.12"
#define WIFI_NAME "IncuNest"
#define CURRENT_FIRMWARE_TITLE "IncuNest"

#define DEFAULT_WIFI_EN ON

#define LOG_MODEM_DATA true
#define LOG_INFORMATION false
#define LOG_ERRORS false
#define LOG_ALARMS true
#define LOG_PULSIOXIMETRY false
#define LOG_DRIVE false // Google Drive upload + MB/HMI crash capture
#define LOG_CHARGER false

// Diagnostic: set to 1 to skip the upload task entirely. Writer keeps rotating
// but every closed window is deleted instead of enqueued. Isolates whether the
// crash originates in TLS/upload (heap corruption) or in the writer/littlefs
// path itself.
#define DRIVE_DISABLE_UPLOAD 0

#define USE_SYSTEM_WITHOUT_ACTUATORS_TEST \
  true // only if previous test was OK and that fail cause is not being able to
       // read current measurements
#define WDT_TIMEOUT 75
#if (HW_NUM >= 14 && HW_NUM <= 16)
#define HEATER_MAX_POWER_AMPS 7.5
#else
#define HEATER_MAX_POWER_AMPS 9.5
#endif

#define HEATER_SAFE_MARGIN_AMPS 1

#define HEATER_POWER_FACTOR_INCREASE 3
#define HEATER_POWER_FACTOR_DECREASE 3
// Number of fresh in3.heater_current samples (see lastCurrentMeasurement)
// between each heaterSafeMAXPWM ramp step, instead of a wall-clock period.
#define HEATER_RAMP_SAMPLE_CYCLES 3
// Consecutive failed I2C presence probes of the SECUNDARY current sensor
// (heater/USB/battery chip) before raising ALARM_HEATER_FAULT for a runtime
// dropout - see currentMonitor() in legacy/sensors.cpp. ~10 x 110ms = 1.1s,
// long enough to ride out a transient EMI glitch without missing a real
// dropout for many cycles.
#define HEATER_SENSOR_DROPOUT_ALARM_CYCLES 10
#define CURRENT_STABILIZE_THRESHOLD_RATIO 0.1

#define FAN_RPM_CONVERSION 13333333
#define FAN_UPDATE_TIME_MIN 1000

#define ALARM_SYSTEM_ENABLED true
#define FAN_MAX_CURRENT_OVERRIDE false

// Minutes the temperature-deviation / humidity-deviation conditions stay
// suppressed after a fresh activation (checkAlarms(), security.cpp) vs. after
// a restoreState boot (crash/WDT - initHardware.cpp), which only needs a short
// re-sync pause rather than a full cold-start stabilization wait.
#define ACTUATORS_ALARM_STABILIZATION_MINS 30
#define RESTART_ALARM_GRACE_MINS 0

#define UKRAINE_MODE false
#define SENEGAL_MODE false

#define HOLD_PRESS_TO_GO_TO_SETTINGS 0

#define BROWN_OUT_BATTERY_MODE 0
#define BROWN_OUT_NORMAL_MODE 0
#define INIT_I2C_RETRIES 3
#define DEFAULT_I2C_SPEED 10000
#define INIT_CURRENT_SENSOR_RETRIES 3

#define ENABLE_WIFI_OTA true // enable wifi OTA
#define ENABLE_GPRS_OTA true // enable GPRS OTA
#define THINGSBOARD_BUFFER_SIZE 4096
// Dimensiona los dos StaticJsonDocument de telemetria (GPRS_JSON y WIFI_JSON),
// a 16 B por slot en ESP32: 112 campos = 1792 B por documento, 3584 B de .bss
// entre los dos.
//
// Subido de 64 a 96 (commit fb3a535) porque 64 NO llegaba y el desbordamiento
// era SILENCIOSO: ArduinoJson deja de anadir claves sin error y
// sendTelemetryJson() sigue devolviendo true. Las que se perdian eran las
// ultimas insertadas -- SpO2, PI y HR1-3 con sus SQI, es decir constantes
// vitales. En el cuadro de mando no se veia un hueco: se veia el ultimo valor
// recibido, congelado.
//
// Subido de 96 a 112 al encender TX_GROUP_SENSORBOARD_GPRS: por GPRS el peor
// caso son 87 claves (CORE 69 + CELLULAR 4 + DIAG 8 + CALIBRATION 6) y el
// bloque sb_* son 12, o sea 99 -- no cabian en 96. Con 112 quedan 13 de
// margen. Cuesta 512 B de .bss. Medir con tools/check_transport_matrix.py
// antes de encender cualquier otro grupo.
//
// Al pasar las claves de alarma de String a const char* (switchAlarmTelemetry*)
// se recuperaron ademas 274 B de pool que se iban en copiar los nombres.
#define THINGSBOARD_FIELDS_AMOUNT 112
#define MAX_MESSAGE_SIZE 1024
#define THINGSBOARD_QOS false
#define TELEMETRIES_DECIMALS 2
#define FIRMWARE_FAILURE_RETRIES 12
// Tamano del trozo de firmware que el cliente pide en cada vuelta de la OTA.
//
// TIENE QUE CABER EN EL BUFFER DEL CLIENTE MQTT, que es MAX_MESSAGE_SIZE
// (1024 B): `ThingsBoard tb(mqttClientGPRS, MAX_MESSAGE_SIZE)` en GPRS.cpp y
// Wifi_OTA.cpp. Ese buffer tiene que alojar el paquete MQTT ENTERO -- cabecera
// y topico ("v2/fw/response/<id>/chunk/<n>", ~30 B) ademas del payload -- asi
// que el trozo util se queda en algo menos de 1 KB. Por eso 512 y no 1024.
//
// Estaba en 4096, que no cabe, y el sintoma no se parecia a la causa: el
// chunk llegaba y se descartaba por tamano, el cliente agotaba sus 10 s
// (WAIT_FAILED_OTA_CHUNKS) y lo volvia a pedir, con algun "Received chunk (0),
// not the same as requested chunk (1)" suelto cuando se cruzaban peticion y
// reenvio. Banco 2026-09-20, OTA por 2G: 92 trozos de 365 en ~9 min con 40
// expiraciones, ~293 B/s. Parecia un problema de cobertura y no lo era.
//
// Subir MAX_MESSAGE_SIZE en vez de bajar esto iria mas rapido (menos vueltas),
// pero el buffer sale del heap interno, y de ese hay ~11 KB libres con WiFi y
// celular arriba (ver docs/known_issues.md y el abort por OOM del port IDF).
// No se toca sin medir.
#define FIRMWARE_PACKET_SIZE 512
#define WAIT_FAILED_OTA_CHUNKS 10U * 1000U * 1000U

// Mutex for protecting the shared variable
extern SemaphoreHandle_t GPRS_monitor_mutex;
extern SemaphoreHandle_t log_mutex;

// Language enum is now in shared control_types.h (Language enum:
// SPANISH=0, ENGLISH, FRENCH, PORTUGUESE, NUM_LANGUAGES)
#define defaultLanguage \
  ENGLISH // Preset number configuration when booting for first time

typedef enum
{
  NTC_BABY_MIN_ERROR = 0,
  NTC_BABY_MAX_ERROR,
  DIG_TEMP_ROOM_MIN_ERROR,
  DIG_TEMP_ROOM_MAX_ERROR,
  DIG_HUM_ROOM_MIN_ERROR,
  DIG_HUM_ROOM_MAX_ERROR,
  DIGITAL_SENSOR_NOTFOUND,
  HEATER_CONSUMPTION_MIN_ERROR,
  FAN_CONSUMPTION_MIN_ERROR,
  PHOTOTHERAPY_CONSUMPTION_MIN_ERROR,
  HUMIDIFIER_CONSUMPTION_MIN_ERROR,
  HEATER_CONSUMPTION_MAX_ERROR,
  FAN_CONSUMPTION_MAX_ERROR,
  PHOTOTHERAPY_CONSUMPTION_MAX_ERROR,
  HUMIDIFIER_CONSUMPTION_MAX_ERROR,
  STANDBY_CONSUMPTION_MAX_ERROR,
  DEFECTIVE_SCREEN,
  DEFECTIVE_BUZZER,
  DEFECTIVE_CURRENT_SENSOR,
  UNCALIBRATED_SENSOR,
  FAN_RPM_MIN_ERROR,
} HW_ERROR_ID;

// AlarmId enum (ALARM_NONE/NO_ALARMS, ALARM_AIR_THERMAL_CUTOUT ...
// ALARM_HUMIDITY_DEVIATION, ALARM_COUNT/NUM_ALARMS,
// MAX_ALARM_STRING_SIZE=255) is now in shared alarm_ids.h.
// CommStatus enum (COMM_STATUS_NONE ... COMM_STATUS_WIFI_SERVER) is now
// in shared control_types.h. Both are included transitively via CommTask.h.

#include "telemetry_keys.h"

extern uint32_t g_bootCount;
// millis() en que arranco sensors_Task; 0 = todavia no. Ver checkStatusOfSensor().
extern uint32_t g_sensorsTaskStartedMs;
extern uint32_t g_gprsKillCount;
extern uint32_t g_monKillCount;
extern int g_hmiBootCount;
extern int g_hmiLastRst;
extern int g_restore_photo_minutes;

#define ANALOGREAD_ADC 0
#define MILLIVOTSREAD_ADC 1

#define ADC_READ_FUNCTION MILLIVOTSREAD_ADC

#define ON true
#define OFF false
#define BASIC_CONTROL false
#define PID_CONTROL true
#define CONTROL_SKIN false
#define CONTROL_AIR true

#include "task_config.h"

#define DIGITAL_CURRENT_SENSOR_READ_PERIOD_MS 500
// 110ms (not 100ms): must exceed one full INA3221 conversion cycle
// (AVG_128 x 140us x 2 (bus+shunt) x 3ch ~= 107.52ms, see initHardware.cpp)
// so every currentMonitor() call reflects a genuinely new conversion, not a
// stale repeat. Matches the interval already used by the boot self-test
// (measureThreeActuatorsParallel(..., 110) in initHardware.cpp).
#define CURRENT_UPDATE_PERIOD_MS 110 // in millis
#define VOLTAGE_UPDATE_PERIOD_MS 50 // in millis
#define UI_SENSOR_UPDATE_PERIOD_MS 1000
#define POWER_SUPPLY_CHECK_PERIOD 2000 // 2 secs

// buzzer variables
#define buzzerStandbyPeriod \
  10000                              // in millis, there will be a periodic tone when regulating baby's
                                     // constants
#define buzzerStandbyTone 500        // in micros, tone freq
#define buzzerRotaryEncoderTone 2200 // in micros, tone freq
#define buzzerStandbyToneDuration 50 // in micros, tone freq
#define buzzerSwitchDuration 10      // in micros, tone freq
#define buzzerStandbyToneTimes 1     // in micros, tone freq

// El patron de rafaga y de pulso de las Tablas 3 y 4 vivia aqui. Se mudo a
// shared/ cuando el display dejo de ser mudo: emite ese mismo patron por su
// propio zumbador para la unica alarma que detecta el solo, la perdida de
// enlace con esta placa. Dos copias de una tabla normativa divergen en el
// primer ajuste, y la divergencia no la delata nadie hasta que suenan los dos
// a la vez con ritmos distintos.
#include "alarm_audio_pattern.h"

// La duracion del AUDIO PAUSED (ALARM_AUDIO_PAUSE_MS) se mudo a
// shared/alarm_policy.h: desde que el display tiene senal acustica propia
// para la perdida de enlace, la pausa de esa senal la cuenta EL.
#include "alarm_policy.h"

#include "preferences_keys.h"

#define SKIN_CALIBRATION_CORRECTION_FACTOR 0

// configuration variables
#define SWITCH_DEBOUNCE_TIME_MS 30 // encoder debouncing time
#define timePressToSettings \
  3000                        // in millis, time to press to go to settings window in UI
#define DEBUG_LOOP_PRINT 1000 // in millis,

#define DEFAULT_CONTROL_MODE CONTROL_AIR

#define setupAutoCalibrationPoint 0
#define firstAutoCalibrationPoint 1
#define secondAutoCalibrationPoint 2

// GPRS variables to transmit
#define turnedOn 0     // transmit first turned ON with hardware verification
#define room 1         // transmit room variables
#define aliveRefresh 2 // message to let know that incubator is still ON

// sensor variables
#define defaultCurrentSamples 30
#define defaultTestingSamples 8000
#define Rsense 3000 // 3 microohm as shunt resistor

#define MAIN 0
#define SECUNDARY 1
// I2C addresses
#define MAIN_DIGITAL_CURRENT_SENSOR_I2C_ADDRESS 0x41
#define SECUNDARY_DIGITAL_CURRENT_SENSOR_I2C_ADDRESS 0x40
#define AMBIENT_SENSOR_I2C_ADDRESS 0x44

// calibration menu
typedef enum
{
  ROOM_SENSOR_STS3X_MAIN = 0,
  ROOM_SENSOR_STS3X_REDUNDANT,
  ROOM_SENSOR_SHTC3,
  ROOM_SENSOR_POSIBILITIES,
} ROOM_SENSORS;

// calibration menu
typedef enum
{
  STS3X_MAIN = 0,
  STS3X_REDUNDANT,
  STS3X_NUM,
} STS3X_SENSORS;

#define ROOM_SENSOR_SHTC3_I2C_ADDRESS 0x70
#define ROOM_SENSOR_STS35_I2C_ADDRESS_MAIN 0x4A
#define ROOM_SENSOR_STS35_I2C_ADDRESS_REDUNDANT 0x4B

// #define system constants
#define HUMIDIFIER_DUTY_CYCLE_MAX \
  95 // maximum humidity cycle in heater to be set
#define HUMIDIFIER_DUTY_CYCLE_MIN \
  0 // minimum humidity cycle in heater to be set

#define stepTemperatureIncrement 0.1 // maximum allowed temperature to be set
#define stepHumidityIncrement 5      // maximum allowed temperature to be set
#define presetHumidity 60            // preset humidity
#define maxHum 90                    // maximum allowed humidity to be set
#define minHum 20                    // minimum allowed humidity to be set

#define SKIN_TEMPERATURE_SET_MIN 35
#define AIR_TEMPERATURE_SET_MIN 30
#define SKIN_TEMPERATURE_SET_MAX 37.5

// CONSIGNA y CORTE TERMICO son dos cosas distintas y hasta 2026-09-14 eran la
// misma constante: AIR_TEMPERATURE_SET_MAX valia 38 y de ahi salian a la vez
// el tope que el operador puede pedir y el umbral al que se dispara el corte.
// Mientras los dos numeros coincidieron nadie lo noto; en cuanto se quiso
// consigna 39 quedo a la vista que subir uno subia el otro.
//
// Ahora van separadas. La de abajo es SOLO el tope de consigna; el umbral del
// corte es in3.airTemperatureSetMax, que arranca de
// AIR_THERMAL_CUTOUT_DEFAULT_C y lo recorta alarm_clamp_air_cutout().
//
// La consigna tiene que quedar POR DEBAJO del corte, si no el equipo no puede
// alcanzar lo que se le pide: al cruzar el umbral salta ALARM_AIR_THERMAL_
// CUTOUT, que es ALTA y corta el calefactor. Hoy 39 < 40 y hay 1 C de margen.
// El numero lo pone shared/alarm_policy.h, que es de donde lo lee tambien el
// display: cuando cada placa tenia el suyo se desincronizaron.
#define AIR_TEMPERATURE_SET_MAX ALARM_AIR_SETPOINT_MAX_C

// Umbral de arranque del corte termico del aire. Ajustable en caliente (por
// /config y por el enlace) y persistido en KEY_AIR_T_MAX, asi que este valor
// solo manda en una unidad sin nada guardado. El techo, y el motivo por el que
// 40 C es una desviacion normativa consciente, estan en shared/alarm_policy.h.
#define AIR_THERMAL_CUTOUT_DEFAULT_C 40

// Encoder variables
#define NUMENCODERS 1 // number of encoders in circuit
#define ENCODER_TICKS_DIV 0
#define encPulseDebounce 200

// Graphic variables
#define ERASE false
#define DRAW true

// graphic text configurations
#define graphicTextOffset 1 // bar pos is counted from 1, but text from 0
#define CENTER true
#define LEFT_MARGIN false

// 2p calibration
#define TEMP_CALIB_UI_ROW 0
#define SET_CALIB_UI_ROW 1

// auto calibration
#define AUTO_CALIB_MESSAGE_UI_ROW 0

#define INIT_I2C_DELAY 50
#define INIT_ROOM_SENSOR_STS3X_DELAY 100
#define INIT_CURRENT_SENSOR_DELAY 50
#define BACKLIGHT_DELAY 2
#define INIT_TFT_DELAY 300
#define WHILE_LOOP_DELAY 1

#define TIME_TRACK_UPDATE_PERIOD 900000 // 15 minutes

typedef struct
{
  int skinSensorCapacitance;
  double temperature[SENSOR_TEMP_QTY];
  double airTemperatureRedundantSensor = 0;
  double humidity[SENSOR_HUM_QTY];
  double desiredControlTemperature = false;
  double desiredControlHumidity = false;
  double fineTuneSkinTemperature = false;
  double fineTuneAirTemperature = false;
  double system_current_standby_test = false;
  double heater_current_test = false;
  double fan_current_test = false;
  double phototherapy_current_test = false;
  double humidifier_current_test = false;
  double display_current_test = false;
  double buzzer_current_test = false;
  bool HW_critical_error = false;
  double HW_test_error_code = false;

  double system_current = false;
  double system_voltage = false;
  double heater_current = false;
  int heaterSafeMAXPWM = HEATER_MAX_PWM;
  double fan_current = false;
  double humidifier_current = false;
  double humidifier_voltage = false;
  double phototherapy_current = false;
  double USB_current = false;
  double USB_voltage = false;
  double BATTERY_current = false;
  double BATTERY_voltage = false;
  int serialNumber = false;
  int resetReason = false;
  bool restoreState = false;
  int actuation = false;

  bool controlMode = DEFAULT_CONTROL_MODE;
  bool temperatureControl = false;
  bool humidityControl = false;
  bool phototherapy = false;
  byte phototherapy_intensity = PWM_MAX_VALUE;
  bool photoFirstRun = true;
  long photoTurnOnTime = 0;

  int fanPwrSupplyPWM = FAN_PWR_SUPPLY_PWM;
  int fanCtlPWM = FAN_CTL_PWM_DEFAULT;
  bool fanHasSpeedFeedback = false;
  bool fanPidEnabled = FAN_PID_ENABLED_DEFAULT;
  float heaterMaxPowerAmps = HEATER_MAX_POWER_AMPS;
  float skinTemperatureSetMax = SKIN_TEMPERATURE_SET_MAX;
  // Pese al nombre no es el tope de consigna, es el UMBRAL DEL CORTE TERMICO
  // (security.cpp: checkThermalCutOuts()). El tope de consigna es
  // AIR_TEMPERATURE_SET_MAX. El nombre se conserva porque viaja al protocolo,
  // a /config como air_tmax y a NVS como KEY_AIR_T_MAX.
  float airTemperatureSetMax = AIR_THERMAL_CUTOUT_DEFAULT_C;
  // Defaults en config/transport_policy.h; /config los sobrescribe en NVS.
  int actuating_gprs_period = TX_GPRS_PERIOD_ACTUATING_S;
  int phototherapy_gprs_period = TX_GPRS_PERIOD_PHOTOTHERAPY_S;
  int standby_gprs_period = TX_GPRS_PERIOD_STANDBY_S;

  bool calibrationError = false;

  long last_check_time = false;
  float standby_time = false;
  float control_active_time = false;
  float heater_active_time = false;
  float fan_active_time = false;
  float phototherapy_active_time = false;
  float humidifier_active_time = false;

  bool alarmsEnabled = true;
  bool alarmToReport[NUM_ALARMS];
  char alarmMessage[MAX_ALARM_STRING_SIZE];
  bool previousAlarmReport;

  double fan_rpm = false;
  bool fanEncoderUpdate = false;
  long fanEncoderPeriod[2] = {false, false};
  bool fanCommandedOn = false;

  byte language;

} IncuNest_parameters;

// ================== POR QUE ESTOS LOGS SON MACROS ==================
//
// El argumento de una llamada se evalua SIEMPRE, antes de entrar. Como casi
// todos los sitios escriben cosas como
//
//     logI("[X] v=" + String(v) + " w=" + String(w));
//
// la cadena se construia —con un temporal y una realocacion por cada `+`—
// aunque el flag del canal estuviera apagado y la funcion fuera a descartarla
// en su primera linea. Trabajo y HEAP gastados para nada, 79 veces repartidas
// por el firmware.
//
// No es teorico: en banco (2026-09-14) una de esas cadenas, la de SPO2.cpp a
// 500 Hz, agoto el heap. `operator new` lanzo std::bad_alloc, nadie lo captura,
// y std::terminate llamo a abort(): la placa que gobierna el calefactor se
// reinicio construyendo una linea de log que NI SIQUIERA SE IMPRIME
// (LOG_PULSIOXIMETRY es false).
//
// Con la macro la expresion queda DENTRO del `if`, y como los flags son
// `#define ... false` el compilador elimina el bloque entero: ni cadena, ni
// asignacion, ni llamada. Encender un canal lo devuelve todo tal cual estaba.
//
// Se conservan los nombres de siempre a proposito: asi los 79 puntos de llamada
// no se tocan, que es justo lo que no conviene mezclar con un arreglo de
// seguridad. Las funciones de verdad pasan a llamarse logX_impl().
//
// El do/while(0) es para que `if (c) logI(x); else ...` siga compilando.
void logE_impl(const String &dataString);
void logAlarm_impl(const String &dataString);
void logI_impl(const String &dataString);
void logCharger_impl(const String &dataString);
void logModemData_impl(const String &dataString);
void logSPO2_impl(const String &dataString);
void logDrive_impl(const String &dataString);

#define logI(expr)         do { if (LOG_INFORMATION)   { logI_impl(expr); } } while (0)
#define logE(expr)         do { if (LOG_ERRORS)        { logE_impl(expr); } } while (0)
#define logAlarm(expr)     do { if (LOG_ALARMS)        { logAlarm_impl(expr); } } while (0)
#define logCharger(expr)   do { if (LOG_CHARGER)       { logCharger_impl(expr); } } while (0)
#define logModemData(expr) do { if (LOG_MODEM_DATA)    { logModemData_impl(expr); } } while (0)
#define logSPO2(expr)      do { if (LOG_PULSIOXIMETRY) { logSPO2_impl(expr); } } while (0)
#define logDrive(expr)     do { if (LOG_DRIVE)         { logDrive_impl(expr); } } while (0)
long secsToMillis(long timeInMillis);
long minsToMillis(long timeInMillis);
float millisToHours(long timeInMillis);
void initHardware(bool printOutputTest);
void updateData();
void buzzerHandler();
void buzzerTone(int beepTimes, int timevTaskDelay, int freq);

void shutBuzzer();

// Motor de audio de alarma gobernado por estado (60601-1-8 6.10): se llama en
// cada ciclo de securityCheck() y regenera el patron de rafaga indefinidamente
// mientras audioRequired sea true. No comparte estado con buzzerHandler()
// (feedback de encoder/HMI/autotest, ajenos a las alarmas).
void buzzerAlarmUpdate(bool audioRequired, AlarmPriority priority);

double measureMeanConsumption(bool, int);
double measureStabilizedCurrent(bool sensor, int shunt, float offsetCurrent,
                                float minExpected, float maxExpected,
                                int maxTimeMs, int intervalMs = 200,
                                int window = 3);
float measureMeanVoltage(bool, int);
void WIFI_TB_Init();
void WifiOTAHandler(void);
// Enciende/apaga la WiFi desde la UART (HMI,WIFI_EN,<0|1>), para poder probar
// el camino 2G. Solo anota la peticion: la aplica WifiOTAHandler() en el lazo
// principal. Volatil — WIFI_EN vuelve a true en cada arranque.
void wifiRequestEnable(bool enable);
void securityCheck();

void turnFans(bool mode);
void setFanPidEnabled(bool enabled);
void alarmTimerStart(long graceMinutes = ACTUATORS_ALARM_STABILIZATION_MINS);
void timeTrackHandler();

// Puertas de actuador. Delegan en la maquina de alarmas
// (src/modules/control/alarm_machine.h): la deteccion le pasa condiciones y
// ella decide. Ya no hay setAlarm()/resetAlarm(): para declarar una condicion
// se llama a alarm_machine_condition().
bool ongoingCriticalAlarm();
bool ongoingCriticalWiringAlarm();
bool ongoingFanCriticalAlarm();
int getActiveAlarmCount();

void PIDInit();
void PIDHandler();
void startPID(byte var);
void stopPID(byte var);

bool ongoingAlarms();
byte activeAlarm();
// Boton de silencio del display HMI (hmi_cmd_msg.muteAlarm): ver definicion
// en security.cpp para el porque del flanco de subida.
void silenceActiveAlarmsFromDisplayMute();
char *alarmIDtoString(byte alarmID);
void resendActiveAlarms();

bool updateRoomSensor();
bool updateAmbientSensor();

void wifiInit(void);

void loaddefaultValues();
void recapVariables();

void initRoomSensor();
void initAmbientSensor();
void initSkinSensor();
void powerMonitor();
void currentMonitor();
void voltageMonitor();

// Incremented only when in3.heater_current is genuinely refreshed
// (legacy/sensors.cpp); used by heaterPowerConsumptionCheck() to detect a
// real new sample instead of currentMonitor() merely having ticked.
extern unsigned long heaterCurrentSampleSeq;
// Same idea, but for in3.system_current - the heaterPowerConsumptionCheck()
// reference on HW18 (see HEATER_POWER_REFERENCE_IS_SYSTEM_CURRENT, board.h).
extern unsigned long systemCurrentSampleSeq;

double roundSignificantDigits(double value, int numberOfDecimals);

void initGPIO();
void initEEPROM();
void initAlarms();
// Registro de alarmas persistido en NVS (6.12.2). Definidas en security.cpp.
void alarmHistorySave();
void alarmHistoryLoad();
void security_check_reboot_cause();
void IRAM_ATTR encoderISR();
void IRAM_ATTR fanEncoderISR();

void fanSpeedHandler();
bool measureSkinSensor();

void pinMode(uint8_t GPIO, uint8_t Mode);
bool GPIORead(uint8_t GPIO);
void digitalWrite(uint8_t GPIO, uint8_t Mode);

void basictemperatureControl();

#endif