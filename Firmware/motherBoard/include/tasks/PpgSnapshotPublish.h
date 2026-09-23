#pragma once

// Publicación del snapshot PPG hacia ThingsBoard, común a GPRS y WiFi.
//
// Vive aparte de PpgSnapshot.cpp a propósito: ese módulo captura y no sabe
// nada de ThingsBoard ni de JSON. Y vive aparte de GPRS.cpp/Wifi_OTA.cpp
// porque una tercera copia a mano del mismo montaje es justo como los dos
// bloques de telemetría acabaron divergiendo en 18 claves.

#include "main.h" // define THINGSBOARD_ENABLE_DYNAMIC antes de ThingsBoard.h

// Publica el snapshot listo en trozos de PPG_SNAPSHOT_CHUNK_SAMPLES muestras
// (ver modules/util/ppg_snapshot_plan.h: de una vez, ~23 KB, se atascaba).
//
// Hay que llamarla en cada vuelta de la tarea del transporte: el envío avanza
// un poco por llamada y devuelve true solo en la llamada que lo termina.
//
// - burst = false (WiFi): como mucho un trozo por llamada, con
//   PPG_SNAPSHOT_CHUNK_GAP_MS entre trozos. Entre medias el llamante sigue
//   con tb.loop() y el TCP se vacía.
// - burst = true (GPRS): todos los trozos en esta llamada. La tarea GPRS solo
//   publica una vez por ciclo (60 s o más); de uno en uno tardaría 16 ciclos.
//
// El primer transporte que encuentra un snapshot listo lo reclama; el otro
// no hace nada hasta que termine o se abandone
// (PPG_SNAPSHOT_PUBLISH_TIMEOUT_MS). Un trozo que falla se reintenta en la
// siguiente llamada. Sin hora real (reloj sin sincronizar) no se empieza:
// los puntos caerían en 1970.
//
// `tag` es solo para el log: "GPRS" o "WIFI".
bool ppgSnapshotPublish(ThingsBoard &client, const char *tag, bool burst);
