# WiFi Configuration from ThingsBoard — Design Spec

**Date:** 2026-05-31  
**Status:** Approved  

---

## Overview

Allow an operator to configure the device's WiFi credentials remotely from the ThingsBoard dashboard. ThingsBoard sends a one-way RPC (`setWifi`) with the new SSID and password. The motherboard attempts the connection; on success it forwards the credentials to the display_HMI via UART so both boards connect to the same network.

---

## Architecture

Three independent pieces connected in sequence:

```
ThingsBoard dashboard
  └─► one-way RPC: setWifi {ssid, password}
          │  (arrives via whichever channel is active: GPRS or WiFi)
          ▼
  RPC handler (GPRS.cpp or Wifi_OTA.cpp)
          │
          └──► applyWifiCredentials(ssid, pass)   [Wifi_OTA.cpp]
                    ├─ backup current NVS credentials
                    ├─ write new credentials to NVS (mb_wifi: ssid, password)
                    ├─ WiFi.disconnect() + WiFi.begin(ssid, pass)
                    ├─ wait up to 15 s
                    │
                    ├─ [SUCCESS] → sendWifiToHMI(ssid, pass)  [CommTask.cpp]
                    └─ [FAILURE] → restore NVS + WiFi.begin(prevSSID, prevPass)
```

Since ThingsBoard is connected via either GPRS or WiFi (never both simultaneously), only one RPC handler will be active at any time. Both handlers register the same callback so the feature works regardless of the active channel.

---

## Components

### 1. RPC handler — `GPRS.cpp` and `Wifi_OTA.cpp`

Added to `subscribeRPCHandlers()` alongside existing `restart` and `getDiag` handlers:

```cpp
const std::array<RPC_Callback, 3> rpc_callbacks = {{
    { "restart", rpcRestart },
    { "getDiag", rpcGetDiag },
    { "setWifi", rpcSetWifi },   // new
}};
```

Handler implementation (shared logic, declared in `Wifi_OTA.h`):

```cpp
RPC_Response rpcSetWifi(const RPC_Data& data) {
    const char* ssid = data["ssid"];
    const char* pass = data["password"];
    if (!ssid || !pass) return RPC_Response();
    applyWifiCredentials(ssid, pass);
    return RPC_Response();   // one-way: empty response
}
```

---

### 2. `applyWifiCredentials()` — `Wifi_OTA.cpp`

```cpp
void applyWifiCredentials(const char* ssid, const char* pass) {
    // Backup current credentials
    Preferences prefs;
    prefs.begin("mb_wifi", true);
    char prevSSID[64], prevPass[64];
    prefs.getString("ssid", prevSSID, sizeof(prevSSID));
    prefs.getString("password", prevPass, sizeof(prevPass));
    prefs.end();

    // Write new credentials to NVS
    prefs.begin("mb_wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("password", pass);
    prefs.end();

    // Reconnect
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
        // Rollback NVS and reconnect with previous credentials
        prefs.begin("mb_wifi", false);
        prefs.putString("ssid", prevSSID);
        prefs.putString("password", prevPass);
        prefs.end();
        WiFi.begin(prevSSID, prevPass);
    }
}
```

---

### 3. `sendWifiToHMI()` — `CommTask.cpp`

New UART message `CTRL,WIFI` (motherboard → HMI direction, inverse of the existing `HMI,WIFI` message):

```cpp
void sendWifiToHMI(const char* ssid, const char* pass) {
    char buf[160];
    snprintf(buf, sizeof(buf), "CTRL,WIFI,%s,%s\n", ssid, pass);
    Serial1.print(buf);
}
```

The display_HMI firmware must parse `CTRL,WIFI,<ssid>,<password>` and call its own `WiFi.begin()`.

---

## UART Protocol Extension

| Direction | Message | Meaning |
|-----------|---------|---------|
| HMI → MB | `HMI,WIFI,<ssid>,<password>` | Already exists — HMI sends creds to MB |
| **MB → HMI** | **`CTRL,WIFI,<ssid>,<password>`** | **New — MB forwards creds to HMI after successful connection** |

---

## Error Handling

| Scenario | Behavior |
|----------|----------|
| Missing `ssid` or `password` in RPC payload | Handler returns early, no change |
| New WiFi unreachable within 15 s | NVS restored, reconnects with previous credentials |
| HMI UART send fails | No retry — next telemetry cycle will reflect actual WiFi status |

---

## Result Confirmation

There is no RPC response (one-way). The operator confirms success by observing the device's next telemetry payload in ThingsBoard — the device will continue reporting if connected. Optionally, add a `wifiSSID` field to the telemetry to make the connected network explicitly visible on the dashboard.

---

## ThingsBoard Dashboard Integration

### For testing — RPC Debug Terminal widget

1. Add widget → *Control widgets* → **RPC Debug Terminal**
2. Send:
   - **Method:** `setWifi`
   - **One-way:** ✓
   - **Params:** `{"ssid": "MyNetwork", "password": "MyPassword"}`

### For production — custom control widget

1. Add widget → *Control widgets* → **Command button** (or custom widget with two text inputs)
2. Configure:
   - **RPC method:** `setWifi`
   - **One-way:** ✓
   - **Body:**
     ```json
     {"ssid": "${ssid}", "password": "${password}"}
     ```
3. Optionally display a `wifiSSID` telemetry value next to the widget as connection feedback.

---

## Files Changed

| File | Change |
|------|--------|
| `src/GPRS.cpp` | Register `rpcSetWifi` in `subscribeRPCHandlers()` |
| `src/Wifi_OTA.cpp` | Register `rpcSetWifi` + implement `applyWifiCredentials()`; calls `sendWifiToHMI()` on success |
| `include/Wifi_OTA.h` | Declare `applyWifiCredentials()` |
| `src/CommTask.cpp` | Implement `sendWifiToHMI()` — `Serial1` is owned by CommTask |
| `include/CommTask.h` | Declare `sendWifiToHMI()` |

---

## Out of Scope

- Encryption of credentials in transit (MQTT is already unencrypted on port 1883)
- Display_HMI firmware changes (separate firmware, separate task)
- WiFi scanning or SSID discovery from the device
