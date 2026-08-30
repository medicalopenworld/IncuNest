#pragma once
#include "incunest_afe4490.h"
#include <freertos/task.h>

#include <type_traits>

// Guard on the one field of the library's data contract this firmware reshapes
// by hand. The library lives in another repo with another owner: in v0.69 it
// changed ppg_disp from int32_t (ADC counts) to float (OT domain, A/A) and the
// firmware kept compiling without a single warning, because float -> int32_t is
// a legal implicit conversion. All four consumers (CommTask's transport scale,
// DriveUpload's CSV, PpgSnapshot's buffer, SPO2's log) silently truncated every
// sample to 0 — a flat trace, not a build error.
//
// The pin in platformio.ini stops that from arriving unannounced; this turns the
// remaining case — someone bumps the pin without revisiting the consumers — into
// a compile error instead of a flat waveform on a patient monitor.
static_assert(
    std::is_same<decltype(AFE4490Data::ppg_disp), float>::value,
    "AFE4490Data::ppg_disp changed type: re-check the scale in CommTask.cpp, the "
    "CSV format in DriveUpload.cpp and the buffer in PpgSnapshot.cpp");

#define SPO2_INIT_TIME 1000

extern INCUNEST_AFE4490 afe;
extern TaskHandle_t     g_spo2_task;
extern volatile AFE4490Data g_spo2_data;

void initSPO2();
void SPO2_Task(void *pvParameters);
