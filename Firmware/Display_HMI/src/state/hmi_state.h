#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
  // Received from MB
  double   airTemperature;
  double   skinTemperature;
  double   humidity;
  int      serverCommStatus;
  int      serialNumber;
  int      hwNum;
  char     hwRev[2];
  char     fwVer[20];

  // Control settings
  int      actuation;
  int      controlMode;
  double   desiredAirTemperature;
  double   desiredSkinTemperature;
  double   desiredHumidity;
  int      phototherapyMode;
  int      photoMinutesRemaining;
  int      photoSecondsRemaining;

  // Alarms
  uint32_t alarmBitmask;
  int      skinProbeState;

  // UI state
  uint8_t  language;
  bool     darkMode;
  bool     humidityEnabled;
  bool     restoreState;

  // Power-off
  bool     pwrOffActive;
  int      pwrOffRemainingMs;
} HmiState;

void     hmi_state_init(void);
HmiState hmi_state_get(void);
void     hmi_state_set(const HmiState *s);
