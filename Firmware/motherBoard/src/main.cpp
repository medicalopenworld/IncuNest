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

// pio run -e IncuNest_V15 -t upload ; pio device monitor

// Firmware version and head title of UI screen

#include "main.h"
#include "modules/util/system_clock.h"
#include "state/state.h"
#include "modules/debug/debug_mode.h"
#include "modules/control/photo_override.h"
#include "modules/control/mains_detector.h"
#include "modules/control/charge_policy.h"
#include "modules/control/battery_gauge.h"
#include "modules/util/retry_backoff.h"
#include "modules/sensorboard_comm/sensorboard_comm.h"
#include "modules/sensors/sensor_source.h"
#include "system/hw_selftest.h"
#include "system/power_rails.h"
#ifdef FTEST_BENCH_DEBUG
#include "esp_heap_caps.h"
#include "modules/factory_test/factory_test_api.h"
#include "driver/gpio.h"
#include "soc/soc.h"
#include "soc/gpio_struct.h"
#include "soc/io_mux_reg.h"
#include "usb/usb_host.h"
#include "modules/control/heater_supply_limit.h"
// HPRT del USB OTG del S3 (TRM: USB OTG en 0x6008_0000, HPRT en +0x440). La
// cabecera usb_reg.h de este IDF usa SOC_DPORT_USB_BASE sin definirla.
#define BENCH_USB_OTG_HPRT_REG (0x60080000u + 0x440u)
#endif
#include "DriveUpload.h"
#include "CrashReporter.h"
#include <Preferences.h>

static Preferences diag_prefs;
uint32_t g_bootCount = 0;
uint32_t g_gprsKillCount = 0;
uint32_t g_monKillCount = 0;
int g_hmiBootCount = 0;
int g_hmiLastRst = 0;
int g_restore_photo_minutes = 0;

// Build-flag crash simulator. Se enciende con la variable de entorno
// INCUNEST_CRASH_TEST (ver main/CMakeLists.txt); la instruccion anterior decia
// platformio.ini, que el porte a ESP-IDF dejo sin efecto. Dispara un panic
// pasados CRASH_TEST_MB_DELAY_S segundos (130 por defecto).
// La alternativa en caliente y sin recompilar es POST /debug/crash.
// Nunca en un binario de produccion.
//   CRASH_TEST_MB=1  → abort() (panic, RST_reason=12)
//   CRASH_TEST_MB=2  → null-pointer LoadProhibited
#ifdef CRASH_TEST_MB
#ifndef CRASH_TEST_MB_DELAY_S
#define CRASH_TEST_MB_DELAY_S 130
#endif
static void CrashTestMBTask(void *pv) {
  for (int i = CRASH_TEST_MB_DELAY_S; i > 0; i--) {
    ESP_LOGW("CRASH_TEST_MB", "MB crash firing in %d s", i);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  ESP_LOGE("CRASH_TEST_MB", "Crashing now (CRASH_TEST_MB=%d)", CRASH_TEST_MB);
  vTaskDelay(pdMS_TO_TICKS(50));
#if CRASH_TEST_MB == 2
  { volatile int *p = (int *)0; *p = 0xDEAD; }
#else
  abort();
#endif
  vTaskDelete(NULL);
}
#endif

#if !CONFIG_IDF_TARGET_ESP32S3
TelemetryMessage ctrl_tel_msg = {0, 0, 0, 0};
HMI_CommandMessage hmi_cmd_msg = {0, 0, 0, 0, 0, 0, 0, false};
#endif
char pendingSSID[64] = "";
char pendingPass[64] = "";
char wifi_ssid[64] = "";
char wifi_pass[64] = "";

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>

TwoWire *wire;
TwoWire *wire2 = nullptr; // second I2C bus (HW16: SHTC3 + STS35 on pins 19/20)
MAM_IncuNest_Humidifier in3_hum(DEFAULT_ADDRESS);
TFT_eSPI tft = TFT_eSPI(); // Invoke custom library
SHTC3 mySHTC3;             // Declare an instance of the SHTC3 class
SensirionI2cSts3x mySTS35[STS3X_NUM];
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
Beastdevices_INA3221 mainDigitalCurrentSensor(INA3221_ADDR41_VCC);
Beastdevices_INA3221 secundaryDigitalCurrentSensor(INA3221_ADDR40_GND);
// BQ25730 gestionado por BQ25730.cpp (chargerPresent definido allí)

// Encoder rotativo. En la linea del port a ESP-IDF se retiro (e2e3ede), y al
// portar el modo depuracion (17312e3) el rebase arrastro la retirada de estos
// globales como un hunk limpio, dejando colgados los `extern` de ISR.cpp y
// security.cpp. En esta linea el encoder sigue en el arbol, asi que los
// globales vuelven aqui tal cual estaban.
RotaryEncoder encoder(ENC_A, ENC_B, RotaryEncoder::LatchMode::TWO03);
boolean A_set;
boolean B_set;
int encoderpinA = ENC_A;         // pin  encoder A
int encoderpinB = ENC_B;         // pin  encoder B
bool encPulsed, encPulsedBefore; // encoder switch status
bool updateUIData;
volatile int EncMove;                 // moved encoder
volatile int lastEncMove;             // moved last encoder
volatile int EncMoveOrientation = -1; // set to -1 to increase values clockwise
volatile int last_encoder_move;       // moved encoder
long encoder_debounce_time =
    true; // in milliseconds, debounce time in encoder to filter signal bounces
long last_encPulsed; // last time encoder was pulsed

bool WIFI_EN = true;
long lastDebugUpdate;
long loopCounts;
int page;

double errorTemperature[SENSOR_TEMP_QTY], temperatureCalibrationPoint;
double ReferenceTemperatureRange, ReferenceTemperatureLow;
double provisionalReferenceTemperatureLow;
float diffSkinTemperature,
    diffAirTemperature; // difference between measured temperature and user
                        // input real temperature
double RawTemperatureLow[SENSOR_TEMP_QTY], RawTemperatureRange[SENSOR_TEMP_QTY];
double provisionalRawTemperatureLow[SENSOR_TEMP_QTY];
double temperatureMax[SENSOR_TEMP_QTY], temperatureMin[SENSOR_TEMP_QTY];
int temperature_array_pos; // temperature sensor number turn to measure
bool humidifierState, humidifierStateChange;
int previousHumidity; // previous sampled humidity
float diffHumidity; // difference between measured humidity and user input real
                    // humidity

byte autoCalibrationProcess;

// Sensor check rate (in ms). Both sensors are checked in same interrupt and
// they have different check rates
byte encoderRate = true;
byte encoderCount = false;

volatile long lastEncPulse;
volatile bool statusEncSwitch;

bool roomSensorPresent[ROOM_SENSOR_POSIBILITIES];
bool ambientSensorPresent = false;
bool digitalCurrentSensorPresent[2];

// room variables

float minDesiredTemp[2] = {
    SKIN_TEMPERATURE_SET_MIN,
    AIR_TEMPERATURE_SET_MIN}; // minimum allowed temperature to be set
float maxDesiredTemp[2] = {
    SKIN_TEMPERATURE_SET_MAX,
    AIR_TEMPERATURE_SET_MAX}; // maximum allowed temperature to be set
int presetTemp[2] = {36, 32}; // preset baby skin temperature

// Text Graphic position variables
int humidityX;
int humidityY;
int temperatureX;
int temperatureY;
int separatorTopYPos, separatorMidYPos, separatorBotYPos;

bool enableSet;
float temperaturePercentage, temperatureAtStart;
float humidityPercentage, humidityAtStart;
int barWidth, barHeight, tempBarPosX, tempBarPosY, humBarPosX, humBarPosY;
int screenTextColor, screenTextBackgroundColour;

// User Interface display variables
bool selected;
char cstring[128];
char *textToWrite;
char *words[12];
char *helpMessage;
byte bar_pos = true;
byte menu_rows;
byte length;
long lastGraphicSensorsUpdate;
long lastSensorsUpdate;
bool enableSetProcess;
long blinking;
bool state_blink;
bool blinkSetMessageState;
long lastBlinkSetMessage;

long lastSuccesfullSensorUpdate[SENSOR_TEMP_QTY];
uint32_t g_sensorsTaskStartedMs = 0;

long lastSkinAttachedSensorUpdate;
long lastRoomSensorUpdate, lastCurrentSensorUpdate;
bool roomSensorOk = false;

IncuNest_parameters in3;

TaskHandle_t taskHandle =
    NULL; // Handle for the task we want to delete if it hangs
long GPRS_lastMillisTaskClear;
bool TB_connected;

void GPRS_Task(void *pvParameters); // forward decl: GPRSMonitorTask restarts it on hang

QueueHandle_t sharedSensorQueue;
// Mutex for protecting the shared variable
SemaphoreHandle_t GPRS_monitor_mutex;
SemaphoreHandle_t log_mutex = NULL;

void GPRSMonitorTask(void *pvParameters) {
  for (;;) {
    if (xSemaphoreTake(GPRS_monitor_mutex, portMAX_DELAY)) // Lock the mutex
    {
      if (millis() - GPRS_lastMillisTaskClear > GPRS_MONITOR_TASK_DELETE) {
        g_monKillCount++;
        diag_prefs.begin("diag", false);
        diag_prefs.putUInt("mon_kill", g_monKillCount);
        diag_prefs.end();
        {
          const char *m = "[MON] GPRS_Task hung, restarting it\n";
          crashReporterPut(m, strlen(m));
        }
        vTaskDelete(taskHandle); // Delete the hung task
        GPRS = GPRSstruct();     // drop state left over from the aborted attempt
        // HW19: la tarea colgada en una llamada AT deja al modem en un estado
        // del que a veces no sale por software. Corte real de VBAT, sin margen
        // (no hay CPOWD que esperar): la tarea nueva restaura GSM_PWR_EN desde
        // GPRSPowerUp() caso 0 pasados >= 2 s. No-op en HW < 19.
        //
        // Si el monitor mata la tarea con un ciclo YA en curso (p. ej. en fase
        // OFF), esta llamada se rechaza y no hace falta: la tarea nueva
        // restaura GSM_PWR_EN igualmente, porque GPRSPowerUp() caso 0 consulta
        // gsmPowerCycleBusy() hasta que la maquina devuelve RESTORE.
        gsmPowerCycleBegin(0);
        GPRS_lastMillisTaskClear = millis();
        // Recreate GPRS_Task so a single hung AT call doesn't kill cellular
        // connectivity for good; the new task spawns its own fresh monitor.
        xTaskCreatePinnedToCore(GPRS_Task, "GPRS", GPRS_TASK_STACK_BYTES, NULL,
                                GPRS_TAST_PRIORITY, &taskHandle,
                                CORE_ID_FREERTOS);
        xSemaphoreGive(GPRS_monitor_mutex);
        vTaskDelete(NULL); // this monitor's job is done
      }
      if (GPRSIsConnectedToServer() || WIFIIsConnectedToServer()) {
        vTaskDelete(NULL); // Delete the monitor task
      }
      // Unlock the mutex
      xSemaphoreGive(GPRS_monitor_mutex);
    }
    vTaskDelay(pdMS_TO_TICKS(GPRS_MONITOR_TASK_PERIOD));
  }
}

void GPRS_Task(void *pvParameters) {
  xTaskCreatePinnedToCore(GPRSMonitorTask, (const char *)"GPRS_MONITOR", 4096,
                          NULL, GPRS_MONITOR_TASK_PRIORITY, NULL,
                          CORE_MONITOR_FREERTOS);
  initGPRS();
  GPRS_TB_Init();
  for (;;) {
    GPRS_Handler();
    // Modify the shared variable
    GPRS_lastMillisTaskClear = millis();
    // Unlock the mutex
    xSemaphoreGive(GPRS_monitor_mutex);
    vTaskDelay(pdMS_TO_TICKS(GPRS_TASK_PERIOD_MS));
  }
}

BQ25730_Status g_bq_status      = {};
bool           g_bq_status_valid = false;
// millis() del ultimo refresco con exito: el test de fabrica exige frescura
// ademas del flag, porque un `true` sin sello sobreviviria a una tarea parada.
uint32_t       g_bq_status_ms    = 0;

// Detector de interrupcion de red (ALARM_MAINS_INTERRUPTION). Lo ALIMENTA solo
// sensors_Task, tras cada lectura del cargador; lo CONSULTA securityCheck()
// (checkMainsInterruption, security.cpp). Ver modules/control/mains_detector.h.
MainsDetector g_mains_detector = MAINS_DETECTOR_INITIALIZER;

// Indicador de carga de la bateria (modules/control/battery_gauge.h). Lo
// alimenta solo sensors_Task con cada lectura completa del cargador; la tarea
// de comunicacion lee g_batt_report, que se publica de una sola escritura de
// 32 bits (battery_gauge_pack). Sin cargador queda en BATT_GAUGE_UNKNOWN y no
// se envia nada al display.
BatteryGauge g_batt_gauge = BATTERY_GAUGE_INITIALIZER;
volatile uint32_t g_batt_report = 0;

// El ultimo porcentaje conocido sobrevive a un apagado (NVS "batt"/"soc"):
// con red el indicador integra la carga desde ahi, y sin nada guardado tendria
// que partir de 0. Se escribe solo cada BATT_PERSIST_STEP_PCT puntos (y al
// llegar al 100): una descarga o una carga completas son ~20 escrituras.
#define BATT_PERSIST_STEP_PCT 5
// "soc" y no "pct": las unidades de banco guardaron en "pct" lo integrado
// desde 0 sin historia (un valor falso y bajo, ver battery_gauge.cpp).
#define BATT_NVS_KEY "soc"
static int s_battSavedPct = -1;  // -1 = nada guardado

static void batt_gauge_restore_from_nvs() {
  Preferences p;
  p.begin("batt", true);
  const int v = p.getUChar(BATT_NVS_KEY, 255);
  p.end();
  if (v <= 100) {
    s_battSavedPct = v;
    battery_gauge_restore(&g_batt_gauge, (uint8_t)v);
  }
}

static void batt_gauge_persist() {
  if (!g_batt_gauge.known || g_batt_gauge.state == BATT_GAUGE_UNKNOWN) return;
  const int pct = g_batt_gauge.percent;
  const int diff = pct - s_battSavedPct;
  const bool fullNow = (pct == 100 && s_battSavedPct != 100);
  if (s_battSavedPct >= 0 && diff < BATT_PERSIST_STEP_PCT &&
      diff > -BATT_PERSIST_STEP_PCT && !fullNow) {
    return;
  }
  Preferences p;
  p.begin("batt", false);
  p.putUChar(BATT_NVS_KEY, (uint8_t)pct);
  p.end();
  s_battSavedPct = pct;
}

// UNICO PUNTO donde se decide "hay red": cualquier fuente nueva entra aqui y
// el detector no se entera. Hoy es el bit AC_STAT del BQ25730 (por I2C).
//
// La rev. V19 anade MAINS_OK_PIN (IO42, CHRG_OK del mismo cargador) y su
// getter mainsPresentPin(). Se combina aqui como AND sobre las fuentes que
// responden — la red cuenta como presente solo si todas dicen que lo esta —,
// con `valid` = registro valido OR pin disponible: el pin sigue decidiendo con
// el bus I2C colgado. AND y no OR porque el fallo que importa es no avisar de un
// corte real (201.12.3.103), y una fuente atascada en "presente" lo taparia.
// Razonado en openspec/changes/archive/*-shared-mains-detector/design.md.
//
// La validez es SOLO la de la lectura (g_bq_status_valid), no chargerPresent:
// ese flag baja mientras init_BQ25730() reconfigura y no dice "no hay fuente".
// MainsReading y la regla de combinacion viven en mains_detector.h
// (mains_reading_combine, con tests de host).
static MainsReading mains_present_now() {
  bool ac = false;
  const bool regValid = charge_ac_present(&ac);
#if (HW_NUM >= 19)
  // El pin no cuenta en la ventana tras una escritura al cargador: baja ~750 ms
  // al escribir la tension de carga con la red presente (mains_pin_usable()).
  const bool pinUsable = mains_pin_usable(true, millis(), g_bq_last_write_ms);
  return mains_reading_combine(regValid, ac, pinUsable, mainsPresentPin());
#else
  return mains_reading_combine(regValid, ac, false, false);
#endif
}

// Fallo tecnico del cargador: lleva > MAINS_DETECTOR_BLIND_MS sin dar lecturas
// validas (el detector de red esta ciego) o su reconfiguracion al volver la red
// esta fallando. NO hay AlarmId para esto (otra rama abierta anade el id 20 y
// chocaria): se ve por log y por telemetria (clave charger_comm_fault, grupo
// DIAG). Lo escribe solo sensors_Task; lo lee la telemetria.
bool g_charger_comm_fault = false;
static bool s_chargerReconfigFailing = false;

// El cargador es "esperado" si la PLACA lo lleva (CHARGER_EXPECTED, de HW_NUM) o
// si el chip ha contestado alguna vez. Que no conteste al arrancar NO lo
// convierte en "sin cargador": en una placa que lo lleva es un fallo (ceguera),
// y el "sin cargador" silencioso queda solo para HW sin BQ25730.
static inline bool chargerExpectedNow() {
  return CHARGER_EXPECTED || chargerDetected;
}

// Reintento de la configuracion del cargador cuando el arranque no la dejo
// aplicada (chargerPresent == false): sin ella la bateria no carga
// (ChargeCurrent de fabrica = 0) y la red no se vigila. Espera progresiva de
// 5 s a 5 min, y un log por episodio y luego cada 5 min. Logica pura en
// modules/util/retry_backoff; el estado a cero es el inicial de las dos.
static Backoff s_cfgBackoff = {};
static LogLimiter s_cfgLog = {};
static LogLimiter s_reconfLog = {};

// Politica de carga segun VBAT (modules/control/charge_policy.h, solo V19).
// g_charge_policy_applied es el modo que el chip tiene AHORA: init_BQ25730() y
// reconfigure_BQ25730() reescriben toda la configuracion (carga sin inhibir y
// ChargeCurrent nominal), asi que tras cada una vuelve a NORMAL y la politica
// se reaplica en la siguiente lectura. Lo lee /debug/state.
static ChargePolicy s_chgPolicy = {CHG_POLICY_NORMAL};
volatile uint8_t g_charge_policy_applied = CHG_POLICY_NORMAL;

void sensors_Task(void *pvParameters) {
  // Instante en que empieza a haber muestras periodicas de verdad. Lo usa
  // checkStatusOfSensor() (security.cpp) como referencia de frescura: el sello
  // que deja el autotest de initHardware() es varios segundos anterior y
  // levantaba ALARM_AIR_SENSOR_FAULT en cada arranque.
  g_sensorsTaskStartedMs = millis();
  if (g_sensorsTaskStartedMs == 0) {
    g_sensorsTaskStartedMs = 1; // 0 es el centinela de "aun no arranco"
  }
  for (;;) {
    fanSpeedHandler();
    if (millis() - lastSkinAttachedSensorUpdate >
        SKIN_SENSOR_UPDATE_PERIOD_MS) {
      measureSkinSensor();
      lastSkinAttachedSensorUpdate = millis();
    }
    {
      long roomPeriod = roomSensorOk ? ROOM_SENSOR_UPDATE_PERIOD_MS
                                     : ROOM_SENSOR_RECONNECT_MS;
      if (millis() - lastRoomSensorUpdate > roomPeriod) {
        roomSensorOk = updateRoomSensor();
        updateAmbientSensor();
        lastRoomSensorUpdate = millis();
      }
    }
    if (millis() - lastCurrentSensorUpdate > DIGITAL_CURRENT_SENSOR_PERIOD_MS) {
      powerMonitor();
      lastCurrentSensorUpdate = millis();
    }
    {
      static long lastChargerUpdate = 0;
      static bool prev_ac_present   = false;
      static bool charger_in_float  = false;
      static long ichg_low_since    = 0;
      // Configuracion fallida en el arranque (o chip mudo en una placa que lo
      // lleva): se reintenta con espera progresiva. init_BQ25730() baja y sube
      // chargerPresent; reconfigure_BQ25730() (retorno de red) no.
      if (chargerExpectedNow() && !chargerPresent &&
          backoff_due(&s_cfgBackoff, millis())) {
        extern TwoWire *wire;
        if (init_BQ25730(wire)) {
          g_charge_policy_applied = CHG_POLICY_NORMAL;
          backoff_ok(&s_cfgBackoff);
          log_limiter_reset(&s_cfgLog);
          logE("[CHG] BQ25730 configurado tras reintento");
        } else {
          backoff_failed(&s_cfgBackoff, millis());
          if (log_limiter_allow(&s_cfgLog, millis())) {
            logE("[CHG] BQ25730 sin configurar: se reintenta con espera "
                 "progresiva (5 s a 5 min); sin el, la bateria no carga");
          }
        }
      }
      // chargerExpectedNow(), no chargerPresent: este ultimo baja mientras el
      // chip se configura. Sin lecturas validas el detector lo dice
      // (mains_detector_blind).
      // Detector de red a 5 Hz (MAINS_DETECTOR_SAMPLE_PERIOD_MS): solo el bit
      // AC_STAT, una lectura I2C. El estado completo (ADC) sigue a 1 Hz.
      static long lastMainsSample = 0;
      if (chargerExpectedNow() &&
          millis() - lastMainsSample >= MAINS_DETECTOR_SAMPLE_PERIOD_MS) {
        const MainsReading mains = mains_present_now();
        mains_detector_sample(&g_mains_detector, mains.valid, mains.present,
                              millis());
        lastMainsSample = millis();
      }
      if (chargerExpectedNow() &&
          millis() - lastChargerUpdate >= CHARGER_STATUS_PERIOD_MS) {
        // Tiempo real entre lecturas completas: el indicador de bateria
        // integra con el la corriente de carga.
        const uint32_t charger_dt_ms =
            lastChargerUpdate ? (uint32_t)(millis() - lastChargerUpdate)
                              : CHARGER_STATUS_PERIOD_MS;
        g_bq_status_valid = charge_status(&g_bq_status);
        if (g_bq_status_valid) g_bq_status_ms = millis();
        lastChargerUpdate = millis();
        // Detecta transición ausente→presente del adaptador y reinicializa el
        // chip: algunos BQ25xxx pierden VINDPM/IIN al re-detectar VBUS, así que
        // reaplicamos toda la config para asegurar carga estable.
        // Al reconectar siempre se vuelve a absorción: la batería pudo haberse
        // descargado parcialmente durante el corte.
        if (g_bq_status_valid && g_bq_status.ac_present && !prev_ac_present) {
          if (LOG_CHARGER) logCharger("[CHG] Adaptador detectado → reinicializando config");
          extern TwoWire *wire;
          // reconfigure_BQ25730(), no init_BQ25730(): esta NO baja chargerPresent
          // (restaura MaxChargeVoltage = 14.4V, absorción).
          s_chargerReconfigFailing = !reconfigure_BQ25730(wire);
          if (!s_chargerReconfigFailing) g_charge_policy_applied = CHG_POLICY_NORMAL;
          if (s_chargerReconfigFailing) {
            // Uno por episodio y luego cada 5 min: el reintento es en cada
            // lectura (5 s) y un fallo persistente llenaria el registro.
            if (log_limiter_allow(&s_reconfLog, millis())) {
              logE("[CHG] Fallo al reconfigurar el BQ25730 al volver la red; "
                   "se reintenta en la proxima lectura (log limitado a 1 cada 5 min)");
            }
          } else {
            log_limiter_reset(&s_reconfLog);
          }
          charger_in_float = false;
          ichg_low_since   = 0;
        }
        // Con la reconfiguracion fallida NO se anota la red como presente: el
        // flanco se repite en la lectura siguiente y se reintenta.
        if (g_bq_status_valid) {
          // Sin red ya no hay nada que reconfigurar: el fallo se da por
          // cerrado y el siguiente flanco lo vuelve a intentar.
          if (!g_bq_status.ac_present) s_chargerReconfigFailing = false;
          if (!s_chargerReconfigFailing) prev_ac_present = g_bq_status.ac_present;
        }

#if (HW_NUM >= 19)
        // ── Politica de carga segun VBAT (charge_policy.h) ──────────────────
        // Solo con adaptador: sin VBUS el BQ no carga y no hay nada que aplicar
        // (la politica sigue la VBAT igualmente, para tener el modo correcto
        // al volver la red).
        {
          const ChargePolicyMode want = charge_policy_update(
              &s_chgPolicy, g_bq_status_valid, g_bq_status.vbat_mv);
          if (g_bq_status_valid && g_bq_status.ac_present &&
              want != (ChargePolicyMode)g_charge_policy_applied) {
            bool ok;
            if (want == CHG_POLICY_INHIBIT) {
              ok = set_charge_inhibit(true);
            } else {
              ok = set_charge_inhibit(false) &&
                   set_charge_current(want == CHG_POLICY_REDUCED
                                          ? (uint16_t)CHG_POLICY_REDUCED_MA
                                          : (uint16_t)BQ25730_ICHG_MA);
            }
            if (ok) {
              ESP_LOGW("CHG", "politica de carga: %s -> %s (VBAT %u mV)",
                       charge_policy_name((ChargePolicyMode)g_charge_policy_applied),
                       charge_policy_name(want), (unsigned)g_bq_status.vbat_mv);
              g_charge_policy_applied = (uint8_t)want;
            } else {
              ESP_LOGW("CHG", "politica de carga: no se pudo aplicar %s; se reintenta",
                       charge_policy_name(want));
            }
          }
        }
#endif
        // ── Transición Absorción → Flotación ─────────────────────────────────
        // Cuando ICHG cae por debajo del umbral de corte durante 60 s seguidos,
        // se cambia la tensión objetivo a flotación para mantener la batería sin
        // seguir gasificándola. El umbral (320 mA ≈ C/22 para 7 Ah) equivale a
        // 2–3 counts ADC; la ventana de 60 s evita falsos disparos por ruido.
        if (g_bq_status_valid && g_bq_status.ac_present && !charger_in_float) {
          if (g_bq_status.ichg_ma < BQ25730_ICHG_TERM_MA) {
            if (ichg_low_since == 0) {
              ichg_low_since = millis();
            } else if (millis() - ichg_low_since >= 60000UL) {
              set_charge_voltage(BQ25730_VCHARGE_FLOAT_MV);
              charger_in_float = true;
              ichg_low_since   = 0;
              if (LOG_CHARGER)
                logCharger("[CHG] Absorcion completa → Flotacion " +
                           String(BQ25730_VCHARGE_FLOAT_MV) + " mV");
            }
          } else {
            ichg_low_since = 0;
          }
        }

        // Indicador de bateria (CTRL,BATT): despues de la transicion a
        // flotacion, para que "lleno" salga en la misma lectura.
        static bool batt_restored = false;
        if (!batt_restored) {
          batt_restored = true;
          batt_gauge_restore_from_nvs();
        }
        const uint16_t ichg = (g_bq_status.ichg_ma > 0)
                                  ? (uint16_t)g_bq_status.ichg_ma : 0u;
        // Sin red todo lo que consume el sistema sale de la bateria: el shunt
        // de sistema de la placa lo mide bien, y el ADC IDCHG del cargador no
        // (0 en banco con 6,7 A de descarga). Con red si sirve el IDCHG: es
        // lo que la bateria pone cuando el convertidor no llega.
        const double sys_a = in3.system_current;
        const uint16_t dischg =
            g_bq_status.ac_present
                ? g_bq_status.idchg_ma
                : ((sys_a > 0.0 && sys_a < 60.0) ? (uint16_t)(sys_a * 1000.0)
                                                 : 0u);
        battery_gauge_sample(&g_batt_gauge, g_bq_status_valid,
                             g_bq_status.ac_present, charger_in_float,
                             g_bq_status.vbat_mv, dischg, ichg, charger_dt_ms);
        g_batt_report = battery_gauge_pack(&g_batt_gauge);
        batt_gauge_persist();
#if (HW_NUM >= 19)
        // V19: el canal 3 de U8 esta a GND (CHANGELOG #26); la bateria se
        // mide con el ADC del BQ25730. Corriente con signo: + carga, - descarga
        // (la misma descarga que usa el indicador de bateria de arriba).
        if (g_bq_status_valid) {
          in3.BATTERY_voltage = g_bq_status.vbat_mv / 1000.0;
          in3.BATTERY_current = ((double)ichg - (double)dischg) / 1000.0;
        }
#endif
      }
      // Fallo tecnico del cargador visible: log al cambiar (no en cada pasada)
      // y bandera para telemetria. Con la alarma de red ya declarada y el
      // detector ciego, la alarma se mantiene (lado seguro): esto solo lo
      // hace VER.
      const bool commFault =
          mains_detector_blind(&g_mains_detector, chargerExpectedNow(), millis()) ||
          s_chargerReconfigFailing;
      if (commFault != g_charger_comm_fault) {
        g_charger_comm_fault = commFault;
        if (commFault) {
          logE("[CHG] Fallo del cargador BQ25730: sin lecturas validas > 30 s "
               "o reconfiguracion fallida; la vigilancia de red puede estar ciega");
        } else {
          logE("[CHG] Cargador BQ25730 recuperado");
        }
      }
    }
    // Las medidas simuladas del modo depuracion se pisan AQUI: despues de que
    // los sensores reales hayan escrito y ANTES de copiar a ctrl_tel_msg, para
    // que el control, securityCheck() y el display vean todos exactamente el
    // mismo valor. Con el modo apagado no hace nada y la medida real vuelve
    // sola en la pasada siguiente. Ver modules/debug/debug_mode.h.
    debug_sensors_apply();

    ctrl_tel_msg.detectedAirTemperature =
        in3.temperature[ROOM_DIGITAL_TEMP_SENSOR];
    ctrl_tel_msg.detectedSkinTemperature = in3.temperature[SKIN_SENSOR];
    ctrl_tel_msg.detectedHumidity = in3.humidity[ROOM_DIGITAL_HUM_SENSOR];
    vTaskDelay(pdMS_TO_TICKS(SENSORS_TASK_PERIOD_MS));
  }
}

// ---------------------------------------------------------------------------
// Consola de depuracion sobre debugSerial (el puerto por el que sale el log).
//
// NO es el enlace con el display: ese es Serial1 en los pines 15/16
// (hmiSerial en CommTask.cpp). Por debugSerial no habla nadie, asi que un
// comando aqui no compite con las tramas del protocolo ni puede corromperlas.
//
// Dos comandos, y a proposito ninguno mas:
//
//   "WIFI_EN,<0|1>"  para probar la OTA por 2G, que con la WiFi levantada no se
//                    ejercita nunca.
//   "PHOTO,<0|1>"    enciende/suelta la fototerapia para probar el lazo de
//                    intensidad sin nadie en la pantalla. PHOTO,1 exige el modo
//                    depuracion encendido; PHOTO,0 se acepta siempre, porque
//                    apagar nunca es lo peligroso. Va SOLO por aqui, sin
//                    endpoint HTTP: acciona un actuador y esta consola pide
//                    acceso fisico al USB (ver debug_mode.h).
//
// Nada de esto se guarda: cualquier reinicio vuelve a dejar la WiFi encendida
// y la fototerapia como la tenga el display. Lo demas se ignora.
//
// Solo en builds de banco (-DFTEST_BENCH_DEBUG, nunca en el firmware normal):
//   "MEM"     reparto de SRAM interna y marca de agua de cada pila.
//   "SIMACT"  lanza el test sim_act aislado (sin actuadores).
// ---------------------------------------------------------------------------
// ESP_LOGx y no logI/logE: main.h compila esos dos fuera del binario
// (LOG_INFORMATION y LOG_ERRORS estan a false), asi que un acuse escrito con
// logI no se imprime nunca y la consola parece muerta aunque funcione.
static const char *DBGCON_TAG __attribute__((unused)) = "DBGCON";

#ifdef FTEST_BENCH_DEBUG
// ---------------------------------------------------------------------------
// Prueba de carga de banco ("LOAD,<pct>,<s>"): calefactor en lazo abierto con
// el ventilador SIEMPRE encendido, para medir carga/descarga de la bateria y
// el reparto adaptador/bateria del BQ25730. El duty pedido se recorta CADA
// SEGUNDO con el tope por fuente (heater_supply_limit, el mismo que aplica el
// PID), asi que desenchufar el adaptador a mitad prueba el tope en vivo.
// Paradas: aire > 37 C o sin lectura, ventilador < FAN_MIN_RPM tras el
// arranque, calefactor > 12 A, alarma critica, control encendido, FTEST, tope
// de tiempo (600 s) o "LOAD,STOP". Al parar: calefactor a 0 y ventilador 10 s
// mas para enfriar. Usa g_factoryTestActive para que PIDHandler()/turnFans()
// no pisen los PWM mientras dura, y g_benchLoadActive para que eso NO calle las
// alarmas de red, bateria baja y ruido (ver hw_selftest.h).
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Diagnostico de banco: cada bajada del pin CHRG_OK (IO42) con su duracion,
// por interrupcion (sin I2C, sin bloquear a nadie). Banco 19/11/2026: un falso
// corte de red al pasar el cargador de absorcion a flotacion.
// ---------------------------------------------------------------------------
static volatile uint32_t s_chrgOkFallUs = 0;
static volatile uint32_t s_chrgOkLowUs = 0;   // ultima duracion en bajo
static volatile uint32_t s_chrgOkPulses = 0;  // pulsos bajos terminados
static volatile bool s_chrgOkLowNow = false;
static void IRAM_ATTR chrgOkIsr() {
  const uint32_t now = micros();
  if (gpio_get_level((gpio_num_t)MAINS_OK_PIN) == 0) {
    s_chrgOkFallUs = now;
    s_chrgOkLowNow = true;
  } else if (s_chrgOkLowNow) {
    s_chrgOkLowUs = now - s_chrgOkFallUs;
    s_chrgOkLowNow = false;
    s_chrgOkPulses++;
  }
}
void benchChrgOkMonitorInit() {
  attachInterrupt(MAINS_OK_PIN, chrgOkIsr, CHANGE);
}
// Desde OTA_WIFI_Task: un log por pulso terminado.
void benchChrgOkMonitorPoll() {
  static uint32_t reported = 0;
  if (s_chrgOkPulses != reported) {
    reported = s_chrgOkPulses;
    ESP_LOGW("CHRGOK", "CHRG_OK estuvo en bajo %lu ms (pulso #%lu)",
             (unsigned long)(s_chrgOkLowUs / 1000), (unsigned long)reported);
  }
}

#define BENCH_LOAD_MAX_S 600
#define BENCH_LOAD_AIR_MAX_C 37.0
#define BENCH_LOAD_HEATER_MAX_A 12.0
#define BENCH_LOAD_FAN_SPINUP_MS 8000
static volatile bool s_loadRunning = false;
static volatile bool s_loadStop = false;
static int s_loadPct = 0;
static int s_loadSecs = 0;

static void benchLoadTask(void *) {
  extern long lastSuccesfullSensorUpdate[SENSOR_TEMP_QTY];
  const char *why = "fin de tiempo";
  // Primero g_benchLoadActive: asi no hay ningun instante con
  // g_factoryTestActive a true que calle las alarmas (ftestSuppressesAlarms()).
  g_benchLoadActive = true;
  g_factoryTestActive = true;
  ledcWrite(HEATER_PWM_CHANNEL, 0);
  digitalWrite(ACTUATORS_EN, HIGH);
  ledcWrite(FAN_PWM_CHANNEL, 0); // Q4 (ventilador antiguo de J3) sin uso
  ledcWrite(FAN_CTL_PWM_CHANNEL, PWM_MAX_VALUE); // ventilador al 100 %
  const uint32_t t0 = millis();
  ESP_LOGW("LOAD", "inicio: %d %% durante %d s (ventilador al 100 %%)", s_loadPct,
           s_loadSecs);
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    const uint32_t el = millis() - t0;
    const bool spunUp = el >= BENCH_LOAD_FAN_SPINUP_MS;
    const bool airFresh =
        millis() - (uint32_t)lastSuccesfullSensorUpdate[ROOM_DIGITAL_TEMP_SENSOR] < 5000;
    const double air = in3.temperature[ROOM_DIGITAL_TEMP_SENSOR];
    if (s_loadStop) { why = "LOAD,STOP"; break; }
    if (el >= (uint32_t)s_loadSecs * 1000u) { why = "fin de tiempo"; break; }
    if (!airFresh) { why = "sin lectura de aire"; break; }
    if (air > BENCH_LOAD_AIR_MAX_C) { why = "aire > 37 C"; break; }
    if (spunUp && in3.fan_rpm < FAN_MIN_RPM) { why = "ventilador bajo"; break; }
    if (in3.heater_current > BENCH_LOAD_HEATER_MAX_A) { why = "calefactor > 12 A"; break; }
    if (ongoingCriticalAlarm()) { why = "alarma critica"; break; }
    if (in3.actuation != ACTUATION_OFF || in3.phototherapy) { why = "control encendido"; break; }
    if (factoryTestRunning()) { why = "FTEST"; break; }

    const bool vbusFresh = g_bq_status_valid &&
                           (millis() - g_bq_status_ms) < HEATER_SUPPLY_VBUS_MAX_AGE_MS;
    const HeaterSupply sup =
        heater_supply_classify(mainsPresentPin(), vbusFresh, g_bq_status.vbus_mv);
    const uint32_t want = (uint32_t)(PWM_MAX_VALUE * s_loadPct / 100);
    const uint32_t cap = heater_supply_max_pwm(sup, (uint16_t)PWM_MAX_VALUE);
    // El calefactor espera a que el ventilador haya arrancado.
    const uint32_t duty = spunUp ? (want < cap ? want : cap) : 0;
    ledcWrite(HEATER_PWM_CHANNEL, duty);
    ESP_LOGW("LOAD",
             "t=%lus duty=%lu/%lu src=%s heat=%.2fA sys=%.2fA sysV=%.2f | "
             "vbus=%u ibus=%d ichg=%d idchg=%u vbat=%u | rpm=%.0f aire=%.2fC",
             (unsigned long)(el / 1000), (unsigned long)duty, (unsigned long)cap,
             heater_supply_name(sup), in3.heater_current, in3.system_current,
             in3.system_voltage, g_bq_status.vbus_mv, g_bq_status.ibus_ma,
             g_bq_status.ichg_ma, g_bq_status.idchg_ma, g_bq_status.vbat_mv,
             in3.fan_rpm, air);
  }
  ledcWrite(HEATER_PWM_CHANNEL, 0);
  ESP_LOGW("LOAD", "parada: %s. Calefactor a 0; ventilador 10 s mas", why);
  for (int i = 0; i < 10 && !s_loadStop; i++) vTaskDelay(pdMS_TO_TICKS(1000));
  ledcWrite(FAN_CTL_PWM_CHANNEL, 0);
  ledcWrite(HEATER_PWM_CHANNEL, 0);
  digitalWrite(ACTUATORS_EN, LOW);
  g_factoryTestActive = false;
  g_benchLoadActive = false;
  ESP_LOGW("LOAD", "terminado");
  s_loadRunning = false;
  vTaskDelete(NULL);
}
#endif

static void debugConsoleHandle(const char *line) {
#ifdef FTEST_BENCH_DEBUG
  // Solo en builds de banco (-DFTEST_BENCH_DEBUG): lanza SIM ACT aislado, el
  // mismo camino que el REINTENTAR del display (factoryTestRunSingle), para
  // diagnosticar la activacion de la SIM sin nadie en la pantalla. Solo ese
  // id: no acciona ningun actuador.
  if (strcmp(line, "MEM") == 0) {
    // Reparto de SRAM interna: cuanto sobra en cada pila (marca de agua) y
    // como esta el heap. Las tareas se buscan por nombre porque la mayoria se
    // crean sin guardar el handle.
    static const char *const kTasks[] = {
        "GPRS",     "OTA",       "COMM_TASK", "COMM_TASK_RX", "SB_COMM",
        "PWR_MGMT", "BUZZER",    "SENSORS",   "SECURITY",     "TimeTrack",
        "SPO2",     "GPRS_MONITOR", "USB_HOST_D", "DRV_WR",   "DRV_UP",
        "FTEST",    "FTEST_SIM", "loopTask",  "async_tcp",    "tiT",
        "wifi",     "esp_timer", "sys_evt",   "ipc0",         "ipc1",
        "IDLE",     "Tmr Svc"};
    ESP_LOGW(DBGCON_TAG, "MEM: SRAM interna total=%u libre=%u minimo=%u "
             "bloque=%u | 8bit libre=%u",
             (unsigned)heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT));
    for (const char *name : kTasks) {
      TaskHandle_t h = xTaskGetHandle(name);
      if (h == NULL) continue;
      ESP_LOGW(DBGCON_TAG, "MEM: tarea %-13s pila sin usar nunca=%u B", name,
               (unsigned)uxTaskGetStackHighWaterMark(h));
    }
    return;
  }
  // "FTEST"      bateria completa (= HMI,FTEST,START del display).
  // "FTEST,<id>" un solo test de motherBoard (= HMI,FTEST,RUN,<id>).
  // "FTEST,ABORT" corta la bateria en curso.
  // Mismas precondiciones que desde el display (factoryTestPrecheck). El
  // display tiene que seguir enviando su trama (dead-man de 5 s).
  // "USBST": registro HPRT del controlador USB OTG en modo host (puerto raiz).
  // Distingue "no hay nada en el bus" (sin conexion, lineas a 0) de "hay un
  // dispositivo que tira de D+/D- pero no enumera". bit0 PrtConnSts, bit2
  // PrtEna, bits 10-11 PrtLnSts (bit10 = D+, bit11 = D-), bit12 PrtPwr.
  if (strcmp(line, "USBST") == 0) {
    const uint32_t hprt = REG_READ(BENCH_USB_OTG_HPRT_REG);
    ESP_LOGW(DBGCON_TAG,
             "USBST: HPRT=0x%08X conn=%u ena=%u D+=%u D-=%u pwr=%u speed=%u",
             (unsigned)hprt, (unsigned)(hprt & 1), (unsigned)((hprt >> 2) & 1),
             (unsigned)((hprt >> 10) & 1), (unsigned)((hprt >> 11) & 1),
             (unsigned)((hprt >> 12) & 1), (unsigned)((hprt >> 17) & 3));
    return;
  }
  // "FANOD,0|1": PWM del ventilador (FAN_CTL, IO11) en push-pull (0) o en
  // drenador abierto (1), para la prueba A/B del ventilador de J5.
  if (strcmp(line, "FANOD,0") == 0 || strcmp(line, "FANOD,1") == 0) {
    const bool od = line[6] == '1';
    // pad_driver y no gpio_set_direction(): esta desconecta el LEDC del pin
    // (ver initPWMGPIO()).
    GPIO.pin[FAN_CTL].pad_driver = od ? 1 : 0;
    ESP_LOGW(DBGCON_TAG, "FANOD: IO11 en %s", od ? "drenador abierto" : "push-pull");
    return;
  }
  // "FANPROBE": con el ventilador alimentado (ACTUATORS_EN), mira IO11 suelto
  // (alta impedancia: un ventilador de 4 hilos con el PWM al aire va al 100 %
  // y su pull-up interno pone el pin en alto) y luego IO11 a 0. En cada fase:
  // nivel de IO11 y de IO38, flancos del tacometro en 1 s y corriente por R13.
  // Levanta g_factoryTestActive mientras dura para que turnFans() no pise los
  // pines (el mismo mecanismo que el test de fabrica). Bloquea ~8 s.
  if (strcmp(line, "FANPROBE") == 0) {
    if (factoryTestRunning()) {
      ESP_LOGW(DBGCON_TAG, "FANPROBE: hay un FTEST en marcha");
      return;
    }
    extern IncuNest_parameters in3;
    const bool prevActive = g_factoryTestActive;
    g_factoryTestActive = true;
    digitalWrite(ACTUATORS_EN, HIGH);
    ledcWrite(FAN_PWM_CHANNEL, 0);
    GPIO.pin[FAN_CTL].pad_driver = 1;            // OD, sin tocar la matriz
    PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[FAN_CTL]);  // para leer el pad
    for (int phase = 0; phase < 2; phase++) {
      // fase 0: duty 255 en OD = pin suelto; fase 1: duty 0 = pin a GND.
      ledcWrite(FAN_CTL_PWM_CHANNEL, phase == 0 ? 255 : 0);
      vTaskDelay(pdMS_TO_TICKS(3000));
      const int io11 = gpio_get_level((gpio_num_t)FAN_CTL);
      int edges = 0;
      int last = gpio_get_level((gpio_num_t)FAN_SPEED_FEEDBACK);
      const uint32_t t0 = millis();
      while (millis() - t0 < 1000) {
        const int v = gpio_get_level((gpio_num_t)FAN_SPEED_FEEDBACK);
        if (v != last) {
          edges++;
          last = v;
        }
        delayMicroseconds(100);
      }
      ESP_LOGW(DBGCON_TAG,
               "FANPROBE %s: IO11=%d IO38=%d flancos_tacometro_1s=%d "
               "I_fan(R13)=%.3f A fan_rpm=%.0f",
               phase == 0 ? "IO11 suelto" : "IO11 a GND", io11, last, edges,
               in3.fan_current, in3.fan_rpm);
    }
    ledcWrite(FAN_CTL_PWM_CHANNEL, 0);
    GPIO.pin[FAN_CTL].pad_driver = 1;
    digitalWrite(ACTUATORS_EN, LOW);
    g_factoryTestActive = prevActive;
    return;
  }
  // "FANMAX": ventilador a tope 10 s solo por FAN_CTL. ACTUATORS_EN alto (FAN+
  // a 12 V) e IO11 en push-pull al 100 % (PWM en alto fijo). FAN (IO12, Q4) se
  // deja a 0. Cada 2 s: corriente por R13, flancos del tacometro y rpm.
  if (strcmp(line, "FANMAX") == 0) {
    if (factoryTestRunning()) {
      ESP_LOGW(DBGCON_TAG, "FANMAX: hay un FTEST en marcha");
      return;
    }
    extern IncuNest_parameters in3;
    const bool prevActive = g_factoryTestActive;
    g_factoryTestActive = true;
    digitalWrite(ACTUATORS_EN, HIGH);
    GPIO.pin[FAN_CTL].pad_driver = 0;            // push-pull, sin tocar la matriz
    PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[FAN_CTL]);
    ledcWrite(FAN_CTL_PWM_CHANNEL, 255);
    ledcWrite(FAN_PWM_CHANNEL, 0);
    for (int i = 0; i < 5; i++) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      int edges = 0;
      int last = gpio_get_level((gpio_num_t)FAN_SPEED_FEEDBACK);
      const uint32_t t0 = millis();
      while (millis() - t0 < 1000) {
        const int v = gpio_get_level((gpio_num_t)FAN_SPEED_FEEDBACK);
        if (v != last) {
          edges++;
          last = v;
        }
        delayMicroseconds(100);
      }
      ESP_LOGW(DBGCON_TAG,
               "FANMAX t=%ds: IO11=%d IO38=%d flancos_tacometro_1s=%d "
               "I_fan(R13)=%.3f A fan_rpm=%.0f",
               (i + 1) * 2, gpio_get_level((gpio_num_t)FAN_CTL), last, edges,
               in3.fan_current, in3.fan_rpm);
    }
    ledcWrite(FAN_CTL_PWM_CHANNEL, 0);
    ledcWrite(FAN_PWM_CHANNEL, 0);
    GPIO.pin[FAN_CTL].pad_driver = 1;
    digitalWrite(ACTUATORS_EN, LOW);
    g_factoryTestActive = prevActive;
    return;
  }
  // "IO11TEST": nivel real del pad de IO11 (FAN_CTL) como GPIO normal, sin
  // LEDC: salida alta, salida baja, entrada con pull-up y con pull-down, con
  // FAN+ apagado y encendido. Distingue un pad atado a GND (lee 0 incluso con
  // salida alta) de un problema de enrutado del LEDC.
  if (strcmp(line, "IO11TEST") == 0) {
    if (factoryTestRunning()) {
      ESP_LOGW(DBGCON_TAG, "IO11TEST: hay un FTEST en marcha");
      return;
    }
    const bool prevActive = g_factoryTestActive;
    g_factoryTestActive = true;
    ledcDetachPin(FAN_CTL);
    for (int pwr = 0; pwr < 2; pwr++) {
      digitalWrite(ACTUATORS_EN, pwr ? HIGH : LOW);
      vTaskDelay(pdMS_TO_TICKS(500));
      gpio_set_direction((gpio_num_t)FAN_CTL, GPIO_MODE_INPUT_OUTPUT);
      gpio_set_level((gpio_num_t)FAN_CTL, 1);
      vTaskDelay(pdMS_TO_TICKS(50));
      const int outHigh = gpio_get_level((gpio_num_t)FAN_CTL);
      gpio_set_level((gpio_num_t)FAN_CTL, 0);
      vTaskDelay(pdMS_TO_TICKS(50));
      const int outLow = gpio_get_level((gpio_num_t)FAN_CTL);
      pinMode(FAN_CTL, INPUT_PULLUP);
      vTaskDelay(pdMS_TO_TICKS(50));
      const int inPu = gpio_get_level((gpio_num_t)FAN_CTL);
      pinMode(FAN_CTL, INPUT_PULLDOWN);
      vTaskDelay(pdMS_TO_TICKS(50));
      const int inPd = gpio_get_level((gpio_num_t)FAN_CTL);
      pinMode(FAN_CTL, INPUT);
      vTaskDelay(pdMS_TO_TICKS(50));
      const int inFloat = gpio_get_level((gpio_num_t)FAN_CTL);
      ESP_LOGW(DBGCON_TAG,
               "IO11TEST FAN+ %s: salida_alta=%d salida_baja=%d pullup=%d "
               "pulldown=%d sin_pull=%d | IO38=%d",
               pwr ? "ON " : "OFF", outHigh, outLow, inPu, inPd, inFloat,
               gpio_get_level((gpio_num_t)FAN_SPEED_FEEDBACK));
    }
    digitalWrite(ACTUATORS_EN, LOW);
    pinMode(FAN_CTL, OUTPUT);
    ledcAttachPin(FAN_CTL, FAN_CTL_PWM_CHANNEL);
    ledcWrite(FAN_CTL_PWM_CHANNEL, 0);
    GPIO.pin[FAN_CTL].pad_driver = 1;
    g_factoryTestActive = prevActive;
    return;
  }
  // "USBLIST": dispositivos que el host USB tiene enumerados ahora mismo, con
  // VID/PID, clase y numero de configuraciones. Registra un cliente propio
  // (el aviso NEW_DEV solo llega a los clientes que ya existian al enumerar,
  // asi que el log de sensorboard_comm puede no haberlo visto).
  if (strcmp(line, "USBLIST") == 0) {
    usb_host_client_handle_t cli = NULL;
    const usb_host_client_config_t cfg = {
        .is_synchronous = false,
        .max_num_event_msg = 3,
        .async = {.client_event_callback =
                      [](const usb_host_client_event_msg_t *, void *) {},
                  .callback_arg = NULL},
    };
    esp_err_t err = usb_host_client_register(&cfg, &cli);
    if (err != ESP_OK) {
      ESP_LOGW(DBGCON_TAG, "USBLIST: usb_host_client_register=%d (host sin instalar?)", err);
      return;
    }
    uint8_t addrs[8];
    int n = 0;
    err = usb_host_device_addr_list_fill(sizeof(addrs), addrs, &n);
    ESP_LOGW(DBGCON_TAG, "USBLIST: %d dispositivo(s) enumerado(s) (err=%d)", n, err);
    for (int i = 0; i < n; i++) {
      usb_device_handle_t dev = NULL;
      if (usb_host_device_open(cli, addrs[i], &dev) != ESP_OK) {
        ESP_LOGW(DBGCON_TAG, "USBLIST: addr %u no se pudo abrir", addrs[i]);
        continue;
      }
      const usb_device_desc_t *d = NULL;
      usb_host_get_device_descriptor(dev, &d);
      if (d) {
        ESP_LOGW(DBGCON_TAG,
                 "USBLIST: addr %u VID 0x%04X PID 0x%04X clase 0x%02X bcdDevice 0x%04X "
                 "configs %u",
                 addrs[i], d->idVendor, d->idProduct, d->bDeviceClass, d->bcdDevice,
                 d->bNumConfigurations);
      }
      usb_host_device_close(cli, dev);
    }
    usb_host_client_deregister(cli);
    return;
  }
  // "LOAD,<pct>,<s>" / "LOAD,STOP": prueba de carga (ver benchLoadTask()).
  if (strcmp(line, "LOAD,STOP") == 0) {
    s_loadStop = true;
    ESP_LOGW(DBGCON_TAG, "LOAD: parada pedida");
    return;
  }
  if (strncmp(line, "LOAD,", 5) == 0) {
    int pct = -1, secs = -1;
    if (sscanf(line + 5, "%d,%d", &pct, &secs) != 2 || pct < 0 || pct > 100 ||
        secs < 1 || secs > BENCH_LOAD_MAX_S) {
      ESP_LOGW(DBGCON_TAG, "LOAD: uso LOAD,<0-100>,<1-%d>", BENCH_LOAD_MAX_S);
      return;
    }
    if (s_loadRunning || factoryTestRunning() ||
        in3.actuation != ACTUATION_OFF || in3.phototherapy) {
      ESP_LOGW(DBGCON_TAG, "LOAD rechazado: ya en marcha, FTEST o control encendido");
      return;
    }
    s_loadPct = pct;
    s_loadSecs = secs;
    s_loadStop = false;
    s_loadRunning = true;
    if (xTaskCreatePinnedToCore(benchLoadTask, "BENCH_LOAD", 4096, NULL, 2, NULL,
                                CORE_ID_FREERTOS) != pdPASS) {
      s_loadRunning = false;
      ESP_LOGW(DBGCON_TAG, "LOAD: no se pudo crear la tarea");
    }
    return;
  }
  if (strcmp(line, "FTEST,ABORT") == 0) {
    factoryTestAbort();
    ESP_LOGW(DBGCON_TAG, "FTEST: abort pedido");
    return;
  }
  if (strcmp(line, "FTEST") == 0 || strncmp(line, "FTEST,", 6) == 0) {
    const bool single = line[5] == ',';
    char *end = nullptr;
    const long id = single ? strtol(line + 6, &end, 10) : -1;
    if (single && (end == line + 6 || *end != ' ' || id < 0)) {
      ESP_LOGW(DBGCON_TAG, "FTEST: id invalido");
      return;
    }
    const int reject =
        factoryTestPrecheck(single ? (unsigned)id : (unsigned)FTEST_ID_NONE);
    if (reject >= 0) {
      ESP_LOGW(DBGCON_TAG, "FTEST rechazado (reject=%d)", reject);
      return;
    }
    const bool ok = single ? factoryTestRunSingle((unsigned)id) : factoryTestStart();
    ESP_LOGW(DBGCON_TAG, "FTEST %s -> %d", single ? "RUN" : "START", (int)ok);
    return;
  }
  if (strcmp(line, "SIMACT") == 0) {
    const int reject = factoryTestPrecheck(FTEST_MB_SIM_ACT);
    if (reject >= 0) {
      ESP_LOGW(DBGCON_TAG, "SIMACT rechazado (reject=%d)", reject);
      return;
    }
    ESP_LOGW(DBGCON_TAG, "SIMACT: arranca factoryTestRunSingle(%d) -> %d",
             (int)FTEST_MB_SIM_ACT, (int)factoryTestRunSingle(FTEST_MB_SIM_ACT));
    return;
  }
#endif
  if (strncmp(line, "PHOTO,", 6) == 0) {
    const char *arg = line + 6;
    // Mismo parseo estricto que WIFI_EN: un '0' o un '1' y nada mas.
    if ((arg[0] != '0' && arg[0] != '1') || arg[1] != '\0') {
      ESP_LOGW(DBGCON_TAG, "PHOTO: argumento invalido, se espera 0 o 1");
      return;
    }
    if (arg[0] == '0') {
      debug_photo_off();
      ESP_LOGW(DBGCON_TAG, "PHOTO,0: fototerapia de depuracion soltada; manda "
               "otra vez el display (se aplica en el siguiente tick, sin "
               "esperar ninguna trama)");
      return;
    }
    // in3.phototherapy es aqui la ultima orden REAL del display: todavia no
    // la ha podido contaminar esta prueba.
    if (!debug_photo_on(in3.phototherapy)) {
      ESP_LOGW(DBGCON_TAG, "PHOTO,1 rechazado: el modo depuracion esta apagado "
               "(POST /debug/mode?on=1)");
      return;
    }
    ESP_LOGW(DBGCON_TAG, "PHOTO,1: fototerapia de depuracion ENCENDIDA (se "
             "aplica en el siguiente tick). La pantalla NO la muestra: el "
             "campo photo de CTRL,STATE emite la orden del display mientras "
             "dura la prueba; ver /debug/state ctl.photo_dbg");
    return;
  }
  if (strncmp(line, "WIFI_EN,", 8) != 0) {
    ESP_LOGI(DBGCON_TAG, "comando desconocido, ignorado: %s", line);
    return;
  }
  const char *arg = line + 8;
  // Exactamente un '0' o un '1' y nada mas: sin atoi(), "WIFI_EN,0abc" no se
  // cuela como un apagado valido.
  if ((arg[0] != '0' && arg[0] != '1') || arg[1] != '\0') {
    ESP_LOGW(DBGCON_TAG, "WIFI_EN: argumento invalido, se espera 0 o 1");
    return;
  }
  wifiRequestEnable(arg[0] == '1');
}

void debugConsolePoll(void) {
  static char buf[40];
  static size_t len = 0;
  while (debugSerial.available()) {
    const char c = (char)debugSerial.read();
    if (c == '\r') continue;
    if (c != '\n') {
      // Al pasarse de largo se marca la linea y se descarta ENTERA al final,
      // en vez de procesar un trozo: media linea podria ser un comando valido
      // por accidente.
      if (len < sizeof(buf) - 1)
        buf[len++] = c;
      else
        len = sizeof(buf);
      continue;
    }
    if (len < sizeof(buf)) {
      buf[len] = '\0';
      debugConsoleHandle(buf);
    }
    len = 0;
  }
}

void OTA_WIFI_Task(void *pvParameters) {
  WIFI_TB_Init();
  for (;;) {
    debugConsolePoll();
#ifdef FTEST_BENCH_DEBUG
    benchChrgOkMonitorPoll();
#endif
    WifiOTAHandler();
    vTaskDelay(pdMS_TO_TICKS(OTA_TASK_PERIOD_MS));
  }
}

void buzzer_Task(void *pvParameters) {
  for (;;) {
    buzzerHandler();
    vTaskDelay(pdMS_TO_TICKS(BUZZER_TASK_PERIOD_MS));
  }
}

void security_Task(void *pvParameters) {
  for (;;) {
    if (ALARM_SYSTEM_ENABLED && in3.alarmsEnabled) {
      securityCheck();
    }
    vTaskDelay(pdMS_TO_TICKS(SECURITY_TASK_PERIOD_MS));
  }
}

void TimeTrack_Task(void *pvParameters) {
  for (;;) {
    timeTrackHandler();
    vTaskDelay(pdMS_TO_TICKS(TIME_TRACK_TASK_PERIOD_MS));
  }
}

// Aplica el estado de la fototerapia: variable, PWM y ventilador. Es el camino
// UNICO que usa una orden del display (newCommand) y el tick periodico que
// cierra el encendido/apagado de depuracion, y NO persiste nada en NVS (eso
// lo decide quien llama).
static void applyPhotoState(bool on) {
  // Estado ANTERIOR, para distinguir el flanco de encendido del keepalive.
  const bool photoEstabaEncendida = in3.phototherapy;
  in3.phototherapy = on;
  if (in3.phototherapy) {
    if (in3.photoFirstRun) {
      in3.phototherapy_intensity = PWM_MAX_VALUE * PHOTOTHERAPY_INITIAL_PWM_PCT / 100;
      in3.photoFirstRun = false;
    }
    // SOLO en el flanco de encendido. Esto estaba fuera del if y se
    // ejecutaba en cada mandato del HMI, que es un KEEPALIVE: CommTask.cpp
    // pone newCommand=true en cada trama recibida, ~1 por segundo.
    //
    // photoTurnOnTime es la marca de asentamiento que mira el lazo de
    // regulacion de intensidad (sensors_module.cpp, currentMonitor):
    //
    //     millis() - in3.photoTurnOnTime > PHOTO_SETTLE_MS   // 3000 ms
    //
    // Refrescandola cada segundo esa condicion NO se cumple NUNCA, asi que
    // el lazo no llegaba a ejecutarse jamas y PHOTO_TARGET_CURRENT era
    // codigo muerto: la lampara se quedaba en la semilla en lazo abierto.
    //
    // Medido en banco el 2026-09-23: semilla PWM 102 (extrapolada por el
    // autotest para 0.27 A desde una lectura de 0.10 A al 10 % de PWM) y
    // corriente real 0.59-0.60 A sostenida durante 6 minutos. La
    // extrapolacion 1/x se equivoca por 2.2x porque el consumo del LED no
    // es lineal con el duty; corregir eso es justo el trabajo del lazo.
    if (!photoEstabaEncendida) {
      in3.photoTurnOnTime = millis();
    }
  }
  // Idem: el canal de fototerapia tambien lo posee el test de fabrica
  // mientras dura la bateria. La reconciliacion con in3.phototherapy
  // (actualizado arriba, sin gate: eso SI debe seguir reflejando la
  // trama) la hace este mismo newCommand en su siguiente vuelta, en
  // cuanto g_factoryTestActive baje (ver cabecera de
  // factory_test_task.cpp).
  //
  // photoPwmRefresh() y no un ledcWrite suelto: el lazo de intensidad
  // (currentMonitor, tarea de seguridad) tambien escribe este canal, y entre
  // que comprobaba in3.phototherapy y escribia podia colarse un apagado desde
  // aqui, dejando la lampara encendida con in3.phototherapy=false y sin alarma.
  if (!g_factoryTestActive)
    photoPwmRefresh();
  turnFans(bool(in3.phototherapy || in3.actuation));
}

void Communication_Receiver(void *pvParameters) {
  for (;;) {
    if (hmi_cmd_msg.newCommand) {
      hmi_cmd_msg.newCommand = false;

      String msg = "HMI CMD -> act=" + String(hmi_cmd_msg.actuation) +
                   " mode=" + String(hmi_cmd_msg.controlMode) +
                   " air=" + String(hmi_cmd_msg.desiredAirTemperature, 1) +
                   " skin=" + String(hmi_cmd_msg.desiredSkinTemperature, 1) +
                   " hum=" + String(hmi_cmd_msg.desiredHumidity, 0) +
                   " photo=" + String(hmi_cmd_msg.phototherapyMode) +
                   " mute=" + String(hmi_cmd_msg.muteAlarm) +
                   " lang=" + String(hmi_cmd_msg.language);

      logI(msg);

      // Reset the alarm stabilization window (see alarmTimerStart(),
      // security.cpp) only on the OFF->ON transition, matching what the
      // now-removed on-board UI (legacy/UI_actuatorsProgress.cpp) used to do
      // when a user confirmed activation from its menu. The HMI-driven path
      // resends this command on every command cycle even while actuation
      // stays on, so gating on the transition (rather than "actuation != 0")
      // avoids perpetually postponing the window and suppressing alarms.
      bool actuationWasOff = (in3.actuation == ACTUATION_OFF);
      in3.actuation = hmi_cmd_msg.actuation;
      { Preferences p; p.begin(NS_STATE, false); p.putUChar(KEY_ACTUATION, in3.actuation); p.end(); }
      if (actuationWasOff && in3.actuation != ACTUATION_OFF) {
        alarmTimerStart();
      }
      if (in3.controlMode != hmi_cmd_msg.controlMode) {
        in3.controlMode = hmi_cmd_msg.controlMode;
        { Preferences p; p.begin(NS_CFG, false); p.putUChar(KEY_CTRL_MODE, in3.controlMode); p.end(); }
      }

      const bool tempBlocked = ongoingCriticalWiringAlarm();

      switch (in3.actuation) {
      case ACTUATION_TEMPERATURE:
        in3.temperatureControl = !tempBlocked;
        in3.humidityControl = false;
        break;
      case ACTUATION_HUMIDITY:
        in3.temperatureControl = false;
        in3.humidityControl = true;
        break;
      case ACTUATION_TEMP_AND_HUMIDITY:
        in3.temperatureControl = !tempBlocked;
        in3.humidityControl = true;
        break;
      default:
        in3.temperatureControl = false;
        in3.humidityControl = false;
        break;
      }

      if (in3.temperatureControl) {
        double newDesiredTemp = in3.controlMode ? hmi_cmd_msg.desiredAirTemperature
                                                 : hmi_cmd_msg.desiredSkinTemperature;
        if (newDesiredTemp != in3.desiredControlTemperature) {
          in3.desiredControlTemperature = newDesiredTemp;
          // Persisted so a restoreState boot (crash/WDT) restarts the PID at
          // the setpoint the user actually configured, not the compiled-in
          // default (KEY_CTRL_TEMP's fallback in recapVariables()).
          Preferences p;
          p.begin(NS_CFG, false);
          p.putFloat(KEY_CTRL_TEMP, in3.desiredControlTemperature);
          p.end();
        }
        startPID(in3.controlMode);
      } else {
        stopPID(CONTROL_AIR);
        stopPID(!CONTROL_AIR);
        // El test de fabrica tiene el canal del calefactor en estado seguro
        // (design.md D4, shared-factory-test): no pisarlo con este keepalive
        // del HMI, que se repite en cada trama aunque actuation ya este OFF.
        if (!g_factoryTestActive)
          ledcWrite(HEATER_PWM_CHANNEL, false);
      }
      if (in3.humidityControl) {
        if (hmi_cmd_msg.desiredHumidity != in3.desiredControlHumidity) {
          in3.desiredControlHumidity = hmi_cmd_msg.desiredHumidity;
          // Same rationale as KEY_CTRL_TEMP above, for humidity restoreState boots.
          Preferences p;
          p.begin(NS_CFG, false);
          p.putUChar(KEY_CTRL_HUM, in3.desiredControlHumidity);
          p.end();
        }
        startPID(humidityPID);
      } else {
        stopPID(humidityPID);
        // Idem HEATER_PWM_CHANNEL arriba: el humidificador tambien es parte
        // del estado seguro del test de fabrica (design.md D4).
        if (!g_factoryTestActive)
          in3_hum.turn(OFF);
      }

      // Normalmente es lo que manda el display. Con una prueba de depuracion
      // activa manda photo_override: fuerza ON y REGISTRA la orden del display
      // para aplicarla al soltar. Ver modules/control/photo_override.h. Se
      // aplica mas abajo con applyPhotoState(), el mismo camino que usa el tick
      // periodico.
      const bool photoOrden = debug_photo_effective(hmi_cmd_msg.phototherapyMode);
      // Se guarda SIEMPRE la orden del display, tambien durante la prueba: el
      // campo photo de CTRL,STATE emite esa misma orden mientras dura (el
      // display nunca adopta el ON forzado), asi que lo que devuelve en su
      // keepalive es una orden REAL del operador y nunca esta contaminado. No
      // guardarla perderia un OFF/ON del operador si la placa cayera a mitad de
      // prueba. El ON forzado de la prueba, en cambio, no se guarda nunca:
      // solo existe en in3.phototherapy, no en hmi_cmd_msg. Coherente con el
      // espacio "photo" del temporizador, que el parser RX de CommTask guarda y
      // borra por su cuenta.
      {
        Preferences p;
        p.begin(NS_STATE, false);
        p.putUChar(KEY_PHOTO_ACTIVE, hmi_cmd_msg.phototherapyMode);
        p.end();
      }
      if (in3.language != hmi_cmd_msg.language) {
        in3.language = hmi_cmd_msg.language;
        resendActiveAlarms();
      }
      applyPhotoState(photoOrden);

      // AQUI NO SE PITA AL RECIBIR UNA TRAMA DEL DISPLAY.
      //
      // Habia un shutBuzzer() + buzzerTone() en este punto: el zumbador de la
      // placa sonaba cada vez que llegaba una trama, que era el truco con el
      // que el display conseguia confirmacion sonora al tocar un boton. Ese
      // papel lo cubre ahora el zumbador del propio display, y los envios de
      // estado que solo existian para dispararlo ya se retiraron.
      //
      // Quitarlo era obligado desde el latido de 1 Hz, que lo convertia en un
      // pitido por segundo. Pero el problema de fondo es peor y llevaba aqui
      // desde antes: buzzerHandler()/buzzerTone() y buzzerAlarmUpdate()
      // escriben el MISMO canal PWM. Cada trama recibida hacia shutBuzzer(),
      // o sea ledcWrite(0), pisando la rafaga de alarma que estuviera sonando
      // y destrozando el patron de la Tabla 3.
      //
      // Y un pulso suelto del zumbador de la placa es acusticamente identico
      // a una rafaga de prioridad BAJA, que es de UN solo pulso: el
      // transductor de las alarmas no puede emitir sonidos que no sean
      // alarmas.
    }

    // Encendido/apagado de depuracion SIN depender de una trama del display.
    // Mientras la prueba esta activa se fuerza ON; al soltarla (PHOTO,0 o al
    // apagar el modo) se aplica la ULTIMA ORDEN DEL DISPLAY (la del operador
    // manda) por el mismo camino que una orden: con el display colgado no llega
    // ningun keepalive y, sin esto, la lampara se quedaria en el ON de la
    // prueba. El ON forzado no se persiste nunca; al SOLTAR se persiste el valor
    // aplicado (la ultima orden del display, o OFF si vencio el temporizador):
    // con el display colgado no llega ningun keepalive que lo guarde, y NVS
    // seguiria con una orden anterior que la placa ya no cumple. Solo actua si
    // el estado difiere, asi que no reescribe el PWM cada vuelta.
    {
      bool photoValor = false;
      if (debug_photo_forced(&photoValor)) {
        if (in3.phototherapy != photoValor) applyPhotoState(photoValor);
      } else if (debug_photo_take_release(&photoValor)) {
        if (in3.phototherapy != photoValor) applyPhotoState(photoValor);
        Preferences p;
        p.begin(NS_STATE, false);
        p.putUChar(KEY_PHOTO_ACTIVE, photoValor ? 1 : 0);
        p.end();
      }
    }

    // Handle Phototherapy timer expiration/calculation constantly
    // This runs even if no HMI is connected, ensuring hardware turns OFF.
    getRemainingPhotoTime();
    // Al caducar el temporizador se borra el slot "photo" de NVS, pero
    // KEY_PHOTO_ACTIVE seguiria en 1 hasta la siguiente trama del display: una
    // caida entre medias arrancaria en fototerapia continua sin limite. Se
    // guarda el 0 aqui, UNA sola vez por caducidad (la bandera la consume esta
    // tarea; getRemainingPhotoTime tambien la llama send_state_to_hmi) y fuera de
    // la seccion critica. Solo cuesta una lectura de bandera por vuelta.
    if (photoExpiryTakePersistOff()) {
      Preferences p;
      p.begin(NS_STATE, false);
      p.putUChar(KEY_PHOTO_ACTIVE, 0);
      p.end();
    }

    // Unconditional: every loop inside gates itself on its own PID mode, and
    // the fan loop must run for phototherapy-only activation too (fan is
    // commanded on with in3.actuation == 0 — gating on actuation left
    // FAN_CTL_PWM_CHANNEL undriven and false-fired ALARM_FAN_FAILURE).
    PIDHandler();
    vTaskDelay(pdMS_TO_TICKS(COMMUNICATION_TASK_PERIOD_MS));
  }
}

#if (HW_NUM >= 16)
// Apagado por pulsación larga del ON_OFF_SWITCH deshabilitado hasta nuevo
// aviso: mientras sea false, el equipo solo se apaga cortando alimentación
// externa (PWR_EN se mantiene latcheado por el arranque en setup()).
constexpr bool ON_OFF_SWITCH_POWEROFF_ENABLED = false;

void PowerManagement_Task(void *pvParameters) {
  if (!ON_OFF_SWITCH_POWEROFF_ENABLED) {
    vTaskDelete(NULL);
  }

  while (GPIORead(ON_OFF_SWITCH)) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  for (;;) {
    if (GPIORead(ON_OFF_SWITCH)) { // button pressed (active HIGH)
      unsigned long pressStart = millis();
      unsigned long lastSend = 0;
      char msg[32];

      // Send initial PWR_OFF message with full hold time
      snprintf(msg, sizeof(msg), "CTRL,PWR_OFF,%d\n", PWR_HOLD_MS);
      CommunicationHost_Send(msg);
      lastSend = millis();

      while (GPIORead(ON_OFF_SWITCH)) {
        unsigned long elapsed = millis() - pressStart;
        if (elapsed >= (unsigned long)PWR_HOLD_MS) {
          CommunicationHost_Send("CTRL,PWR_OFF,0\n");
          logI("[PWR] Long press detected, powering off");
          vTaskDelay(pdMS_TO_TICKS(50)); // allow UART to flush
          digitalWrite(PWR_EN, LOW);
          while (true) {
            vTaskDelay(pdMS_TO_TICKS(100));
          }
        }
        // Send remaining time every PWR_OFF_UPDATE_INTERVAL_MS
        if (millis() - lastSend >= PWR_OFF_UPDATE_INTERVAL_MS) {
          int remaining = PWR_HOLD_MS - (int)elapsed;
          if (remaining < 0)
            remaining = 0;
          snprintf(msg, sizeof(msg), "CTRL,PWR_OFF,%d\n", remaining);
          CommunicationHost_Send(msg);
          lastSend = millis();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
      }
      // Button released before timeout — cancel
      CommunicationHost_Send("CTRL,PWR_OFF_CANCEL\n");
    }
    vTaskDelay(pdMS_TO_TICKS(POWER_MANAGEMENT_TASK_PERIOD_MS));
  }
}
#endif

extern "C" int sync_vprintf(const char *fmt, va_list args) {
  // Tee formatted output into the crash-report ring so that, after a panic,
  // the last few KB of ESP_LOG context survive in RTC memory.
  char ring_buf[256];
  va_list args_copy;
  va_copy(args_copy, args);
  int ring_len = vsnprintf(ring_buf, sizeof(ring_buf), fmt, args_copy);
  va_end(args_copy);
  if (ring_len > 0) {
    if (ring_len > (int)sizeof(ring_buf))
      ring_len = sizeof(ring_buf);
    crashReporterPut(ring_buf, ring_len);
  }

  if (log_mutex == NULL) {
    return vprintf(fmt, args);
  }
  if (xSemaphoreTakeRecursive(log_mutex, portMAX_DELAY) == pdTRUE) {
    int res = vprintf(fmt, args);
    xSemaphoreGiveRecursive(log_mutex);
    return res;
  }
  return vprintf(fmt, args);
}

void setup() {
  state_init();

#if (HW_NUM >= 16)
  // Power latch: a single press (button already held when boot starts) is
  // enough to keep the device ON. Latch PWR_EN immediately, then wait for
  // the button to be released so the runtime task starts from a clean state.
  {
    pinMode(PWR_EN, OUTPUT);
    digitalWrite(PWR_EN, HIGH);
  }
#endif
  esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
#if (HW_NUM >= 19)
  // Serial es la UART0 hardware, que ya sale por IO43/IO44 (J1) con los pines
  // por defecto del S3. Se pasan explicitos para dejar escrito donde esta.
  debugSerial.begin(115200, SERIAL_8N1, DEBUG_UART_RX_PIN, DEBUG_UART_TX_PIN);
#else
  debugSerial.begin(115200);
#endif
  log_mutex = xSemaphoreCreateRecursiveMutex();
  crashReporterInit();
  esp_log_set_vprintf(sync_vprintf);

  // Antes de que arranque ninguna tarea que haga configTime(): con SNTP el
  // reloj lo escribe lwIP por su cuenta, y sin este callback la motherBoard no
  // se enteraria de que la hora vigente la puso NTP. Sin eso el arbitro la
  // daria por desconocida y una fuente peor podria pisarla.
  systemClockInit();

  GPRS_monitor_mutex = xSemaphoreCreateBinary();
  security_check_reboot_cause();

  diag_prefs.begin("diag", false);
  g_bootCount = diag_prefs.getUInt("boots", 0) + 1;
  diag_prefs.putUInt("boots", g_bootCount);
  diag_prefs.putInt("last_rst", (int)esp_reset_reason());
  g_gprsKillCount = diag_prefs.getUInt("gprs_kill", 0);
  g_monKillCount  = diag_prefs.getUInt("mon_kill", 0);
  diag_prefs.end();
  logI("[DIAG] bootCount=" + String(g_bootCount) +
       " lastRst="    + String((int)esp_reset_reason()) +
       " gprs_kill="  + String(g_gprsKillCount) +
       " mon_kill="   + String(g_monKillCount));

  initEEPROM();
  initGPIO();
  powerRailsInit();
#if defined(FTEST_BENCH_DEBUG) && (HW_NUM >= 19)
  benchChrgOkMonitorInit();
#endif
  // HW19: el raíl de 5 V (humidificador, LEDs del AFE, SensorBoard por USB,
  // display HMI) esta apagado por pull-down hasta que el firmware lo levanta.
  // Va DESPUES de initGPIO() a proposito: initGPIO() deja USB_EN (humidificador)
  // en LOW, asi que el humidificador no recibe un 5 V repentino con la linea de
  // activacion flotando. Y ANTES del enlace con el display, del host USB de la
  // SensorBoard, de initSPO2() y del test de actuadores de initHardware().
  // No-op en HW < 19.
  pwr5vSet(true);
#if (HW_NUM >= 19)
  // CHANGELOG V19, secuencia de arranque (paso 2): arranque suave de 1,5 ms del
  // TPS563231 mas el LDO U15 (3v3_ANA). El ADS1110 de la temperatura de piel y
  // el RX del AFE cuelgan de 3v3_ANA: sin esta espera, initHardware() los
  // sondea con el raíl todavia subiendo.
  delay(10);
  // El SIM800C arranca solo al aplicar VBAT (PWRKEY a GND). Se levanta aqui,
  // bastante antes de crear la tarea GPRS, para que cuando empiece a sondear AT
  // el modem lleve ya varios segundos arrancando.
  gsmPowerSet(true);
#endif

  // Set the serial number and log it
  ctrl_tel_msg.serialNumber = in3.serialNumber;
  logI("IncuNest debug uart, version v" + String(FWversion) + "/" +
       String(HWversion) + ", SN: " + String(in3.serialNumber));
  // PSRAM: en la V19 (N8R2, quad) se espera found=1 y ~2 MB; en V17/V18 no hay
  // PSRAM fisica y sale found=0 size=0. OJO: esto NO depende de
  // BOARD_HAS_PSRAM. El sdkconfig de las librerias precompiladas lleva
  // CONFIG_SPIRAM=y y el core llama a psramInit() en initArduino() en TODAS las
  // placas; en una sin PSRAM falla y se queda en found=0.
  logI("[HW] -> PSRAM found=" + String((int)psramFound()) +
       " size=" + String((uint32_t)ESP.getPsramSize()));
#if (HW_NUM >= 19)
  logI("[HW] -> PWR_5V_EN=HIGH GSM_PWR_EN=HIGH mains_ok=" +
       String((int)mainsPresentPin()));
#endif

  initHardware(false);
  // El detector de red ya sale inicializado con MAINS_DETECTOR_INITIALIZER; se
  // llama igualmente aqui, ANTES de crear sensors_Task, para que el estado
  // inicial no dependa de un inicializador estatico que alguien pueda cambiar
  // a `= {}` (con el que el plazo minimo valia 0 sin mirar la constante).
  mains_detector_init(&g_mains_detector);
  initDriveUpload();
  crashReporterMaybeFlush();
  // SPO2/AFE must initialize SPI (GPIO19 as MISO) before USB host starts.
  // On V15, usb_host_install() claims GPIO19 as USB D−, which conflicts with
  // AFE SPI MISO. Initializing AFE first ensures the chip is configured and
  // DRDY is active before USB takes GPIO19.
  initSPO2();

#if CONFIG_IDF_TARGET_ESP32S3
  // --- Initialize UART/USB communication between ESP32 boards ---
  logI("Initializing communication task ...");
  CommunicationHost_Init();

  // 8 KB, up from 4 KB: the baby-profile handlers added a much deeper path
  // through this task (protocol parse -> LittleFS read/write -> float
  // formatting in the NTE range builder), which 4 KB could not absorb.
  xTaskCreatePinnedToCore(Communication_Task, "COMM_TASK", 8192, NULL,
                          COMMUNICATION_TASK_PRIORITY, NULL,
                          CORE_ID_FREERTOS // o 0/1 según tu placa
  );

  xTaskCreatePinnedToCore(Communication_Receiver, "COMM_TASK_RX", 4096, NULL,
                          COMMUNICATION_RECEIVER_PRIORITY, NULL,
                          CORE_ID_FREERTOS // o 0/1 según tu placa
  );
  logI("Communication task successfully created!\n");

  // ¿De dónde vienen la temperatura y la humedad de cabina en ESTE equipo?
  // Los pines 19/20 son el bus I2C2 hacia la PCBA de sensores en los equipos
  // antiguos y el USB hacia el SensorBoard en los nuevos. El sondeo del bus ya
  // lo hizo initRoomSensor() dentro de initHardware(): si respondió alguien,
  // este equipo lleva los sensores por I2C y NO se levanta el host USB —
  // hacerlo reclamaría esos pads y dejaría al PID sin variable de control.
  //
  // Va aquí, después de initSPO2(), para no cambiar el orden de arranque que
  // ya estaba validado.
  if (roomSensorI2CDetected()) {
    sensorSourceSet(SENSOR_SOURCE_I2C);
    logI("Air sensors found on I2C2: SensorBoard USB link not started");
  } else {
    sensorSourceSet(SENSOR_SOURCE_SENSORBOARD);
    logI("No air sensor on I2C2: starting SensorBoard USB link ...");
    releaseI2C2ForUsb();
    sensorboard_comm_init();
    xTaskCreatePinnedToCore(sensorboard_comm_task, "SB_COMM", 4096, NULL,
                            SENSORBOARD_TASK_PRIORITY, NULL, CORE_ID_FREERTOS);
    logI("SensorBoard task successfully created!\n");
  }
#endif
  if (WIFI_EN) {
    wifiInit();
    logI("WiFi Initialization started.");
  }

#if (HW_NUM >= 16)
  logI("Creating power management task ...\n");
  while (xTaskCreatePinnedToCore(PowerManagement_Task, "PWR_MGMT", 2048, NULL,
                                 POWER_MANAGEMENT_TASK_PRIORITY, NULL,
                                 CORE_ID_FREERTOS) != pdPASS)
    ;
  logI("Power management task successfully created!\n");
#endif

  logI("Creating buzzer task ...\n");
  while (xTaskCreatePinnedToCore(buzzer_Task, "BUZZER", SMALL_TASK_STACK_BYTES, NULL,
                                 BUZZER_TASK_PRIORITY, NULL,
                                 CORE_ID_FREERTOS) != pdPASS)
    ;
  logI("Buzzer task successfully created!\n");

  logI("Creating sensors task ...\n");
  while (xTaskCreatePinnedToCore(sensors_Task, "SENSORS", 4096, NULL,
                                 SENSORS_TASK_PRIORITY, NULL,
                                 CORE_ID_FREERTOS) != pdPASS)
    ;
  logI("Sensors task successfully created!\n");

  logI("Creating security task ...\n");
  while (xTaskCreatePinnedToCore(security_Task, "SECURITY", SMALL_TASK_STACK_BYTES, NULL,
                                 SECURITY_TASK_PRIORITY, NULL,
                                 CORE_ID_FREERTOS) != pdPASS)
    ;
  logI("Security task successfully created!\n");

  logI("Creating GPRS task ...\n");
  while (xTaskCreatePinnedToCore(GPRS_Task, "GPRS", GPRS_TASK_STACK_BYTES, NULL,
                                 GPRS_TAST_PRIORITY, &taskHandle,
                                 CORE_ID_FREERTOS) != pdPASS)
    ;
  logI("GPRS task successfully created!\n");

  logI("Creating OTA task ...\n");
  while (xTaskCreatePinnedToCore(OTA_WIFI_Task, "OTA", OTA_TASK_STACK_BYTES, NULL,
                                 OTA_TASK_PRIORITY, NULL,
                                 CORE_ID_FREERTOS) != pdPASS)
    ;
  logI("OTA task successfully created!\n");

  logI("Creating time track task ...\n");
  while (xTaskCreatePinnedToCore(TimeTrack_Task, "TimeTrack", SMALL_TASK_STACK_BYTES, NULL,
                                 TIME_TRACK_TASK_PRIORITY, NULL,
                                 CORE_ID_FREERTOS) != pdPASS)
    ;
  logI("Time track task successfully created!\n");

  // Watchdog externo: arranca AQUI, no antes. setup() dura mucho mas que el
  // tWD de fabrica del TPS3431 y no alimenta el WDI. No-op salvo HW19 con
  // EXT_WDT_ENABLED.
  extWdtArm();

#ifdef CRASH_TEST_MB
  xTaskCreatePinnedToCore(CrashTestMBTask, "CRASH_TEST_MB", 2048, NULL, 1,
                          NULL, CORE_ID_FREERTOS);
#endif
}

void loop() {
  watchdogReload();
  updateData();
  vTaskDelay(pdMS_TO_TICKS(LOOP_TASK_PERIOD_MS));
}