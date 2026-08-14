#pragma once

// Publicación del snapshot PPG hacia ThingsBoard, común a GPRS y WiFi.
//
// Vive aparte de PpgSnapshot.cpp a propósito: ese módulo captura y no sabe
// nada de ThingsBoard ni de JSON. Y vive aparte de GPRS.cpp/Wifi_OTA.cpp
// porque una tercera copia a mano del mismo montaje es justo como los dos
// bloques de telemetría acabaron divergiendo en 18 claves.

#include "main.h" // define THINGSBOARD_ENABLE_DYNAMIC antes de ThingsBoard.h

// Si hay un snapshot listo, lo reclama en exclusiva y lo publica por el
// cliente dado. Devuelve true solo si se publicó de verdad.
//
// No hace nada (y devuelve false) si no hay snapshot, si otro transporte ya
// lo reclamó, o si el reloj no está sincronizado — sin hora real los puntos
// caerían en 1970 y ensuciarían la serie.
//
// `tag` es solo para el log: "GPRS" o "WIFI".
bool ppgSnapshotPublish(ThingsBoard &client, const char *tag);
