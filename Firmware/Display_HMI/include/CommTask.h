#ifndef COMM_TASK_H
#define COMM_TASK_H

#include "main.h"
#include <Arduino.h>
#include <lvgl.h>

#define COMMUNICATION_DEBUG true

#if COMMUNICATION_DEBUG
#define COMM_LOG(...) Serial.printf(__VA_ARGS__)
#else
#define COMM_LOG(...)
#endif

#define COMM_SERIAL Serial

#define ALARM_TYPE_LEN 30
#define ALARM_DESC_LEN 100

// ======================
//   DATA STRUCTURES
// ======================

// Message received from the HMI board
typedef struct {
  double desiredAirTemperature;
  double desiredSkinTemperature;
  double desiredHumidity;
  int actuation;
  bool controlMode;
  bool phototherapyMode;
  bool shouldSendData;
  bool muteAlarm = false;
  int language;
  bool skinModeEnabled;
  int photoMinutesRemaining;
} HMI_Message;

// Message with sensor data for control logic
typedef struct {
  double temperature[3];
  double humidity[2];
  bool shouldSendData;
} ControlBoard_Message;

// Telemetry message that is sent every second
typedef struct {
  double detectedAirTemperature;
  double detectedSkinTemperature;
  double detectedHumidity;
  int serverCommStatus;
  bool shouldSendData;
} ControlBoard_Message_Telemetry;

// Alarm message
typedef struct {
  int id;
  char type[ALARM_TYPE_LEN];
  char description[ALARM_DESC_LEN];
  bool state;
} ControlBoard_Message_Alarm;

typedef struct {
  int actuation;
  int controlMode;
  double desiredAirTemperature;
  double desiredSkinTemperature;
  double desiredHumidity;
  int phototherapyMode;
  int muteAlarm;
  int serialNumber;
  int hwNum;
  char hwRev[2];
  char fwVer[20];
  int language;
  bool newState;
  int skinModeEnabled;
  int serverCommStatus;
  int photoMinutesRemaining;
  int photoSecondsRemaining;  // Segundos restantes de fototerapia (0-59)
} ControlBoard_Message_State;

// Expected prefix of incoming messages
#if IS_HMI
#define EXPECTED_PREFIX "CTRL"
#else
#define EXPECTED_PREFIX "HMI"
#endif

// ======================
//   GLOBAL VARIABLES
// ======================
extern HMI_Message hmi_msg;
extern ControlBoard_Message ctrl_msg;
extern ControlBoard_Message_Telemetry ctrl_tel_msg;
extern ControlBoard_Message_Alarm ctrl_msg_alarm;
extern ControlBoard_Message_State ctrl_state_msg;

extern bool error;

// ======================
//   PUBLIC FUNCTIONS
// ======================
void CreateCommTask();
void Communication_RequestState(void);
void Communication_SendWiFiCredentials(const char *ssid, const char *password);

#endif
