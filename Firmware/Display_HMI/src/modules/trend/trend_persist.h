#pragma once
// Persistencia de la tendencia en LittleFS (/trend.bin): restauracion unica
// tras el primer sincronismo de hora y volcado periodico TROCEADO. Pegamento
// con E/S real (hmi_fs + PSRAM): lo verificable en host vive en trend_file.* y
// trend_policy.*; esto se verifica compilando y en banco.
//
// Todo desde UI_Task y FUERA de LVGL_Lock (ARQ-LOCK-001): es E/S de flash.

#include <stdint.h>

#include "modules/trend/trend_model.h"

// Una pasada del servicio; llamar en cada vuelta del bucle de UI, fuera del
// lock. Con hora sincronizada:
//   1. la primera vez, restaura /trend.bin ANTEPONIENDOLO al modelo;
//   2. si toca (cada 5 min, o al entrar en bloqueo con >= 2 min desde el
//      ultimo), serializa una instantanea consistente a un buffer de PSRAM y la
//      escribe en trozos de 4 KB, uno por pasada y no mas de uno cada 50 ms,
//      sobre un temporal que se renombra al terminar (atomico).
// `monoMs` = esp_timer en ms (la base de la que el reloj del modelo deriva las
// ranuras); `nowMs` = millis() (temporizacion del volcado).
// Devuelve true si ha cambiado el modelo (restauracion): el llamador debe
// repintar la vista, bajo LVGL_Lock, en la siguiente pasada.
bool TrendPersist_Service(TrendModel *m, int64_t monoMs, uint32_t nowMs);

// Pide un volcado en cuanto la politica lo permita (entrada en bloqueo). Solo
// levanta una bandera: seguro desde cualquier contexto, incluso bajo LVGL_Lock.
void TrendPersist_RequestDumpOnLock(void);
