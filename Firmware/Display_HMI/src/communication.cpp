#include "communication.h"

HMI_Message hmi_msg;
ControlBoard_Message ctrl_msg;
ControlBoard_Message_Telemetry ctrl_tel_msg;
ControlBoard_Message_Alarm ctrl_msg_alarm;
ControlBoard_Message_State ctrl_state_msg = {0,0,0,0,0,0,0,false};


bool error = false;

static String rxBuffer = "";

// ======================
//  INITIALIZATION
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

// ---- Request STATE ----
void Communication_RequestState(void) {
#if IS_HMI
  COMM_SERIAL.print("HMI,REQ,STATE\n");
#endif
}


// ======================
//  TRANSMISSION
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
//  MAIN TASK
// ======================
void Communication_Task(void *pvParameters) {
  const TickType_t period = pdMS_TO_TICKS(200);  // 200 ms
  TickType_t lastWakeTime = xTaskGetTickCount();

  for (;;) {
    if (COMM_SERIAL.available()) {
      ReceiveMessageFromOtherESP();
    }

#if IS_HMI
    // Only send if there's new data to send
    if (hmi_msg.shouldSendData) {
      SendMessageToOtherESP();
      hmi_msg.shouldSendData = false;
    }
#else
    // Send data every 200ms or if there's new data to send
    if (ctrl_msg.shouldSendData) {
      SendMessageToOtherESP();
      ctrl_msg.shouldSendData = false;
    } else {
      // Send in every cycle
      SendMessageToOtherESP();
    }
#endif

    vTaskDelayUntil(&lastWakeTime, period);
  }
}

// ======================
//  RECEPTION
// ======================
bool ReceiveMessageFromOtherESP() {

  // --- UART RX timeout system ---
  static uint32_t lastCharTime = 0;
  const uint32_t RX_TIMEOUT_MS = 1000;   // recommended between 20–50 ms

  // If buffer has partial data but no bytes received recently → reset buffer
  if (rxBuffer.length() > 0 && (millis() - lastCharTime) > RX_TIMEOUT_MS) {
    COMM_LOG("[COMM] RX timeout, clearing buffer (content was: %s)\n", rxBuffer.c_str());
    rxBuffer = "";
  }

  while (COMM_SERIAL.available()) {

    char c = COMM_SERIAL.read();
    lastCharTime = millis();   // update timestamp on every char

    // Ignore CR
    if (c == '\r')
      continue;

    //
    // START OF A NEW MESSAGE
    //
    if (rxBuffer.length() == 0) {

      // Ignore blank characters
      if (c == '\n' || c == ' ')
        continue;

      // Accept first char ONLY if it matches prefix (CTRL or HMI)
      if (c != EXPECTED_PREFIX[0]) {
        // Bad start → discard entire line
        while (COMM_SERIAL.available() && COMM_SERIAL.read() != '\n');
        return false;
      }
    }

    //
    // END OF LINE → PARSE
    //
    if (c == '\n') {

      // Must begin with expected prefix
      if (!rxBuffer.startsWith(EXPECTED_PREFIX)) {
        rxBuffer = "";
        return false;
      }

      // ==========================
      // HMI RECEIVES CONTROL MESSAGES
      // ==========================
#if IS_HMI
      if (rxBuffer.startsWith("CTRL,TEL")) {

        int result = sscanf(rxBuffer.c_str(), "CTRL,TEL,%lf,%lf,%lf",
                            &ctrl_tel_msg.detectedAirTemperature,
                            &ctrl_tel_msg.detectedSkinTemperature,
                            &ctrl_tel_msg.detectedHumidity);

        if (result != 3) {
          COMM_LOG("[COMM] HMI failed to parse CTRL,TEL: %s\n", rxBuffer.c_str());
        }

      } else if (rxBuffer.startsWith("CTRL,STATE")) {

        int act, mode, photo, mute;
        double airSet, skinSet, humSet;

        int result = sscanf(rxBuffer.c_str(),
                            "CTRL,STATE,%d,%d,%lf,%lf,%lf,%d,%d",
                            &act, &mode, &airSet, &skinSet, &humSet, &photo, &mute);

        if (result == 7) {
          ctrl_state_msg.actuation            = act;
          ctrl_state_msg.controlMode          = mode;
          ctrl_state_msg.desiredAirTemperature  = airSet;
          ctrl_state_msg.desiredSkinTemperature = skinSet;
          ctrl_state_msg.desiredHumidity        = humSet;
          ctrl_state_msg.phototherapyMode     = photo;
          ctrl_state_msg.muteAlarm            = mute;
          ctrl_state_msg.newState             = true;
        } else {
          COMM_LOG("[COMM] HMI failed to parse CTRL,STATE: %s\n", rxBuffer.c_str());
        }

      } else if (rxBuffer.startsWith("CTRL,ALM")) {

        int id, stateInt;
        char type[ALARM_TYPE_LEN];
        char description[ALARM_DESC_LEN];

        int result = sscanf(rxBuffer.c_str(),
                            "CTRL,ALM,%d,%[^,],%[^,],%d",
                            &id, type, description, &stateInt);

        if (result == 4) {
          ctrl_msg_alarm.id = id;
          strncpy(ctrl_msg_alarm.type, type, ALARM_TYPE_LEN);
          ctrl_msg_alarm.description[ALARM_DESC_LEN - 1] = '\0';

          strncpy(ctrl_msg_alarm.description, description, ALARM_DESC_LEN);
          ctrl_msg_alarm.description[ALARM_DESC_LEN - 1] = '\0';

          ctrl_msg_alarm.state = (stateInt != 0);
        } else {
          COMM_LOG("[COMM] HMI failed to parse CTRL,ALM: %s\n", rxBuffer.c_str());
        }

      } else {
        COMM_LOG("[COMM] HMI received unknown CTRL msg: %s\n", rxBuffer.c_str());
      }
#endif

      // ==========================
      // CONTROL RECEIVES HMI MESSAGES
      // ==========================
#if !IS_HMI
      if (rxBuffer.startsWith("HMI")) {

        int act, mode, photo, mute;
        double air, skin, hum;

        int result = sscanf(rxBuffer.c_str(),
                            "HMI,%d,%d,%lf,%lf,%lf,%d,%d",
                            &act, &mode, &air, &skin, &hum, &photo, &mute);

        if (result == 7) {
          hmi_msg.actuation = act;
          hmi_msg.controlMode = mode;
          hmi_msg.desiredAirTemperature = air;
          hmi_msg.desiredSkinTemperature = skin;
          hmi_msg.desiredHumidity = hum;
          hmi_msg.phototherapyMode = photo;
          hmi_msg.muteAlarm = mute;
        } else {
          COMM_LOG("[COMM] CTRL failed to parse HMI msg: %s\n", rxBuffer.c_str());
        }
      }
#endif

      rxBuffer = "";
      return true;
    }

    // =================================
    // Build the message
    // =================================
    rxBuffer += c;
  }

  return false;
}
