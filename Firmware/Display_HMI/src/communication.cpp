#include "communication.h"

HMI_Message hmi_msg;
ControlBoard_Message ctrl_msg;
ControlBoard_Message_Telemetry ctrl_tel_msg;
ControlBoard_Message_Alarm ctrl_msg_alarm;

bool error = false;

static String rxBuffer = "";

// ======================
//  INITIALIZATION
// ======================
void Communication_Init() {
  COMM_SERIAL.begin(115200);
  delay(200);
  COMM_LOG("[COMM] Initialized\n");

  ctrl_msg_alarm.id = -1;

  // Create the FreeRTOS communication task
  while (xTaskCreatePinnedToCore(Communication_Task, "COMM_TASK", 4096, NULL, 1,
                                 NULL, 1) != pdPASS) {
    COMM_LOG("[COMM] Retrying COMM task creation...\n");
    delay(100);
  }

  COMM_LOG("[COMM] Task successfully created!\n");
}

// ======================
//  MAIN COMM TASK
// ======================
void Communication_Task(void *pvParameters) {
  const TickType_t period = pdMS_TO_TICKS(1); // run every 1 ms

  for (;;) {

    // Handle incoming messages
    if (COMM_SERIAL.available()) {
      ReceiveMessageFromOtherESP();
    }

    // Send telemetry (internally rate-limited)
    SendTelemetry();

    // Send alarm only once
    if (ctrl_msg_alarm.id >= 0) {
      SendAlarm();
      ctrl_msg_alarm.id = -1; // clear after sending
    }

    vTaskDelay(period);
  }
}

// ======================
//  SEND TELEMETRY
// ======================
void SendTelemetry() {
#if !IS_HMI
  // Static variable to keep track of last telemetry send
  static uint32_t lastSendMs = 0;
  uint32_t now = millis();

  // Send only every 1000 ms
  if (now - lastSendMs >= 1000) {
    lastSendMs = now;

    COMM_SERIAL.printf("CTRL,TEL,%.1f,%.1f,%d\n",
                       ctrl_tel_msg.detectedAirTemperature,
                       ctrl_tel_msg.detectedSkinTemperature,
                       (int)ctrl_tel_msg.detectedHumidity);
  }

#else
  // HMI sends only when requested
  if (hmi_msg.shouldSendData) {
    COMM_SERIAL.printf("HMI,%d,%d,%.1f,%.1f,%.0f,%d,%d\n", hmi_msg.actuation,
                       hmi_msg.controlMode, hmi_msg.desiredAirTemperature,
                       hmi_msg.desiredSkinTemperature, hmi_msg.desiredHumidity,
                       hmi_msg.phototherapyMode, hmi_msg.muteAlarm);
    hmi_msg.shouldSendData = false;
  }
#endif
}

// ======================
//  SEND ALARM
// ======================
void SendAlarm() {
#if !IS_HMI
  COMM_SERIAL.printf("CTRL,ALM,%d,%s,%s,%d\n", ctrl_msg_alarm.id,
                     ctrl_msg_alarm.type, ctrl_msg_alarm.description,
                     ctrl_msg_alarm.state ? 1 : 0);
#endif
}

// ======================
//  RECEIVE + FILTER
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
          Serial.println(ctrl_tel_msg.detectedAirTemperature);
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