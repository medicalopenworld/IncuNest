#pragma once
#include <Arduino.h>

// Expected prefix of incoming messages
#if IS_HMI
#define EXPECTED_PREFIX "CTRL"
#else
#define EXPECTED_PREFIX "HMI"
#endif

// ======================================================
//  TELEMETRY (CTRL → HMI)
// ======================================================
typedef struct {
  double detectedAirTemperature;
  double detectedSkinTemperature;
  double detectedHumidity;
  int serverCommStatus;
  int serialNumber;
} TelemetryMessage;

extern TelemetryMessage ctrl_tel_msg;

// ======================================================
//  HMI COMMAND MESSAGE (HMI → CTRL)
// ======================================================
typedef struct {
  int actuation;
  int controlMode;
  double desiredAirTemperature;
  double desiredSkinTemperature;
  double desiredHumidity;
  int phototherapyMode;
  int muteAlarm;
  int language;
  bool newCommand;
} HMI_CommandMessage;

extern HMI_CommandMessage hmi_cmd_msg;

// ======================================================
//  PUBLIC API
// ======================================================
void CommunicationHost_Init();             // Install USB HOST
void Communication_Task(void *pv);         // FreeRTOS task
void CommunicationHost_Send(const char *); // Manual send
void setHMIConnected(bool connected);      // Notify HMI connection status