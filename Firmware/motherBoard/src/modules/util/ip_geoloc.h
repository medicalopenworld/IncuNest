#pragma once

#include <stdbool.h>

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
