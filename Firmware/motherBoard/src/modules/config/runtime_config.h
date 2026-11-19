#pragma once
// Aplicacion de la configuracion en caliente (openspec: mb-remote-config).
//
// UN UNICO PUNTO DE APLICACION. POST /config (web), /config,<param>,<valor>
// (consola USB) y los atributos compartidos de ThingsBoard por GPRS y por WiFi
// llegan todos aqui. Esta funcion valida con la regla de
// runtime_config_rules.h, y solo si la acepta: escribe en in3, persiste en NVS
// con las mismas claves que lee recapVariables() y lanza el efecto lateral
// (ledcWrite del ventilador, setFanPidEnabled). Un valor rechazado no cambia
// nada.
//
// REGLAS POR ORIGEN (revision de seguridad: el MQTT va en claro, ver
// runtime_config_policy.h):
//   web      regla base, los cortes se recortan a 34..40.
//   usb      regla base estricta: los cortes fuera de 34..40 se rechazan; los
//            nombres son los de siempre en MAYUSCULAS, valor en crudo.
//   tb-*     lista blanca remota (con RCFG_REMOTE_SAFETY_PARAMS = 0, solo los
//            tres periodos GPRS), reglas remotas, un cambio por clave cada
//            10 s y los de seguridad con terapia activa aplazados a standby.
//
// REAPLICADO AL RECONECTAR. Por clave se guarda en NVS (namespace mb_rcfg) el
// ultimo valor remoto visto, si un cambio LOCAL posterior lo gana ("override")
// y el origen y epoch del ultimo cambio. La respuesta a la peticion de
// atributos de cada conexion solo aplica un valor que DIFIERE del ultimo visto
// (una escritura remota nueva, no un reenvio de ThingsBoard) y nunca pisa un
// override local. Solo una escritura remota nueva que llega por la
// SUSCRIPCION (no por la peticion) levanta el override. Un cambio local que
// deja el mismo valor no marca override: un formulario web que reenvia campos
// sin tocar no puede anular lo remoto en bloque.
//
// FUERA A PROPOSITO: serial, reference_temp, set_time y BABY_WIPE.
//
// CONCURRENCIA. Todo el estado del modulo lo protege un mutex; los
// callbacks del SDK, el handler web y la consola pueden llamar desde tareas
// distintas.
//
// Esta cabecera necesita Arduino/FreeRTOS: la parte pura esta en
// runtime_config_rules.h.

#include <stddef.h>
#include <stdint.h>

#include <ArduinoJson.h>

#include "modules/config/runtime_config_core.h"
#include "modules/config/runtime_config_rules.h"


// Etiqueta de log del origen: "usb", "web", "tb-gprs", "tb-wifi".
const char *runtime_config_origin_name(RuntimeConfigOrigin origin);

// Valida y aplica `name` = `value` (un origen local; para un origen ThingsBoard
// usa runtime_config_apply_json). true si se acepto. En `msg` (si no es NULL)
// queda una frase corta para el cuerpo de un 400 o para el log.
bool runtime_config_apply(RuntimeConfigOrigin origin, const char *name,
                          float value, char *msg, size_t msg_len);

// Igual, pero el valor llega como texto en crudo (argumento web, campo de la
// consola). Lectura estricta: "abc", "12x" o "0x10" se rechazan.
bool runtime_config_apply_text(RuntimeConfigOrigin origin, const char *name,
                               const char *text, char *msg, size_t msg_len);

// Solo comprueba que `name` = `text` se aceptaria desde `origin`, sin cambiar,
// registrar ni marcar nada. Sirve para validar TODOS los campos de un POST
// antes de aplicar ninguno.
bool runtime_config_check_text(RuntimeConfigOrigin origin, const char *name,
                               const char *text, char *msg, size_t msg_len);

// Procesa un objeto de atributos compartidos de ThingsBoard (origen TB_*).
// `new_write` es true si llega por la SUSCRIPCION (escritura remota nueva) y
// false si es la respuesta a la peticion de una conexion. Mira SOLO las nueve
// claves conocidas; una de seguridad con RCFG_REMOTE_SAFETY_PARAMS = 0 se
// ignora con aviso y cfg_rejected. Devuelve cuantas se aplicaron.
size_t runtime_config_apply_json(RuntimeConfigOrigin origin,
                                 JsonObjectConst data, bool new_write);

// Callbacks del SDK, uno por transporte y tipo. NO publican.
void runtime_config_on_update_gprs(JsonObjectConst const &data);
void runtime_config_on_update_wifi(JsonObjectConst const &data);
void runtime_config_on_request_gprs(JsonObjectConst const &data);
void runtime_config_on_request_wifi(JsonObjectConst const &data);

// Las claves de la lista remota EFECTIVA (con el flag a 0, los tres periodos)
// en un array estatico de punteros (el SDK copia los punteros, nunca las
// cadenas). `count` puede ser NULL.
const char *const *runtime_config_keys(size_t *count);

// Aplica lo aplazado (terapia activa o ritmo de 10 s) cuando ya toca. Barato si
// no hay nada pendiente. La llama el servicio de cada cliente ThingsBoard.
void runtime_config_poll(void);

// --- eco del valor efectivo -------------------------------------------------
// Tras aplicar (desde cualquier origen) el parametro queda "pendiente de eco".
// La tarea duena del cliente ThingsBoard lo publica fuera del callback y lo
// da por enviado solo si el broker lo acepto.

typedef struct {
  uint16_t mask;              // lo que lleva el mensaje
  float sent[RCFG_COUNT];     // valor de cada uno al construirlo
} RuntimeConfigEchoToken;

// Marca los nueve como pendientes (una vez por conexion: ThingsBoard ve lo que
// tiene la placa aunque nadie lo haya tocado).
void runtime_config_echo_all(void);

// Construye el JSON de lo pendiente (valor ACTUAL, ya recortado, mas el origen
// y epoch del ultimo cambio). Devuelve la longitud, o 0 si no hay nada.
size_t runtime_config_echo_build(char *json, size_t len,
                                 RuntimeConfigEchoToken *token);

// Da por publicado lo del token. Un parametro cambiado entre el build y el done
// se vuelve a marcar y sale en el siguiente.
void runtime_config_echo_done(const RuntimeConfigEchoToken *token);

// --- auditoria: un evento de telemetria cfg_change por cada cambio ----------
size_t runtime_config_audit_build(char *json, size_t len, uint32_t *seq);
void runtime_config_audit_done(uint32_t seq);

// --- cfg_rejected: ultimo valor remoto rechazado ----------------------------
size_t runtime_config_rejected_build(char *json, size_t len, uint32_t *seq);
void runtime_config_rejected_done(uint32_t seq);
