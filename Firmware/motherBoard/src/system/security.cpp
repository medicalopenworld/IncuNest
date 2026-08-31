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
#include "alarm_text.h"
#include <Preferences.h>

#include "modules/baby_profile/baby_profile_store.h"
#include "modules/control/alarm_history.h"
#include "modules/control/alarm_machine.h"
#include "modules/control/alarm_test.h"
#include "modules/control/alarm_window.h"

static const char *TAG __attribute__((unused)) = "SECURITY";
extern SemaphoreHandle_t log_mutex;

// Cola de eventos de alarma para cuando el display no esta conectado.
//
// Dimensionada por ALARM_COUNT y no por un numero fijo: resendActiveAlarms()
// puede empujar una linea por cada una de las 16 condiciones de una sola vez,
// y con una cola de 10 las 6 ultimas se perdian en silencio — una instantanea
// incompleta, ademas de arbitraria en que condiciones se caian.
//
// Lo que esto NO hace es eliminar el descarte. La cola solo se vacia en
// setHMIConnected(true), y la trama periodica de estado (CommTask.cpp) llama a
// resendActiveAlarms() siempre que alarmCount > 0, este el display conectado o
// no: con una alarma viva y el display caido, bastan un par de periodos para
// llenarla y volver a descartar. Lo que se gana es que las ALARM_COUNT
// posiciones contienen ya una instantanea COMPLETA de las condiciones
// senalizando, asi que lo que se descarta a partir de ahi son repeticiones de
// lo que la cola ya lleva. Al reconectar, el display recibe esa instantanea y
// se resincroniza ademas por el bitmask de la trama de estado.
struct PendingAlarm
{
  char message[ALARM_LINE_BUF_SIZE];
  bool valid;
};

#define PENDING_ALARM_QUEUE_LEN ALARM_COUNT

static PendingAlarm pending_alarms[PENDING_ALARM_QUEUE_LEN];
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

// 201.15.4.2.1 dd): la desviacion respecto a la consigna en control por AIRE
// alarma a +-3 C. ee): en control por PIEL, a +-1 C. Son cuatro condiciones
// distintas (modo x sentido) porque el calefactor solo se corta por el lado
// caliente: por el frio tiene que seguir calentando.
#define AIR_TEMP_DEVIATION_LIMIT_C 3.0f
#define SKIN_TEMP_DEVIATION_LIMIT_C 1.0f
#define HUMIDITY_ERROR 10   // 10 %RH to trigger alarm

#define TEMPERATURE_ERROR_HYSTERESIS \
  0.05                              // 0.05 degrees difference to disable alarm
#define HUMIDITY_ERROR_HYSTERESIS 5 // 5 %RH to disable alarm

// ACTUATORS_ALARM_STABILIZATION_MINS / RESTART_ALARM_GRACE_MINS now live in
// main.h (initHardware.cpp needs them too for the restoreState resume path).

#define FAN_TEST_CURRENTDIF_MIN \
  0.2 // when the fan is spinning, heater cools down and consume less current
#define FAN_TEST_PREHEAT_TIME \
  30000 // when the fan is spinning, heater cools down and consume less current

// security config
#define AIR_THERMAL_CUTOUT_HYSTERESIS 0.2f
#define SKIN_THERMAL_CUTOUT_HYSTERESIS 0.2f

// Ventana de staleness. Las dos valen 5 s, pero se mantienen separadas porque
// lo que las justifica es distinto y sus cadencias de muestreo tambien: si
// manana cambia ROOM_SENSOR_UPDATE_PERIOD_MS o SKIN_SENSOR_UPDATE_PERIOD_MS,
// solo hay que revisar la suya.
//
// Aire: lectura cada ROOM_SENSOR_UPDATE_PERIOD_MS = 1000 ms (500 ms mientras
// reintenta reconectar), luego 5 s son >= 5 ciclos perdidos seguidos. Es el
// margen mas justo de los dos, y el que hay que revisar si aparecen falsos
// positivos: esta condicion corta el calefactor.
//
// Piel: lectura cada SKIN_SENSOR_UPDATE_PERIOD_MS = 200 ms, luego 5 s son ~25
// muestras seguidas descartadas. El antirrebote efectivo es cinco veces mayor
// que el del aire pese al numero identico, que es lo que hace que bajar de
// 20 s a 5 s no arriesgue fatiga de alarma en un frontal analogico.
#define MINIMUM_SUCCESSFULL_AIR_SENSOR_UPDATE 5000  // in millis
#define MINIMUM_SUCCESSFULL_SKIN_SENSOR_UPDATE 5000 // in millis

// ALARM_AUDIO_PAUSE_MS vive ahora en include/main.h: lo necesita tambien
// CommTask.cpp para atender HMI,ALM_SILENCE.

// Ventana de estabilizacion: una incubadora que arranca fria tarda en alcanzar
// la consigna. Se aplica de forma distinta segun lo que gobierne la condicion
// (ver checkAlarms()): al lado CALIENTE de la desviacion solo se le retrasa el
// anuncio, porque su corte de calefactor es ESSENTIAL PERFORMANCE y no puede
// esperar; al lado FRIO y a la humedad, que no gobiernan actuador alguno, se
// les cierra la evaluacion entera.
// uint32_t, no long: el reloj es millis() y la resta modular sin signo es la
// que sigue dando el intervalo correcto al desbordar a los ~49 dias.
uint32_t lastAlarmTrigger[NUM_ALARMS];
long lastPowerSupplyCheck;

// La sonda de piel no es obligatoria en modo aire. applyNTCResult()
// (sensors_module.cpp) solo refresca lastSuccesfullSensorUpdate[SKIN_SENSOR]
// cuando los milivoltios caen dentro de la ventana de descarte, asi que con la
// sonda desconectada se queda en 0 de por vida. Sin este latch, el equipo de
// fabrica (modo aire, sin sonda) levantaria una alarma BAJA permanente a los
// 5 s de arrancar: fatiga de alarma pura. Una sonda ausente no es un fallo.
static bool skinProbeEverRead = false;

// Bitmask de condiciones senalizando en el ciclo anterior. Comparar contra el
// actual es lo que produce los eventos que van al display (sendAlarmUSB): la
// maquina de alarmas es un estado, no un flujo de eventos. El audio ya no
// necesita este mismo rastreo de flanco: buzzerAlarmUpdate() (Buzzer.cpp) se
// gobierna por estado, no por evento (ver driveAlarmBuzzer() mas abajo).
static uint32_t previousAlarmBitmask = 0;

extern IncuNest_parameters in3;

static inline bool alarmSignalling(AlarmId id)
{
  return alarm_machine_state(id) != ALARM_STATE_INACTIVE;
}

// Umbral con banda muerta: la condicion se declara al superar `threshold` y
// no se retira hasta caer por debajo de `threshold - hysteresis`. Sin ella el
// corte de calefactor conmutaria en cada muestra alrededor del limite.
static bool thresholdWithHysteresis(bool wasPresent, float value,
                                    float threshold, float hysteresis)
{
  return wasPresent ? (value > threshold - hysteresis) : (value > threshold);
}

// Lo que le queda a la ventana de estabilizacion de `id`, en ms. La aritmetica
// vive en modules/control/alarm_window.cpp para poder verificarla en el
// entorno nativo. El lado caliente la usa como retardo de ANUNCIO en el flanco
// en que aparece la condicion (la alarma espera, el corte de calefactor no,
// porque heater_must_cut() mira la condicion fisica y no el estado de senal);
// el lado frio y la humedad la usan como puerta de evaluacion.
static uint32_t stabilizationRemainingMs(AlarmId id, uint32_t now)
{
  return alarm_window_remaining_ms(
      lastAlarmTrigger[id],
      (uint32_t)minsToMillis(ACTUATORS_ALARM_STABILIZATION_MINS), now);
}

static bool stabilizationElapsed(AlarmId id, uint32_t now)
{
  return stabilizationRemainingMs(id, now) == 0u;
}

// Declara una desviacion del LADO CALIENTE arrancando su retardo de anuncio en
// el flanco de subida. `wasPresent` es el valor del ciclo anterior.
//
// Ojo: el retardo de anuncio solo aplaza el AUDIO. El estado PENDING sigue
// entrando en alarm_machine_bitmask(), asi que la condicion se ve en el
// display y se publica a nube desde el primer ciclo. Es aceptable en el lado
// caliente (estar 3 C por encima de la consigna durante el calentamiento no
// es normal y ademas ya ha cortado el calefactor), y es justamente lo que
// obliga a cerrar el lado frio con una puerta en vez de con un retardo.
static void declareHotDeviation(AlarmId id, bool wasPresent, bool present,
                                uint32_t now)
{
  if (present && !wasPresent)
  {
    alarm_machine_set_announce_delay(id, stabilizationRemainingMs(id, now));
  }
  alarm_machine_condition(id, present, now);
}

void initAlarms()
{
  alarm_machine_init();
  previousAlarmBitmask = 0;
  skinProbeEverRead = false;
  for (int i = 0; i < NUM_ALARMS; i++)
  {
    in3.alarmToReport[i] = false;
  }
  // El registro SI sobrevive al arranque: su valor esta justamente en poder
  // mirar que paso mientras nadie estaba delante, incluido un reinicio por
  // watchdog. La maquina de estados se reinicia; el historial se recupera.
  alarmHistoryLoad();
}

void checkThermalCutOuts()
{
  // Los limites ya llegan acotados a 38/40 C (alarm_clamp_*_cutout() se
  // aplica en EEPROM.cpp y en los puntos de entrada de configuracion).
  static bool airCutoutPresent = false;
  static bool skinCutoutPresent = false;
  const uint32_t now = millis();

  airCutoutPresent = thresholdWithHysteresis(
      airCutoutPresent, in3.temperature[ROOM_DIGITAL_TEMP_SENSOR],
      in3.airTemperatureSetMax, AIR_THERMAL_CUTOUT_HYSTERESIS);
  skinCutoutPresent = thresholdWithHysteresis(
      skinCutoutPresent, in3.temperature[SKIN_SENSOR],
      in3.skinTemperatureSetMax, SKIN_THERMAL_CUTOUT_HYSTERESIS);

  // 201.15.4.2.1 aa)/bb): ambos son latching, asi que retirar la condicion no
  // apaga el aviso — hace falta un reset manual (alarm_machine_reset()). Lo
  // que si se libera de inmediato es el corte de calefactor, que depende de
  // la condicion presente y no del estado de la senal.
  alarm_machine_condition(ALARM_AIR_THERMAL_CUTOUT, airCutoutPresent, now);
  alarm_machine_condition(ALARM_SKIN_THERMAL_CUTOUT, skinCutoutPresent, now);
}

void checkStatusOfSensor(byte sensor)
{
  const uint32_t now = millis();
  const uint32_t staleLimit = (sensor == ROOM_DIGITAL_TEMP_SENSOR)
                                  ? MINIMUM_SUCCESSFULL_AIR_SENSOR_UPDATE
                                  : MINIMUM_SUCCESSFULL_SKIN_SENSOR_UPDATE;
  const bool stale =
      (millis() - lastSuccesfullSensorUpdate[sensor] > staleLimit);
  switch (sensor)
  {
  case ROOM_DIGITAL_TEMP_SENSOR:
    alarm_machine_condition(ALARM_AIR_SENSOR_FAULT, stale, now);
    break;
  case SKIN_SENSOR:
    // 201.12.3.102: perder la sonda de piel gobernando el lazo es ALTA y
    // corta el calefactor; en modo aire la sonda no controla nada, asi que
    // es BAJA y no toca el calefactor. Son dos condiciones distintas, y al
    // cambiar de modo hay que retirar la del par contrario o queda colgada.
    if (lastSuccesfullSensorUpdate[SKIN_SENSOR] != 0)
    {
      // applyNTCResult() usa el 0 como centinela de "aun no ha leido nunca".
      skinProbeEverRead = true;
    }
    if (in3.controlMode == CONTROL_SKIN)
    {
      // En modo piel la sonda es obligatoria: su ausencia SI es un fallo.
      alarm_machine_condition(ALARM_SKIN_SENSOR_FAULT_SKIN_MODE, stale, now);
      alarm_machine_condition(ALARM_SKIN_SENSOR_FAULT_AIR_MODE, false, now);
    }
    else
    {
      // En modo aire hay que distinguir AUSENCIA de FALLO: una sonda que
      // nunca llego a leer en este ciclo de alimentacion es una sonda que no
      // esta puesta, y eso es una configuracion normal, no una averia.
      alarm_machine_condition(ALARM_SKIN_SENSOR_FAULT_AIR_MODE,
                              stale && skinProbeEverRead, now);
      alarm_machine_condition(ALARM_SKIN_SENSOR_FAULT_SKIN_MODE, false, now);
    }
    break;
  default:
    break;
  }
}

void sensorHealthMonitor()
{
  checkStatusOfSensor(ROOM_DIGITAL_TEMP_SENSOR);
  if (in3.temperatureControl)
  {
    checkStatusOfSensor(SKIN_SENSOR);
  }
  else
  {
    // Sin control de temperatura la sonda no se usa: nada que avisar.
    const uint32_t now = millis();
    alarm_machine_condition(ALARM_SKIN_SENSOR_FAULT_SKIN_MODE, false, now);
    alarm_machine_condition(ALARM_SKIN_SENSOR_FAULT_AIR_MODE, false, now);
  }
}

void powerMonitor()
{
  currentMonitor();
  voltageMonitor();
}

void alarmTimerStart(long graceMinutes)
{
  // Marca el inicio de la ventana de estabilizacion. El lado CALIENTE de las
  // desviaciones la usa como retardo de ANUNCIO (declareHotDeviation(): la
  // condicion se declara igual, con corte de calefactor incluido, solo se
  // retrasa el aviso). El lado FRIO y la desviacion de humedad, que no
  // gobiernan actuador alguno, la usan como puerta de evaluacion.
  // Offsetting it into the past by (stabilization - graceMinutes) makes
  // that window elapse `graceMinutes` from now instead of the full wait -
  // a fresh activation passes graceMinutes=0 (full wait, unchanged
  // behavior), a restoreState boot passes RESTART_ALARM_GRACE_MINS.
  //
  // "Into the past" cuenta desde AHORA. Antes era `-1 * ...`, es decir desde
  // el origen del reloj, y eso rompia la ventana de dos maneras: con la
  // terapia iniciada mas de 30 min despues de encender el equipo — preparar la
  // incubadora antes de empezar es un flujo clinico normal — la ventana nacia
  // ya cerrada y el lado frio alarmaba durante toda la rampa de calentamiento.
  long alreadyElapsedMins = ACTUATORS_ALARM_STABILIZATION_MINS - graceMinutes;
  if (alreadyElapsedMins < 0)
  {
    alreadyElapsedMins = 0;
  }
  const uint32_t windowStart =
      alarm_window_start(millis(), (uint32_t)minsToMillis(alreadyElapsedMins));
  lastAlarmTrigger[ALARM_AIR_TEMP_DEVIATION_HIGH] = windowStart;
  lastAlarmTrigger[ALARM_AIR_TEMP_DEVIATION_LOW] = windowStart;
  lastAlarmTrigger[ALARM_SKIN_TEMP_DEVIATION_HIGH] = windowStart;
  lastAlarmTrigger[ALARM_SKIN_TEMP_DEVIATION_LOW] = windowStart;
  lastAlarmTrigger[ALARM_HUMIDITY_DEVIATION] = windowStart;
  // The thermal cutouts stay immediately eligible either way - they are a
  // hard over-temperature safety limit, not a setpoint-tracking alarm.
}

byte activeAlarm()
{
  const uint32_t mask = alarm_machine_bitmask();
  for (int i = NO_ALARMS + 1; i < NUM_ALARMS; i++)
  {
    if (mask & (1u << i))
    {
      return (byte)i;
    }
  }
  return 0;
}

bool ongoingAlarms() { return alarm_machine_any_signalling(); }

int getActiveAlarmCount()
{
  const uint32_t mask = alarm_machine_bitmask();
  int count = 0;
  for (int i = NO_ALARMS + 1; i < NUM_ALARMS; i++)
  {
    if (mask & (1u << i))
    {
      count++;
    }
  }
  return count;
}

// Puerta del calefactor. Ya no enumera condiciones: la politica de que
// condicion obliga a desconectar la alimentacion del calefactor vive en
// alarm_cuts_heater() (shared/), y la maquina la aplica sobre la condicion
// FISICA presente, no sobre la senal — un corte termico latching sigue
// avisando tras enfriarse, pero deja de bloquear el calefactor.
bool ongoingCriticalAlarm() { return alarm_machine_heater_must_cut(); }

bool ongoingCriticalWiringAlarm()
{
  return (alarmSignalling(ALARM_HEATER_FAULT) ||
          alarmSignalling(ALARM_FAN_FAILURE) ||
          alarmSignalling(ALARM_SUPPLY_UNDERVOLTAGE));
}

// Unlike ongoingCriticalWiringAlarm() (used for heater/humidifier gating),
// a heater fault only has to take the fan down with it when this unit has
// no independent way (RPM feedback) to verify the fan is still spinning.
bool ongoingFanCriticalAlarm()
{
  return (alarmSignalling(ALARM_FAN_FAILURE) ||
          alarmSignalling(ALARM_SUPPLY_UNDERVOLTAGE) ||
          (alarmSignalling(ALARM_HEATER_FAULT) && !in3.fanHasSpeedFeedback));
}

// Los textos de operador (titulo y linea de accion) viven en
// shared/src/alarm_text.cpp: tienen que caber en los campos de la linea
// CTRL,ALM o el display descarta la linea entera, y solo desde shared/ son
// alcanzables por el test nativo que fija ese limite (test_alarm_text).
// Este envoltorio existe solo porque el resto del firmware ya llamaba a
// alarmIDtoString() con un byte y esperaba un char*.
char *alarmIDtoString(byte alarmID)
{
  return (char *)alarm_title_text((AlarmId)alarmID, (Language)in3.language);
}

void sendPendingAlarms()
{
  for (int i = 0; i < pending_alarm_count && i < PENDING_ALARM_QUEUE_LEN; i++)
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
  char msg[ALARM_LINE_BUF_SIZE];
  const Language lang = (Language)in3.language;

  // Titulo y linea de accion salen de la misma tabla que el resto del sistema
  // (shared/src/alarm_text.cpp). El titulo dice QUE ha pasado y la linea de
  // accion QUE HACER; ambas caben en los campos del protocolo por
  // construccion, y el test nativo test_alarm_text lo fija.
  const char *title = alarm_title_text((AlarmId)alarmID, lang);
  const char *desc = alarm_action_text((AlarmId)alarmID, lang);

  // La marca de prioridad se antepone al titulo aqui, al componer la linea,
  // y no vive en los literales traducidos. IEC 60601-1-8 6.3.2.2.2 exige que
  // la senal visual de 1 m identifique la condicion **y su prioridad**, y el
  // protocolo CTRL,ALM no lleva campo de prioridad, asi que el titulo es el
  // unico vehiculo que hay. La convencion de uno, dos o tres signos es la que
  // la propia clausula ofrece como ejemplo valido.
  //
  // El ancho %.*s corta al limite del campo aunque la marca crezca: sin el,
  // un titulo largo con "!!! " delante desbordaria ALARM_TITLE_MAX_CHARS y el
  // display descartaria la linea entera, que es el fallo que dejo 12 de 16
  // alarmas invisibles antes de esta rama.
  char titled[ALARM_TITLE_MAX_CHARS + 1];
  snprintf(titled, sizeof(titled), "%s %s",
           alarm_priority_mark((AlarmId)alarmID), title);

  // La prioridad viaja como campo propio y no se deja deducir al display. El
  // display no debe tener logica de alarmas: la motherBoard es la dueña de la
  // informacion y aquella se limita a pintarla. Antes de esto el display
  // llamaba a alarm_priority() por su cuenta para colorear el banner, que es
  // una segunda copia de la politica esperando a desincronizarse.
  snprintf(msg, sizeof(msg), "CTRL,ALM,%d,%.*s,%.*s,%d,%d\n", alarmID,
           ALARM_TITLE_MAX_CHARS, titled, ALARM_DESC_MAX_CHARS, desc,
           isActive ? 1 : 0, (int)alarm_priority((AlarmId)alarmID));
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
    if (pending_alarm_count < PENDING_ALARM_QUEUE_LEN)
    {
      strncpy(pending_alarms[pending_alarm_count].message, msg,
              sizeof(pending_alarms[pending_alarm_count].message) - 1);
      pending_alarms[pending_alarm_count]
          .message[sizeof(pending_alarms[pending_alarm_count].message) - 1] =
          '\0';
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
  const uint32_t mask = alarm_machine_bitmask();
  for (int i = NO_ALARMS + 1; i < NUM_ALARMS; i++)
  {
    if (mask & (1u << i))
    {
      sendAlarmUSB(i, true);
    }
  }
}

// Boton de silencio del display HMI: la unica interaccion de operador que
// existe hoy (el encoder es hardware de una revision anterior, ya no se
// monta). 60601-1-8 6.8.1 permite inactivar el audio, pero la senal visual
// tiene que seguir mientras la condicion persista, y silenciar una condicion
// no puede silenciar las demas — por eso se silencia una a una y no existe
// un silencio global.
//
// Llamar solo en el flanco de subida de hmi_cmd_msg.muteAlarm (ver
// CommTask.cpp): la trama HMI llega periodicamente con el bit a nivel alto
// mientras el operador mantenga la vista muteada, y silenciar en cada ciclo
// reiniciaria la ventana de ALARM_AUDIO_PAUSE_MS eternamente sin que el audio
// llegase a reanudarse nunca.
void silenceActiveAlarmsFromDisplayMute()
{
  const uint32_t now = millis();
  for (int i = NO_ALARMS + 1; i < NUM_ALARMS; i++)
  {
    alarm_machine_silence((AlarmId)i, ALARM_AUDIO_PAUSE_MS, now);
  }
}

void checkAlarms()
{
  const uint32_t now = millis();
  // Estado de la condicion fisica (no de la senal) para la banda muerta: los
  // cortes termicos son latching y su senal sobrevive a la condicion, asi que
  // la histeresis no puede leerse del estado de la maquina.
  static bool airHighPresent = false, airLowPresent = false;
  static bool skinHighPresent = false, skinLowPresent = false;
  static bool humidityPresent = false;

  // 201.15.4.2.1 dd)/ee): cuatro condiciones direccionales. La medida y el
  // umbral dependen del modo; el par del modo inactivo se retira siempre, o
  // quedaria colgado al cambiar de modo.
  //
  // Los dos lados se tratan de forma ASIMETRICA, y no es una inconsistencia:
  //
  // - dd) y ee) empiezan literalmente con "After STEADY TEMPERATURE CONDITIONS
  //   ... have been achieved", asi que durante el calentamiento la condicion
  //   de desviacion NO EXISTE segun la norma. El LADO FRIO se cierra durante
  //   la ventana de estabilizacion: no gobierna ningun actuador, cerrarlo no
  //   afecta a ninguna proteccion, y evita que cada arranque normal (22 C de
  //   sala hacia una consigna de 36 C) muestre una alarma MEDIA en pantalla y
  //   la publique a nube durante media hora.
  // - El LADO CALIENTE se declara SIEMPRE, por conservadurismo: es mas
  //   estricto que lo que dd)/ee) exigen, y es lo que mantiene inmediato el
  //   corte de calefactor de alarm_cuts_heater(). Retenerlo durante la ventana
  //   no silenciaba un aviso, inhibia una proteccion, y justo durante la rampa
  //   desde frio, que es cuando el sobreimpulso del PID es mas probable. Su
  //   aviso si se aplaza, con el retardo de anuncio que permite 201.12.3.104.
  const bool airMode = (in3.controlMode == CONTROL_AIR);
  const bool controlling = in3.temperatureControl;
  // Cada lado consulta su propio indice. Hoy alarmTimerStart() escribe el mismo
  // valor en los cinco, pero leer el indice del aire para decidir sobre la piel
  // acoplaba en silencio dos condiciones distintas y dejaba un indice muerto.
  const bool steadyAir = stabilizationElapsed(ALARM_AIR_TEMP_DEVIATION_LOW, now);
  const bool steadySkin =
      stabilizationElapsed(ALARM_SKIN_TEMP_DEVIATION_LOW, now);

  const float measured = airMode ? in3.temperature[ROOM_DIGITAL_TEMP_SENSOR]
                                 : in3.temperature[SKIN_SENSOR];
  const float deviation = measured - (float)in3.desiredControlTemperature;
  const float limit =
      airMode ? AIR_TEMP_DEVIATION_LIMIT_C : SKIN_TEMP_DEVIATION_LIMIT_C;

  const bool airHighWas = airHighPresent;
  const bool airLowWas = airLowPresent;
  const bool skinHighWas = skinHighPresent;
  const bool skinLowWas = skinLowPresent;

  airHighPresent = controlling && airMode &&
                   thresholdWithHysteresis(airHighWas, deviation, limit,
                                           TEMPERATURE_ERROR_HYSTERESIS);
  airLowPresent = controlling && airMode && steadyAir &&
                  thresholdWithHysteresis(airLowWas, -deviation, limit,
                                          TEMPERATURE_ERROR_HYSTERESIS);
  skinHighPresent = controlling && !airMode &&
                    thresholdWithHysteresis(skinHighWas, deviation, limit,
                                            TEMPERATURE_ERROR_HYSTERESIS);
  skinLowPresent = controlling && !airMode && steadySkin &&
                   thresholdWithHysteresis(skinLowWas, -deviation, limit,
                                           TEMPERATURE_ERROR_HYSTERESIS);

  declareHotDeviation(ALARM_AIR_TEMP_DEVIATION_HIGH, airHighWas, airHighPresent,
                      now);
  declareHotDeviation(ALARM_SKIN_TEMP_DEVIATION_HIGH, skinHighWas,
                      skinHighPresent, now);
  // El lado frio no lleva retardo de anuncio: las puertas steadyAir/steadySkin
  // ya impiden que aparezca antes de tiempo, asi que el retardo seria siempre 0
  // — codigo sin efecto. Se anuncia en cuanto se declara.
  alarm_machine_condition(ALARM_AIR_TEMP_DEVIATION_LOW, airLowPresent, now);
  alarm_machine_condition(ALARM_SKIN_TEMP_DEVIATION_LOW, skinLowPresent, now);

  const bool evaluateHumidity =
      in3.humidityControl &&
      stabilizationElapsed(ALARM_HUMIDITY_DEVIATION, now);
  const float humidityDeviation =
      fabsf((float)in3.humidity[ROOM_DIGITAL_HUM_SENSOR] -
            (float)in3.desiredControlHumidity);
  humidityPresent =
      evaluateHumidity &&
      thresholdWithHysteresis(humidityPresent, humidityDeviation,
                              HUMIDITY_ERROR, HUMIDITY_ERROR_HYSTERESIS);
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, humidityPresent, now);
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

  // Histeresis en el mismo sentido que thresholdWithHysteresis() pero con el
  // signo invertido (aqui alarma el valor BAJO): se declara por debajo de
  // FAN_MIN_RPM y no se retira hasta superar FAN_MIN_RPM + histeresis.
  static bool fanFailurePresent = false;
  fanFailurePresent = fanFailurePresent
                          ? (in3.fan_rpm < FAN_MIN_RPM + FAN_MIN_RPM_HYSTERESIS)
                          : (in3.fan_rpm < FAN_MIN_RPM);
  alarm_machine_condition(ALARM_FAN_FAILURE, fanFailurePresent, millis());
}

void checkAirBlockage()
{
#if AIR_BLOCKED_DETECTION_ENABLED
  static bool wasFanCommandedOn = false;
  static long fanCommandedOnSince = 0;
  static long dutyHighSince = 0;
  // Toda salida temprana RETIRA la condicion antes de volver, PERO solo una vez
  // que esta funcion ha llegado a medir de verdad con el lazo cerrado.
  //
  // Retirarla es obligatorio porque la maquina conserva `present` hasta que
  // alguien declare false: declarada la obstruccion, bastaba con desactivar el
  // PID del ventilador (conmutable en caliente desde USB o /config, y
  // persistido) para que la funcion saliera por el primer return de por vida,
  // con heater_must_cut() en true para siempre y el reset manual rechazandola
  // por no ser latching.
  //
  // Y hace falta la guarda porque antes de la primera medida en lazo cerrado
  // la unica declaracion existente es la del autotest de arranque
  // (initHardware.cpp), que mide el duty en banco y despues deja el PID en
  // MANUAL con el ventilador cortado: sin guarda, el primer securityCheck()
  // entraria por la salida de GetMode() != AUTOMATIC y tiraria el resultado
  // del autotest. Mismo patron que la guarda de fanHasSpeedFeedback en
  // checkFanSpeed().
  //
  // Con una excepcion: si el PID del ventilador esta deshabilitado, esta
  // deteccion es PERMANENTEMENTE inviable — PIDHandler() no volvera a poner
  // AUTOMATIC — y la guarda dejaria la declaracion de arranque congelada de
  // por vida, con heater_must_cut() en true y el reset manual rechazandola por
  // no ser latching. Es el mismo dano que cerro C-2. En ese caso la afirmacion
  // honesta es que no hay obstruccion observable, asi que se retira.
  static bool hasObservedClosedLoop = false;

  if (fanControlPID.GetMode() != AUTOMATIC)
  {
    dutyHighSince = 0;
    if (hasObservedClosedLoop || !in3.fanPidEnabled)
    {
      alarm_machine_condition(ALARM_AIR_OUTLET_BLOCKED, false, millis());
    }
    return; // not under closed-loop control — no duty signal to evaluate
  }
  // Las dos salidas siguientes solo se alcanzan con el lazo en AUTOMATIC, que
  // ya implica in3.fanPidEnabled: alli la guarda sola es suficiente.
  if (alarmSignalling(ALARM_FAN_FAILURE))
  {
    dutyHighSince = 0;
    if (hasObservedClosedLoop)
    {
      alarm_machine_condition(ALARM_AIR_OUTLET_BLOCKED, false, millis());
    }
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
    // Idem que arriba: con el ventilador parado no hay obstruccion que medir,
    // y dejarla puesta la congelaria igual.
    dutyHighSince = 0;
    if (hasObservedClosedLoop)
    {
      alarm_machine_condition(ALARM_AIR_OUTLET_BLOCKED, false, millis());
    }
    return;
  }

  // A partir de aqui la medida es valida: lazo cerrado, ventilador girando y
  // spin-up superado. Esta funcion pasa a ser la autoridad sobre la condicion.
  hasObservedClosedLoop = true;

  if (fanControlPIDOutput > FAN_DUTY_BLOCKED_THRESHOLD)
  {
    // Require the excess to be sustained: transients (heater kicking in and
    // sagging the supply, setpoint recovery) legitimately spike the duty
    // while the loop compensates.
    if (dutyHighSince == 0)
    {
      dutyHighSince = millis();
    }
    if (millis() - dutyHighSince >= AIR_BLOCKED_SUSTAIN_MS)
    {
      alarm_machine_condition(ALARM_AIR_OUTLET_BLOCKED, true, millis());
    }
  }
  else
  {
    dutyHighSince = 0;
    if (fanControlPIDOutput <=
        FAN_DUTY_BLOCKED_THRESHOLD - FAN_DUTY_BLOCKED_HYSTERESIS)
    {
      alarm_machine_condition(ALARM_AIR_OUTLET_BLOCKED, false, millis());
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
      const bool undervoltage = (digitalCurrentSensorPresent[MAIN] &&
                                 in3.system_voltage > MIN_SYSTEM_VOLTAGE_TRIGGER &&
                                 in3.system_voltage < MAX_SYSTEM_VOLTAGE_TRIGGER);
      alarm_machine_condition(ALARM_SUPPLY_UNDERVOLTAGE, undervoltage,
                              millis());
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

// La maquina de alarmas es un ESTADO, no un flujo de eventos: el display y la
// nube esperan eventos. Comparar el bitmask con el del ciclo anterior es lo
// que los reconstruye. Sin esto el display se queda sin recibir alarmas, que
// es lo que hacia setAlarm()/resetAlarm() al llamar a sendAlarmUSB().
// --- Persistencia del registro de alarmas -----------------------------------
// El modulo alarm_history no conoce NVS a proposito: expone el blob y lo acepta
// de vuelta, que es lo que lo hace testeable en host. Estas dos funciones son
// el unico sitio que habla con Preferences.
static const char kAlarmHistNs[] = "alm_hist";
static const char kAlarmHistKey[] = "blob";

void alarmHistorySave()
{
  uint8_t blob[256];
  const size_t n = alarm_history_serialize(blob, sizeof(blob));
  if (n == 0)
  {
    logE("[ALARM] historial: blob mas grande que el buffer, no se guarda");
    return;
  }
  Preferences p;
  p.begin(kAlarmHistNs, false);
  p.putBytes(kAlarmHistKey, blob, n);
  p.end();
}

void alarmHistoryLoad()
{
  alarm_history_init();
  uint8_t blob[256];
  Preferences p;
  p.begin(kAlarmHistNs, true);
  const size_t got = p.getBytes(kAlarmHistKey, blob, sizeof(blob));
  p.end();
  if (got == 0)
  {
    return;  // primer arranque: no hay nada guardado todavia
  }
  if (!alarm_history_deserialize(blob, got))
  {
    // Formato viejo o blob corrupto. Se descarta entero y se empieza de cero:
    // leer registros clinicos con el paso equivocado decodificaria basura.
    logE("[ALARM] historial ilegible en NVS, se descarta");
  }
}

// 6.12.2 recomienda anotar en el registro el limite de alarma en vigor cuando
// es ajustable por el operador — los cortes termicos lo son — y el dato que
// disparo la condicion. Se guardan x100 para no arrastrar coma flotante hasta
// NVS ni hasta el protocolo. Las condiciones sin magnitud asociada (fallo de
// ventilador, corte de red) devuelven 0/0, que la pantalla muestra como vacio.
static void alarmMagnitudes(int id, int16_t *limitCenti, int16_t *valueCenti)
{
  float limit = 0.0f, value = 0.0f;
  switch (id)
  {
  case ALARM_AIR_THERMAL_CUTOUT:
    limit = in3.airTemperatureSetMax;
    value = in3.temperature[ROOM_DIGITAL_TEMP_SENSOR];
    break;
  case ALARM_SKIN_THERMAL_CUTOUT:
    limit = in3.skinTemperatureSetMax;
    value = in3.temperature[SKIN_SENSOR];
    break;
  case ALARM_AIR_TEMP_DEVIATION_HIGH:
  case ALARM_AIR_TEMP_DEVIATION_LOW:
    limit = (float)in3.desiredControlTemperature;
    value = in3.temperature[ROOM_DIGITAL_TEMP_SENSOR];
    break;
  case ALARM_SKIN_TEMP_DEVIATION_HIGH:
  case ALARM_SKIN_TEMP_DEVIATION_LOW:
    limit = (float)in3.desiredControlTemperature;
    value = in3.temperature[SKIN_SENSOR];
    break;
  case ALARM_HUMIDITY_DEVIATION:
    limit = (float)in3.desiredControlHumidity;
    value = in3.humidity[ROOM_DIGITAL_HUM_SENSOR];
    break;
  default:
    break;
  }
  *limitCenti = (int16_t)(limit * 100.0f);
  *valueCenti = (int16_t)(value * 100.0f);
}

// Enlace con el display perdido.
//
// El display manda un latido cada HMI_KEEPALIVE_PERIOD_MS (1 s); se dan cinco
// tramas de margen antes de declararlo, que es tambien la misma ventana de
// staleness que usan los sensores. Un solo numero de "cuanto silencio es
// demasiado" en todo el sistema es mas facil de defender que tres distintos.
//
// Prioridad MEDIA (alarm_policy): perder la pantalla no para la terapia, pero
// deja al operador sin lectura y sin la unica via para silenciar.
#define HMI_LINK_TIMEOUT_MS 5000u

// Margen desde el arranque antes de dar por ausente un display que no ha
// hablado NUNCA. Mismo valor y misma razon que BOARD_LINK_BOOT_GRACE_MS en el
// display: los dos extremos cuentan igual, que es el criterio que ya seguian
// las ventanas de silencio.
#define HMI_LINK_BOOT_GRACE_MS 10000u

static void checkHmiLink()
{
  extern uint32_t g_lastHmiLineMs;
  extern bool g_hmiEverSeen;

  // Antes de la primera trama no hay enlace que perder: la placa arranca antes
  // que el display. Declararlo aqui seria una alarma en cada encendido.
  //
  // Pero esa espera TERMINA. Salir sin declarar nada mientras el display no
  // hubiera hablado nunca dejaba a la placa encendida sin pantalla conectada
  // sin decir una palabra — justo la condicion que esta alarma existe para
  // avisar. Pasado el margen de arranque, el display no viene de camino.
  if (!g_hmiEverSeen) {
    alarm_machine_condition(ALARM_HMI_LINK_LOST,
                            millis() > HMI_LINK_BOOT_GRACE_MS, millis());
    return;
  }
  const bool lost =
      (uint32_t)(millis() - g_lastHmiLineMs) > HMI_LINK_TIMEOUT_MS;
  alarm_machine_condition(ALARM_HMI_LINK_LOST, lost, millis());
}

static void publishAlarmChanges()
{
  const uint32_t mask = alarm_machine_bitmask();
  const uint32_t changed = mask ^ previousAlarmBitmask;
  bool historyDirty = false;

  for (int i = NO_ALARMS + 1; i < NUM_ALARMS; i++)
  {
    const bool active = (mask & (1u << i)) != 0;
    if (changed & (1u << i))
    {
      logAlarm("[ALARM] ->" + String(alarmIDtoString(i)) +
               (active ? " has been triggered" : " has been disable"));
      sendAlarmUSB(i, active);

      // El registro se lleva desde aqui y no desde los detectores porque este
      // es el unico punto que ve una TRANSICION. Un detector declara la misma
      // condicion en cada ciclo, asi que registrar alli anotaria un alta por
      // segundo hasta llenar el anillo con una sola alarma.
      const uint32_t nowEpoch = babyStore_nowEpoch();
      if (active)
      {
        int16_t limitCenti = 0, valueCenti = 0;
        alarmMagnitudes(i, &limitCenti, &valueCenti);
        alarm_history_record_raise((AlarmId)i,
                                   (uint8_t)alarm_priority((AlarmId)i),
                                   nowEpoch, limitCenti, valueCenti);
        historyDirty = true;
      }
      else if (alarm_history_record_clear((AlarmId)i, nowEpoch))
      {
        historyDirty = true;
      }
    }
    // La telemetria a nube (GPRS.cpp / Wifi_OTA.cpp) publica desde aqui.
    in3.alarmToReport[i] = active;
  }
  previousAlarmBitmask = mask;

  // Se persiste solo cuando algo cambio de verdad. Escribir el blob en cada
  // ciclo desgastaria la flash sin aportar nada: entre transiciones el
  // contenido es identico.
  if (historyDirty)
  {
    alarmHistorySave();
  }
}

// El audio lo gobierna la maquina (6.10 incluye la rafaga minima y la pausa
// que pide el operador); aqui solo se consulta su estado en cada ciclo.
// buzzerAlarmUpdate() (Buzzer.cpp) es quien regenera el patron de rafaga
// indefinidamente mientras audioRequired siga en true, y quien reacciona de
// inmediato a un cambio de prioridad sin esperar a que termine la rafaga en
// curso — por eso esto ya no dispara por flanco (asi lo hacia el motor viejo,
// y era la causa de que una alarma de prioridad mas alta pudiera quedar muda
// tras una de prioridad mas baja, ver revision de la tarea 9).
static void driveAlarmBuzzer()
{
  // alarm_machine_audible_priority(), no alarm_machine_top_priority(): esta
  // ultima incluye SILENCED/ACKED para que la senal VISUAL siga mostrando la
  // prioridad mas alta tras una inactivacion del operador, pero eso es
  // precisamente lo que el zumbador NO debe hacer - si una ALTA esta
  // silenciada y una BAJA distinta esta activa, el audio tiene que sonar como
  // BAJA. alarm_machine_audible_priority() usa el mismo criterio que
  // alarm_machine_audio_required() (revision de la tarea 11, hallazgo C-2).
  // Prueba de funcionamiento (201.12.3.105). Una alarma REAL la cancela en el
  // acto: la prueba es una comodidad de mantenimiento y no puede quedarse ni
  // un ciclo por delante de una condicion de verdad. Por eso se comprueba
  // any_signalling() y no solo audio_required() — una condicion silenciada o
  // en PENDING sigue siendo una alarma real en curso.
  if (alarm_machine_any_signalling()) {
    alarm_test_abort();
  } else if (alarm_test_active()) {
    buzzerAlarmUpdate(alarm_test_audio_required(),
                      (AlarmPriority)alarm_test_priority());
    return;
  }

  buzzerAlarmUpdate(alarm_machine_audio_required(),
                    alarm_machine_audible_priority());
}

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
  // El tick va DESPUES de la deteccion y ANTES de publicar: es el que hace
  // madurar PENDING -> ACTIVE y el que expira las pausas de audio.
  checkHmiLink();
  alarm_machine_tick(millis());
  alarm_test_tick(millis());
  publishAlarmChanges();
  driveAlarmBuzzer();
}