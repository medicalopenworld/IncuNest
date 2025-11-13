#pragma once
#include <Arduino.h>
#include "main.h"

// UART pins y velocidad
#define UART_BAUDRATE 115200
#define UART_DISPLAY Serial2

// Comandos recibidos desde el display
struct DisplayCommand {
    bool startButtonPressed;
    bool stopButtonPressed;
    float targetTemperatureAir;
    float targetTemperatureSkin;
    float targetHumidity;
};

// Datos enviados al display
struct DisplayMessage {
    float temperatureAir;
    float temperatureSkin;
    float humidity;
    bool alarmActive;
    bool controlMode;
    bool phototherapyOn;
};

// Inicialización UART
void initCommunication();

// Leer comando del display
bool readDisplayCommand(DisplayCommand *cmd);

// Enviar datos al display
void sendDisplayMessage(const DisplayMessage *msg);
