#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Permanencia del equipo en una red WiFi: en cual esta y desde cuando.
//
// Para que existe: un equipo instalado en un hospital con WiFi propia ya no
// usa la SIM para nada — GPRS_Handler() deja de publicar telemetria en cuanto
// hay WiFi (GPRS.cpp) y el modem solo sigue atado a la red movil para la
// posicion y el reloj — pero la SIM sigue costando su cuota. Para poder darla
// de baja hace falta saber que el equipo ha LLEGADO a su destino, y el
// problema real es que muchas unidades se montan en un sitio y solo despues
// se llevan al hospital: dar de baja la SIM en el sitio del montaje deja la
// unidad sin ningun enlace para el viaje y sin forma de recuperarse.
//
// Las horas de uso no sirven para distinguir esos dos sitios: in3.control_
// active_time sube igual en el montaje, en el banco y en los cursos de
// formacion. Lo que si distingue es el TIEMPO en la misma red, y eso es lo
// que cuenta este modulo.
//
// Lo que este modulo NO hace, a proposito: decidir. El umbral (14 dias), la
// lista de redes propias de la organizacion y la llamada a Onomondo viven en
// el servidor. Aqui no hay ninguna constante de politica, y el equipo no
// tiene forma de dar de baja su propia SIM por error. La razon de fondo es
// que ONOMONDO_API_KEY controla TODA la flota y los firmware.bin de release
// estan publicados en un repo publico.
//
// Logica pura a proposito, como tz_source: sin Arduino y sin Preferences,
// para que entre en [env:native] y se pruebe de verdad con Unity. El estado
// es un struct que persiste el llamante (Wifi_OTA.cpp), y se escribe en NVS
// SOLO cuando wifi_dwell_update() dice que ha cambiado algo — con eso un
// enlace que parpadea no produce ni una escritura.

#ifdef __cplusplus
extern "C" {
#endif

// 802.11: un SSID son 32 bytes como maximo, y no tiene por que ser texto.
#define WIFI_DWELL_SSID_MAX 32

// Umbral de validez del reloj: 2021-01-01T00:00:00Z. El mismo que usa
// ftest_sim_activation.cpp para decidir si su marca de tiempo vale.
#define WIFI_DWELL_EPOCH_VALID 1609459200u

typedef struct {
  char ssid[WIFI_DWELL_SSID_MAX + 1]; // red vigilada, NUL-terminada
  uint32_t firstEpoch;                // primera asociacion con reloj; 0 = aun no
  uint32_t lastDayIndex;              // dia UTC (epoch/86400) del ultimo contado
  uint16_t days;                      // dias UTC distintos en esta red
} WifiDwell;

// Deja el estado como el de un equipo que nunca se ha asociado.
void wifi_dwell_clear(WifiDwell *st);

// Aplica una asociacion a `ssid` observada en `nowEpoch` (0 = reloj sin poner
// en hora). Devuelve true SOLO si el estado ha cambiado, y entonces el
// llamante tiene que persistirlo.
//
// Reglas:
//   - Cambio de SSID -> reset completo a la red nueva, con reloj o sin el. Si
//     no fuera asi, un equipo movido antes de tener hora seguiria acumulando
//     dias a nombre de la red anterior, que es justo el fallo que esta
//     feature existe para evitar.
//   - Misma SSID y reloj valido -> se cuenta un dia mas solo si el dia UTC ha
//     cambiado respecto al ultimo contado. Dias DISTINTOS, no tiempo
//     transcurrido: asi un equipo embalado y desenchufado dos semanas tras
//     una sola asociacion no acumula nada.
//   - Misma SSID sin reloj valido -> no se cuenta nada y no se toca el
//     estado.
//   - `ssid` nulo o vacio no es una asociacion: no cambia nada.
//
// El limite del dia es la medianoche UTC y no la local a proposito: la zona
// puede resolverse horas despues de la asociacion, o no resolverse nunca, y
// UTC es el unico limite que no se mueve por debajo del contador.
//
// Un reloj puesto a mano hacia atras puede contar un dia de mas al volver a
// avanzar. Es un error acotado a un par de dias frente a un umbral de 14, y
// el servidor ve tambien firstEpoch para cruzarlo.
bool wifi_dwell_update(WifiDwell *st, const char *ssid, uint32_t nowEpoch);

// Dias transcurridos desde la primera asociacion. 0 si no hay ancla, si el
// reloj no es valido o si ha retrocedido por debajo del ancla — nunca un
// negativo envuelto.
uint16_t wifi_dwell_span_days(const WifiDwell *st, uint32_t nowEpoch);

// Copia `in` a `out` dejando solo ASCII imprimible (0x20..0x7E) y sustituyendo
// cualquier otro byte por '?'. Trunca a outSize-1 y termina siempre en NUL.
//
// Un SSID son bytes arbitrarios: ArduinoJson escapa bien la comilla y la
// barra invertida, pero no arregla un UTF-8 invalido, y eso dentro de un
// payload MQTT es un problema del broker. Se corta en el origen.
void wifi_dwell_sanitize_ssid(const char *in, char *out, size_t outSize);

#ifdef __cplusplus
}
#endif
