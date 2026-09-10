#pragma once
#include <stdbool.h>
#include <stdint.h>

// Activacion de la SIM Onomondo de la unidad contra https://api.onomondo.com,
// cuerpo del test de fabrica FTEST_MB_SIM_ACT.
//
// Vive en su propio modulo, y no como un cuerpo mas de factory_test_hw.cpp,
// porque necesita BLOQUEAR: un handshake TLS mas dos peticiones HTTPS tardan
// segundos, y los cuerpos pasivos del test de fabrica no pueden bloquear ni
// hacer su propio vTaskDelay -- el runner los sondea en pasos de <= 250 ms
// (ver la cabecera de factory_test_hw.h). El trabajo real corre aqui en una
// tarea propia de vida corta; el pasivo la arranca UNA vez y despues solo
// observa el resultado, el mismo patron que sb_status/sb_camera usan con las
// peticiones asincronas a sensorboard_comm.
//
// La clave de la API sale de ONOMONDO_API_KEY (Credentials.h, no versionado;
// Credentials_public.h da un valor dummy para que compile tras un clone
// limpio). Nunca se escribe en un log ni en el detail del test.

// SOLO se compila el camino real en los entornos *_factory de platformio.ini,
// que son los unicos que ponen esto a 1.
//
// Por que: Credentials_public.h hace `#if __has_include("Credentials.h")`, asi
// que en la maquina de quien tenga el Credentials.h real TODOS los builds
// llevaban la clave dentro -- incluido el IncuNest_V18 que alimenta
// flasher_tool/data/firmware/ y los assets de GitHub Releases, en un repo
// PUBLICO. Y ONOMONDO_API_KEY no es la credencial de una unidad: controla
// TODAS las SIM de la organizacion.
//
// No basta con sobrescribir la clave a la dummy por -D: dependeria del orden
// de los #define y de un fichero que no esta versionado. Lo que se apaga aqui
// es la FUNCIONALIDAD, asi que con esto a 0 el fichero no NOMBRA la clave en
// ningun sitio y por tanto no puede acabar en el binario.
//
// El firmware de campo no tiene nada que hacer activando SIMs: eso es un paso
// de fabrica. Con esto a 0, el cuerpo del test devuelve UNREACHABLE (WARN) con
// el motivo, nunca un PASS silencioso.
#ifndef FTEST_SIM_ACT_ENABLED
#define FTEST_SIM_ACT_ENABLED 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  FTEST_SIM_IDLE = 0,        // sin arrancar
  FTEST_SIM_RUNNING,         // peticion en vuelo
  FTEST_SIM_ALREADY_ACTIVE,  // ya estaba activada: no se toco nada
  FTEST_SIM_ACTIVATED,       // activada en esta pasada
  // La API contesto y la SIM NO queda activada (404/4xx en el GET, cuerpo sin
  // "activated", PATCH rechazado): la SIM de esta unidad sale de fabrica sin
  // activar y eso es FAIL.
  FTEST_SIM_ERROR,
  // No se pudo hablar con la API (sin TLS/HTTP, 5xx/429 persistentes, sin
  // clave en el build, sin tarea): no sabemos el estado de la SIM. No es un
  // fallo de la placa -> el test es WARN con el motivo, para repetirlo con red.
  FTEST_SIM_UNREACHABLE,
} FtestSimState;

// Arranca la tarea de activacion para `iccid`. Idempotente: si ya hay una en
// marcha o ya termino, no hace nada y devuelve false. `iccid` se copia.
// Requiere WiFi ya conectada (el llamante lo comprueba).
bool ftest_sim_activation_start(const char *iccid);

FtestSimState ftest_sim_activation_state(void);

// Detalle corto para el `detail` del test (cadena vacia si no hay nada).
// Valido en cuanto el estado deja de ser RUNNING.
const char *ftest_sim_activation_detail(void);

// Deja el modulo listo para otra tanda. La llama
// ftest_hw_reset_passive_state() al arrancar cada bateria/RUN. No corta una
// tarea ya en vuelo (no hay forma seria de abortar un handshake TLS a medias):
// si la hubiera, se respeta y este reset no hace nada.
void ftest_sim_activation_reset(void);

#ifdef __cplusplus
}
#endif
