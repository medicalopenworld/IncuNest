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
#include "modules/control/alarm_machine.h"

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

#define MINIMUM_SUCCESSFULL_SENSOR_UPDATE 20000 // in millis

// 60601-1-8 6.8.3: la pausa de audio no puede exceder 2 min sin reanudarse
// sola. Es lo que dura el silencio que pide el operador con el encoder.
#define ALARM_AUDIO_PAUSE_MS 120000u

// Ventana de estabilizacion de las desviaciones de temperatura y humedad. El
// resto de condiciones se evaluan siempre; estas dos familias no, porque una
// incubadora que arranca fria tarda en alcanzar la consigna (201.12.3.104).
long lastAlarmTrigger[NUM_ALARMS];
long lastPowerSupplyCheck;

// Bitmask de condiciones senalizando en el ciclo anterior. Comparar contra el
// actual es lo que produce los eventos que van al display (sendAlarmUSB) y al
// buzzer: la maquina de alarmas es un estado, no un flujo de eventos.
static uint32_t previousAlarmBitmask = 0;
static bool previousAudioRequired = false;

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

void initAlarms()
{
  alarm_machine_init();
  previousAlarmBitmask = 0;
  previousAudioRequired = false;
  for (int i = 0; i < NUM_ALARMS; i++)
  {
    in3.alarmToReport[i] = false;
  }
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
  const bool stale = (millis() - lastSuccesfullSensorUpdate[sensor] >
                      MINIMUM_SUCCESSFULL_SENSOR_UPDATE);
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
    if (in3.controlMode == CONTROL_SKIN)
    {
      alarm_machine_condition(ALARM_SKIN_SENSOR_FAULT_SKIN_MODE, stale, now);
      alarm_machine_condition(ALARM_SKIN_SENSOR_FAULT_AIR_MODE, false, now);
    }
    else
    {
      alarm_machine_condition(ALARM_SKIN_SENSOR_FAULT_AIR_MODE, stale, now);
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
  // checkAlarms() only evaluates the temperature deviations / humidity once
  // ACTUATORS_ALARM_STABILIZATION_MINS have passed since lastAlarmTrigger.
  // Offsetting it into the past by (stabilization - graceMinutes) makes
  // that window elapse `graceMinutes` from now instead of the full wait -
  // a fresh activation passes graceMinutes=0 (full wait, unchanged
  // behavior), a restoreState boot passes RESTART_ALARM_GRACE_MINS.
  long alreadyElapsedMins = ACTUATORS_ALARM_STABILIZATION_MINS - graceMinutes;
  if (alreadyElapsedMins < 0)
  {
    alreadyElapsedMins = 0;
  }
  const long windowStart = -1 * minsToMillis(alreadyElapsedMins);
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

// Textos por condicion. Sin acentos ni caracteres fuera de ASCII: las fuentes
// del display no los tienen y saldrian como basura justo cuando mas importa
// leerlos. Cortos y accionables para personal clinico, no diagnosticos.
char *alarmIDtoString(byte alarmID)
{
  const byte lang = in3.language; // or hmi_cmd_msg.language
  switch (alarmID)
  {
  case ALARM_AIR_THERMAL_CUTOUT:
    if (lang == SPANISH)
      return (char *)("CORTE TERMICO AIRE");
    if (lang == FRENCH)
      return (char *)("COUPURE THERMIQUE AIR");
    return (char *)("AIR THERMAL CUTOUT");
  case ALARM_SKIN_THERMAL_CUTOUT:
    if (lang == SPANISH)
      return (char *)("CORTE TERMICO PIEL");
    if (lang == FRENCH)
      return (char *)("COUPURE THERMIQUE PEAU");
    return (char *)("SKIN THERMAL CUTOUT");
  case ALARM_AIR_SENSOR_FAULT:
    if (lang == SPANISH)
      return (char *)("FALLO SENSOR AIRE");
    if (lang == FRENCH)
      return (char *)("PANNE CAPTEUR AIR");
    return (char *)("AIR SENSOR FAULT");
  case ALARM_SKIN_SENSOR_FAULT_SKIN_MODE:
    if (lang == SPANISH)
      return (char *)("FALLO SONDA PIEL");
    if (lang == FRENCH)
      return (char *)("PANNE SONDE PEAU");
    return (char *)("SKIN PROBE FAULT");
  case ALARM_FAN_FAILURE:
    if (lang == SPANISH)
      return (char *)("FALLO VENTILADOR");
    if (lang == FRENCH)
      return (char *)("PANNE VENTILATEUR");
    return (char *)("FAN FAILURE");
  case ALARM_AIR_OUTLET_BLOCKED:
    if (lang == SPANISH)
      return (char *)("SALIDA DE AIRE OBSTRUIDA");
    if (lang == FRENCH)
      return (char *)("SORTIE D'AIR OBSTRUEE");
    return (char *)("AIR OUTLET BLOCKED");
  case ALARM_MAINS_INTERRUPTION:
    if (lang == SPANISH)
      return (char *)("CORTE DE RED");
    if (lang == FRENCH)
      return (char *)("COUPURE SECTEUR");
    return (char *)("MAINS INTERRUPTION");
  case ALARM_AIR_TEMP_DEVIATION_HIGH:
    if (lang == SPANISH)
      return (char *)("TEMP AIRE ALTA");
    if (lang == FRENCH)
      return (char *)("TEMP AIR ELEVEE");
    return (char *)("AIR TEMP HIGH");
  case ALARM_AIR_TEMP_DEVIATION_LOW:
    if (lang == SPANISH)
      return (char *)("TEMP AIRE BAJA");
    if (lang == FRENCH)
      return (char *)("TEMP AIR BASSE");
    return (char *)("AIR TEMP LOW");
  case ALARM_SKIN_TEMP_DEVIATION_HIGH:
    if (lang == SPANISH)
      return (char *)("TEMP PIEL ALTA");
    if (lang == FRENCH)
      return (char *)("TEMP PEAU ELEVEE");
    return (char *)("SKIN TEMP HIGH");
  case ALARM_SKIN_TEMP_DEVIATION_LOW:
    if (lang == SPANISH)
      return (char *)("TEMP PIEL BAJA");
    if (lang == FRENCH)
      return (char *)("TEMP PEAU BASSE");
    return (char *)("SKIN TEMP LOW");
  case ALARM_HEATER_FAULT:
    if (lang == SPANISH)
      return (char *)("FALLO CALENTADOR");
    if (lang == FRENCH)
      return (char *)("PANNE CHAUFFAGE");
    return (char *)("HEATER FAULT");
  case ALARM_SUPPLY_UNDERVOLTAGE:
    if (lang == SPANISH)
      return (char *)("TENSION BAJA");
    if (lang == FRENCH)
      return (char *)("TENSION BASSE");
    return (char *)("SUPPLY UNDERVOLTAGE");
  case ALARM_HMI_LINK_LOST:
    if (lang == SPANISH)
      return (char *)("SIN ENLACE PANTALLA");
    if (lang == FRENCH)
      return (char *)("LIAISON ECRAN PERDUE");
    return (char *)("DISPLAY LINK LOST");
  case ALARM_SKIN_SENSOR_FAULT_AIR_MODE:
    if (lang == SPANISH)
      return (char *)("SONDA PIEL NO VALIDA");
    if (lang == FRENCH)
      return (char *)("SONDE PEAU INVALIDE");
    return (char *)("SKIN PROBE UNUSABLE");
  case ALARM_HUMIDITY_DEVIATION:
    if (lang == SPANISH)
      return (char *)("DESVIACION HUMEDAD");
    if (lang == FRENCH)
      return (char *)("ECART HUMIDITE");
    return (char *)("HUMIDITY DEVIATION");
  default:
    return (char *)("ALARM");
  }
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

  // El titulo lo produce alarmIDtoString() para no mantener dos tablas de
  // textos en paralelo; aqui solo vive la linea de accion, que dice al
  // personal clinico QUE HACER, no que ha pasado. Solo ASCII: las fuentes del
  // display no tienen acentos.
  title = alarmIDtoString(alarmID);

  switch (alarmID)
  {
  case ALARM_AIR_THERMAL_CUTOUT:
  case ALARM_SKIN_THERMAL_CUTOUT:
    if (lang == SPANISH)
      desc = "CALEFACTOR CORTADO - REVISAR AL BEBE";
    else if (lang == FRENCH)
      desc = "CHAUFFAGE COUPE - VERIFIER LE BEBE";
    else
      desc = "HEATER CUT - CHECK THE BABY";
    break;
  case ALARM_AIR_SENSOR_FAULT:
    if (lang == SPANISH)
      desc = "SIN MEDIDA DE AIRE - CONTROL DETENIDO";
    else if (lang == FRENCH)
      desc = "PAS DE MESURE D AIR - CONTROLE ARRETE";
    else
      desc = "NO AIR READING - CONTROL STOPPED";
    break;
  case ALARM_SKIN_SENSOR_FAULT_SKIN_MODE:
    if (lang == SPANISH)
      desc = "REVISAR SONDA O PASAR A MODO AIRE";
    else if (lang == FRENCH)
      desc = "VERIFIER LA SONDE OU PASSER EN MODE AIR";
    else
      desc = "CHECK PROBE OR SWITCH TO AIR MODE";
    break;
  case ALARM_FAN_FAILURE:
    if (lang == SPANISH)
      desc = "SIN CIRCULACION DE AIRE - REVISAR EQUIPO";
    else if (lang == FRENCH)
      desc = "PAS DE CIRCULATION D AIR - VERIFIER";
    else
      desc = "NO AIR CIRCULATION - SERVICE UNIT";
    break;
  case ALARM_AIR_OUTLET_BLOCKED:
    if (lang == SPANISH)
      desc = "DESPEJAR LA SALIDA DE AIRE";
    else if (lang == FRENCH)
      desc = "DEGAGER LA SORTIE D AIR";
    else
      desc = "CLEAR THE AIR OUTLET";
    break;
  case ALARM_MAINS_INTERRUPTION:
    if (lang == SPANISH)
      desc = "REVISAR LA CONEXION A LA RED";
    else if (lang == FRENCH)
      desc = "VERIFIER LE RACCORDEMENT SECTEUR";
    else
      desc = "CHECK THE MAINS CONNECTION";
    break;
  case ALARM_AIR_TEMP_DEVIATION_HIGH:
    if (lang == SPANISH)
      desc = "AIRE MAS DE 3 C SOBRE LA CONSIGNA";
    else if (lang == FRENCH)
      desc = "AIR A PLUS DE 3 C AU DESSUS DE LA CONSIGNE";
    else
      desc = "AIR OVER 3 C ABOVE SETPOINT";
    break;
  case ALARM_AIR_TEMP_DEVIATION_LOW:
    if (lang == SPANISH)
      desc = "AIRE MAS DE 3 C BAJO LA CONSIGNA";
    else if (lang == FRENCH)
      desc = "AIR A PLUS DE 3 C SOUS LA CONSIGNE";
    else
      desc = "AIR OVER 3 C BELOW SETPOINT";
    break;
  case ALARM_SKIN_TEMP_DEVIATION_HIGH:
    if (lang == SPANISH)
      desc = "PIEL MAS DE 1 C SOBRE LA CONSIGNA";
    else if (lang == FRENCH)
      desc = "PEAU A PLUS DE 1 C AU DESSUS DE LA CONSIGNE";
    else
      desc = "SKIN OVER 1 C ABOVE SETPOINT";
    break;
  case ALARM_SKIN_TEMP_DEVIATION_LOW:
    if (lang == SPANISH)
      desc = "PIEL MAS DE 1 C BAJO LA CONSIGNA";
    else if (lang == FRENCH)
      desc = "PEAU A PLUS DE 1 C SOUS LA CONSIGNE";
    else
      desc = "SKIN OVER 1 C BELOW SETPOINT";
    break;
  case ALARM_HEATER_FAULT:
    if (lang == SPANISH)
      desc = "EL EQUIPO NO CALIENTA - REVISAR EQUIPO";
    else if (lang == FRENCH)
      desc = "L APPAREIL NE CHAUFFE PAS - VERIFIER";
    else
      desc = "UNIT NOT HEATING - SERVICE UNIT";
    break;
  case ALARM_SUPPLY_UNDERVOLTAGE:
    if (lang == SPANISH)
      desc = "REVISAR FUENTE Y CABLEADO";
    else if (lang == FRENCH)
      desc = "VERIFIER L ALIMENTATION ET LE CABLAGE";
    else
      desc = "CHECK SUPPLY AND WIRING";
    break;
  case ALARM_HMI_LINK_LOST:
    if (lang == SPANISH)
      desc = "DATOS NO FIABLES - REVISAR AL BEBE";
    else if (lang == FRENCH)
      desc = "DONNEES NON FIABLES - VERIFIER LE BEBE";
    else
      desc = "DATA UNRELIABLE - CHECK THE BABY";
    break;
  case ALARM_SKIN_SENSOR_FAULT_AIR_MODE:
    if (lang == SPANISH)
      desc = "SIN TEMP DE PIEL - MODO AIRE ACTIVO";
    else if (lang == FRENCH)
      desc = "PAS DE TEMP PEAU - MODE AIR ACTIF";
    else
      desc = "NO SKIN TEMP - AIR MODE ACTIVE";
    break;
  case ALARM_HUMIDITY_DEVIATION:
    if (lang == SPANISH)
      desc = "REVISAR DEPOSITO DE AGUA";
    else if (lang == FRENCH)
      desc = "VERIFIER LE RESERVOIR D EAU";
    else
      desc = "CHECK THE WATER TANK";
    break;
  default:
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
  const uint32_t mask = alarm_machine_bitmask();
  for (int i = NO_ALARMS + 1; i < NUM_ALARMS; i++)
  {
    if (mask & (1u << i))
    {
      sendAlarmUSB(i, true);
    }
  }
}

// Pulsacion del encoder: 60601-1-8 6.8.1 permite inactivar el audio, pero la
// senal visual tiene que seguir mientras la condicion persista, y silenciar
// una condicion no puede silenciar las demas — por eso se silencia una a una
// y no existe un silencio global.
void reestartOngoingAlarms()
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
  const bool airMode = (in3.controlMode == CONTROL_AIR);
  bool evaluateDeviation =
      in3.temperatureControl &&
      (millis() - lastAlarmTrigger[ALARM_AIR_TEMP_DEVIATION_HIGH] >=
       minsToMillis(ACTUATORS_ALARM_STABILIZATION_MINS));

  const float measured = airMode ? in3.temperature[ROOM_DIGITAL_TEMP_SENSOR]
                                 : in3.temperature[SKIN_SENSOR];
  const float deviation = measured - (float)in3.desiredControlTemperature;
  const float limit =
      airMode ? AIR_TEMP_DEVIATION_LIMIT_C : SKIN_TEMP_DEVIATION_LIMIT_C;

  airHighPresent = evaluateDeviation && airMode &&
                   thresholdWithHysteresis(airHighPresent, deviation, limit,
                                           TEMPERATURE_ERROR_HYSTERESIS);
  airLowPresent = evaluateDeviation && airMode &&
                  thresholdWithHysteresis(airLowPresent, -deviation, limit,
                                          TEMPERATURE_ERROR_HYSTERESIS);
  skinHighPresent = evaluateDeviation && !airMode &&
                    thresholdWithHysteresis(skinHighPresent, deviation, limit,
                                            TEMPERATURE_ERROR_HYSTERESIS);
  skinLowPresent = evaluateDeviation && !airMode &&
                   thresholdWithHysteresis(skinLowPresent, -deviation, limit,
                                           TEMPERATURE_ERROR_HYSTERESIS);

  alarm_machine_condition(ALARM_AIR_TEMP_DEVIATION_HIGH, airHighPresent, now);
  alarm_machine_condition(ALARM_AIR_TEMP_DEVIATION_LOW, airLowPresent, now);
  alarm_machine_condition(ALARM_SKIN_TEMP_DEVIATION_HIGH, skinHighPresent, now);
  alarm_machine_condition(ALARM_SKIN_TEMP_DEVIATION_LOW, skinLowPresent, now);

  const bool evaluateHumidity =
      in3.humidityControl &&
      (millis() - lastAlarmTrigger[ALARM_HUMIDITY_DEVIATION] >=
       minsToMillis(ACTUATORS_ALARM_STABILIZATION_MINS));
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

  if (fanControlPID.GetMode() != AUTOMATIC)
  {
    dutyHighSince = 0;
    return; // not under closed-loop control — no duty signal to evaluate
  }
  if (alarmSignalling(ALARM_FAN_FAILURE))
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
static void publishAlarmChanges()
{
  const uint32_t mask = alarm_machine_bitmask();
  const uint32_t changed = mask ^ previousAlarmBitmask;

  for (int i = NO_ALARMS + 1; i < NUM_ALARMS; i++)
  {
    const bool active = (mask & (1u << i)) != 0;
    if (changed & (1u << i))
    {
      logAlarm("[ALARM] ->" + String(alarmIDtoString(i)) +
               (active ? " has been triggered" : " has been disable"));
      sendAlarmUSB(i, active);
    }
    // La telemetria a nube (GPRS.cpp / Wifi_OTA.cpp) publica desde aqui.
    in3.alarmToReport[i] = active;
  }
  previousAlarmBitmask = mask;
}

// El audio lo gobierna la maquina (6.10 incluye la rafaga minima y la pausa
// que pide el operador); aqui solo se traducen sus flancos al buzzer.
static void driveAlarmBuzzer()
{
  const bool audioRequired = alarm_machine_audio_required();
  if (audioRequired && !previousAudioRequired)
  {
    buzzerTone(buzzerAlarmBeepCount, buzzerAlarmBeepTime, buzzerAlarmTone);
  }
  else if (!audioRequired && previousAudioRequired)
  {
    shutBuzzer();
  }
  previousAudioRequired = audioRequired;
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
  alarm_machine_tick(millis());
  publishAlarmChanges();
  driveAlarmBuzzer();
}