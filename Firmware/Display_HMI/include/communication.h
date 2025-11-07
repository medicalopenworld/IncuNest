#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <Arduino.h>

// Cambia esto según el firmware
#define IS_HMI true   // Cambia a false para la placa de control

#define COMM_SERIAL Serial

// ======================
//  ESTRUCTURAS DE DATOS
// ======================
typedef struct {
  double desiredControlTemperature;
  double desiredControlHumidity;
  int actuation;
  bool controlMode;
  bool shouldSendData;   // <--- NUEVO
} HMI_Message;

typedef struct {
  double temperature[3];
  double humidity[2];
  bool shouldSendData;   // <--- NUEVO
} ControlBoard_Message;

// ======================
//  VARIABLES GLOBALES
// ======================
extern HMI_Message hmi_msg;
extern ControlBoard_Message ctrl_msg;

// ======================
//  FUNCIONES PÚBLICAS
// ======================
void Communication_Init();
void Communication_Task(void *pvParameters);
void SendMessageToOtherESP();
bool ReceiveMessageFromOtherESP();

#endif
