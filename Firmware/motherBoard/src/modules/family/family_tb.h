#pragma once
// Cableado del aviso "Coge mi mano" con un cliente ThingsBoard. Lo comparten
// GPRS.cpp y Wifi_OTA.cpp; cada una pasa SU cliente y SUS callbacks.
//
// INCLUIR DESPUES de main.h (que define THINGSBOARD_ENABLE_DYNAMIC antes de
// ThingsBoard.h): las clases Shared_Attribute_Callback y Attribute_Request_
// Callback cambian de forma segun esa macro.
//
// Mismas reglas que runtime_config_tb.h (leerlo): la suscripcion en caliente va
// UNA vez por cliente (`*subscribed`; el SDK resuscribe solo al reconectar) y
// la peticion de los valores actuales va en CADA conexion, con otro callback
// (on_request). Los callbacks solo llaman a family_state_apply_json: NUNCA
// publican (PubSubClient comparte buffer entre el mensaje entrante y publish).

#include <Arduino.h>
#include <ThingsBoard.h>

#include "modules/family/family_state.h"

template <typename TB>
inline void family_tb_connected(TB &tb,
                                void (*on_update)(JsonObjectConst const &),
                                void (*on_request)(JsonObjectConst const &),
                                bool *subscribed) {
  static const char *const keys[] = {"hand_hold_until", "family_qr_url"};
  const size_t n = sizeof(keys) / sizeof(keys[0]);

  if (!*subscribed) {
    const Shared_Attribute_Callback cb(on_update, keys, keys + n);
    if (tb.Shared_Attributes_Subscribe(cb)) {
      *subscribed = true;
    } else {
      ESP_LOGW("FAMILY", "no se pudo suscribir a los atributos compartidos");
    }
  }

  const Attribute_Request_Callback req(on_request, keys, keys + n);
  if (!tb.Shared_Attributes_Request(req)) {
    ESP_LOGW("FAMILY", "no se pudo pedir los atributos compartidos");
  }
}
