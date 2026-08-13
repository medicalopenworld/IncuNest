#pragma once

#include <Arduino.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "incunest_afe4490.h"

// One CSV per window; tuned so that even during a slow TLS upload the active
// file plus the in-flight upload file stay within the 2 MB LittleFS partition.
#define DRIVE_UPLOAD_WINDOW_MS 60000UL

// ~1 s buffer at 500 Hz between SPO2 producer and the file writer consumer.
#define DRIVE_SAMPLE_QUEUE_LEN 500

// Each window writes to a unique file /pox_<ms>.csv to avoid path reuse,
// which triggers an esp_littlefs fd-table corruption bug in Arduino-ESP32 2.x
// (asserted lfs_mlist_isopen after a few rename/reopen cycles).
#define DRIVE_CSV_PATH_PREFIX "/pox_"
#define DRIVE_CSV_PATH_SUFFIX ".csv"

// Drive subfolder layout inside <SN>/
#define DRIVE_SUBFOLDER_LOGS   "1-Logs"
#define DRIVE_SUBFOLDER_PULSIOX "2-Pulsioximetry"

#define DRIVE_GAS_HOST "script.google.com"
#define DRIVE_GAS_PATH                                                         \
  "/macros/s/"                                                                 \
  "AKfycbwqaOIO7DsiqSeXGUxAz8LpKwROhGneH36Id8bk5asovmXefO5Z236If6eR0AksuV8"    \
  "/exec"

// valid_signal: probe_state == PROBE_APPLIED && rsqi == 1 for this sample —
// the AFE4490 library's own probe-attached/raw-signal-valid flags, not a
// derived HR quality metric. Filename gets a "DH_" prefix (driveWriteTask)
// when any sample in the window sets this.
struct DrivePpgSample {
  uint32_t t_ms;
  int32_t  led1_sub;
  int32_t  led2_sub;
  int32_t  ppg_disp;
  bool     valid_signal;
};

void initDriveUpload();
void drivePushSample(const AFE4490Data &data);

// Enqueue an arbitrary file to be uploaded to <SN>/1-Logs/<drive_filename>.
// The source file is deleted from LittleFS on success OR on permanent
// failure (no WiFi etc.) so the partition does not fill up. Returns false if
// the upload queue is full — caller is responsible for the source file then.
bool driveEnqueueLogUpload(const char *source_path, const char *drive_filename);
