#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
  // Sensors
  double   temperatureSkin;
  double   temperatureAir;
  double   temperatureAirRedundant;
  double   humidity;
  double   ambientTemperature;
  int      skinCapacitance;
  float    fan_rpm;

  // Control
  int      actuation;
  bool     controlMode;
  bool     temperatureControl;
  bool     humidityControl;
  double   desiredControlTemperature;
  double   desiredControlHumidity;

  // Phototherapy
  bool     phototherapy;
  uint8_t  phototherapy_intensity;
  bool     photoFirstRun;
  long     photoTurnOnTime;

  // Calibration
  double   fineTuneSkinTemperature;
  double   fineTuneAirTemperature;
  bool     calibrationError;

  // Power / current measurements
  double   system_current;
  double   system_voltage;
  double   heater_current;
  double   fan_current;
  double   humidifier_current;
  double   humidifier_voltage;
  double   phototherapy_current;
  double   BATTERY_current;
  double   BATTERY_voltage;
  int      heaterSafeMAXPWM;

  // Alarms
  bool     alarmsEnabled;
  bool     alarmToReport[10];
  char     alarmMessage[255];
  bool     previousAlarmReport;

  // Runtime tracking
  float    standby_time;
  float    control_active_time;
  float    heater_active_time;
  float    fan_active_time;
  float    phototherapy_active_time;
  float    humidifier_active_time;
  long     last_check_time;

  // Device identity
  int      serialNumber;
  int      resetReason;
  bool     restoreState;
  uint8_t  language;

  // Settings
  int      fanPwrSupplyPWM;
  int      fanCtlPWM;
  float    heaterMaxPowerAmps;
  float    skinTemperatureSetMax;
  float    airTemperatureSetMax;
  int      actuating_gprs_period;
  int      phototherapy_gprs_period;
  int      standby_gprs_period;

  // Comm
  uint8_t  commStatus;
} DeviceState;

void        state_init(void);
DeviceState state_get(void);
void        state_set(const DeviceState *s);

double      state_get_skin_temp(void);
double      state_get_air_temp(void);
double      state_get_humidity(void);
int         state_get_actuation(void);
bool        state_get_phototherapy(void);
void        state_set_alarm(uint8_t alarm_id, bool active);
bool        state_get_alarm(uint8_t alarm_id);
uint32_t    state_get_alarm_bitmask(void);
void        state_set_commstatus(uint8_t status);
