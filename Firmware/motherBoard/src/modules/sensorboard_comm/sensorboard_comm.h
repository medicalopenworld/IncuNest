#pragma once
// Enlace USB con el SensorBoard: la motherboard actua de USB HOST y el
// SensorBoard es un dispositivo CDC-ACM nativo (TinyUSB, VID 0x303A /
// PID 0x4001). Este modulo abre el dispositivo, reensambla el framing
// (sb_frame_parser), decodifica el JSON (sb_json_codec) y mantiene el ultimo
// valor conocido de cada magnitud.
//
// Lo que NO hace, a proposito: no alimenta el lazo de control termico ni la
// UI. La telemetria del SensorBoard es auxiliar y sus dos alarmas
// (ALARM_SENSORBOARD_LINK_LOST, ALARM_SENSORBOARD_DOOR_FAULT) no cortan el
// calefactor -- ver alarm_policy.cpp.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Una magnitud por posicion fisica de sensor. valid[i]=false = ese sensor
// llego como null (caido); la fusion/votacion es de quien consuma esto,
// nunca de este modulo (ADR-0002 del SensorBoard).
typedef struct {
  bool valid[3];
  float value[3];
} SbTriple;

typedef struct {
  bool link_ok;  // heartbeat dentro de los ultimos 90 s
  SbTriple temp;
  SbTriple hum;
  bool lux_valid;
  float lux;
  bool dba_valid;
  float dba;
  bool door_known;
  bool door_open;
  bool door_faulty;  // hall sospechoso (flapping)
} SbSnapshot;

// Instala USB Host + driver CDC-ACM y arranca el demonio de eventos USB.
// Debe llamarse DESPUES de initSPO2(): al levantar el host USB se reclama
// GPIO19, que en V15 es el MISO del AFE (ver el comentario en main.cpp).
void sensorboard_comm_init(void);

// Tarea: reconexion del dispositivo y evaluacion periodica del enlace.
void sensorboard_comm_task(void *pv);

bool sensorboard_comm_connected(void);

// Copia coherente del ultimo estado conocido (tomada bajo mutex).
void sensorboard_get_snapshot(SbSnapshot *out);

// Pide una captura JPEG. false si el enlace esta caido o ya hay una en
// vuelo. El resultado llega de forma asincrona.
bool sensorboard_capture_request(void);

// Ultima captura completada. El puntero es propiedad de este modulo y sigue
// siendo valido hasta que se complete la SIGUIENTE captura.
bool sensorboard_capture_result(const uint8_t **jpeg, size_t *len);

#ifdef ARDUINO
#include <ArduinoJson.h>
// Anade las claves sb_* al JSON de telemetria. Vive aqui y no duplicado en
// GPRS.cpp y Wifi_OTA.cpp porque la parte delicada -- omitir la clave cuando
// la lectura no es valida, en vez de publicar un cero que parece una medida
// -- tiene que decirse una sola vez.
void sensorboard_add_telemetry(JsonObject &json);
#endif
