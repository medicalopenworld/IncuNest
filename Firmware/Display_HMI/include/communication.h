#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <Arduino.h>
#include "main.h"

// Set to true only on the HMI board
#define IS_HMI true   

#define COMMUNICATION_DEBUG true

#if COMMUNICATION_DEBUG
    #define COMM_LOG(...)  Serial.printf(__VA_ARGS__)
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
  bool shouldSendData;
} ControlBoard_Message_Telemetry;

// Alarm message
typedef struct {
  int id;
  char type[ALARM_TYPE_LEN];
  char description[ALARM_DESC_LEN];
  bool state;
} ControlBoard_Message_Alarm;

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

extern bool error;

// ======================
//   PUBLIC FUNCTIONS
// ======================
void Communication_Init();
void Communication_Task(void *pvParameters);

void SendTelemetry();
void SendAlarm();

bool ReceiveMessageFromOtherESP();

#endif
