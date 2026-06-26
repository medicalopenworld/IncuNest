# Webserver Credentials Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move hardcoded webserver credentials out of source files into the existing gitignored `Credentials.h`, add compile-time fallbacks in `Credentials_public.h`, and enforce server-side auth on all sensitive routes in the motherboard.

**Architecture:** Both projects already have a `Credentials_public.h` that does `#if __has_include("Credentials.h") / #else / defaults / #endif`. We simply add `WEB_SERVER_USERNAME` and `WEB_SERVER_PASSWORD` to that pattern — real values in `Credentials.h` (gitignored), placeholder in the `#else` block of `Credentials_public.h` (committed). No new files needed.

**Tech Stack:** C/C++, PlatformIO, ESP32 Arduino WebServer library (`WebServer::authenticate()`).

## Global Constraints

- `Credentials.h` is gitignored in both projects — never commit it.
- `Credentials_public.h` is committed — it must compile from a fresh clone with no `Credentials.h` present.
- Do not change any authentication logic in Display HMI — it already works. Only move credentials to macros.
- Motherboard `/` route (JS login page) is left unchanged.
- Username: `incunestadmin` | Password: `savinglives` | Placeholder username: `incunest` | Placeholder password: `changeme`

---

## File Map

| File | Action | Why |
|------|--------|-----|
| `Display_HMI/include/Credentials.h` | Modify | Add `WEB_SERVER_USERNAME` / `WEB_SERVER_PASSWORD` real values |
| `Display_HMI/include/protocol/Credentials_public.h` | Modify | Add same defines with placeholder values in `#else` block |
| `Display_HMI/src/tasks/Wifi_OTA.cpp` | Modify | Remove hardcoded `www_username`/`www_password`, use macros |
| `motherBoard/include/Credentials.h` | Modify | Add `WEB_SERVER_USERNAME` / `WEB_SERVER_PASSWORD` real values |
| `motherBoard/include/protocol/Credentials_public.h` | Modify | Add same defines with placeholder values in `#else` block |
| `motherBoard/src/tasks/Wifi_OTA.cpp` | Modify | Add `authenticate()` to `/serverIndex`, `/update`, `/config` |

---

## Task 1: Display HMI — Credentials files

**Files:**
- Modify: `Display_HMI/include/Credentials.h`
- Modify: `Display_HMI/include/protocol/Credentials_public.h`

**Interfaces:**
- Produces: `WEB_SERVER_USERNAME` and `WEB_SERVER_PASSWORD` macros available to any file that includes `main.h` (which chains to `Credentials_public.h` → `Credentials.h`).

- [ ] **Step 1: Add real credentials to `Credentials.h`**

Open `Display_HMI/include/Credentials.h`. Add before the closing `#endif`:

```cpp
#define WEB_SERVER_USERNAME "incunestadmin"
#define WEB_SERVER_PASSWORD "savinglives"
```

Final file should look like:
```cpp
#ifndef _CREDENTIALS_
#define _CREDENTIALS_

#define THINGSBOARD_SERVER "mon.medicalopenworld.org"
#define THINGSBOARD_PORT 1883

#define FACTORY_SERVER 0
#define DEMO_SERVER 1

#define THINGSBOARD_PROVISION_SERVER FACTORY_SERVER

#if (THINGSBOARD_PROVISION_SERVER == DEMO_SERVER)
#define PROVISION_DEVICE_KEY "bztump0738iuc2ggreix"
#define PROVISION_DEVICE_SECRET "0znp47gkyh9hbljq1opm"
#elif (THINGSBOARD_PROVISION_SERVER == FACTORY_SERVER)
#define PROVISION_DEVICE_KEY "1ea5qharvbfdwzhdqinm"
#define PROVISION_DEVICE_SECRET "agamc0dtsrfdg9738sf7"
#endif

#define WIFI_SSID "in3wifi"
#define WIFI_PASSWORD "12345678"

#define WEB_SERVER_USERNAME "incunestadmin"
#define WEB_SERVER_PASSWORD "savinglives"
#endif // _CREDENTIALS_
```

- [ ] **Step 2: Add placeholder fallbacks to `Credentials_public.h`**

Open `Display_HMI/include/protocol/Credentials_public.h`. In the `#else` block, add after the last `#define`:

```cpp
#define WEB_SERVER_USERNAME "incunest"
#define WEB_SERVER_PASSWORD "changeme"
```

Final `#else` block should look like:
```cpp
#else
// -------- Dummy defaults (compile-friendly) --------
#define THINGSBOARD_SERVER "myURL"
#define THINGSBOARD_PORT 1883

#define FACTORY_SERVER 0
#define DEMO_SERVER 1

#define THINGSBOARD_PROVISION_SERVER FACTORY_SERVER

#define PROVISION_DEVICE_KEY "mydevicekey"
#define PROVISION_DEVICE_SECRET "mydevicekeysecret"

#define WIFI_SSID "myssid"
#define WIFI_PASSWORD "mypassword"

#define WEB_SERVER_USERNAME "incunest"
#define WEB_SERVER_PASSWORD "changeme"
#endif
```

- [ ] **Step 3: Commit**

```bash
git add Display_HMI/include/protocol/Credentials_public.h
git commit -m "feat(display_hmi): add WEB_SERVER_USERNAME/PASSWORD to credentials"
```

(Do NOT stage `Display_HMI/include/Credentials.h` — it is gitignored.)

---

## Task 2: Display HMI — use macros in Wifi_OTA.cpp

**Files:**
- Modify: `Display_HMI/src/tasks/Wifi_OTA.cpp`

**Interfaces:**
- Consumes: `WEB_SERVER_USERNAME`, `WEB_SERVER_PASSWORD` from Task 1 (available via `#include "main.h"` chain).

- [ ] **Step 1: Remove hardcoded credential variables**

In `Display_HMI/src/tasks/Wifi_OTA.cpp`, remove lines 62–63:
```cpp
const char *www_username = "in3admin";
const char *www_password = "savinglives";
```

- [ ] **Step 2: Replace all uses of the removed variables**

Find all occurrences of `www_username` and `www_password` in `Wifi_OTA.cpp` (there are 5, all inside `wifiServer.authenticate()` calls in `configWifiServer()`). Replace each pair:

```cpp
// Before:
wifiServer.authenticate(www_username, www_password)

// After:
wifiServer.authenticate(WEB_SERVER_USERNAME, WEB_SERVER_PASSWORD)
```

Affected routes: `/` (line ~221), `/serverIndex` (line ~228), `/set_freq` (line ~245), `/update` completion handler (line ~259), `/update` upload handler (line ~267).

- [ ] **Step 3: Verify build compiles**

In PlatformIO, build the `main` environment:
```
pio run -e main -d Display_HMI
```
Expected: build succeeds with no errors about `www_username` or `www_password`.

- [ ] **Step 4: Commit**

```bash
git add Display_HMI/src/tasks/Wifi_OTA.cpp
git commit -m "refactor(display_hmi): replace hardcoded web credentials with WEB_SERVER_USERNAME/PASSWORD macros"
```

---

## Task 3: Motherboard — Credentials files

**Files:**
- Modify: `motherBoard/include/Credentials.h`
- Modify: `motherBoard/include/protocol/Credentials_public.h`

**Interfaces:**
- Produces: `WEB_SERVER_USERNAME` and `WEB_SERVER_PASSWORD` macros available to any file that includes `main.h`.

- [ ] **Step 1: Add real credentials to `Credentials.h`**

Open `motherBoard/include/Credentials.h`. Add before the closing `#endif`:

```cpp
#define WEB_SERVER_USERNAME "incunestadmin"
#define WEB_SERVER_PASSWORD "savinglives"
```

Final file:
```cpp
#ifndef _CREDENTIALS_
#define _CREDENTIALS_

#define THINGSBOARD_SERVER "mon.medicalopenworld.org"
#define THINGSBOARD_PORT 1883

#define PROVISION_DEVICE_KEY "8f9yvlkqxirz2pq9n5co"
#define PROVISION_DEVICE_SECRET "bz9fwzi8t3pnqxdlipqz"

#define WIFI_SSID "in3wifi"
#define WIFI_PASSWORD "12345678"

#define WEB_SERVER_USERNAME "incunestadmin"
#define WEB_SERVER_PASSWORD "savinglives"
#endif // _CREDENTIALS_
```

- [ ] **Step 2: Add placeholder fallbacks to `Credentials_public.h`**

Open `motherBoard/include/protocol/Credentials_public.h`. In the `#else` block, add after the last `#define`:

```cpp
#define WEB_SERVER_USERNAME "incunest"
#define WEB_SERVER_PASSWORD "changeme"
```

Final `#else` block:
```cpp
#else
// -------- Dummy defaults (compile-friendly) --------
#define THINGSBOARD_SERVER "myURL"
#define THINGSBOARD_PORT 1883

#define PROVISION_DEVICE_KEY "mydevicekey"
#define PROVISION_DEVICE_SECRET "mydevicekeysecret"

#define WIFI_SSID "myssid"
#define WIFI_PASSWORD "mypassword"

#define WEB_SERVER_USERNAME "incunest"
#define WEB_SERVER_PASSWORD "changeme"
#endif
```

- [ ] **Step 3: Commit**

```bash
git add motherBoard/include/protocol/Credentials_public.h
git commit -m "feat(motherboard): add WEB_SERVER_USERNAME/PASSWORD to credentials"
```

(Do NOT stage `motherBoard/include/Credentials.h` — it is gitignored.)

---

## Task 4: Motherboard — enforce server-side auth on sensitive routes

**Files:**
- Modify: `motherBoard/src/tasks/Wifi_OTA.cpp`

**Interfaces:**
- Consumes: `WEB_SERVER_USERNAME`, `WEB_SERVER_PASSWORD` from Task 3.

Context: The motherboard currently shows a JS login page at `/` that redirects to `/serverIndex` on success — but `/serverIndex`, `/update`, and `/config` have no server-side auth check. Anyone who knows the URL bypasses the JS form entirely.

- [ ] **Step 1: Protect `/serverIndex`**

In `motherBoard/src/tasks/Wifi_OTA.cpp`, find the `/serverIndex` handler (around line 361):

```cpp
// Before:
wifiServer.on("/serverIndex", HTTP_GET, []() {
  wifiServer.sendHeader("Connection", "close");
  wifiServer.send(200, "text/html", serverIndex);
});

// After:
wifiServer.on("/serverIndex", HTTP_GET, []() {
  if (!wifiServer.authenticate(WEB_SERVER_USERNAME, WEB_SERVER_PASSWORD)) {
    return wifiServer.requestAuthentication();
  }
  wifiServer.sendHeader("Connection", "close");
  wifiServer.send(200, "text/html", serverIndex);
});
```

- [ ] **Step 2: Protect `/update` (both handlers)**

Find the `/update` handler (around line 456). It has two lambdas — the completion handler and the upload handler. Add auth to both:

```cpp
// Before:
wifiServer.on(
    "/update", HTTP_POST,
    []() {
      wifiServer.sendHeader("Connection", "close");
      wifiServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      ESP.restart();
    },
    []() {
      HTTPUpload &upload = wifiServer.upload();
      ...
    });

// After:
wifiServer.on(
    "/update", HTTP_POST,
    []() {
      if (!wifiServer.authenticate(WEB_SERVER_USERNAME, WEB_SERVER_PASSWORD)) {
        return wifiServer.requestAuthentication();
      }
      wifiServer.sendHeader("Connection", "close");
      wifiServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      ESP.restart();
    },
    []() {
      if (!wifiServer.authenticate(WEB_SERVER_USERNAME, WEB_SERVER_PASSWORD)) return;
      HTTPUpload &upload = wifiServer.upload();
      ...
    });
```

- [ ] **Step 3: Protect `/config` (GET and POST)**

Find both `/config` handlers (around lines 380 and 403). Add auth to both:

```cpp
// GET — Before:
wifiServer.on("/config", HTTP_GET, []() {
  wifiServer.sendHeader("Connection", "close");
  wifiServer.send(200, "text/html", configIndex);
});

// GET — After:
wifiServer.on("/config", HTTP_GET, []() {
  if (!wifiServer.authenticate(WEB_SERVER_USERNAME, WEB_SERVER_PASSWORD)) {
    return wifiServer.requestAuthentication();
  }
  wifiServer.sendHeader("Connection", "close");
  wifiServer.send(200, "text/html", configIndex);
});

// POST — Before:
wifiServer.on("/config", HTTP_POST, []() {
  ...body unchanged...
  wifiServer.send(200, "text/plain", "Saved. Settings applied immediately.");
});

// POST — After:
wifiServer.on("/config", HTTP_POST, []() {
  if (!wifiServer.authenticate(WEB_SERVER_USERNAME, WEB_SERVER_PASSWORD)) {
    return wifiServer.requestAuthentication();
  }
  ...body unchanged...
  wifiServer.send(200, "text/plain", "Saved. Settings applied immediately.");
});
```

- [ ] **Step 4: Verify build compiles**

```
pio run -e main -d motherBoard
```
Expected: build succeeds with no errors.

- [ ] **Step 5: Commit**

```bash
git add motherBoard/src/tasks/Wifi_OTA.cpp
git commit -m "feat(motherboard): enforce HTTP Basic Auth on /serverIndex, /update, /config"
```
