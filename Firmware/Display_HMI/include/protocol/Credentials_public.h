#ifndef _CREDENTIALS_PUBLIC_H_
#define _CREDENTIALS_PUBLIC_H_

// This project expects real credentials to live in a local file named
// "Credentials.h" That file is intentionally NOT tracked by git (it contains
// secrets).
//
// If the local file exists, we include it.
// Otherwise, we fall back to safe dummy values so the project compiles after a
// fresh clone.

#if __has_include("Credentials.h")
#include "Credentials.h"
#else
// -------- Dummy defaults (compile-friendly) --------
#define THINGSBOARD_SERVER "myURL"
#define THINGSBOARD_PORT 1883 // default port

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

#endif // _CREDENTIALS_PUBLIC_H_
