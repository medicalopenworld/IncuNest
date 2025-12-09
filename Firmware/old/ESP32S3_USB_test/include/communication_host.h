#ifndef COMMUNICATION_HOST_H
#define COMMUNICATION_HOST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ===========================
// MESSAGE STRUCTURES
// ===========================
typedef struct {
    double detectedAirTemperature;
    double detectedSkinTemperature;
    double detectedHumidity;
} TelemetryMessage;

typedef struct {
    int id;
    char type[32];
    char description[128];
    int state;
} AlarmMessage;

typedef struct {
    int actuation;
    int controlMode;
    double desiredAirTemperature;
    double desiredSkinTemperature;
    double desiredHumidity;
    int phototherapyMode;
    int muteAlarm;
} HMIMessage;

// ===========================
// EXTERN GLOBALS
// ===========================
extern TelemetryMessage host_tel_msg;
extern AlarmMessage     host_alarm_msg;
extern HMIMessage       host_hmi_msg;

// ===========================
// API
// ===========================
void CommunicationHost_Init();  
bool CommunicationHost_ReceiveBytes(const uint8_t *data, size_t len, void *arg);
void CommunicationHost_Send(const char *msg);

#endif
