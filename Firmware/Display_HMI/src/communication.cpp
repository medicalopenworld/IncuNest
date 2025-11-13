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
  COMM_SERIAL.printf("HMI,%d,%d,%0.2f,%0.2f,%0.0f,%d,%d\n",
                      hmi_msg.actuation,
                      hmi_msg.controlMode,
                      hmi_msg.desiredAirTemperature,
                      hmi_msg.desiredSkinTemperature,
                      hmi_msg.desiredHumidity,
                      hmi_msg.phototherapyMode,
                      hmi_msg.muteAlarm);
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
        // Debug: Mostrar el buffer recibido
        //Serial.print("Raw buffer: ");
        //Serial.println(rxBuffer);
        
        int result = sscanf(rxBuffer.c_str(), "HMI,%d,%d,%lf,%lf,%lf,%d,%d",
              &hmi_msg.actuation,
              &hmi_msg.controlMode,
              &hmi_msg.desiredAirTemperature,
              &hmi_msg.desiredSkinTemperature,
              &hmi_msg.desiredHumidity,
              &hmi_msg.phototherapyMode,
              &hmi_msg.muteAlarm);
        
        // Debug: Mostrar si el parsing fue exitoso
        //Serial.print("Parse result: ");
        //Serial.println(result);
        
        if (result == 7) {  // 7 expected fields
          Serial.println("✓ HMI data parsed successfully");
          log_i("Received HMI data");
        } else {
          Serial.println("✗ HMI parsing FAILED");
          rxBuffer = "";
          return false;
        }
        
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
