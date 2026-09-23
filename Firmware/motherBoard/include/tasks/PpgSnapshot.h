#pragma once

#include <Arduino.h>

#include "incunest_afe4490.h"

// "Foto" ocasional de la forma de onda PPG para ThingsBoard (solo WiFi) —
// no confundir con el streaming continuo de DriveUpload.cpp (que sube TODO
// mientras la sonda está aplicada, hacia Google Drive).
//
// 500 Hz de origen es de sobra para HR/SpO2, pero muy por encima de lo que
// hace falta para juzgar ritmo/morfología a simple vista: el contenido útil
// de un pulso PPG está por debajo de ~20 Hz. Diezmar a 50 Hz da margen
// (incluye la muesca dicrótica) con una décima parte del volumen de datos.
#define PPG_SNAPSHOT_SRC_HZ     500
#define PPG_SNAPSHOT_FS_HZ      50
#define PPG_SNAPSHOT_DURATION_S 8
#define PPG_SNAPSHOT_DECIM      (PPG_SNAPSHOT_SRC_HZ / PPG_SNAPSHOT_FS_HZ)
#define PPG_SNAPSHOT_SAMPLES    (PPG_SNAPSHOT_FS_HZ * PPG_SNAPSHOT_DURATION_S)

// Captura automática mientras WiFi/ThingsBoard estén arriba y el gate pase.
#define PPG_SNAPSHOT_AUTO_INTERVAL_MS (15UL * 60UL * 1000UL)

// Si una captura no termina en 3x su duración esperada (p. ej. el AFE4490
// deja de entregar muestras), se aborta y libera el slot para el siguiente
// intento — nunca debe quedar bloqueada indefinidamente.
#define PPG_SNAPSHOT_TIMEOUT_MS (PPG_SNAPSHOT_DURATION_S * 1000UL * 3UL)

enum class PpgSnapshotStatus {
  STARTED,           // captura iniciada
  BUSY,              // ya hay una captura en curso
  SIGNAL_NOT_READY,  // gate falló: sonda no aplicada o rsqi == 0
};

// Se llama una vez por muestra del AFE4490 (500 Hz) desde SPO2_Task. No hace
// nada si no hay una captura en curso — seguro de llamar siempre.
void ppgSnapshotFeed(const AFE4490Data &data, uint32_t now_ms);

// Arranca una captura si el gate pasa y no hay ya una en curso.
// Gate: probe_state == PROBE_APPLIED && rsqi == 1 — el mismo criterio que el
// marcador de latido de DriveUpload, en vez de HR2/HR3 SQI (que miden calidad
// de la extracción de frecuencia cardiaca, no si hay señal real de sonda).
PpgSnapshotStatus ppgSnapshotRequestCapture(bool probeApplied, uint8_t rsqi,
                                            uint32_t now_ms);

// True cuando ya se han recogido PPG_SNAPSHOT_SAMPLES muestras.
bool ppgSnapshotIsReady();

// Reclama en exclusiva un snapshot listo para publicarlo. Devuelve true a UN
// SOLO llamante; el resto ve false hasta que se libere.
//
// Hace falta porque GPRS_Task y OTA_WIFI_Task son tareas distintas y comparten
// este único buffer: sin esto los dos podrían publicar el mismo snapshot, o
// uno limpiarlo mientras el otro lo está serializando.
bool ppgSnapshotTryAcquire();

// Libera el snapshot reclamado y deja el slot listo para la siguiente captura.
// Llamar SIEMPRE tras un ppgSnapshotTryAcquire() que devolvió true, publicara
// bien o mal: si no, no se vuelve a capturar nunca.
void ppgSnapshotRelease();

// Acceso al buffer ya capturado. Válido solo mientras ppgSnapshotIsReady()
// sea true; el módulo no sabe nada de ThingsBoard/JSON — el llamador decide
// cómo serializarlo (Wifi_OTA.cpp lo manda como serie temporal real, un
// punto por muestra, para poder usar un chart estándar en el dashboard).
// Las muestras son ppg_disp tal cual lo entrega la libreria: float en dominio
// OT (A/A, ~1e-5..1e-6) desde la v0.69. Sin escalar ni normalizar aqui.
uint16_t ppgSnapshotSampleCount();
const float *ppgSnapshotSamples();

// Libera el slot para la siguiente captura (llamar tras leer el buffer).
void ppgSnapshotClear();
