#include "communication.h"

HMI_Message hmi_msg;
ControlBoard_Message ctrl_msg;
static String rxBuffer = "";

// ======================
//  INICIALIZACIÓN
// ======================
void Communication_Init() {
  COMM_SERIAL.begin(115200);
  delay(200);
  log_i("Communication initialized");

  while (xTaskCreatePinnedToCore(
             Communication_Task,
             "COMM_TASK",
             4096,
             NULL,
             1,
             NULL,
             1) != pdPASS) {
    log_i("Retrying COMM task creation...");
    delay(100);
  }
  log_i("Communication task successfully created!");
}

// ======================
//  TAREA PRINCIPAL
// ======================
void Communication_Task(void *pvParameters) {
  const TickType_t period = pdMS_TO_TICKS(200);  // 200 ms
  TickType_t lastWakeTime = xTaskGetTickCount();

  for (;;) {
    if (COMM_SERIAL.available()) {
      ReceiveMessageFromOtherESP();
    }

#if IS_HMI
    // Solo enviar si hay evento del HMI
    if (hmi_msg.shouldSendData) {
      SendMessageToOtherESP();
      hmi_msg.shouldSendData = false;
    }
#else
    // Enviar datos de sensores cada 200 ms
    if (ctrl_msg.shouldSendData) {
      SendMessageToOtherESP();
      ctrl_msg.shouldSendData = false;
    } else {
      // Por defecto, enviar cada ciclo
      SendMessageToOtherESP();
    }
#endif

    vTaskDelayUntil(&lastWakeTime, period);
  }
}

// ======================
//  ENVÍO
// ======================
void SendMessageToOtherESP() {
#if IS_HMI
  COMM_SERIAL.printf("HMI,%0.2f,%0.2f,%d,%d\n",
                     hmi_msg.desiredControlTemperature,
                     hmi_msg.desiredControlHumidity,
                     hmi_msg.actuation,
                     hmi_msg.controlMode);
#else
  COMM_SERIAL.printf("CTRL,%0.2f,%0.2f,%0.2f,%0.2f,%0.2f\n",
                     ctrl_msg.temperature[0],
                     ctrl_msg.temperature[1],
                     ctrl_msg.temperature[2],
                     ctrl_msg.humidity[0],
                     ctrl_msg.humidity[1]);
#endif
}

// ======================
//  RECEPCIÓN
// ======================
bool ReceiveMessageFromOtherESP() {
  while (COMM_SERIAL.available()) {
    char c = COMM_SERIAL.read();
    if (c == '\n') {
      if (rxBuffer.startsWith("HMI")) {
        sscanf(rxBuffer.c_str(), "HMI,%lf,%lf,%d,%d",
               &hmi_msg.desiredControlTemperature,
               &hmi_msg.desiredControlHumidity,
               &hmi_msg.actuation,
               (int *)&hmi_msg.controlMode);
        log_i("Received HMI data");
      } else if (rxBuffer.startsWith("CTRL")) {
        sscanf(rxBuffer.c_str(), "CTRL,%lf,%lf,%lf,%lf,%lf",
               &ctrl_msg.temperature[0],
               &ctrl_msg.temperature[1],
               &ctrl_msg.temperature[2],
               &ctrl_msg.humidity[0],
               &ctrl_msg.humidity[1]);
        log_i("Received Control Board data");
      }
      rxBuffer = "";
      return true;
    } else {
      rxBuffer += c;
    }
  }
  return false;
}
