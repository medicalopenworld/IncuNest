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

#define PROVISION_DEVICE_KEY "mydevicekey"
#define PROVISION_DEVICE_SECRET "mydevicekeysecret"

#define WIFI_SSID "myssid"
#define WIFI_PASSWORD "mypassword"

#define WEB_SERVER_USERNAME "incunest"
#define WEB_SERVER_PASSWORD "changeme"
#endif

// Clave de la API de Onomondo, usada SOLO por el test de fabrica para activar
// la SIM de la unidad (FTEST_MB_SIM_ACT). El valor real va en Credentials.h:
//
//     #define ONOMONDO_API_KEY "onok_xxxxxxxx.xxxxxxxxxxxxxxxxxxxxxxxx"
//
// El fallback va con #ifndef y FUERA del #if __has_include de arriba a
// proposito: asi un Credentials.h ya existente que todavia no declare esta
// clave sigue compilando. Con el valor dummy el test da FAIL con detail
// "sin key", que es exactamente lo que debe pasar fuera de fabrica -- nunca
// un PASS silencioso.
#ifndef ONOMONDO_API_KEY
#define ONOMONDO_API_KEY "myonomondokey"
#endif
#define ONOMONDO_API_KEY_DUMMY "myonomondokey"

#endif // _CREDENTIALS_PUBLIC_H_
