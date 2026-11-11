#pragma once

#include <Arduino.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// Uploads files from LittleFS to Google Drive through a Google Apps Script.
// Today that is only the crash reports; the raw PPG capture that used to live
// here was removed in 18.56.0 (see the note at the top of DriveUpload.cpp).

// Drive subfolder layout inside <SN>/
#define DRIVE_SUBFOLDER_LOGS   "1-Logs"

#define DRIVE_GAS_HOST "script.google.com"
#define DRIVE_GAS_PATH                                                         \
  "/macros/s/"                                                                 \
  "AKfycbwqaOIO7DsiqSeXGUxAz8LpKwROhGneH36Id8bk5asovmXefO5Z236If6eR0AksuV8"    \
  "/exec"

void initDriveUpload();

// Enqueue an arbitrary file to be uploaded to <SN>/1-Logs/<drive_filename>.
// The source file is deleted from LittleFS on success OR on permanent
// failure (no WiFi etc.) so the partition does not fill up. Returns false if
// the upload queue is full — caller is responsible for the source file then.
bool driveEnqueueLogUpload(const char *source_path, const char *drive_filename);
