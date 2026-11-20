#pragma once

// Where an output quantity of incunest_afe4490 becomes an output of this
// firmware, with no hardware so it can be tested on the host.
//
// The library's data contracts (v0.124): the clinical quantities spo2, pi,
// hr1, hr2 and hr3 (its spec 2.1.1) and the display quantities spo2_disp,
// hr_disp and pi_disp (2.1.2) are a finite number inside their declared range,
// or NaN. NaN is the only "no value"; an SQI never says whether there is one.
// Each output of this firmware has its own convention for "no value", applied
// here and nowhere else:
//
//   CTRL,VIT (HMI)   the display quantities: hr = 0, spo2 = 0, pi = 0.00
//   telemetry        the clinical quantities: the key is not sent
//                    (vitals_has_value())
//
// The bounds do not restate the library's ranges (30-260 bpm, 70-100 %): they
// only keep the float -> integer conversion defined (NaN, infinity or a value
// out of the destination range is undefined behaviour) and the text field
// short, with margin above what the library publishes.
//
// Tests: motherBoard/test/test_vitals_out.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VITALS_HR_MAX_BPM 300.0f
#define VITALS_SPO2_MAX_PCT 100.0f
#define VITALS_PI_MAX_PCT 100.0f

// true when the library published a value (finite), false for NaN or infinity.
bool vitals_has_value(float x);

// Nearest integer bpm; 0 = no value (not finite, <= 0 or above the bound).
uint16_t vitals_hr_bpm(float hr);

// Nearest integer %; 0 = no value (not finite, <= 0 or above 100).
uint16_t vitals_spo2_pct(float spo2);

// The perfusion index in %; 0.0f = no value (not finite, negative or above
// the bound).
float vitals_pi_pct(float pi);

#ifdef __cplusplus
}
#endif
