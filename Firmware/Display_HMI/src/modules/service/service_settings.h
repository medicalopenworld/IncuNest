#pragma once
// Persistencia en NVS (`hmi_cfg`, clave `svc_en`) del interruptor propio del
// aviso de servicio. La logica esta en service_state.h (pura, con tests de
// host); esto es solo el pegamento con Preferences, por eso no entra en el
// entorno native.
#include <stdbool.h>

// Carga el valor de NVS. Sin clave (NVS virgen) el aviso queda ACTIVADO. Llamar
// una vez en el arranque de UI_Task, antes de UI_ApplyLanguage().
void ServiceSettings_Init(void);

// Cambia el interruptor y lo guarda. No toca NVS si el valor no cambia.
void ServiceSettings_SetEnabled(bool on);
