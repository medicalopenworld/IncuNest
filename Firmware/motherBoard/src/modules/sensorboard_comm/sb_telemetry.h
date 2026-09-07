#pragma once
// Mapeo del ultimo estado conocido del SensorBoard a las claves sb_* de
// ThingsBoard. Puro: no lee el snapshot ni el reloj, se le pasan -- asi el
// mapeo se prueba en host, igual que el resto de los sb_*.cpp del modulo.
// sensorboard_add_telemetry() es solo el envoltorio que toma el snapshot bajo
// mutex y llama aqui.
//
// LA PROPIEDAD QUE IMPORTA, y por la que esto tiene test propio: una posicion
// caida OMITE su clave, no manda un cero. Un cero en la nube es
// indistinguible de una medida, y en el cuadro de mando un SHT40 averiado se
// veria como un sensor vivo a 0 C. Es el mismo modo de fallo que el commit
// fb3a535: no un hueco, un valor plausible que nadie mira dos veces.
#include <ArduinoJson.h>
#include <stdint.h>

#include "sensorboard_comm.h"  // SbSnapshot

// `now` = millis() del llamante. `env_used` = cuantas posiciones sostienen la
// temperatura de aire (sb_fuse().used); vive en el modulo, no en el snapshot,
// porque lo produce el camino de control y no el de recepcion.
void sb_build_telemetry(JsonObject &json, const SbSnapshot &s, uint32_t now,
                        uint8_t env_used);
