#include "CommTask.h"
#include "UITask.h"
#include "esp_log.h"
#include "main.h"
#include "ui.h"
#include <cstdio>
#include <string.h>

static const char *TAG = "CommTask";

// --- Global variables (moved from communication.cpp) ---
HMI_Message hmi_msg;
ControlBoard_Message ctrl_msg;
ControlBoard_Message_Telemetry ctrl_tel_msg;
ControlBoard_Message_Alarm ctrl_msg_alarm;
ControlBoard_Message_State ctrl_state_msg = {0};

bool error = false;
static char rxBuffer[512];
static int rxIndex = 0;

// --- Phototherapy Timer (from UITask.cpp) ---
extern int photoTimerMinutes;
extern bool photoTimerActive;
extern unsigned long photoTimerStartMs;

// --- Shared flags/state (from UITask.cpp) ---
extern bool tempSwitched;
extern bool alarmsMuted;
extern bool g_stateSynced;
extern uint32_t g_lastStateReqMs;
extern int selectedPanel;
extern int lastSelectedPanel;
extern bool skinPanelEnabled;
extern bool g_ui_initialized;

// ======================
//  LOW-LEVEL COMMS
// ======================

void Communication_RequestState(void) {
#if IS_HMI
  COMM_SERIAL.print("HMI,REQ,STATE\n");
#endif
}

void Communication_SendWiFiCredentials(const char *ssid, const char *password) {
#if IS_HMI
  COMM_SERIAL.printf("HMI,WIFI,%s,%s\n", ssid, password);
#endif
}

static void SendMessageToOtherESP() {
#if IS_HMI
  COMM_SERIAL.printf(
      "HMI,%d,%d,%d,%0.2f,%0.2f,%0.0f,%d,%d,%d,%d\n", hmi_msg.actuation,
      (int)hmi_msg.skinModeEnabled, hmi_msg.controlMode,
      hmi_msg.desiredAirTemperature, hmi_msg.desiredSkinTemperature,
      hmi_msg.desiredHumidity, hmi_msg.phototherapyMode, hmi_msg.muteAlarm,
      hmi_msg.language, hmi_msg.photoMinutesRemaining);
#else
  COMM_SERIAL.printf("CTRL,%0.2f,%0.2f,%0.2f,%0.2f,%0.2f\n",
                     ctrl_msg.temperature[0], ctrl_msg.temperature[1],
                     ctrl_msg.temperature[2], ctrl_msg.humidity[0],
                     ctrl_msg.humidity[1]);
#endif
}

static void parse_message(const char *line) {
#if IS_HMI
  if (strncmp(line, "CTRL,TEL", 8) == 0) {
    int result = sscanf(
        line, "CTRL,TEL,%lf,%lf,%lf,%d", &ctrl_tel_msg.detectedAirTemperature,
        &ctrl_tel_msg.detectedSkinTemperature, &ctrl_tel_msg.detectedHumidity,
        &ctrl_tel_msg.serverCommStatus);
    if (result < 3) {
      // COMM_LOG("[COMM] HMI failed to parse CTRL,TEL: %s\n", line);
    }
    if (result == 3) {
      // Backward compatibility or partial parse
      ctrl_tel_msg.serverCommStatus = COMM_STATUS_NONE;
    }
  } else if (strncmp(line, "CTRL,STATE", 10) == 0) {
    int act, mode, photo, mute, sn, hwNum, numAlarms, skinE, commStatus, lang;
    double photoTimeRemaining;  // Formato MM.SS (ej: 18.33 = 18 min 33 seg)
    char hwRev;
    char fwVer[20];
    double airSet, skinSet, humSet;
    int result =
        sscanf(line, "CTRL,STATE,%d,%d,%lf,%lf,%lf,%d,%d,%d,%d,%c,%19[^,],%d,%d,%d,%lf,%d",
               &act, &mode, &airSet, &skinSet, &humSet, &photo, &mute,
               &sn, &hwNum, &hwRev, fwVer, &numAlarms, &skinE, &commStatus, &photoTimeRemaining, &lang);
    
    // Accept 12 (old), 13 (with alarms), 14 (with alarms + skinModeEnabled), or 15 (with photoTimeRemaining)
    if (result >= 12) {
      ctrl_state_msg.actuation = act;
      ctrl_state_msg.controlMode = mode;
      ctrl_state_msg.desiredAirTemperature = airSet;
      ctrl_state_msg.desiredSkinTemperature = skinSet;
      ctrl_state_msg.desiredHumidity = humSet;
      ctrl_state_msg.phototherapyMode = photo;
      ctrl_state_msg.muteAlarm = mute;
      if (result >= 16) ctrl_state_msg.language = lang;
      ctrl_state_msg.serialNumber = sn;
      ctrl_state_msg.hwNum = hwNum;
      ctrl_state_msg.hwRev[0] = hwRev;
      ctrl_state_msg.hwRev[1] = '\0';
      strncpy(ctrl_state_msg.fwVer, fwVer, sizeof(ctrl_state_msg.fwVer));
      ctrl_state_msg.skinModeEnabled = (result >= 14) ? skinE : (mode == CONTROL_SKIN);
      ctrl_state_msg.serverCommStatus = (result >= 15) ? commStatus : 0;
      
      // Extract minutes and seconds from MM.SS format
      if (result >= 15) {
        int mins = (int)photoTimeRemaining;
        int secs = (int)((photoTimeRemaining - mins) * 100.0 + 0.5);  // Round to nearest second
        ctrl_state_msg.photoMinutesRemaining = mins;
        ctrl_state_msg.photoSecondsRemaining = secs;
      } else {
        ctrl_state_msg.photoMinutesRemaining = 0;
        ctrl_state_msg.photoSecondsRemaining = 0;
      }
      
      ctrl_state_msg.newState = true;
      
      // If we have alarms count (result == 13), we could use it for verification
      // but individual alarm messages will follow anyway.
    } else {
      COMM_LOG("[COMM] HMI failed to parse CTRL,STATE: %s\n", line);
    }
  } else if (strncmp(line, "CTRL,ALM", 8) ==
             0) { // Fix bug: was "CTRL,ALM" len 8, logic matches
    int id, stateInt;
    char type[ALARM_TYPE_LEN];
    char description[ALARM_DESC_LEN];
    // Use width limits in sscanf to prevent buffer overflow
    // ALARM_TYPE_LEN is typically small, we assume 20-30 chars.
    // We'll use %31[^,] as a safe approximation if LEN is 32.
    // Ideally we should use macros but for now we hardcode a safe limit.
    int result = sscanf(line, "CTRL,ALM,%d,%31[^,],%31[^,],%d", &id, type,
                        description, &stateInt);
    if (result == 4) {
      ctrl_msg_alarm.id = id;
      strncpy(ctrl_msg_alarm.type, type, ALARM_TYPE_LEN);
      ctrl_msg_alarm.type[ALARM_TYPE_LEN - 1] = '\0';
      strncpy(ctrl_msg_alarm.description, description, ALARM_DESC_LEN);
      ctrl_msg_alarm.description[ALARM_DESC_LEN - 1] = '\0';
      ctrl_msg_alarm.state = (stateInt != 0);
    } else {
      COMM_LOG("[COMM] HMI failed to parse CTRL,ALM: %s\n", line);
    }
  }
#endif
}

static bool ReceiveMessageFromOtherESP() {
  bool msgReceived = false;
  static uint32_t lastRxTime = 0;

  while (COMM_SERIAL.available()) {
    // Timeout check: if buffer has data but no new char for >50ms, clear it
    if (rxIndex > 0 && (millis() - lastRxTime > 50)) {
      rxIndex = 0;
      COMM_LOG("[COMM] RX Timeout, buffer cleared\n");
    }

    char c = COMM_SERIAL.read();
    lastRxTime = millis(); // Update time after receiving char

    if (c == '\r')
      continue;

    if (c == '\n') {
      rxBuffer[rxIndex] = '\0'; // Null-terminate
      if (rxIndex > 0) {
        if (strncmp(rxBuffer, EXPECTED_PREFIX, strlen(EXPECTED_PREFIX)) == 0) {
          parse_message(rxBuffer);
          msgReceived = true;
        }
      }
      rxIndex = 0; // Reset buffer
    } else {
      if (rxIndex < sizeof(rxBuffer) - 1) {
        rxBuffer[rxIndex++] = c;
      } else {
        // Buffer overflow protection: overflowed, reset and ignore this line
        rxIndex = 0;
        COMM_LOG("[COMM] Buffer overflow, line too long\n");
      }
    }
  }
  return msgReceived;
}

// ======================
//  HIGH-LEVEL LOGIC
// ======================

static bool Display_ApplyCtrlState(const ControlBoard_Message_State &st) {
  if (!g_ui_initialized)
    return false;
  ui_set_switch_state_silent(ui_Switch1, st.actuation & 0x01);
  ui_set_switch_state_silent(ui_Switch2, (st.actuation >> 1) & 0x01);
  ui_set_switch_state_silent(ui_Switch3, st.phototherapyMode);
  ui_set_switch_state_silent(ui_Switch4, st.skinModeEnabled);

  skinPanelEnabled = st.skinModeEnabled; // Ensure sync with UITask global
  if (skinPanelEnabled) {
    if (ui_SkinPanelCont) lv_obj_clear_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);
  } else {
    if (ui_SkinPanelCont) lv_obj_add_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);
    lastSelectedPanel = AIR_PANEL_SELECTED;
  }

  /*
  if (st.language != g_lang) {
    // Only update if different to avoid loop, but Applying Language is safe
    // here
    UI_ApplyLanguage((ui_lang_t)st.language);
    if (ui_LanguagesDropDown) {
      lv_dropdown_set_selected(ui_LanguagesDropDown, st.language);
    }
  }
  */

  if (st.desiredAirTemperature > 0.1)
    airTempValue = st.desiredAirTemperature;
  if (st.desiredSkinTemperature > 0.1)
    skinTempValue = st.desiredSkinTemperature;
  if (st.language != (int)g_lang) {
    // Only update if it's a valid change to avoid loops
  }

  // Sync internal hmi_msg to avoid sending "all OFF" on next user action
  hmi_msg.actuation = st.actuation;
  hmi_msg.controlMode = st.controlMode;
  hmi_msg.desiredAirTemperature = airTempValue;
  hmi_msg.desiredSkinTemperature = skinTempValue;
  hmi_msg.desiredHumidity = humValue;
  hmi_msg.phototherapyMode = st.phototherapyMode;
  hmi_msg.muteAlarm = st.muteAlarm;
  // hmi_msg.language = st.language; // REMOVIDO: No sobrescribir idioma local
  hmi_msg.skinModeEnabled = st.skinModeEnabled;
  hmi_msg.photoMinutesRemaining = st.photoMinutesRemaining;

  // Restore Phototherapy Timer if active in received state
  if (st.phototherapyMode && (st.photoMinutesRemaining > 0 || st.photoSecondsRemaining > 0)) {
      if (!photoTimerActive) {
          photoTimerActive = true;
          
          // Calculate total seconds remaining and elapsed
          long totalSecondsRemaining = st.photoMinutesRemaining * 60 + st.photoSecondsRemaining;
          long totalSecondsOriginal = photoTimerMinutes * 60;
          long elapsedSeconds = totalSecondsOriginal - totalSecondsRemaining;
          
          // Adjust start time to reflect elapsed time
          photoTimerStartMs = millis() - (elapsedSeconds * 1000);
          
          // Sync to avoid sending 0 back
          hmi_msg.photoMinutesRemaining = st.photoMinutesRemaining;
          
          ESP_LOGI(TAG, "Phototherapy timer restored: %d:%02d remaining", 
                   st.photoMinutesRemaining, st.photoSecondsRemaining);
      }
  }

  if (st.controlMode == CONTROL_SKIN) {
    selectedPanel = SKIN_PANEL_SELECTED;
    lastSelectedPanel = SKIN_PANEL_SELECTED;
    // If we are in Skin mode, the enabler switch MUST be ON
    if (ui_Switch4) lv_obj_add_state(ui_Switch4, LV_STATE_CHECKED);
  } else {
    selectedPanel = AIR_PANEL_SELECTED;
    lastSelectedPanel = AIR_PANEL_SELECTED;
    // We don't force ui_Switch4 to OFF here, as it might be enabled but in AIR mode.
    // However, the user said "by default it should be OFF". 
    // This will be handled by the initial state and MB default controlMode (AIR).
  }

  if (st.serialNumber != 0 && st.serialNumber != in3.serialNumber) {
    in3.serialNumber = st.serialNumber;
    EEPROM.writeInt(EEPROM_SERIAL_NUMBER, in3.serialNumber);
    EEPROM.commit();
    ESP_LOGI(TAG, "Serial Number updated from motherboard: %d",
             in3.serialNumber);
  }

  UI_SyncAll();
  return true;
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
    if (Display_ApplyCtrlState(ctrl_state_msg)) {
      ctrl_state_msg.newState = false;
      g_stateSynced = true;
      hmi_msg.shouldSendData = true; // Forzar envío inicial para sincronizar idioma y setpoints locales
    }
  }
}

static void applyHMIData() {
  if (!g_ui_initialized)
    return;
  airTempValueDetected = ctrl_tel_msg.detectedAirTemperature;
  skinTempValueDetected = ctrl_tel_msg.detectedSkinTemperature;
  humValueDetected = (int)ctrl_tel_msg.detectedHumidity;
  update_labels();
  if (tempSwitched) {
    chart_add_air_temp((float)airTempValueDetected);
    chart_add_skin_temp((float)skinTempValueDetected);
  }
  chart_add_hum_value((float)humValueDetected);
  chart_save_history();
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
  if (!g_ui_initialized)
    return;
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