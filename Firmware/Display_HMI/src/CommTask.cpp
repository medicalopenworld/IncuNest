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
ControlBoard_Message_PPG ctrl_ppg_msg     = {0, false};
ControlBoard_Message_VIT ctrl_vit_msg     = {0, 0, false};
int g_skinProbeState = SKIN_PROBE_NOT_CONNECTED; // Last received skin probe state

// --- Power Off countdown state (written here, read by UITask) ---
volatile bool g_pwrOffActive = false;
volatile int  g_pwrOffRemainingMs = 0;

bool error = false;
static char rxBuffer[COMM_RX_BUFFER_SIZE];
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

void Communication_UIReady(void) {
#if IS_HMI
  COMM_SERIAL.print("HMI,UI_READY\n");
#endif
}

void Communication_SendBootInfo(void) {
#if IS_HMI
  extern uint32_t g_hmiBootCount;
  extern int      g_hmiLastRst;
  COMM_SERIAL.printf("HMI,BOOT,%d,%u\n", g_hmiLastRst,
                     (unsigned)g_hmiBootCount);
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
      "HMI,%d,%d,%d,%0.2f,%0.2f,%0.0f,%d,%d,%d,%d,%d,%d,%d\n", hmi_msg.actuation,
      (int)hmi_msg.skinModeEnabled, hmi_msg.controlMode,
      hmi_msg.desiredAirTemperature, hmi_msg.desiredSkinTemperature,
      hmi_msg.desiredHumidity, hmi_msg.phototherapyMode, hmi_msg.muteAlarm,
      hmi_msg.language, hmi_msg.photoMinutesRemaining,
      hmi_msg.babyWeightGrams, hmi_msg.babyGestWeeks, hmi_msg.babyAgeDays);
#else
  COMM_SERIAL.printf("CTRL,%0.2f,%0.2f,%0.2f,%0.2f,%0.2f\n",
                     ctrl_msg.temperature[0], ctrl_msg.temperature[1],
                     ctrl_msg.temperature[2], ctrl_msg.humidity[0],
                     ctrl_msg.humidity[1]);
#endif
}

static void parse_message(const char *line) {
#if IS_HMI
  if (strcmp(line, "CTRL,PWR_OFF_CANCEL") == 0) {
    g_pwrOffActive = false;
    g_pwrOffRemainingMs = 0;
    COMM_LOG("[COMM] PWR_OFF cancelled\n");
    return;
  }
  if (strncmp(line, "CTRL,PWR_OFF,", 13) == 0) {
    int ms = 0;
    if (sscanf(line, "CTRL,PWR_OFF,%d", &ms) == 1) {
      g_pwrOffRemainingMs = ms;
      g_pwrOffActive = true;
      COMM_LOG("[COMM] PWR_OFF remaining=%d ms\n", ms);
    }
    return;
  }
  if (strncmp(line, "CTRL,TEL", strlen("CTRL,TEL")) == 0) {
    int probeState = SKIN_PROBE_NOT_CONNECTED;
    int result = sscanf(
        line, "CTRL,TEL,%lf,%lf,%lf,%d,%d", &ctrl_tel_msg.detectedAirTemperature,
        &ctrl_tel_msg.detectedSkinTemperature, &ctrl_tel_msg.detectedHumidity,
        &ctrl_tel_msg.serverCommStatus, &probeState);
    if (result < 3) {
      // COMM_LOG("[COMM] HMI failed to parse CTRL,TEL: %s\n", line);
    }
    if (result == 3) {
      // Backward compatibility or partial parse
      ctrl_tel_msg.serverCommStatus = COMM_STATUS_NONE;
    }
    if (result >= 5) {
      // Update skin probe state from periodic telemetry (RF-SKIN-008, ARQ-SKIN-003)
      g_skinProbeState = probeState;
    }
  } else if (strncmp(line, "CTRL,STATE", strlen("CTRL,STATE")) == 0) {
    int act, mode, photo, mute, sn, hwNum, numAlarms, skinE, commStatus, lang, probeState = 0;
    uint32_t alarmBitmask = 0;
    double photoTimeRemaining;  // Formato MM.SS (ej: 18.33 = 18 min 33 seg)
    char hwRev;
    char fwVer[20];
    double airSet, skinSet, humSet;
    int result =
        sscanf(line, "CTRL,STATE,%d,%d,%lf,%lf,%lf,%d,%d,%d,%d,%c,%19[^,],%d,%d,%d,%lf,%d,%d,0x%X",
               &act, &mode, &airSet, &skinSet, &humSet, &photo, &mute,
               &sn, &hwNum, &hwRev, fwVer, &numAlarms, &skinE, &commStatus, &photoTimeRemaining, &lang, &probeState, &alarmBitmask);

    // Accept 12 (old), 13 (with alarms), 14+skinModeEnabled, 15+photoTime, 16+lang, 17+probeState, 18+bitmask
    if (result >= 12) {
      ctrl_state_msg.actuation = act;
      ctrl_state_msg.controlMode = mode;
      ctrl_state_msg.desiredAirTemperature = airSet;
      ctrl_state_msg.desiredSkinTemperature = skinSet;
      ctrl_state_msg.desiredHumidity = humSet;
      ctrl_state_msg.phototherapyMode = photo;
      ctrl_state_msg.muteAlarm = mute;
      if (result >= 13) ctrl_state_msg.skinModeEnabled = skinE;
      if (result >= 16) ctrl_state_msg.language = lang;
      if (result >= 17) {
        ctrl_state_msg.skinProbeState = probeState;
        g_skinProbeState = probeState; // Expose globally for UITask (RF-SKIN-008, ARQ-SKIN-003)
      }
      if (result >= 18) ctrl_state_msg.alarmBitmask = alarmBitmask;
      else ctrl_state_msg.alarmBitmask = (uint32_t)-1; // Valor nulo si no viene
      ctrl_state_msg.serialNumber = sn;

      strncpy(ctrl_state_msg.fwVer, fwVer, sizeof(ctrl_state_msg.fwVer));
      ctrl_state_msg.fwVer[sizeof(ctrl_state_msg.fwVer) - 1] = '\0';

      // Extract minutes and seconds from MM.SS format
      if (result >= 15) {
        int mins = (int)photoTimeRemaining;
        int secs = (int)((photoTimeRemaining - mins) * 100.0 + 0.5);  // Extract SS from MM.SS format, round to nearest
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
  } else if (strncmp(line, "CTRL,PPG", 8) == 0) {
    int ppg = 0;
    if (sscanf(line, "CTRL,PPG,%d", &ppg) == 1) {
      ctrl_ppg_msg.ppg     = (uint8_t)ppg;
      ctrl_ppg_msg.updated = true;
    }
  } else if (strncmp(line, "CTRL,VIT", 8) == 0) {
    int hr = 0, spo2 = 0;
    if (sscanf(line, "CTRL,VIT,%d,%d", &hr, &spo2) == 2) {
      ctrl_vit_msg.hr      = (uint8_t)hr;
      ctrl_vit_msg.spo2    = (uint8_t)spo2;
      ctrl_vit_msg.updated = true;
    }
  } else if (strncmp(line, "CTRL,ALM", strlen("CTRL,ALM")) ==
             0) {
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
    if (rxIndex > 0 && (millis() - lastRxTime > COMM_RX_TIMEOUT_MS)) {
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
  LVGL_Lock();
  ui_set_switch_state_silent(ui_Switch1, st.actuation & 0x01);
  ui_set_switch_state_silent(ui_Switch2, (st.actuation >> 1) & 0x01);
  ui_set_switch_state_silent(ui_Switch3, st.phototherapyMode);
  // Restore skin: only activate if saved state says ON *and* probe is present.
  // If saved ON but no probe → fall back to Air and force switch OFF.
  bool probeAvailable = (st.skinProbeState == SKIN_PROBE_VALID);
  bool restoreSkin = (st.skinModeEnabled && probeAvailable);

  ui_set_switch_state_silent(ui_Switch4, restoreSkin);

  skinPanelEnabled = restoreSkin;
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

  if (st.desiredAirTemperature > COMM_TEMP_VALID_THRESHOLD)
    airTempValue = st.desiredAirTemperature;
  if (st.desiredSkinTemperature > COMM_TEMP_VALID_THRESHOLD)
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
  hmi_msg.skinModeEnabled = restoreSkin;
  hmi_msg.photoMinutesRemaining = st.photoMinutesRemaining;

  // Restore Phototherapy Timer if active in received state
  if (st.phototherapyMode && (st.photoMinutesRemaining > 0 || st.photoSecondsRemaining > 0)) {
      if (!photoTimerActive) {
          photoTimerActive = true;
          
          // Calculate total seconds remaining and elapsed
          long totalSecondsRemaining = st.photoMinutesRemaining * SECONDS_PER_MINUTE + st.photoSecondsRemaining;
          long totalSecondsOriginal = photoTimerMinutes * SECONDS_PER_MINUTE;
          long elapsedSeconds = totalSecondsOriginal - totalSecondsRemaining;
          
          // Adjust start time to reflect elapsed time
          photoTimerStartMs = millis() - (elapsedSeconds * MS_PER_SECOND);
          
          // Sync to avoid sending 0 back
          hmi_msg.photoMinutesRemaining = st.photoMinutesRemaining;
          
          ESP_LOGI(TAG, "Phototherapy timer restored: %d:%02d remaining", 
                   st.photoMinutesRemaining, st.photoSecondsRemaining);
      }
  }

  if (st.controlMode == CONTROL_SKIN && restoreSkin) {
    selectedPanel = SKIN_PANEL_SELECTED;
    lastSelectedPanel = SKIN_PANEL_SELECTED;
    if (ui_Switch4) lv_obj_add_state(ui_Switch4, LV_STATE_CHECKED);
  } else {
    selectedPanel = AIR_PANEL_SELECTED;
    lastSelectedPanel = AIR_PANEL_SELECTED;
  }

  if (st.serialNumber != 0 && st.serialNumber != in3.serialNumber) {
    in3.serialNumber = st.serialNumber;
    EEPROM.writeInt(EEPROM_SERIAL_NUMBER, in3.serialNumber);
    EEPROM.commit();
    ESP_LOGI(TAG, "Serial Number updated from motherboard: %d",
             in3.serialNumber);
  }

  // Actualizar labels de información si están disponibles
  if (ui_MBVerValue) lv_label_set_text(ui_MBVerValue, st.fwVer);
  if (ui_SNValue) {
      char sn_buf[16];
      snprintf(sn_buf, sizeof(sn_buf), "%d", st.serialNumber);
      lv_label_set_text(ui_SNValue, sn_buf);
  }
  if (ui_ConnValue) {
      lv_label_set_text(ui_ConnValue, getConnectivityString(st.serverCommStatus, g_lang));
  }

  // --- Sincronización de Alarmas via Bitmask (CORRECCIÓN: usa ID como bit, igual que la Board) ---
  // La Board calcula: bitmask |= (1 << alarmID). El HMI debe descodificarlo igual.
  // El slot en alarmList es: alarmList[alarmID] (mapeo directo ID->índice).
  if (st.alarmBitmask != (uint32_t)-1) {
      extern Alarm alarmList[];
      extern volatile bool g_pendingAlarmUpdate;
      bool changed = false;
      // Iterar por IDs válidos (1..MAX_ALARMS-1), igual que el enum ALARMS_ID
      for (int id = 1; id < MAX_ALARMS; id++) {
          // El bit del ID corresponde a la posición 'id' en el bitmask
          bool boardActive = (st.alarmBitmask >> id) & 1;
          if (alarmList[id].state && !boardActive) {
              // La alarma está pintada en el HMI pero la Board dice que ya no está activa
              COMM_LOG("[COMM] Bitmask sync: limpiando alarma ID %d (%s)\n", id, alarmList[id].type);
              alarmList[id].state = false;
              changed = true;
          }
      }
      if (changed) {
          g_pendingAlarmUpdate = true;
          AlarmSound_Update(); // Audio is thread-safe or runs in Core 0 separately
      }
  }

  UI_SyncAll();
  LVGL_Unlock();
  return true;
}

static void Display_StateSync_Service(void) {
  if (g_stateSynced)
    return;
  uint32_t now = millis();
  if (now - g_lastStateReqMs >= COMM_STATE_SYNC_MS) {
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
  LVGL_Lock();
  update_labels();
  if (tempSwitched) {
    chart_add_air_temp((float)airTempValueDetected);
    chart_add_skin_temp((float)skinTempValueDetected);
  }
  chart_add_hum_value((float)humValueDetected);
  chart_save_history();
  LVGL_Unlock();
}

static void processReceivedAlarm(const ControlBoard_Message_Alarm &alarm) {
  extern volatile bool g_pendingAlarmUpdate;
  // --- MAPEO DIRECTO ID -> ÍNDICE ---
  // El ID de la alarma (enum ALARMS_ID: 1..9) se usa directamente como índice en alarmList[].
  // Esto elimina búsquedas, colisiones y duplicados garantizando que cada alarma
  // ocupe siempre el mismo slot, independientemente del orden de llegada.
  if (alarm.id <= 0 || alarm.id >= MAX_ALARMS) {
    COMM_LOG("[COMM] Alarm ID %d fuera de rango, ignorando.\n", alarm.id);
    return;
  }

  int index = alarm.id; // Slot determinista: ID 5 -> alarmList[5], ID 6 -> alarmList[6]

  bool wasActive = alarmList[index].state;

  // Actualizar slot con los datos recibidos
  alarmList[index].id = alarm.id;
  strncpy(alarmList[index].type, alarm.type, ALARM_TYPE_LEN - 1);
  alarmList[index].type[ALARM_TYPE_LEN - 1] = '\0';
  strncpy(alarmList[index].description, alarm.description, ALARM_DESC_LEN - 1);
  alarmList[index].description[ALARM_DESC_LEN - 1] = '\0';
  alarmList[index].state = alarm.state;

  COMM_LOG("[COMM] Alarma ID %d (%s): %s -> %s\n",
           alarm.id, alarm.type,
           wasActive ? "ACTIVA" : "inactiva",
           alarm.state ? "ACTIVA" : "inactiva");

  // Si se activa una alarma nueva, desmutar
  if (alarm.state && !wasActive) {
    alarmsMuted = false;
    hmi_msg.muteAlarm = 0;
  }

  if (!g_ui_initialized)
    return;
  g_pendingAlarmUpdate = true;
  AlarmSound_Update();
}

void Comm_Task(void *pvParameters) {
  ESP_LOGI(TAG, "Communication Task Started");
  COMM_SERIAL.begin(COMM_BAUD_RATE);

  Communication_SendBootInfo();
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

    vTaskDelay(pdMS_TO_TICKS(COMM_TASK_LOOP_MS));
  }
}

void CreateCommTask() {
  xTaskCreatePinnedToCore(Comm_Task, "Comm", COMM_TASK_STACK_SIZE, NULL, COMM_TASK_PRIORITY, NULL,
                          CORE_ID_FREERTOS);
}