#pragma once

#include <Arduino.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "incunest_afe4490.h"

// One CSV per window; tuned so that even during a slow TLS upload the active
// file plus the in-flight upload file stay within the 2 MB LittleFS partition.
#define DRIVE_UPLOAD_WINDOW_MS 60000UL

// Heartbeat marker: filename is prefixed with "DH_" when any sample in the
// window passes the quality gate: max(hr2_sqi, hr3_sqi) >= 0.8 AND
// min(hr2_sqi, hr3_sqi) >= 0.5.
#define DRIVE_HR_SQI_MAX_THRESHOLD 0.8f
#define DRIVE_HR_SQI_MIN_THRESHOLD 0.5f

// ~1 s buffer at 500 Hz between SPO2 producer and the file writer consumer.
#define DRIVE_SAMPLE_QUEUE_LEN 500

#define DRIVE_CSV_ACTIVE_PATH "/pox_active.csv"
#define DRIVE_CSV_UPLOAD_PATH "/pox_upload.csv"

// Drive subfolder layout inside <SN>/
#define DRIVE_SUBFOLDER_LOGS   "1-Logs"
#define DRIVE_SUBFOLDER_PULSIOX "2-Pulsioximetry"

#define DRIVE_GAS_HOST "script.google.com"
#define DRIVE_GAS_PATH                                                         \
  "/macros/s/"                                                                 \
  "AKfycbwqaOIO7DsiqSeXGUxAz8LpKwROhGneH36Id8bk5asovmXefO5Z236If6eR0AksuV8"    \
  "/exec"

struct DrivePpgSample {
  uint32_t t_ms;
  int32_t  led1_aled1;
  int32_t  led2_aled2;
  int32_t  ppg;
  float    hr2_sqi;
  float    hr3_sqi;
};

void initDriveUpload();
void drivePushSample(const AFE4490Data &data);

// Enqueue an arbitrary file to be uploaded to <SN>/1-Logs/<drive_filename>.
// The source file is deleted from LittleFS on success OR on permanent
// failure (no WiFi etc.) so the partition does not fill up. Returns false if
// the upload queue is full — caller is responsible for the source file then.
bool driveEnqueueLogUpload(const char *source_path, const char *drive_filename);
