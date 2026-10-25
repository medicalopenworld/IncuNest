#pragma once
// Cableado de runtime_config con un cliente ThingsBoard (openspec:
// mb-remote-config). Lo comparten GPRS.cpp y Wifi_OTA.cpp para no tener dos
// copias que diverjan; cada una pasa SU cliente y SU callback de origen.
//
// INCLUIR DESPUES de main.h (que define THINGSBOARD_ENABLE_DYNAMIC antes de
// ThingsBoard.h): las clases Shared_Attribute_Callback y Attribute_Request_
// Callback cambian de forma segun esa macro.

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
//    lo que se cambio mientras la unidad estaba apagada.
//  - Se marcan los nueve para el eco: ThingsBoard ve lo que tiene la placa.
template <typename TB>
inline void runtime_config_tb_connected(
    TB &tb, void (*on_shared)(JsonObjectConst const &), bool *subscribed,
    const char *tag) {
  size_t n = 0;
  const char *const *keys = runtime_config_keys(&n);

  if (!*subscribed) {
    const Shared_Attribute_Callback cb(on_shared, keys, keys + n);
    if (tb.Shared_Attributes_Subscribe(cb)) {
      *subscribed = true;
    } else {
      ESP_LOGW("CFG", "[%s] no se pudo suscribir a los atributos compartidos",
               tag);
    }
  }

  const Attribute_Request_Callback req(on_shared, keys, keys + n);
  if (!tb.Shared_Attributes_Request(req)) {
    ESP_LOGW("CFG", "[%s] no se pudo pedir los atributos compartidos", tag);
  }

  runtime_config_echo_all();
}

// Publica el eco pendiente como atributo de CLIENTE. Se llama desde la tarea
// duena del cliente, nunca desde un callback del SDK: PubSubClient comparte un
// unico buffer entre el mensaje entrante y publish(), asi que publicar dentro
// del callback puede corromper el payload que se esta leyendo.
//
// Barato cuando no hay nada pendiente (una lectura atomica).
template <typename TB>
inline void runtime_config_tb_publish_echo(TB &tb) {
  char json[RCFG_ECHO_JSON_MAX];
  uint16_t mask = 0;
  if (runtime_config_echo_build(json, sizeof(json), &mask) == 0) {
    return;
  }
  // Solo se da por enviado si el broker lo acepto; si no, se reintenta.
  if (tb.sendAttributeJson(json)) {
    runtime_config_echo_done(mask);
  }
}
