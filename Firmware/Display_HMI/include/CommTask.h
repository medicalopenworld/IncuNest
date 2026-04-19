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
  // Auto Air baby data (0 = not set)
  int babyWeightGrams;
  int babyGestWeeks;
  int babyAgeDays;
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

// PPG waveform sample (CTRL,PPG — 25 Hz)
typedef struct {
  uint8_t ppg;        // normalised 0–255
  bool    updated;    // true after each new sample, cleared by consumer
} ControlBoard_Message_PPG;

// Vital signs (CTRL,VIT — 1 Hz)
typedef struct {
  uint8_t hr;         // 40–240 bpm; 0 = no valid signal
  uint8_t spo2;       // 0–100 %; 0 = no valid signal (reserved, always 0 for now)
  bool    updated;
} ControlBoard_Message_VIT;

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
  uint32_t alarmBitmask;      // Mascara de bits de alarmas activas
  int skinProbeState;         // Estado validado de la sonda de piel (RF-SKIN-006)
} ControlBoard_Message_State;

// Skin probe state values (must match SkinProbeState_t in motherboard main.h)
#define SKIN_PROBE_NOT_CONNECTED                  0
#define SKIN_PROBE_PENDING_VALIDATION             1
#define SKIN_PROBE_VALID                          2
#define SKIN_PROBE_INVALID                        3
#define SKIN_PROBE_OUT_OF_RANGE                   4
#define SKIN_PROBE_DISCONNECTED_DURING_OPERATION  5
#define SKIN_PROBE_UNSTABLE                       6

// Expected prefix of incoming messages
#if IS_HMI
#define EXPECTED_PREFIX "CTRL"
#else
#define EXPECTED_PREFIX "HMI"
#endif

// ======================
//   POWER OFF STATE
// ======================
extern volatile bool g_pwrOffActive;       // true while MB is counting down
extern volatile int  g_pwrOffRemainingMs;  // ms remaining (3000 → 0)
constexpr int PWR_OFF_TOTAL_MS = 3000;     // must match MB PWR_HOLD_MS

// ======================
//   GLOBAL VARIABLES
// ======================
extern HMI_Message hmi_msg;
extern ControlBoard_Message ctrl_msg;
extern ControlBoard_Message_Telemetry ctrl_tel_msg;
extern ControlBoard_Message_Alarm ctrl_msg_alarm;
extern ControlBoard_Message_State ctrl_state_msg;
extern ControlBoard_Message_PPG ctrl_ppg_msg;
extern ControlBoard_Message_VIT ctrl_vit_msg;
extern int g_skinProbeState; // Last received skin probe state (SKIN_PROBE_*)

extern bool error;

// ======================
//   PUBLIC FUNCTIONS
// ======================
void CreateCommTask();
void Communication_RequestState(void);
void Communication_UIReady(void);
void Communication_SendWiFiCredentials(const char *ssid, const char *password);

#endif
