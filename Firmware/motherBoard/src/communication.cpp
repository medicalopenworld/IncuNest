#include "communication.h"

static String rxBuffer = "";

// Inicializa UART de comunicación con el display
void initCommunication() {
    UART_DISPLAY.begin(UART_BAUDRATE, SERIAL_8N1, 16, 17); // RX=16, TX=17
    logI("UART display communication initialized");
}

// Lee comandos enviados desde el display (CMD Start Stop TempAir TempSkin Hum)
bool readDisplayCommand(DisplayCommand *cmd) {
    while (UART_DISPLAY.available()) {
        char c = UART_DISPLAY.read();
        if (c == '\n') {
            if (rxBuffer.startsWith("CMD")) {
                sscanf(rxBuffer.c_str(), "CMD %d %d %f %f %f",
                       (int *)&cmd->startButtonPressed,
                       (int *)&cmd->stopButtonPressed,
                       &cmd->targetTemperatureAir,
                       &cmd->targetTemperatureSkin,
                       &cmd->targetHumidity);
                rxBuffer = "";
                return true;
            }
            rxBuffer = "";
        } else {
            rxBuffer += c;
        }
    }
    return false;
}

// Envía datos al display: TempAir TempSkin Humidity + flags
void sendDisplayMessage(const DisplayMessage *msg) {
    UART_DISPLAY.printf("DATA %.2f %.2f %.2f %d %d %d\n",
                        msg->temperatureAir,
                        msg->temperatureSkin,
                        msg->humidity,
                        msg->alarmActive,
                        msg->controlMode,
                        msg->phototherapyOn);
}
