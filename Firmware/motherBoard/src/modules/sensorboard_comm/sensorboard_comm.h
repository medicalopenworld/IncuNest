#pragma once
// Enlace USB con el SensorBoard: la motherboard actua de USB HOST y el
// SensorBoard es un dispositivo CDC-ACM nativo (TinyUSB, VID 0x303A /
// PID 0x4001). Este modulo abre el dispositivo, reensambla el framing
// (sb_frame_parser), decodifica el JSON (sb_json_codec) y mantiene el ultimo
// valor conocido de cada magnitud con su marca de frescura.
//
// EN UN EQUIPO CON SENSORBOARD ESTO NO ES TELEMETRIA AUXILIAR: sus SHT40 son
// el sensor de aire y de humedad de la incubadora, es decir la variable del
// PID y la fuente de los cortes termicos. sensorboard_apply_room_sensor() es
// el puente hacia in3.temperature[]/in3.humidity[], y lo llama la tarea de
// sensores -- la misma que escribe esas variables en el camino I2C -- para no
// introducir escrituras concurrentes sobre datos clinicos.
//
// El modulo solo se arranca si la deteccion de arranque eligio
// SENSOR_SOURCE_SENSORBOARD (ver modules/sensors/sensor_source.h). En un
// equipo antiguo no existe, y por eso su alarma de enlace no puede sonar en
// una flota que no lleva la placa.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Una magnitud por posicion fisica de sensor. valid[i]=false = ese sensor
// llego como null o con un tipo no numerico; la fusion/votacion la hace
// sensorboard_apply_room_sensor(), nunca el codec (ADR-0002 del SensorBoard).
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

  // Respuesta extendida de "status" (design.md D6, mb-factory-test). Solo la
  // rellena sensorboard_status_request(); el resto del firmware no la toca.
  bool status_seen;
  char sb_fw[16];
  bool avail_sht[3];
  bool avail_als;
  bool avail_door;
  bool avail_cam;
  bool usb_swap;

  // Instante local (millis) del ultimo mensaje de cada clase. Un heartbeat
  // vivo no prueba que las tareas de sensor del SensorBoard sigan
  // publicando: el heartbeat lo emite su app_main, independiente de ellas.
  uint32_t last_env_ms;
  uint32_t last_door_ms;
  uint32_t last_sound_ms;
  bool env_seen;
  bool sound_seen;
} SbSnapshot;

// Libera el bus I2C2, instala USB Host + driver CDC-ACM y arranca el demonio
// de eventos USB. Solo debe llamarse en modo SENSOR_SOURCE_SENSORBOARD: al
// levantar el host USB los pines 19/20 pasan a ser D-/D+ y dejan de poder
// hablar I2C.
void sensorboard_comm_init(void);

// Tarea: reconexion del dispositivo, caducidad de capturas, evaluacion del
// enlace y de la puerta. Es la UNICA que abre, cierra y transmite por el
// dispositivo, para que no haya dos duenos del handle.
void sensorboard_comm_task(void *pv);

bool sensorboard_comm_connected(void);

// Copia coherente del ultimo estado conocido (tomada bajo mutex).
void sensorboard_get_snapshot(SbSnapshot *out);

// Puente hacia el sensor de aire/humedad de la incubadora. LO LLAMA LA TAREA
// DE SENSORES desde updateRoomSensor(): funde las posiciones validas, aplica
// el mismo gate de plausibilidad que el camino I2C y refresca el sello de
// frescura. Devuelve false si el enlace esta caido, si el dato es rancio o si
// no queda ninguna posicion plausible -- y entonces NO refresca el sello, con
// lo que ALARM_AIR_SENSOR_FAULT salta por el mecanismo de siempre.
bool sensorboard_apply_room_sensor(void);

// Pide una captura JPEG. La transmision la hace la tarea del modulo en su
// siguiente tick (<= 1 s), no el llamante: asi el handle del dispositivo
// tiene un solo dueno. false si el enlace esta caido o ya hay una en vuelo.
bool sensorboard_capture_request(void);

// Toma la ultima captura completada TRANSFIRIENDO LA PROPIEDAD: tras un true
// el buffer es del llamante, que debe devolverlo con
// sensorboard_capture_free(). Se hizo asi porque la version anterior
// entregaba un puntero que la tarea del driver liberaba al llegar la captura
// siguiente -- use-after-free de decenas de KB en mitad de una subida.
bool sensorboard_capture_take(uint8_t **jpeg, size_t *len);
void sensorboard_capture_free(uint8_t *jpeg);

// Pide un "status" extendido (fw + disponibilidad por recurso). Mismo patron
// que sensorboard_capture_request(): deja un flag que la tarea del modulo
// consume y envia en su siguiente tick (<= 1 s). false si el enlace esta
// caido. Solo lo usa el test de fabrica (SB_STATUS).
bool sensorboard_status_request(void);

#ifdef ARDUINO
#include <ArduinoJson.h>
// Anade las claves sb_* al JSON de telemetria: luz, sonido, puerta, estado del
// enlace y LAS TRES POSICIONES CRUDAS de temperatura y humedad
// (sb_temp0/1/2, sb_hum0/1/2), cada una solo si llego valida.
//
// Las tres crudas no duplican a Air_temp: Air_temp es el valor que gobierna el
// lazo (la mediana, ver sb_env_fusion.h) y estas son las lecturas de las que
// sale. Viajan para poder ver en remoto que una posicion se desvia de sus
// companeras -- sb_env_used avisa de que se pierde redundancia, pero no de
// cual ni de cuanto -- y para disenar el cribado con datos reales de flota.
void sensorboard_add_telemetry(JsonObject &json);
#endif
