# WiFi Config from ThingsBoard — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Allow an operator to send WiFi credentials from ThingsBoard via a one-way RPC (`setWifi`); the motherboard reconnects and, on success, forwards the credentials to the display_HMI via UART.

**Architecture:** RPC handler (GPRS + WiFi TB paths) → `applyWifiCredentials()` in Wifi_OTA.cpp → `sendWifiToHMI()` in CommTask.cpp → display_HMI parses `CTRL,WIFI` and reconnects its own WiFi.

**Tech Stack:** ESP-IDF / Arduino, FreeRTOS, ThingsBoard C++ SDK, ESP32 Preferences (NVS), UART.

**Spec:** `docs/superpowers/specs/2026-05-31-wifi-config-thingsboard-design.md`

---

## Files changed

| Firmware | File | Change |
|----------|------|--------|
| motherBoard | `include/CommTask.h` | Declare `sendWifiToHMI()` |
| motherBoard | `src/CommTask.cpp` | Implement `sendWifiToHMI()` |
| motherBoard | `include/Wifi_OTA.h` | Declare `applyWifiCredentials()` |
| motherBoard | `src/Wifi_OTA.cpp` | Implement `applyWifiCredentials()` + register RPC on WiFi TB path |
| motherBoard | `src/GPRS.cpp` | Register `rpc_setwifi_cb` on GPRS TB path |
| display_HMI | `include/Wifi_OTA.h` | Declare `wifiApplyNewCredentials()` |
| display_HMI | `src/Wifi_OTA.cpp` | Implement `wifiApplyNewCredentials()` |
| display_HMI | `src/CommTask.cpp` | Parse `CTRL,WIFI` message and call `wifiApplyNewCredentials()` |

---

## Task 1 — `sendWifiToHMI()` declaration and implementation (motherBoard)

**Files:**
- Modify: `include/CommTask.h`
- Modify: `src/CommTask.cpp`

- [ ] **Step 1.1 — Declare in header**

  In `include/CommTask.h`, add after the last function declaration (after `getRemainingPhotoTime()`):

  ```cpp
  void sendWifiToHMI(const char* ssid, const char* pass);
  ```

- [ ] **Step 1.2 — Implement in CommTask.cpp**

  In `src/CommTask.cpp`, add this function after `send_state_to_hmi()` (around line 198):

  ```cpp
  void sendWifiToHMI(const char* ssid, const char* pass) {
    char msg[160];
    snprintf(msg, sizeof(msg), "CTRL,WIFI,%s,%s\n", ssid, pass);
    CommunicationHost_Send(msg);
  }
  ```

- [ ] **Step 1.3 — Build to verify no errors**

  ```
  pio run -e main
  ```
  Expected: compilation successful, no new errors.

---

## Task 2 — `applyWifiCredentials()` declaration and implementation (motherBoard)

**Files:**
- Modify: `include/Wifi_OTA.h`
- Modify: `src/Wifi_OTA.cpp`

- [ ] **Step 2.1 — Declare in Wifi_OTA.h**

  In `include/Wifi_OTA.h`, add after the last function declaration:

  ```cpp
  void applyWifiCredentials(const char* ssid, const char* pass);
  ```

- [ ] **Step 2.2 — Add include in Wifi_OTA.cpp**

  `src/Wifi_OTA.cpp` already includes `CommTask.h` (line 29). Confirm it's there; if not, add it:

  ```cpp
  #include "CommTask.h"
  ```

- [ ] **Step 2.3 — Implement applyWifiCredentials() in Wifi_OTA.cpp**

  Add this function anywhere after the includes/globals section (e.g., after line 61):

  ```cpp
  void applyWifiCredentials(const char* ssid, const char* pass) {
    // Backup current credentials
    Preferences prefs;
    char prevSSID[64] = "";
    char prevPass[64] = "";
    prefs.begin("mb_wifi", true);
    prefs.getString("ssid", prevSSID, sizeof(prevSSID));
    prefs.getString("password", prevPass, sizeof(prevPass));
    prefs.end();

    // Write new credentials to NVS
    prefs.begin("mb_wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("password", pass);
    prefs.end();

    // Reconnect with new credentials
    WiFi.disconnect();
    WiFi.begin(ssid, pass);

    // Wait up to 15 s
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (WiFi.status() == WL_CONNECTED) {
      sendWifiToHMI(ssid, pass);
    } else {
      // Rollback: restore previous credentials
      prefs.begin("mb_wifi", false);
      prefs.putString("ssid", prevSSID);
      prefs.putString("password", prevPass);
      prefs.end();
      WiFi.begin(prevSSID, prevPass);
    }
  }
  ```

- [ ] **Step 2.4 — Build to verify no errors**

  ```
  pio run -e main
  ```
  Expected: compilation successful.

---

## Task 3 — RPC handler on GPRS path (motherBoard)

**Files:**
- Modify: `src/GPRS.cpp`

- [ ] **Step 3.1 — Add include for Wifi_OTA.h**

  In `src/GPRS.cpp`, confirm or add this include (near the top with other includes):

  ```cpp
  #include "Wifi_OTA.h"
  ```

- [ ] **Step 3.2 — Add rpc_setwifi_cb function**

  In `src/GPRS.cpp`, add this function after `rpc_diag_cb` (after line 91):

  ```cpp
  static void rpc_setwifi_cb(JsonVariantConst const & data,
                             JsonDocument & /*response*/) {
    const char* ssid = data["ssid"];
    const char* pass = data["password"];
    if (!ssid || !pass || ssid[0] == '\0' || pass[0] == '\0') {
      logModemData("[RPC] setWifi: missing ssid or password");
      return;
    }
    logModemData("[RPC] setWifi received, applying credentials");
    applyWifiCredentials(ssid, pass);
  }
  ```

- [ ] **Step 3.3 — Add to rpc_callbacks array**

  In `src/GPRS.cpp`, update `rpc_callbacks[]` (currently at line 93) to add the new entry:

  ```cpp
  static RPC_Callback rpc_callbacks[] = {
    RPC_Callback("restart",  rpc_restart_cb),
    RPC_Callback("getDiag",  rpc_diag_cb),
    RPC_Callback("setWifi",  rpc_setwifi_cb),
  };
  ```

  The `RPC_CB_COUNT` calculation on line 97 uses `sizeof` so it updates automatically — no change needed there.

- [ ] **Step 3.4 — Build to verify no errors**

  ```
  pio run -e main
  ```
  Expected: compilation successful.

- [ ] **Step 3.5 — Commit**

  ```
  git add include/CommTask.h src/CommTask.cpp include/Wifi_OTA.h src/Wifi_OTA.cpp src/GPRS.cpp
  git commit -m "feat(wifi): add setWifi RPC handler on GPRS path and applyWifiCredentials"
  ```

---

## Task 4 — RPC handler on WiFi TB path (motherBoard)

**Files:**
- Modify: `src/Wifi_OTA.cpp`

- [ ] **Step 4.1 — Find where tb_wifi subscribes/connects**

  In `src/Wifi_OTA.cpp`, search for `tb_wifi.connect` or `tb_wifi.provision` or `Wifi_TB.serverConnectionStatus = true`.
  This is the point where RPC handlers must be subscribed (same pattern as `subscribeRPCHandlers()` in GPRS.cpp).

- [ ] **Step 4.2 — Add static callback and subscription array**

  Add this block near the top of `src/Wifi_OTA.cpp` (after the globals, before the task function):

  ```cpp
  static void rpc_setwifi_wifi_cb(JsonVariantConst const & data,
                                  JsonDocument & /*response*/) {
    const char* ssid = data["ssid"];
    const char* pass = data["password"];
    if (!ssid || !pass || ssid[0] == '\0' || pass[0] == '\0') {
      ESP_LOGW("WiFi", "[RPC] setWifi: missing ssid or password");
      return;
    }
    ESP_LOGI("WiFi", "[RPC] setWifi received, applying credentials");
    applyWifiCredentials(ssid, pass);
  }

  static RPC_Callback wifi_rpc_callbacks[] = {
    RPC_Callback("setWifi", rpc_setwifi_wifi_cb),
  };
  ```

- [ ] **Step 4.3 — Subscribe handlers after tb_wifi connects**

  At the point found in Step 4.1 (right after a successful `tb_wifi.connect()` or provision), add:

  ```cpp
  for (size_t i = 0; i < sizeof(wifi_rpc_callbacks) / sizeof(wifi_rpc_callbacks[0]); i++) {
    tb_wifi.RPC_Subscribe(wifi_rpc_callbacks[i]);
  }
  ```

- [ ] **Step 4.4 — Build to verify no errors**

  ```
  pio run -e main
  ```
  Expected: compilation successful.

- [ ] **Step 4.5 — Commit**

  ```
  git add src/Wifi_OTA.cpp
  git commit -m "feat(wifi): add setWifi RPC handler on WiFi TB path"
  ```

---

## Task 5 — `wifiApplyNewCredentials()` in display_HMI

**Files:**
- Modify: `Firmware/display_HMI/include/Wifi_OTA.h`
- Modify: `Firmware/display_HMI/src/Wifi_OTA.cpp`

- [ ] **Step 5.1 — Declare in display_HMI Wifi_OTA.h**

  In `Firmware/display_HMI/include/Wifi_OTA.h`, add a declaration for the new function (after existing declarations):

  ```cpp
  void wifiApplyNewCredentials(const char* ssid, const char* pass);
  ```

- [ ] **Step 5.2 — Implement in display_HMI Wifi_OTA.cpp**

  `pendingSSID` and `pendingPass` are already defined in this file (lines 57-58). Add this function after `wifiInit()` (after line 197):

  ```cpp
  void wifiApplyNewCredentials(const char* ssid, const char* pass) {
    strncpy(pendingSSID, ssid, sizeof(pendingSSID) - 1);
    pendingSSID[sizeof(pendingSSID) - 1] = '\0';
    strncpy(pendingPass, pass, sizeof(pendingPass) - 1);
    pendingPass[sizeof(pendingPass) - 1] = '\0';
    WiFi.disconnect();
    WiFi.begin(ssid, pass);
    // GOT_IP event handler in wifiInit() will detect pendingSSID != ""
    // and set s_persistCredentials = true to save to NVS automatically.
  }
  ```

- [ ] **Step 5.3 — Build display_HMI to verify no errors**

  ```
  pio run -e main
  ```
  Expected: compilation successful.

---

## Task 6 — Parse `CTRL,WIFI` in display_HMI CommTask.cpp

**Files:**
- Modify: `Firmware/display_HMI/src/CommTask.cpp`

- [ ] **Step 6.1 — Add include for Wifi_OTA.h**

  In `Firmware/display_HMI/src/CommTask.cpp`, confirm or add:

  ```cpp
  #include "Wifi_OTA.h"
  ```

- [ ] **Step 6.2 — Add CTRL,WIFI case in parse_message()**

  In `parse_message()`, add a new `else if` branch after the last `else if` block (currently `CTRL,ALM` ending at line 222), before the closing `#endif`:

  ```cpp
  } else if (strncmp(line, "CTRL,WIFI,", 10) == 0) {
    char ssid[64], pass[64];
    if (sscanf(line, "CTRL,WIFI,%63[^,],%63[^\n]", ssid, pass) == 2) {
      wifiApplyNewCredentials(ssid, pass);
      COMM_LOG("[COMM] CTRL,WIFI: reconnecting to %s\n", ssid);
    } else {
      COMM_LOG("[COMM] CTRL,WIFI parse error: %s\n", line);
    }
  }
  ```

- [ ] **Step 6.3 — Build to verify no errors**

  ```
  pio run -e main
  ```
  Expected: compilation successful.

- [ ] **Step 6.4 — Commit (display_HMI)**

  From the display_HMI firmware directory:

  ```
  git add include/Wifi_OTA.h src/Wifi_OTA.cpp src/CommTask.cpp
  git commit -m "feat(wifi): handle CTRL,WIFI from motherboard to apply new WiFi credentials"
  ```

---

## Task 7 — End-to-end verification

- [ ] **Step 7.1 — Flash both firmwares**

  Flash motherBoard and display_HMI to the device.

- [ ] **Step 7.2 — Send RPC from ThingsBoard**

  Using the RPC Debug Terminal widget in ThingsBoard:
  - Method: `setWifi`
  - One-way: ✓ (or two-way — both work since response is empty)
  - Params:
    ```json
    {"ssid": "TestNetwork", "password": "TestPassword"}
    ```

- [ ] **Step 7.3 — Verify motherboard serial log**

  Expected log output (via monitor):
  ```
  [RPC] setWifi received, applying credentials
  ```
  Followed by either:
  - WiFi connected → `CTRL,WIFI,TestNetwork,TestPassword` sent to HMI
  - WiFi failed → rollback log + reconnect with previous credentials

- [ ] **Step 7.4 — Verify display_HMI serial log**

  Expected log output:
  ```
  [COMM] CTRL,WIFI: reconnecting to TestNetwork
  ```

- [ ] **Step 7.5 — Verify ThingsBoard telemetry**

  In ThingsBoard, confirm the device continues sending telemetry after the RPC — this confirms the connection succeeded and the ThingsBoard session was re-established.
