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

// Encendido por defecto desde el 2026-09-20: el test de hardware activa la SIM
// si no lo esta, en cualquier build. Antes hacia falta compilar un entorno
// *_factory aparte, y acordarse de hacerlo en la linea de montaje era un paso
// de mas que no aportaba nada al operario.
//
// LO QUE ESTO IMPLICA, y no hay que perder de vista: Credentials_public.h hace
// `#if __has_include("Credentials.h")`, asi que en una maquina con el
// Credentials.h real TODO binario lleva dentro ONOMONDO_API_KEY. Y esa clave no
// es la credencial de una unidad: controla TODAS las SIM de la organizacion.
// Un `strings firmware.bin` la saca entera.
//
// De donde NO sale:
//   - GitHub Releases: los compila el runner de CI, que no tiene Credentials.h
//     y usa los valores dummy (.github/workflows/release.yml).
//   - El paquete del flasher: build.bat corre check_no_secrets.py, que busca la
//     clave DENTRO de los .bin de data/firmware/ y se niega a dar las
//     instrucciones de empaquetado si aparece.
//
// De donde SI saldria: cualquier .bin compilado aqui que se mande por correo,
// se suba a un drive o se adjunte a un issue. Eso ya no lo para el nombre de
// un entorno; hay que mirar el contenido.
//
// Poner esto a 0 (-DFTEST_SIM_ACT_ENABLED=0) deja un binario en el que la clave
// no se nombra y por tanto no existe: es la salida si alguna vez hace falta un
// firmware publicable de verdad. Con 0 el cuerpo del test devuelve UNREACHABLE
// (WARN) con el motivo, nunca un PASS silencioso.
#ifndef FTEST_SIM_ACT_ENABLED
#define FTEST_SIM_ACT_ENABLED 1
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
