# Webserver Credentials — Design Spec
Date: 2026-06-17

## Goal

Move hardcoded webserver credentials out of source files and into a gitignored `credentials.h`, while ensuring the project compiles for anyone who clones the repo without that file.

## Pattern

Each project gets two files:
- `credentials_template.h` — committed, contains default/placeholder values, serves as documentation and compile fallback.
- `credentials.h` — gitignored, contains real credentials.

Include logic in `Wifi_OTA.cpp`:
```cpp
#if __has_include("credentials.h")
#include "credentials.h"
#else
#include "credentials_template.h"
#endif
```

---

## Display HMI

### New files
- `Display_HMI/include/credentials_template.h` — committed, defines `WEB_SERVER_USERNAME` and `WEB_SERVER_PASSWORD` with placeholder values.
- `Display_HMI/include/credentials.h` — gitignored, defines real credentials (`incunestadmin` / `savinglives`).

### Changes to `Display_HMI/src/tasks/Wifi_OTA.cpp`
- Remove the two hardcoded `const char*` lines (`www_username`, `www_password`).
- Add the `#if __has_include` block above.
- Replace all uses of `www_username` / `www_password` with `WEB_SERVER_USERNAME` / `WEB_SERVER_PASSWORD`.

Authentication is already enforced on all sensitive routes (`/`, `/serverIndex`, `/set_freq`, `/update`) — no route changes needed.

---

## Motherboard

`motherBoard/include/Credentials.h` already exists and is committed (ThingsBoard server, provisioning keys, WiFi SSID/password). It is **not** the file to gitignore — web credentials go in a separate file.

### New files
- `motherBoard/include/credentials_template.h` — committed, defines `WEB_SERVER_USERNAME` and `WEB_SERVER_PASSWORD` with placeholder values.
- `motherBoard/include/credentials.h` — gitignored, defines real credentials (`incunestadmin` / `savinglives`).

### Include logic in `motherBoard/src/tasks/Wifi_OTA.cpp`
Add the same `#if __has_include` block used in Display HMI:
```cpp
#if __has_include("credentials.h")
#include "credentials.h"
#else
#include "credentials_template.h"
#endif
```

### Route protection in `motherBoard/src/tasks/Wifi_OTA.cpp`
Add `wifiServer.authenticate(WEB_SERVER_USERNAME, WEB_SERVER_PASSWORD)` checks to:
- `/serverIndex` (currently unprotected — JS login is client-side only, easily bypassed)
- `/update` (currently unprotected)
- `/config` GET and POST (currently unprotected)

The `/` route (JS login page) is left unchanged — it is cosmetic UX, not a security gate.

---

## .gitignore

Add `credentials.h` to `.gitignore` in both `Display_HMI/` and `motherBoard/` (or the repo root if a root `.gitignore` exists).

---

## Credentials

| Field    | Value           |
|----------|-----------------|
| Username | `incunestadmin` |
| Password | `savinglives`   |
