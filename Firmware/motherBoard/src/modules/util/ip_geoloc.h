#pragma once

#include <stdbool.h>
#include <stdint.h>

// Posicion aproximada del equipo a partir de su IP publica.
//
// Para que existe: la unica fuente de posicion de la flota es la
// triangulacion por torres del modem (tri_latitud/tri_longitud, ver
// GPRSUpdateLocationIfDue()), y TX_FEATURE_TRIANGULATION_WIFI esta a 0
// porque el WiFi no puede producir un fix de torre. Al dar de baja la SIM de
// una unidad instalada en un hospital con WiFi propia, esa fuente
// desaparece: la unidad se sale del mapa de la flota. Esto la mantiene
// dentro, con precision de ciudad.
//
// No sustituye al fix GSM: lo suple cuando NO hay ninguno. El fix GSM manda
// siempre, y la clave loc_source dice cual de los dos ha producido el valor
// publicado.
//
// El dato sale de la consulta que ensureWifiTimeZoneSynced() (Wifi_OTA.cpp)
// ya hace una vez al dia a ip-api.com para la zona horaria: solo crece la
// lista fields=. Ni una peticion mas, ni un host mas, ni otra cadencia.
//
// OJO: esa consulta viaja en HTTP EN CLARO — el nivel gratuito del servicio
// no tiene TLS — asi que la respuesta es manipulable por quien este en
// medio. Por eso el resultado es MERAMENTE INFORMATIVO: llega a dos claves
// de telemetria y a nada mas. No toca el PID, ni las alarmas, ni ningun
// actuador, ni el reloj, ni la zona horaria, ni ningun registro de paciente.
// Lo peor que consigue quien manipule la respuesta es un punto mal puesto en
// un mapa. Es el mismo riesgo ya aceptado para la zona horaria y por los
// mismos motivos.

#ifdef __cplusplus
extern "C" {
#endif

// Precision que se publica en tri_accuracy para un fix por IP, en metros.
// "En algun sitio de esta ciudad": esta ahi para que nadie lea estas
// coordenadas como un fix de torre y menos como un GPS.
#define IP_GEOLOC_ACCURACY_M 25000

// Cada cuanto se repone la posicion aunque no haya cambiado, en ms. 6 h.
//
// Es un LATIDO, no la cadencia normal: lo normal es publicar solo cuando la
// posicion cambia. Existe porque el montaje de la telemetria no sabe si el
// publish llego a salir (broker caido, JSON truncado), y sin esto un fallo
// puntual dejaria al equipo sin posicion hasta que le cambiara la IP publica.
#define IP_GEOLOC_REPUBLISH_MS 21600000u

// Decide si toca meter la posicion por IP en la telemetria.
//
// POR QUE ESTO NO SE PUBLICA EN CADA CICLO: el root chain de ThingsBoard manda
// toda la telemetria al chain "Country estimator", que llama a Nominatim
// mientras el equipo no tenga guardado el atributo `country`. Por WiFi se
// publica cada TX_WIFI_PUBLISH_MS (5 s), asi que soltar ahi la posicion en
// cada ciclo son ~17.000 peticiones al dia POR UNIDAD, todas con el mismo
// valor. Eso banea la IP del servidor y se lleva por delante la estimacion de
// pais de toda la flota. La posicion por IP cambia una vez al dia como mucho:
// la cadencia correcta es "cuando cambia", con un latido de respaldo.
//
// `everPublished` false ignora el resto de los parametros previos.
// La resta de tiempos es SIN SIGNO a proposito: millis() desborda a los 49,7
// dias y con una resta con signo un equipo que cruzara ese punto dejaria de
// publicar la posicion para siempre.
bool ip_geoloc_due(bool everPublished, float pubLat, float pubLon,
                   uint32_t pubMs, float lat, float lon, uint32_t nowMs);

// Extrae lat/lon del cuerpo de una respuesta de ip-api.com.
//
// Devuelve false ante cualquier duda —sin "status":"success", campo ausente,
// valor entrecomillado, JSON truncado, fuera de rango fisico, o el 0,0 que
// media industria usa como "no lo se"— y entonces NO toca *lat ni *lon.
// Politica de descarte silencioso de .claude/rules/security.md: esto parsea
// texto que viene de la red y nunca debe producir un dato a medias.
bool ip_geoloc_parse(const char *json, float *lat, float *lon);

#ifdef __cplusplus
}
#endif
