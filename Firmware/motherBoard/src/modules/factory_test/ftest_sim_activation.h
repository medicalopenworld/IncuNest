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

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  FTEST_SIM_IDLE = 0,        // sin arrancar
  FTEST_SIM_RUNNING,         // peticion en vuelo
  FTEST_SIM_ALREADY_ACTIVE,  // ya estaba activada: no se toco nada
  FTEST_SIM_ACTIVATED,       // activada en esta pasada
  FTEST_SIM_ERROR,           // no se pudo activar -> el test es FAIL
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
