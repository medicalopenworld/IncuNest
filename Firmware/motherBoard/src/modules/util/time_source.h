#pragma once
// Arbitro del INSTANTE del reloj de pared. Hermano de tz_source, que arbitra
// el HUSO.
//
// El reloj lo fijaban cuatro rutas que no se hablaban entre si: SNTP por WiFi
// (Wifi_OTA, DriveUpload), SNTP por PPP (gprs_modem), NITZ/NTP del modem
// (GPRS.cpp) y la entrada manual de /config. Solo la ultima declaraba
// prioridad —un booleano y un esp_sntp_stop()—, asi que las otras tres se
// pisaban por orden de llegada: la ultima en contestar ganaba, fuese la mejor
// o la peor. Este modulo pone ese orden por escrito y lo hace comprobable.
//
// Logica pura a proposito, igual que tz_source: sin Arduino, sin red, sin
// settimeofday() y sin estado global del firmware, para que entre en los
// tests de host (tools/host_tests) y se pruebe de verdad con Unity. Quien
// toca el reloj del sistema es system_clock, que consulta a este modulo
// primero.
//
// El rango vive SOLO EN RAM, igual que la marca manual que sustituye: un
// ciclo de alimentacion pierde el reloj de todas formas, asi que tras un
// reinicio las fuentes automaticas vuelven a tener via libre.
#include <stdbool.h>
#include <stdint.h>

#include "protocol.h" // Proto_TimeSource

#ifdef __cplusplus
extern "C" {
#endif

// Misma ventana que civil_to_unix_utc(): 2021-01-01 .. 2100-01-01.
// Un RTC con la pila agotada devuelve basura y buena parte de esa basura cae
// en 1970, asi que el filtro no es decorativo.
#define TIME_SOURCE_MIN_EPOCH 1609459200u
#define TIME_SOURCE_MAX_EPOCH 4102444800u

// Vuelve al estado "sin hora". Existe para los tests y para nada mas: en el
// firmware el rango se pierde reiniciando, que es justo lo que se quiere.
void time_source_reset(void);

// True si `src` puede fijar el reloj ahora mismo, o sea si su rango es mayor
// o igual que el vigente. SIN EFECTOS: se consulta antes de decidir si vale
// la pena llamar a settimeofday(), y preguntarlo dos veces da lo mismo.
bool time_source_accepts(Proto_TimeSource src);

// Registra que `src` ha fijado el reloj en `epoch`. Devuelve true solo si el
// estado ha cambiado, o sea si la fuente gana Y el epoch es valido.
//
// Rangos iguales: gana el mas reciente. Un NTP que vuelve a sincronizar tiene
// que poder corregir la deriva, y el operador tiene que poder corregirse a si
// mismo; sin esto el primer valor quedaria congelado de por vida.
//
// Un epoch fuera de ventana se rechaza SIEMPRE, venga de donde venga, y nunca
// degrada una hora valida ya obtenida de una fuente peor.
bool time_source_set(uint32_t epoch, Proto_TimeSource src);

// Rango vigente, PROTO_TIME_SOURCE_NONE si el reloj no se ha fijado. Es lo
// que se difunde en el campo `src` de CTRL,TIME.
Proto_TimeSource time_source_origin(void);

// Ultimo epoch aceptado, 0 si no hay ninguno. NO es la hora actual: el reloj
// del sistema avanza y este valor no. Sirve para los tests y para diagnostico,
// no para leer la hora — para eso esta time(nullptr).
uint32_t time_source_epoch(void);

bool time_source_known(void);

#ifdef __cplusplus
}
#endif
