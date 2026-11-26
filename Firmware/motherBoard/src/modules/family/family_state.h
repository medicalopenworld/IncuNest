#pragma once
// Envoltorio seguro entre hilos del aviso "Coge mi mano" (family_notice):
// guarda UN FamilyNotice global tras un spinlock. Lo escriben los callbacks de
// ThingsBoard (tareas GPRS y WiFi) y la tarea de la UART (alta); lo lee la
// tarea de la UART. Toca Arduino y la hora: no entra en env:native.
#include <ArduinoJson.h>
#include <stdbool.h>
#include <stdint.h>

#include "family_proto.h"

// Lee `hand_hold_until` (numero, epoch ms) y `family_qr_url` (texto; null u
// otro tipo = borrar). Una clave ausente no toca su parte. `fromRequest` =
// respuesta a la peticion de reconexion. Nunca publica nada.
void family_state_apply_json(JsonObjectConst const &data, bool fromRequest);

// Alta confirmada en la placa: apaga el aviso y olvida la URL.
void family_state_on_discharge(void);

// Ver family_notice_take_msg: llamar en cada pasada del bucle de CommTask.
bool family_state_take_msg(uint32_t nowMs, bool force, FamilyMsg *out);
