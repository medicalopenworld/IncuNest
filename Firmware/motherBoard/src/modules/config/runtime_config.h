#pragma once
// Aplicacion de la configuracion en caliente (openspec: mb-remote-config).
//
// UN UNICO PUNTO DE APLICACION. POST /config (web), /config,<param>,<valor>
// (consola USB) y los atributos compartidos de ThingsBoard por GPRS y por WiFi
// llegan todos aqui. Esta funcion valida con la regla de
// runtime_config_rules.h, y solo si la acepta: escribe en in3, persiste en NVS
// con las mismas claves que lee recapVariables() y lanza el efecto lateral
// (ledcWrite del ventilador, setFanPidEnabled, maxDesiredTemp[]). Un valor
// rechazado no cambia nada.
//
// FUERA A PROPOSITO: serial, reference_temp, set_time y BABY_WIPE. No se
// aplican por aqui; los dos primeros dependen de algo local (identidad,
// medida fisica) y el ultimo borra datos clinicos (ver el comentario sobre
// rpc_callbacks[] en GPRS.cpp).
//
// Esta cabecera necesita Arduino/FreeRTOS: la parte pura (reglas, lectura de
// texto y JSON, formato del eco) esta en runtime_config_rules.h.

#include <stddef.h>
#include <stdint.h>

#include <ArduinoJson.h>

#include "modules/config/runtime_config_rules.h"

typedef enum {
  RCFG_ORIGIN_USB = 0,
  RCFG_ORIGIN_WEB,
  RCFG_ORIGIN_TB_GPRS,
  RCFG_ORIGIN_TB_WIFI
} RuntimeConfigOrigin;

// Etiqueta de log del origen: "usb", "web", "tb-gprs", "tb-wifi".
const char *runtime_config_origin_name(RuntimeConfigOrigin origin);

// Valida y aplica `name` = `value`. true si se acepto (incluye el recorte de
// air_tmax/skin_tmax, que se dice en `msg`). En `msg` (si no es NULL) queda una
// frase corta lista para el cuerpo de un 400 o para el log. Un nombre fuera de
// la lista blanca devuelve false.
bool runtime_config_apply(RuntimeConfigOrigin origin, const char *name,
                          float value, char *msg, size_t msg_len);

// Igual, pero el valor llega como texto (argumento web). Lectura estricta:
// "abc" o "12x" se rechazan en lugar de convertirse en 0.
bool runtime_config_apply_text(RuntimeConfigOrigin origin, const char *name,
                               const char *text, char *msg, size_t msg_len);

// Solo comprueba que `name` = `text` se aceptaria: mismas reglas que
// runtime_config_apply_text(), pero sin cambiar nada, sin registrar nada y sin
// marcar eco. Sirve para validar TODOS los campos de un POST antes de aplicar
// ninguno, de modo que un formulario con un campo malo no queda a medias.
bool runtime_config_check_text(const char *name, const char *text, char *msg,
                               size_t msg_len);

// Aplica las claves de la lista blanca presentes en `data` (el objeto que
// entrega el SDK de ThingsBoard). Ignora cualquier otra clave y cualquier valor
// que no sea numero o booleano. Devuelve cuantas se aceptaron.
size_t runtime_config_apply_json(RuntimeConfigOrigin origin,
                                 JsonObjectConst data);

// Callbacks del SDK, uno por transporte (solo se diferencian en el origen que
// llevan al log). NO publican: ver runtime_config_tb.h.
void runtime_config_on_shared_gprs(JsonObjectConst const &data);
void runtime_config_on_shared_wifi(JsonObjectConst const &data);

// Las nueve claves en un array estatico de punteros (el SDK copia los
// punteros, nunca las cadenas). `count` puede ser NULL.
const char *const *runtime_config_keys(size_t *count);

// --- eco del valor efectivo -------------------------------------------------
// Tras aplicar (desde cualquier origen) el parametro queda "pendiente de eco".
// La tarea duena del cliente ThingsBoard lo publica fuera del callback y lo
// da por enviado solo si el broker lo acepto.

// Marca los nueve como pendientes (una vez por conexion: ThingsBoard ve lo que
// tiene la placa aunque nadie lo haya tocado).
void runtime_config_echo_all(void);

// Construye el JSON de lo pendiente con el valor ACTUAL (ya recortado). Devuelve
// la longitud, o 0 si no hay nada pendiente. `*mask` recibe lo que lleva el
// mensaje, para pasarlo a runtime_config_echo_done().
size_t runtime_config_echo_build(char *json, size_t len, uint16_t *mask);

// Da por publicado lo que iba en `mask`. Un parametro cambiado entre el build y
// el done se pierde para este envio; se repone en el siguiente
// runtime_config_echo_all() (siguiente conexion).
void runtime_config_echo_done(uint16_t mask);
