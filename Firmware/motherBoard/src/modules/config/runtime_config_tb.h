#pragma once
// Cableado de runtime_config con un cliente ThingsBoard (openspec:
// mb-remote-config). Lo comparten GPRS.cpp y Wifi_OTA.cpp para no tener dos
// copias que diverjan; cada una pasa SU cliente y SUS callbacks de origen.
//
// INCLUIR DESPUES de main.h (que define THINGSBOARD_ENABLE_DYNAMIC antes de
// ThingsBoard.h): las clases Shared_Attribute_Callback y Attribute_Request_
// Callback cambian de forma segun esa macro.

#include <Arduino.h>
#include <ThingsBoard.h>

#include "modules/config/runtime_config.h"

// Se llama tras CADA conexion correcta a ThingsBoard.
//
//  - La suscripcion a cambios en caliente va UNA vez por cliente
//    (`*subscribed`). El SDK vuelve a suscribir el topico MQTT por su cuenta al
//    reconectar (set_connect_callback -> Resubscribe_Topics) y conserva el
//    vector de callbacks: repetirla duplicaria el callback y cada cambio se
//    aplicaria dos veces. Es el defecto que ya tienen las RPC de GPRS y WiFi
//    (se re-registran en cada conexion); aqui no se copia.
//  - La peticion de los valores actuales SI va en cada conexion: el SDK
//    descarta las peticiones sin responder al reconectar, y es lo que recoge
//    lo que se cambio mientras la unidad estaba apagada. Su callback es OTRO
//    (on_request): una respuesta solo aplica lo que difiere del ultimo valor
//    remoto visto y no pisa un cambio local; una escritura nueva llega por la
//    suscripcion (on_update).
//  - SOLO se piden las claves de la lista remota EFECTIVA (runtime_config_keys).
//  - Se marcan los nueve para el eco: ThingsBoard ve lo que tiene la placa.
template <typename TB>
inline void runtime_config_tb_connected(
    TB &tb, void (*on_update)(JsonObjectConst const &),
    void (*on_request)(JsonObjectConst const &), bool *subscribed,
    const char *tag) {
  size_t n = 0;
  const char *const *keys = runtime_config_keys(&n);
  if (n == 0) {
    return;
  }

  if (!*subscribed) {
    const Shared_Attribute_Callback cb(on_update, keys, keys + n);
    if (tb.Shared_Attributes_Subscribe(cb)) {
      *subscribed = true;
    } else {
      ESP_LOGW("CFG", "[%s] no se pudo suscribir a los atributos compartidos",
               tag);
    }
  }

  const Attribute_Request_Callback req(on_request, keys, keys + n);
  if (!tb.Shared_Attributes_Request(req)) {
    ESP_LOGW("CFG", "[%s] no se pudo pedir los atributos compartidos", tag);
  }

  runtime_config_echo_all();
}

// Estado de reintento de las publicaciones de un cliente (uno por tarea).
typedef struct {
  uint32_t failMs;  // millis() del ultimo intento fallido
  bool failed;
} RuntimeConfigPubState;

// Servicio periodico de un cliente CONECTADO, desde la tarea duena del
// cliente y NUNCA desde un callback del SDK (PubSubClient comparte un unico
// buffer entre el mensaje entrante y publish(): publicar dentro del callback
// puede corromper el payload que se esta leyendo).
//
//  1. Aplica lo aplazado que ya toca (terapia terminada, 10 s cumplidos).
//  2. Publica UNA cosa por pasada, por orden: cfg_rejected (atributo de
//     cliente), cfg_change (telemetria) y el eco (atributo de cliente).
//
// Si el broker rechaza un envio no se reintenta hasta pasados
// RCFG_PUBLISH_RETRY_MS: en GPRS un reintento por pasada martillea un enlace
// que ya va mal. Barato cuando no hay nada pendiente.
template <typename TB>
inline void runtime_config_tb_service(TB &tb, RuntimeConfigPubState *st) {
  runtime_config_poll();

  if (st->failed &&
      (uint32_t)(millis() - st->failMs) < (uint32_t)RCFG_PUBLISH_RETRY_MS) {
    return;
  }

  char json[RCFG_ECHO_JSON_MAX];
  uint32_t seq = 0;
  bool ok = true;

  if (runtime_config_rejected_build(json, sizeof(json), &seq) > 0) {
    ok = tb.sendAttributeJson(json);
    if (ok) runtime_config_rejected_done(seq);
  } else if (runtime_config_audit_build(json, sizeof(json), &seq) > 0) {
    ok = tb.sendTelemetryJson(json);
    if (ok) runtime_config_audit_done(seq);
  } else {
    RuntimeConfigEchoToken token;
    if (runtime_config_echo_build(json, sizeof(json), &token) > 0) {
      ok = tb.sendAttributeJson(json);
      if (ok) runtime_config_echo_done(&token);
    } else {
      return;  // nada que publicar
    }
  }

  st->failed = !ok;
  if (!ok) {
    st->failMs = millis();
  }
}
