#pragma once

#include <stdbool.h>
#include <stdint.h>

// Cuándo capturar un snapshot PPG y cómo trocear su envío a ThingsBoard.
//
// Por qué trocear: el snapshot son 400 muestras, ~23 KB de JSON. Mandado de
// una vez, en banco (2026-09-23, WiFi) el envío se atascó dos de tres veces:
// WiFiClient::write() devolvía EAGAIN una vez por segundo durante ~30 s, se
// caía el enlace (BEACON_TIMEOUT) y no llegaba ni un punto. En trozos, uno
// por vuelta de la tarea, tb_wifi.loop() sigue corriendo entre medias y el
// TCP tiene tiempo de vaciarse.
//
// Lógica pura a propósito, como wifi_dwell: sin Arduino, para que entre en
// [env:native] y se pruebe con Unity. El que publica es PpgSnapshotPublish.cpp.

// Muestras por trozo. Cada punto es ~60 B de JSON
// ({"ts":1790199201020,"values":{"PPG_snapshot_v1":-2.5e-06}}), así que un
// trozo son ~1,5 KB: 16 trozos por snapshot.
#define PPG_SNAPSHOT_CHUNK_SAMPLES 25

// Pausa mínima entre trozos por WiFi. La tarea da vuelta cada 50 ms
// (OTA_TASK_PERIOD_MS); sin esto los 16 trozos saldrían casi seguidos.
#define PPG_SNAPSHOT_CHUNK_GAP_MS 250

// Un envío que no termina en este tiempo se abandona y suelta el snapshot. El
// caso real: se cae el WiFi a mitad y entra el GPRS, que no puede continuar
// un envío que no es suyo. Sin esto el slot quedaría reclamado para siempre y
// no se volvería a capturar nunca.
#define PPG_SNAPSHOT_PUBLISH_TIMEOUT_MS (2UL * 60UL * 1000UL)

// Captura automática: una cada intervalo mientras haya señal, y si no la hay
// (sin sonda o rsqi == 0), reintento corto en vez de esperar otro intervalo
// entero. Si no, poner el dedo justo después de un intento fallido costaba
// esperar el intervalo completo.
#define PPG_SNAPSHOT_AUTO_RETRY_MS (10UL * 1000UL)

// Fin (exclusivo) del trozo que empieza en `next`, sin pasarse de `n`.
uint16_t ppg_chunk_end(uint16_t next, uint16_t n, uint16_t chunk);

// Timestamp de la muestra i de n: la última es `lastMs` y el resto retrocede
// en pasos de stepMs — el orden temporal real de la captura.
uint64_t ppg_sample_ts_ms(uint64_t lastMs, uint16_t n, uint16_t i,
                          uint32_t stepMs);

// true si han pasado al menos `interval` ms desde `since`. Correcto al dar la
// vuelta millis() (uint32_t, ~49 días).
bool ppg_elapsed(uint32_t now, uint32_t since, uint32_t interval);

struct PpgAutoCapture {
  uint32_t lastTryMs;
  uint32_t lastStartMs;
  bool everTried;
  bool everStarted;
};

// ¿Toca intentar una captura automática? El primer intento es inmediato (no
// se espera un intervalo desde el arranque); tras una captura, `intervalMs`;
// tras un intento sin señal, `retryMs`.
bool ppg_autocapture_due(const PpgAutoCapture *st, uint32_t now,
                         uint32_t intervalMs, uint32_t retryMs);

// Anota un intento. `started` = la captura arrancó (o ya había una en curso).
void ppg_autocapture_record(PpgAutoCapture *st, uint32_t now, bool started);
