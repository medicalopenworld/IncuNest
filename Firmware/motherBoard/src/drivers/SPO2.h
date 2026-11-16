#pragma once
#include "incunest_afe4490.h"
#include <freertos/task.h>

#include <type_traits>

// Guard on the one field of the library's data contract this firmware reshapes
// by hand. The library lives in another repo with another owner: in v0.69 it
// changed ppg_disp from int32_t (ADC counts) to float (OT domain, A/A) and the
// firmware kept compiling without a single warning, because float -> int32_t is
// a legal implicit conversion. All the consumers (CommTask's transport scale,
// PpgSnapshot's buffer, SPO2's log, the Drive CSV that existed then) silently
// truncated every sample to 0 — a flat trace, not a build error.
//
// The pin in platformio.ini stops that from arriving unannounced; this turns the
// remaining case — someone bumps the pin without revisiting the consumers — into
// a compile error instead of a flat waveform on a patient monitor.
//
// The type is only half of the contract: the RANGE is the other half, and no
// static_assert can see it. Since lib v0.116 the range depends on the display
// mode initSPO2() selects (NORM_PI -> [-1, +1]; NO_NORM -> A/A, ~1e-6), and the
// consumers below are written for NORM_PI. Changing the mode means revisiting
// CommTask's wire scale and PpgSnapshot's samples again.
static_assert(
    std::is_same<decltype(AFE4490Data::ppg_disp), float>::value,
    "AFE4490Data::ppg_disp changed type: re-check the scale in CommTask.cpp "
    "and the buffer in PpgSnapshot.cpp");

#define SPO2_INIT_TIME 1000

extern INCUNEST_AFE4490 afe;
extern TaskHandle_t     g_spo2_task;
extern volatile AFE4490Data g_spo2_data;

void initSPO2();

// diag_code de la ultima muestra: bits 0-12 = REG_DIAG (0x30) del AFE4490 que
// rellena runAfeDiagnostics() cada 5 s con la sonda ausente; resto = banderas
// RSQM. Se publica como AFE_diag (DIAG) y en /debug/state.
uint32_t afeDiagCode(void);
// Episodios de parada del AFE (sin muestra > AFE_WD_STALL_MS) desde el arranque.
uint32_t afeStallCount(void);
void SPO2_Task(void *pvParameters);
