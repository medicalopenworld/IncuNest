#include "CommTask.h"
#include "UITask.h"
#include "esp_log.h"
#include "main.h"
#include "ui.h"
#include <string.h>

static const char *TAG = "CommTask";

// --- Global variables (moved from communication.cpp) ---
HMI_Message hmi_msg;
ControlBoard_Message ctrl_msg;
ControlBoard_Message_Telemetry ctrl_tel_msg;
ControlBoard_Message_Alarm ctrl_msg_alarm;
ControlBoard_Message_State ctrl_state_msg = {0, 0, 0, 0, 0, 0, 0, false};

bool error = false;
static String rxBuffer = "";

// --- Shared flags/state (from UITask.cpp) ---
extern bool tempSwitched;
extern bool alarmsMuted;
extern bool g_stateSynced;
extern uint32_t g_lastStateReqMs;

// ======================
//  LOW-LEVEL COMMS
// ======================

void Communication_RequestState(void) {
#if IS_HMI
  COMM_SERIAL.print("HMI,REQ,STATE\n");
#endif
}

static void SendMessageToOtherESP() {
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

static bool ReceiveMessageFromOtherESP() {
  static uint32_t lastCharTime = 0;
  const uint32_t RX_TIMEOUT_MS = 1000;

  if (rxBuffer.length() > 0 && (millis() - lastCharTime) > RX_TIMEOUT_MS) {
    COMM_LOG("[COMM] RX timeout, clearing buffer (content was: %s)\n",
             rxBuffer.c_str());
    rxBuffer = "";
  }

  while (COMM_SERIAL.available()) {
    char c = COMM_SERIAL.read();
    lastCharTime = millis();

    if (c == '\r')
      continue;

    if (rxBuffer.length() == 0) {
      if (c == '\n' || c == ' ')
        continue;
      if (c != EXPECTED_PREFIX[0]) {
        while (COMM_SERIAL.available() && COMM_SERIAL.read() != '\n')
          ;
        return false;
      }
    }

    if (c == '\n') {
      if (!rxBuffer.startsWith(EXPECTED_PREFIX)) {
        rxBuffer = "";
        return false;
      }

#if IS_HMI
      if (rxBuffer.startsWith("CTRL,TEL")) {
        int result = sscanf(rxBuffer.c_str(), "CTRL,TEL,%lf,%lf,%lf",
                            &ctrl_tel_msg.detectedAirTemperature,
                            &ctrl_tel_msg.detectedSkinTemperature,
                            &ctrl_tel_msg.detectedHumidity);
        if (result != 3) {
          COMM_LOG("[COMM] HMI failed to parse CTRL,TEL: %s\n",
                   rxBuffer.c_str());
        }
      } else if (rxBuffer.startsWith("CTRL,STATE")) {
        int act, mode, photo, mute;
        double airSet, skinSet, humSet;
        int result =
            sscanf(rxBuffer.c_str(), "CTRL,STATE,%d,%d,%lf,%lf,%lf,%d,%d", &act,
                   &mode, &airSet, &skinSet, &humSet, &photo, &mute);
        if (result == 7) {
          ctrl_state_msg.actuation = act;
          ctrl_state_msg.controlMode = mode;
          ctrl_state_msg.desiredAirTemperature = airSet;
          ctrl_state_msg.desiredSkinTemperature = skinSet;
          ctrl_state_msg.desiredHumidity = humSet;
          ctrl_state_msg.phototherapyMode = photo;
          ctrl_state_msg.muteAlarm = mute;
          ctrl_state_msg.newState = true;
        } else {
          COMM_LOG("[COMM] HMI failed to parse CTRL,STATE: %s\n",
                   rxBuffer.c_str());
        }
      } else if (rxBuffer.startsWith("CTRL,ALM")) {
        int id, stateInt;
        char type[ALARM_TYPE_LEN];
        char description[ALARM_DESC_LEN];
        int result = sscanf(rxBuffer.c_str(), "CTRL,ALM,%d,%[^,],%[^,],%d", &id,
                            type, description, &stateInt);
        if (result == 4) {
          ctrl_msg_alarm.id = id;
          strncpy(ctrl_msg_alarm.type, type, ALARM_TYPE_LEN);
          ctrl_msg_alarm.type[ALARM_TYPE_LEN - 1] = '\0';
          strncpy(ctrl_msg_alarm.description, description, ALARM_DESC_LEN);
          ctrl_msg_alarm.description[ALARM_DESC_LEN - 1] = '\0';
          ctrl_msg_alarm.state = (stateInt != 0);
        } else {
          COMM_LOG("[COMM] HMI failed to parse CTRL,ALM: %s\n",
                   rxBuffer.c_str());
        }
      }
#endif
      rxBuffer = "";
      return true;
    }
    rxBuffer += c;
  }
  return false;
}

// ======================
//  HIGH-LEVEL LOGIC
// ======================

static void Display_ApplyCtrlState(const ControlBoard_Message_State &st) {
  ui_set_switch_state_silent(ui_Switch1, st.actuation & 0x01);
  ui_set_switch_state_silent(ui_Switch2, (st.actuation >> 1) & 0x01);
  ui_set_switch_state_silent(ui_Switch3, st.controlMode);
  ui_set_switch_state_silent(ui_Switch4, st.phototherapyMode);

  // airTempValue = st.desiredAirTemperature;
  // skinTempValue = st.desiredSkinTemperature;
  // humValue = (int)st.desiredHumidity;

  update_labels();
}

static void Display_StateSync_Service(void) {
  if (g_stateSynced)
    return;
  uint32_t now = millis();
  if (now - g_lastStateReqMs >= 500) {
    Communication_RequestState();
    g_lastStateReqMs = now;
  }
  if (ctrl_state_msg.newState) {
    ctrl_state_msg.newState = false;
    Display_ApplyCtrlState(ctrl_state_msg);
    g_stateSynced = true;
  }
}

static void applyHMIData() {
  airTempValueDetected = ctrl_tel_msg.detectedAirTemperature;
  skinTempValueDetected = ctrl_tel_msg.detectedSkinTemperature;
  humValueDetected = (int)ctrl_tel_msg.detectedHumidity;
  update_labels();
  if (tempSwitched) {
    chart_add_air_temp((float)airTempValueDetected);
    chart_add_skin_temp((float)skinTempValueDetected);
  }
  chart_add_hum_value((float)humValueDetected);
}

static void processReceivedAlarm(const ControlBoard_Message_Alarm &alarm) {
  int idxById = -1;
  for (int i = 0; i < MAX_ALARMS; i++) {
    if (alarmList[i].id == alarm.id) {
      idxById = i;
      break;
    }
  }
  int index = idxById;
  if (index == -1) {
    for (int i = 0; i < MAX_ALARMS; i++) {
      if (alarmList[i].state == false) {
        index = i;
        break;
      }
    }
  }
  if (index == -1)
    return;

  bool prevState = (idxById != -1) ? alarmList[index].state : false;
  bool isNewAlarm = (idxById == -1) || (!prevState && alarm.state);

  alarmList[index].id = alarm.id;
  strncpy(alarmList[index].type, alarm.type, ALARM_TYPE_LEN);
  alarmList[index].type[ALARM_TYPE_LEN - 1] = '\0';
  strncpy(alarmList[index].description, alarm.description, ALARM_DESC_LEN);
  alarmList[index].description[ALARM_DESC_LEN - 1] = '\0';
  alarmList[index].state = alarm.state;

  if (isNewAlarm && alarm.state) {
    alarmsMuted = false;
    hmi_msg.muteAlarm = 0;
  }
  update_alarm_panels();
  AlarmSound_Update();
}

void Comm_Task(void *pvParameters) {
  ESP_LOGI(TAG, "Communication Task Started");
  COMM_SERIAL.begin(115200);

  Communication_RequestState();
  g_lastStateReqMs = millis();

  for (;;) {
    Display_StateSync_Service();

    if (ReceiveMessageFromOtherESP()) {
      if (ctrl_msg_alarm.id != 0) {
        processReceivedAlarm(ctrl_msg_alarm);
        ctrl_msg_alarm.id = 0;
        ctrl_msg_alarm.state = false;
      } else if (error == false) {
        applyHMIData();
      }
    }

#if IS_HMI
    if (hmi_msg.shouldSendData) {
      SendMessageToOtherESP();
      hmi_msg.shouldSendData = false;
    }
#endif

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void CreateCommTask() {
  xTaskCreatePinnedToCore(Comm_Task, "Comm", 8192, NULL, 3, NULL,
                          CORE_ID_FREERTOS);
}
