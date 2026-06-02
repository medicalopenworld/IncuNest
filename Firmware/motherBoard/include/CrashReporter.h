#pragma once

#include <Arduino.h>
#include <stddef.h>

// Ring buffer size kept in RTC slow memory. Survives software resets and
// panics, lost on cold power cycle.
#define CRASH_RING_SIZE 4096

void crashReporterInit();

// Non-blocking, ISR-unsafe (expected to be called from sync_vprintf only).
void crashReporterPut(const char *data, size_t len);

// Call after DriveUpload + LittleFS are ready. If the previous boot ended in
// a reset that looks like a crash, dumps the captured ring plus reset metadata
// into a file on LittleFS and enqueues it for Drive upload. No-op otherwise.
void crashReporterMaybeFlush();
