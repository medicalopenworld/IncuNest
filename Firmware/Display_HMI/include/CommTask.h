#ifndef COMM_TASK_H
#define COMM_TASK_H

#include "main.h"
#include <Arduino.h>
#include <lvgl.h>
#include "protocol.h"
#include "control_types.h"
#include "alarm_ids.h"

#define COMMUNICATION_DEBUG true
#if COMMUNICATION_DEBUG
#define COMM_LOG(...) Serial.printf(__VA_ARGS__)
#else
#define COMM_LOG(...)
#endif

#define COMM_SERIAL Serial

// Expected prefix of incoming messages
#if IS_HMI
#define EXPECTED_PREFIX "CTRL"
#else
#define EXPECTED_PREFIX "HMI"
#endif

// Backward-compatibility aliases for existing HMI code.
// ControlBoard_Message_Telemetry and ControlBoard_Message_Alarm map 1:1
// to the shared protocol types.
typedef Proto_CtrlTelemetry   ControlBoard_Message_Telemetry;
typedef Proto_CtrlAlarm       ControlBoard_Message_Alarm;

// HMI_Message: protocol fields from Proto_HmiCommand plus HMI-internal flag.
typedef struct {
  // Protocol fields (matching Proto_HmiCommand)
  int    actuation;
  int    controlMode;
  double desiredAirTemperature;
  double desiredSkinTemperature;
  double desiredHumidity;
  int    phototherapyMode;
  bool   muteAlarm;
  int    language;
  bool   skinModeEnabled;
  int    photoMinutesRemaining;
  int    babyWeightGrams;
  int    babyGestWeeks;
  int    babyAgeDays;
  // HMI-internal flag (not part of the protocol)
  bool   shouldSendData;
} HMI_Message;

// ControlBoard_Message_State: protocol fields from Proto_CtrlState plus
// HMI-internal flag.
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
  // HMI-internal flag (not part of the protocol)
  bool     newState;
} ControlBoard_Message_State;

// Legacy name for the probe state enum
typedef SkinProbeState ProbeContactState;
// SPO2_PROBE_* aliases (HMI code uses these names)
#define SPO2_PROBE_DISCONNECTED SKIN_PROBE_NOT_CONNECTED
#define SPO2_PROBE_NOT_APPLIED  SKIN_PROBE_PENDING_VALIDATION
#define SPO2_PROBE_APPLIED      SKIN_PROBE_VALID

// PPG waveform sample (CTRL,PPG — 25 Hz)
typedef struct {
  uint8_t ppg;     // normalised 0-255
  bool    updated; // true after each new sample, cleared by consumer
} ControlBoard_Message_PPG;

// Vital signs (CTRL,VIT — 1 Hz)
typedef struct {
  uint8_t hr;   // 40-240 bpm; 0 = no valid signal
  uint8_t spo2; // 0-100 %; 0 = no valid signal
  bool    updated;
} ControlBoard_Message_VIT;

// SpO2 probe contact state
typedef struct {
  ProbeContactState state;
  bool              updated;
} ControlBoard_Message_Probe;

// Sensor data message (HMI-internal, not a protocol type)
typedef struct {
  double temperature[3];
  double humidity[2];
  bool   shouldSendData;
} ControlBoard_Message;

// ======================
//   POWER OFF STATE
// ======================
extern volatile bool g_pwrOffActive;
extern volatile int  g_pwrOffRemainingMs;
constexpr int PWR_OFF_TOTAL_MS = 3000;

// ======================
//   GLOBAL VARIABLES
// ======================
extern HMI_Message                    hmi_msg;
extern ControlBoard_Message           ctrl_msg;
extern ControlBoard_Message_Telemetry ctrl_tel_msg;
extern ControlBoard_Message_Alarm     ctrl_msg_alarm;
extern ControlBoard_Message_State     ctrl_state_msg;
extern ControlBoard_Message_PPG       ctrl_ppg_msg;
extern ControlBoard_Message_VIT       ctrl_vit_msg;
extern ControlBoard_Message_Probe     ctrl_probe_msg;
extern int  g_skinProbeState;
extern bool error;
extern volatile bool g_pendingTelemetryApply;

// ======================
//   PUBLIC FUNCTIONS
// ======================
void CreateCommTask();
TaskHandle_t CommTask_GetHandle(void);
void Communication_RequestState(void);
void Communication_UIReady(void);
void Communication_SendBootInfo(void);
void Communication_SendWiFiCredentials(const char *ssid, const char *password);
bool Display_ApplyCtrlState(const ControlBoard_Message_State &st);

#endif
