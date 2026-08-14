#include "CommTask.h"
#include "UITask.h"
#include "Wifi_OTA.h"
#include "esp_log.h"
#include "main.h"
#include "ui.h"
#include <cstdio>
#include <cstdlib>
#include <string.h>

static const char *TAG = "CommTask";

// --- Global variables (moved from communication.cpp) ---
HMI_Message hmi_msg;
ControlBoard_Message ctrl_msg;
ControlBoard_Message_Telemetry ctrl_tel_msg;
ControlBoard_Message_Alarm ctrl_msg_alarm;
ControlBoard_Message_State ctrl_state_msg = {0};
ControlBoard_Message_PPG ctrl_ppg_msg     = {0, false};
ControlBoard_Message_VIT ctrl_vit_msg     = {0, 0, 0.0f, false};
ControlBoard_Message_Probe ctrl_probe_msg = {SPO2_PROBE_DISCONNECTED, false};
int g_skinProbeState = SKIN_PROBE_NOT_CONNECTED; // Last received skin probe state

// --- Baby profile wizard protocol state ---
volatile bool       g_pendingProfileList = false;
BabyProfileListMsg  g_profileList = {0, {}};
volatile bool       g_pendingProfileAck = false;
uint32_t            g_profileAck = 0;
volatile bool       g_pendingProfileRange = false;
BabyProfileRangeMsg g_profileRange = {0, false, 0, -1.0f, -1.0f, -1.0f, true};

// --- Baby history viewer protocol state ---
volatile bool        g_pendingBabyHistory = false;
BabyHistoryMsg       g_babyHistory = {0, 0, 0, {}};
volatile bool        g_pendingWeightHistory = false;
BabyWeightHistoryMsg g_weightHistory = {0, 0, {}, {}};

// --- Power Off countdown state (written here, read by UITask) ---
volatile bool g_pwrOffActive = false;
volatile int  g_pwrOffRemainingMs = 0;

// --- Pending LVGL work flags (set by CommTask, consumed by UITask) ---
volatile bool g_pendingTelemetryApply = false;
volatile int  g_tempDutyPwm           = 0;
volatile int  g_humDutyPwm            = 0;
volatile bool g_pendingDutyApply      = false;

// --- Spinlock protecting double-width telemetry writes (Fix: ARQ-THREAD-001) ---
portMUX_TYPE g_telemetry_mux = portMUX_INITIALIZER_UNLOCKED;

static TaskHandle_t s_comm_task_handle = NULL;

bool error = false;
static char rxBuffer[COMM_RX_BUFFER_SIZE];
static int rxIndex = 0;

// Motherboard-provided wall clock (CTRL,TIME). 0 = not synced there yet.
static uint32_t s_mbEpoch = 0;
static uint32_t s_mbEpochAtMs = 0;

uint32_t HMI_GetEpochNow() {
  if (s_mbEpoch == 0) return 0;
  return s_mbEpoch + (millis() - s_mbEpochAtMs) / 1000u;
}

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

// --- Baby profile wizard protocol (PROTOCOL.md v1.6.0) ---
void Communication_SendProfileListReq(void) {
#if IS_HMI
  COMM_SERIAL.print("HMI,PROFILE_LIST_REQ\n");
#endif
}

void Communication_SendProfileNew(const char *name, uint8_t gestWeeks) {
#if IS_HMI
  COMM_SERIAL.printf("HMI,PROFILE_NEW,%s,%u\n", name, (unsigned)gestWeeks);
#endif
}

void Communication_SendProfileSelect(uint32_t seq) {
#if IS_HMI
  COMM_SERIAL.printf("HMI,PROFILE_SELECT,%u\n", (unsigned)seq);
#endif
}

void Communication_SendProfileWeight(uint32_t seq, uint16_t grams) {
#if IS_HMI
  if (grams == 0) {
    COMM_SERIAL.printf("HMI,PROFILE_WEIGHT,%u,SKIP\n", (unsigned)seq);
  } else {
    COMM_SERIAL.printf("HMI,PROFILE_WEIGHT,%u,%u\n", (unsigned)seq,
                       (unsigned)grams);
  }
#endif
}

void Communication_SendProfileAgeManual(uint32_t seq, uint16_t ageDays) {
#if IS_HMI
  COMM_SERIAL.printf("HMI,PROFILE_AGE_MANUAL,%u,%u\n", (unsigned)seq,
                     (unsigned)ageDays);
#endif
}

void Communication_SendProfileDischarge(uint32_t seq, uint8_t outcome) {
#if IS_HMI
  COMM_SERIAL.printf("HMI,PROFILE_DISCHARGE,%u,%u\n", (unsigned)seq,
                     (unsigned)outcome);
#endif
}

// Baby taken out to be with the mother. Deliberately NOT a discharge:
// the motherBoard keeps the profile in its active slot.
void Communication_SendProfileKangaroo(uint32_t seq) {
#if IS_HMI
  COMM_SERIAL.printf("HMI,PROFILE_KANGAROO,%u\n", (unsigned)seq);
#endif
}

void Communication_SendProfileHistoryReq(uint32_t page) {
#if IS_HMI
  COMM_SERIAL.printf("HMI,PROFILE_HISTORY_REQ,%u\n", (unsigned)page);
#endif
}

void Communication_SendWeightHistoryReq(uint32_t seq) {
#if IS_HMI
  COMM_SERIAL.printf("HMI,WEIGHT_HISTORY_REQ,%u\n", (unsigned)seq);
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
    float pi = 0.0f;
    int n = sscanf(line, "CTRL,VIT,%d,%d,%f", &hr, &spo2, &pi);
    if (n >= 2) {
      ctrl_vit_msg.hr      = (uint8_t)hr;
      ctrl_vit_msg.spo2    = (uint8_t)spo2;
      ctrl_vit_msg.pi      = (n >= 3) ? pi : 0.0f;
      ctrl_vit_msg.updated = true;
    }
  } else if (strncmp(line, "CTRL,PROBE", 10) == 0) {
    int state = 0;
    if (sscanf(line, "CTRL,PROBE,%d", &state) == 1) {
      // Fail-safe: un estado parseable NUNCA se descarta. Solo APPLIED habilita
      // la traza; cualquier otro valor — incluido uno que esta version del HMI
      // no conozca — significa "sin contacto valido". Descartarlo dejaria en pie
      // el ultimo APPLIED y con el la traza PPG congelada en la pantalla de
      // bloqueo, que es justo el fallo que esto corrige (SATURATING=3 llegaba y
      // se rechazaba por estar fuera del rango 0..2).
      if (state < SPO2_PROBE_DISCONNECTED || state > SPO2_PROBE_SATURATING) {
        COMM_LOG("[COMM] CTRL,PROBE estado desconocido (%d) -> NOT_APPLIED\n",
                 state);
        state = SPO2_PROBE_NOT_APPLIED;
      }
      ctrl_probe_msg.state   = (ProbeContactState)state;
      ctrl_probe_msg.updated = true;
      static const char *const probe_state_names[] = {
          "DISCONNECTED", "NOT_APPLIED", "APPLIED", "SATURATING"};
      (void)probe_state_names;
      // COMM_LOG("[COMM] CTRL,PROBE -> %s\n", probe_state_names[state]);
    } else {
      COMM_LOG("[COMM] CTRL,PROBE parse error: %s\n", line);
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
  } else if (strncmp(line, "CTRL,WIFI,", 10) == 0) {
    char ssid[64], pass[64];
    if (sscanf(line, "CTRL,WIFI,%63[^,],%63[^\n]", ssid, pass) == 2) {
      wifiApplyNewCredentials(ssid, pass);
      COMM_LOG("[COMM] CTRL,WIFI: reconnecting to %s\n", ssid);
    } else {
      COMM_LOG("[COMM] CTRL,WIFI parse error: %s\n", line);
    }
  } else if (strncmp(line, "CTRL,TIME,", 10) == 0) {
    unsigned long epoch = 0;
    if (sscanf(line, "CTRL,TIME,%lu", &epoch) == 1) {
      s_mbEpoch = (uint32_t)epoch;
      s_mbEpochAtMs = millis();
    } else {
      COMM_LOG("[COMM] TIME parse error: %s\n", line);
    }
  } else if (strncmp(line, "CTRL,DUTY", 9) == 0) {
    int t = 0, h = 0;
    if (sscanf(line, "CTRL,DUTY,%d,%d", &t, &h) == 2) {
      g_tempDutyPwm      = t;
      g_humDutyPwm       = h;
      g_pendingDutyApply = true;
    }
  } else if (strncmp(line, "CTRL,PROFILE_LIST", 17) == 0) {
    // CTRL,PROFILE_LIST,<n>{,<seq>,<name>,<gestWeeks>,<weightGrams>}xn
    // Manual comma-split (variable arity) — validate every field before
    // indexing/using it; malformed lines are silently discarded (security.md).
    char buf[COMM_RX_BUFFER_SIZE];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *save = nullptr;
    strtok_r(buf, ",", &save);       // "CTRL"
    strtok_r(nullptr, ",", &save);   // "PROFILE_LIST"
    char *nTok = strtok_r(nullptr, ",", &save);
    char *endp = nullptr;
    long n = nTok ? strtol(nTok, &endp, 10) : -1;
    bool ok = (nTok != nullptr) && endp && *endp == '\0' && n >= 0 && n <= 3;
    BabyProfileListMsg msg;
    msg.count = ok ? (int)n : 0;
    for (int i = 0; ok && i < n; i++) {
      char *seqTok = strtok_r(nullptr, ",", &save);
      char *nameTok = strtok_r(nullptr, ",", &save);
      char *gestTok = strtok_r(nullptr, ",", &save);
      char *wTok = strtok_r(nullptr, ",", &save);
      char *kTok = strtok_r(nullptr, ",", &save);
      char *ptTok = strtok_r(nullptr, ",", &save);
      char *thTok = strtok_r(nullptr, ",", &save);
      if (!seqTok || !nameTok || !gestTok || !wTok || !kTok || !ptTok ||
          !thTok) {
        ok = false;
        break;
      }
      char *e1, *e2, *e3, *e4, *e5, *e6;
      long seq = strtol(seqTok, &e1, 10);
      long gest = strtol(gestTok, &e2, 10);
      long w = strtol(wTok, &e3, 10);
      long kang = strtol(kTok, &e4, 10);
      unsigned long photoMin = strtoul(ptTok, &e5, 10);
      unsigned long thermoMin = strtoul(thTok, &e6, 10);
      if (*e1 || *e2 || *e3 || *e4 || *e5 || *e6 || seq < 0 || gest < 0 ||
          gest > 255 || w < 0 || w > 65535 || kang < 0 || kang > 65535) {
        ok = false;
        break;
      }
      msg.items[i].seq = (uint32_t)seq;
      snprintf(msg.items[i].name, sizeof(msg.items[i].name), "%s", nameTok);
      msg.items[i].gestWeeks = (uint8_t)gest;
      msg.items[i].weightGrams = (uint16_t)w;
      msg.items[i].kangarooCount = (uint16_t)kang;
      msg.items[i].phototherapyMinutes = (uint32_t)photoMin;
      msg.items[i].thermoMinutes = (uint32_t)thermoMin;
      msg.items[i].thermoMinutes = (uint32_t)thermoMin;
    }
    if (ok) {
      g_profileList = msg;
      g_pendingProfileList = true;
    } else {
      COMM_LOG("[COMM] PROFILE_LIST malformed: %s\n", line);
    }
  } else if (strncmp(line, "CTRL,PROFILE_ACK", 16) == 0) {
    unsigned seq = 0;
    if (sscanf(line, "CTRL,PROFILE_ACK,%u", &seq) == 1) {
      g_profileAck = (uint32_t)seq;
      g_pendingProfileAck = true;
    } else {
      COMM_LOG("[COMM] PROFILE_ACK parse error: %s\n", line);
    }
  } else if (strncmp(line, "CTRL,PROFILE_RANGE", 18) == 0) {
    unsigned seq = 0;
    int ageKnown = 0, ageDays = 0, estimated = 0;
    float lo = -1.0f, hi = -1.0f, mid = -1.0f;
    if (sscanf(line, "CTRL,PROFILE_RANGE,%u,%d,%d,%f,%f,%f,%d", &seq,
               &ageKnown, &ageDays, &lo, &hi, &mid, &estimated) == 7 &&
        ageDays >= 0 && ageDays <= 65535) {
      g_profileRange.seq = (uint32_t)seq;
      g_profileRange.ageKnown = (ageKnown != 0);
      g_profileRange.ageDays = (uint16_t)ageDays;
      g_profileRange.lo = lo;
      g_profileRange.hi = hi;
      g_profileRange.mid = mid;
      g_profileRange.estimated = (estimated != 0);
      g_pendingProfileRange = true;
    } else {
      COMM_LOG("[COMM] PROFILE_RANGE parse error: %s\n", line);
    }
  } else if (strncmp(line, "CTRL,PROFILE_HISTORY", 20) == 0) {
    // CTRL,PROFILE_HISTORY,<page>,<totalCount>,<n>{,<seq>,<name>,<gestWeeks>,
    //   <lastWeightGrams>,<admissionEpoch>,<dischargeEpoch>,<outcome>}xn
    // Same manual comma-split pattern as PROFILE_LIST: every field validated
    // before use, malformed line = silent discard (security.md).
    char buf[COMM_RX_BUFFER_SIZE];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *save = nullptr;
    strtok_r(buf, ",", &save);      // "CTRL"
    strtok_r(nullptr, ",", &save);  // "PROFILE_HISTORY"
    char *pageTok = strtok_r(nullptr, ",", &save);
    char *totTok = strtok_r(nullptr, ",", &save);
    char *nTok = strtok_r(nullptr, ",", &save);
    char *e0 = nullptr, *e1 = nullptr, *e2 = nullptr;
    long page = pageTok ? strtol(pageTok, &e0, 10) : -1;
    long tot = totTok ? strtol(totTok, &e1, 10) : -1;
    long n = nTok ? strtol(nTok, &e2, 10) : -1;
    bool ok = pageTok && totTok && nTok && e0 && !*e0 && e1 && !*e1 && e2 &&
              !*e2 && page >= 0 && tot >= 0 && n >= 0 && n <= 10;
    BabyHistoryMsg msg;
    msg.page = ok ? (uint32_t)page : 0;
    msg.totalCount = ok ? (uint32_t)tot : 0;
    msg.count = ok ? (int)n : 0;
    for (int i = 0; ok && i < n; i++) {
      char *seqTok = strtok_r(nullptr, ",", &save);
      char *nameTok = strtok_r(nullptr, ",", &save);
      char *gestTok = strtok_r(nullptr, ",", &save);
      char *wTok = strtok_r(nullptr, ",", &save);
      char *admTok = strtok_r(nullptr, ",", &save);
      char *disTok = strtok_r(nullptr, ",", &save);
      char *ocTok = strtok_r(nullptr, ",", &save);
      char *kTok = strtok_r(nullptr, ",", &save);
      char *ptTok = strtok_r(nullptr, ",", &save);
      char *thTok = strtok_r(nullptr, ",", &save);
      if (!seqTok || !nameTok || !gestTok || !wTok || !admTok || !disTok ||
          !ocTok || !kTok || !ptTok || !thTok) {
        ok = false;
        break;
      }
      char *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9;
      long seq = strtol(seqTok, &p1, 10);
      long gest = strtol(gestTok, &p2, 10);
      long w = strtol(wTok, &p3, 10);
      unsigned long adm = strtoul(admTok, &p4, 10);
      unsigned long dis = strtoul(disTok, &p5, 10);
      long oc = strtol(ocTok, &p6, 10);
      long kang = strtol(kTok, &p7, 10);
      unsigned long photoMin = strtoul(ptTok, &p8, 10);
      unsigned long thermoMin = strtoul(thTok, &p9, 10);
      if (*p1 || *p2 || *p3 || *p4 || *p5 || *p6 || *p7 || *p8 || *p9 ||
          seq < 0 ||
          gest < 0 || gest > 255 || w < 0 || w > 65535 || oc < 0 || oc > 3 ||
          kang < 0 || kang > 65535) {
        ok = false;
        break;
      }
      msg.items[i].seq = (uint32_t)seq;
      snprintf(msg.items[i].name, sizeof(msg.items[i].name), "%s", nameTok);
      msg.items[i].gestWeeks = (uint8_t)gest;
      msg.items[i].lastWeightGrams = (uint16_t)w;
      msg.items[i].admissionEpoch = (uint32_t)adm;
      msg.items[i].dischargeEpoch = (uint32_t)dis;
      msg.items[i].outcome = (uint8_t)oc;
      msg.items[i].kangarooCount = (uint16_t)kang;
      msg.items[i].phototherapyMinutes = (uint32_t)photoMin;
    }
    if (ok) {
      g_babyHistory = msg;
      g_pendingBabyHistory = true;
    } else {
      COMM_LOG("[COMM] PROFILE_HISTORY malformed: %s\n", line);
    }
  } else if (strncmp(line, "CTRL,WEIGHT_HISTORY", 19) == 0) {
    // CTRL,WEIGHT_HISTORY,<seq>,<n>{,<dayOffset>,<weightGrams>}xn — n<=50.
    char buf[COMM_RX_BUFFER_SIZE];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *save = nullptr;
    strtok_r(buf, ",", &save);      // "CTRL"
    strtok_r(nullptr, ",", &save);  // "WEIGHT_HISTORY"
    char *seqTok = strtok_r(nullptr, ",", &save);
    char *nTok = strtok_r(nullptr, ",", &save);
    char *e0 = nullptr, *e1 = nullptr;
    long seq = seqTok ? strtol(seqTok, &e0, 10) : -1;
    long n = nTok ? strtol(nTok, &e1, 10) : -1;
    bool ok = seqTok && nTok && e0 && !*e0 && e1 && !*e1 && seq >= 0 &&
              n >= 0 && n <= 50;
    BabyWeightHistoryMsg msg;
    msg.seq = ok ? (uint32_t)seq : 0;
    msg.count = ok ? (int)n : 0;
    for (int i = 0; ok && i < n; i++) {
      char *dTok = strtok_r(nullptr, ",", &save);
      char *wTok = strtok_r(nullptr, ",", &save);
      if (!dTok || !wTok) { ok = false; break; }
      char *p1, *p2;
      long d = strtol(dTok, &p1, 10);
      long w = strtol(wTok, &p2, 10);
      if (*p1 || *p2 || d < 0 || d > 65535 || w < 0 || w > 65535) {
        ok = false;
        break;
      }
      msg.dayOffset[i] = (uint16_t)d;
      msg.weightGrams[i] = (uint16_t)w;
    }
    if (ok) {
      g_weightHistory = msg;
      g_pendingWeightHistory = true;
    } else {
      COMM_LOG("[COMM] WEIGHT_HISTORY malformed: %s\n", line);
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

bool Display_ApplyCtrlState(const ControlBoard_Message_State &st) {
  if (!g_ui_initialized)
    return false;
  LVGL_Lock();
  bool tempOn = st.actuation & 0x01;
  ui_set_switch_state_silent(ui_Switch1, tempOn);
  temp_content_set_visible(tempOn);
  ui_set_switch_state_silent(ui_Switch2, (st.actuation >> 1) & 0x01);
  bool photoOn = st.phototherapyMode;
  ui_set_switch_state_silent(ui_Switch3, photoOn);
  if (ui_PhotoTimerCont) {
    if (photoOn) lv_obj_clear_flag(ui_PhotoTimerCont, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(ui_PhotoTimerCont, LV_OBJ_FLAG_HIDDEN);
  }
  // Restore skin: only activate if saved state says ON *and* probe is present.
  // If saved ON but no probe → fall back to Air and force switch OFF.
  bool probeAvailable = (st.skinProbeState == SKIN_PROBE_VALID);
  bool restoreSkin = (st.skinModeEnabled && probeAvailable);

  ui_set_switch_state_silent(ui_Switch4, restoreSkin);

  skinPanelEnabled = restoreSkin;
  if (skinPanelEnabled && tempOn) {
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
    { Preferences p; p.begin(HMI_NS_CFG, false); p.putInt(HMI_KEY_SERIAL, in3.serialNumber); p.end(); }
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
          // AlarmSound_Update() moved to UITask — consumed in g_pendingAlarmUpdate handler
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
  // UITask handles Display_ApplyCtrlState when ctrl_state_msg.newState == true
}

static void applyHMIData() {
  if (!g_ui_initialized)
    return;
  taskENTER_CRITICAL(&g_telemetry_mux);
  airTempValueDetected    = ctrl_tel_msg.detectedAirTemperature;
  skinTempValueDetected   = ctrl_tel_msg.detectedSkinTemperature;
  humValueDetected        = (int)ctrl_tel_msg.detectedHumidity;
  g_pendingTelemetryApply = true;
  taskEXIT_CRITICAL(&g_telemetry_mux);
  // LVGL calls (update_labels, chart_add_*, chart_save_history) have been
  // moved to UITask — it consumes g_pendingTelemetryApply inside LVGL_Lock().
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
  // AlarmSound_Update() moved to UITask — consumed in g_pendingAlarmUpdate handler
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
  xTaskCreatePinnedToCore(Comm_Task, "Comm", COMM_TASK_STACK_SIZE, NULL,
                          COMM_TASK_PRIORITY, &s_comm_task_handle, CORE_ID_FREERTOS);
}

TaskHandle_t CommTask_GetHandle(void) { return s_comm_task_handle; }