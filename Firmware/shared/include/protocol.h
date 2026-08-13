#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "alarm_ids.h"
#include "control_types.h"

typedef enum {
  SKIN_PROBE_NOT_CONNECTED = 0,
  SKIN_PROBE_PENDING_VALIDATION,
  SKIN_PROBE_VALID,
  SKIN_PROBE_INVALID,
  SKIN_PROBE_OUT_OF_RANGE,
  SKIN_PROBE_DISCONNECTED_DURING_OPERATION,
  SKIN_PROBE_UNSTABLE,
} SkinProbeState;

typedef struct {
  double detectedAirTemperature;
  double detectedSkinTemperature;
  double detectedHumidity;
  int    serverCommStatus;
  int    serialNumber;
} Proto_CtrlTelemetry;

typedef struct {
  int      actuation;
  int      controlMode;
  double   desiredAirTemperature;
  double   desiredSkinTemperature;
  double   desiredHumidity;
  int      phototherapyMode;
  int      muteAlarm;
  int      serialNumber;
  int      hwNum;
  char     hwRev[2];
  char     fwVer[20];
  int      language;
  int      skinModeEnabled;
  int      serverCommStatus;
  int      photoMinutesRemaining;
  int      photoSecondsRemaining;
  uint32_t alarmBitmask;
  int      skinProbeState;
} Proto_CtrlState;

typedef struct {
  int  id;
  char type[30];
  char description[100];
  uint8_t state;
} Proto_CtrlAlarm;

typedef struct {
  uint8_t ppg;
} Proto_CtrlPPG;

typedef struct {
  uint8_t hr;
  uint8_t spo2;
} Proto_CtrlVitals;

typedef struct {
  SkinProbeState state;
} Proto_CtrlProbe;

typedef struct {
  int    actuation;
  int    controlMode;
  double desiredAirTemperature;
  double desiredSkinTemperature;
  double desiredHumidity;
  int    phototherapyMode;
  int    muteAlarm;
  int    language;
  int    skinModeEnabled;
  int    photoMinutesRemaining;
} Proto_HmiCommand;

#ifdef __cplusplus
}
#endif
