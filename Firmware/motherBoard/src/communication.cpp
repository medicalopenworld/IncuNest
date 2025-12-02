#include "communication.h"

HMI_Message hmi_msg;
ControlBoard_Message ctrl_msg;
ControlBoard_Message_Telemetry ctrl_tel_msg;
ControlBoard_Message_Alarm ctrl_msg_alarm;

bool error = false;

static String rxBuffer = "";

// ======================
//  INICIALIZACIÓN
// ======================
void Communication_Init() {
  COMM_SERIAL.begin(115200);
  delay(200);
  log_i("Communication initialized");

  while (xTaskCreatePinnedToCore(Communication_Task, "COMM_TASK", 4096, NULL, 1,
                                 NULL, 1) != pdPASS) {
    log_i("Retrying COMM task creation...");
    delay(100);
  }
  log_i("Communication task successfully created!");
}

// ======================
//  TAREA PRINCIPAL
// ======================
void Communication_Task(void *pvParameters) {
  const TickType_t period = pdMS_TO_TICKS(200); // 200 ms
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
  COMM_SERIAL.printf("HMI,%d,%d,%0.2f,%0.2f,%0.0f,%d,%d\n", hmi_msg.actuation,
                     hmi_msg.controlMode, hmi_msg.desiredAirTemperature,
                     hmi_msg.desiredSkinTemperature, hmi_msg.desiredHumidity,
                     hmi_msg.phototherapyMode, hmi_msg.muteAlarm);
#else
  COMM_SERIAL.printf("CTRL,%0.2f,%0.2f,%0.2f,%0.2f,%0.2f\n",
                     ctrl_msg.temperature[0], ctrl_msg.temperature[1],
                     ctrl_msg.temperature[2], ctrl_msg.humidity[0],
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

      // ===========================
      //  MENSAJE: CTRL,ALM
      // ===========================
      if (rxBuffer.startsWith("CTRL,ALM")) {

        int id;
        char type[ALARM_TYPE_LEN];
        char description[ALARM_DESC_LEN];
        int stateInt;

        int result = sscanf(rxBuffer.c_str(), "CTRL,ALM,%d,%[^,],%[^,],%d", &id,
                            type, description, &stateInt);

        if (result == 4) {
          ctrl_msg_alarm.id = id;
          strncpy(ctrl_msg_alarm.type, type, ALARM_TYPE_LEN);
          ctrl_msg_alarm.type[ALARM_TYPE_LEN - 1] = '\0';

          strncpy(ctrl_msg_alarm.description, description, ALARM_DESC_LEN);
          ctrl_msg_alarm.description[ALARM_DESC_LEN - 1] = '\0';

          ctrl_msg_alarm.state = (stateInt != 0);

          log_i("Received ALARM id=%d type=%s state=%d", ctrl_msg_alarm.id,
                ctrl_msg_alarm.type, ctrl_msg_alarm.state);

        } else {
          log_e("Failed to parse CTRL,ALM message");
          rxBuffer = "";
          return false;
        }

      }

      // ===========================
      //  MENSAJE: CTRL,TEL
      // ===========================
      else if (rxBuffer.startsWith("CTRL,TEL")) {

        int result = sscanf(rxBuffer.c_str(), "CTRL,TEL,%lf,%lf,%lf",
                            &ctrl_tel_msg.detectedAirTemperature,
                            &ctrl_tel_msg.detectedSkinTemperature,
                            &ctrl_tel_msg.detectedHumidity);

        if (result == 3) {
          log_i("Received CTRL TEL data");
          error = false;
        } else {
          log_e("CTRL TEL parsing FAILED");
          rxBuffer = "";
          return false;
        }

      } else {
        log_w("ERROR 404, Unknown message: %s", rxBuffer.c_str());
        error = true;
      }

      // limpiar SIEMPRE tras procesar la línea
      rxBuffer = "";
      return true;
    }

    // Si no es \n, acumular caracteres
    else {
      rxBuffer += c;
    }
  }

  return false;
}
