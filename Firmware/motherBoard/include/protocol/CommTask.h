#pragma once
#include <Arduino.h>
#include "protocol.h"
#include "control_types.h"
#include "alarm_ids.h"

#if IS_HMI
#define EXPECTED_PREFIX "CTRL"
#else
#define EXPECTED_PREFIX "HMI"
#endif

// Skin probe values — now in SkinProbeState enum in protocol.h.
// These aliases maintain backward compatibility with existing MB code.
#define SKIN_PROBE_NOT_CONNECTED  0
#define SKIN_PROBE_VALID          2

#define DUTY_MSG_BUF_SIZE 24

// Backward-compatibility aliases so callers compile unchanged.
// TelemetryMessage and HMI_CommandMessage are the MB-local wrappers
// that add internal flags on top of the shared protocol types.
typedef struct {
  double detectedAirTemperature;
  double detectedSkinTemperature;
  double detectedHumidity;
  int    serverCommStatus;
  int    serialNumber;
} TelemetryMessage;

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
  int    babyWeightGrams;
  int    babyGestWeeks;
  int    babyAgeDays;
  // MB-internal flags (not part of the protocol)
  bool   newBabyData;
  bool   newBabyDataForTelemetry;
  bool   newCommand;
} HMI_CommandMessage;

extern TelemetryMessage   ctrl_tel_msg;
extern HMI_CommandMessage hmi_cmd_msg;

void   CommunicationHost_Init();
void   Communication_Task(void *pv);
void   CommunicationHost_Send(const char *);
void   setHMIConnected(bool connected);
double getRemainingPhotoTime();
void   sendWifiToHMI(const char *ssid, const char *pass);
