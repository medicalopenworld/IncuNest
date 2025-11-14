#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <Arduino.h>
#include "main.h"

// Cambia esto según el firmware
#define IS_HMI true   // Cambia a false para la placa de control

#define COMM_SERIAL Serial

// ======================
//  ESTRUCTURAS DE DATOS
// ======================
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

typedef struct {
  double temperature[3];
  double humidity[2];
  bool shouldSendData;   
} ControlBoard_Message;

typedef struct {
  int id;
  char type[ALARM_TYPE_LEN];
  char description[ALARM_DESC_LEN];
  bool state;  
} ControlBoard_Message_Alarm;

// ======================
//  VARIABLES GLOBALES
// ======================
extern HMI_Message hmi_msg;
extern ControlBoard_Message ctrl_msg;
extern ControlBoard_Message ctrl_msg;
extern ControlBoard_Message_Alarm ctrl_msg_alarm;
// ======================
//  FUNCIONES PÚBLICAS
// ======================
void Communication_Init();
void Communication_Task(void *pvParameters);
void SendMessageToOtherESP();
bool ReceiveMessageFromOtherESP();

#endif
