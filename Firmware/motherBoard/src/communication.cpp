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
  log_i("Communication initialized");

  ctrl_msg_alarm.id = -1;

  // Create the FreeRTOS communication task
  while (xTaskCreatePinnedToCore(
             Communication_Task,
             "COMM_TASK",
             4096,
             NULL,
             1,
             NULL,
             1) != pdPASS)
  {
    log_i("Retrying COMM task creation...");
    delay(100);
  }

  log_i("Communication task successfully created!");
}

// ======================
//  MAIN COMM TASK
// ======================
void Communication_Task(void *pvParameters) {
  const TickType_t period = pdMS_TO_TICKS(1000);  // 1 second
  TickType_t lastWakeTime = xTaskGetTickCount();

  for (;;) {

    // Handle incoming messages
    if (COMM_SERIAL.available()) {
      ReceiveMessageFromOtherESP();
    }

    // Send telemetry every second
    SendTelemetry();

    // Send alarm only once
    if (ctrl_msg_alarm.id >= 0) {
      SendAlarm();
      ctrl_msg_alarm.id = -1;  // clear after sending
    }

    vTaskDelayUntil(&lastWakeTime, period);
  }
}

// ======================
//  SEND TELEMETRY
// ======================
// Format: CTRL,TEL,AirTemp,SkinTemp,Humidity
void SendTelemetry() {
#if !IS_HMI
  COMM_SERIAL.printf("CTRL,TEL,%.1f,%.1f,%d\n",
                     ctrl_tel_msg.detectedAirTemperature,
                     ctrl_tel_msg.detectedSkinTemperature,
                     (int)ctrl_tel_msg.detectedHumidity);
#else
  // HMI does not send telemetry
#endif
}

// ======================
//  SEND ALARM
// ======================
// Format: CTRL,ALM,id,type,description,state
void SendAlarm() {
#if !IS_HMI
  COMM_SERIAL.printf("CTRL,ALM,%d,%s,%s,%d\n",
                     ctrl_msg_alarm.id,
                     ctrl_msg_alarm.type,
                     ctrl_msg_alarm.description,
                     ctrl_msg_alarm.state ? 1 : 0);
#endif
}

// ======================
//  RECEIVE + FILTER
// ======================
bool ReceiveMessageFromOtherESP() {
  while (COMM_SERIAL.available()) {

    char c = COMM_SERIAL.read();

    if (c == '\r')
      continue;  // ignore CR

    // If it's the first character of a new message,
    // throw away anything not matching the expected prefix.
    if (rxBuffer.length() == 0) {
      if (c != EXPECTED_PREFIX[0]) {
        // Ignore entire line until newline
        while (COMM_SERIAL.available() && COMM_SERIAL.read() != '\n');
        return false;
      }
    }

    if (c == '\n') {

      // discard if prefix is wrong (robust validation)
      if (!rxBuffer.startsWith(EXPECTED_PREFIX)) {
        rxBuffer = "";
        return false;
      }

      // ==========================
      // HMI RECEIVES CONTROL MESSAGES
      // ==========================
#if IS_HMI
      if (rxBuffer.startsWith("CTRL,TEL")) {

        int result = sscanf(rxBuffer.c_str(),
                            "CTRL,TEL,%lf,%lf,%lf",
                            &ctrl_tel_msg.detectedAirTemperature,
                            &ctrl_tel_msg.detectedSkinTemperature,
                            &ctrl_tel_msg.detectedHumidity);

        if (result == 3) {
          error = false;
        } else {
          log_e("HMI failed to parse CTRL,TEL");
        }
      }

      else if (rxBuffer.startsWith("CTRL,ALM")) {

        int id;
        char type[ALARM_TYPE_LEN];
        char description[ALARM_DESC_LEN];
        int stateInt;

        int result = sscanf(rxBuffer.c_str(),
                            "CTRL,ALM,%d,%[^,],%[^,],%d",
                            &id, type, description, &stateInt);

        if (result == 4) {
          ctrl_msg_alarm.id = id;
          strncpy(ctrl_msg_alarm.type, type, ALARM_TYPE_LEN);
          ctrl_msg_alarm.type[ALARM_TYPE_LEN - 1] = '\0';

          strncpy(ctrl_msg_alarm.description, description, ALARM_DESC_LEN);
          ctrl_msg_alarm.description[ALARM_DESC_LEN - 1] = '\0';

          ctrl_msg_alarm.state = (stateInt != 0);
        } 
        else {
          log_e("HMI failed to parse CTRL,ALM");
        }
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
        }
        else {
          log_e("CTRL failed to parse HMI message");
        }
      }
#endif

      rxBuffer = "";
      return true;
    }

    else {
      rxBuffer += c;
    }
  }

  return false;
}
