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
// El respaldo era SILENCIOSO: una unidad construida sin Credentials.h salia
// con la contrasena de WEB_SERVER_PASSWORD que hay mas abajo, y este
// repositorio es publico. Esa misma contrasena es la unica puerta de /update,
// que acepta un binario arbitrario.
//
// Con -DREQUIRE_REAL_CREDENTIALS el respaldo pasa a ser un error de
// compilacion. Es lo que debe llevar cualquier build que vaya a una placa;
// sin el, un clon nuevo sigue compilando y solo avisa.
#ifdef REQUIRE_REAL_CREDENTIALS
#error "Falta include/Credentials.h y se ha pedido REQUIRE_REAL_CREDENTIALS: este build saldria con las credenciales publicas del repositorio."
#endif
#warning "Sin include/Credentials.h: se compila con las credenciales PUBLICAS del repositorio. No flashees esto en una unidad."
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

// -------- Soporte (menu de ayuda, spec hmi-help-center) --------
// No son secretos: viven aqui porque este es el fichero de configuracion de
// despliegue, y con guardas #ifndef para que un Credentials.h local pueda
// redefinirlos (otro buzon, otra URL) sin tocar codigo versionado.
#ifndef SUPPORT_EMAIL
#define SUPPORT_EMAIL "support@medicalopenworld.org"
#endif
#ifndef SUPPORT_TUTORIAL_URL
#define SUPPORT_TUTORIAL_URL "https://medicalopenworld.org/incunest/tutorial"
#endif
// Destinatario del certificado de los cursos de formacion (spec
// hmi-training-courses). Por defecto el mismo buzon de soporte.
#ifndef TRAINING_EMAIL
#define TRAINING_EMAIL SUPPORT_EMAIL
#endif

#endif // _CREDENTIALS_PUBLIC_H_
