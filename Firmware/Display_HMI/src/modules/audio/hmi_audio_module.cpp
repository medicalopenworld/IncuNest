#include "hmi_audio_module.h"
#include "buzzer.h"
#include <Arduino.h>

// The HMI buzzer is controlled via I2C (STC8H1K28 @ 0x30).
// buzzer.h exposes buzzerOn() / buzzerOff() only — no freq/duration API.
// hmi_audio_module_beep() approximates a timed beep using those primitives.

void hmi_audio_module_init(void) {
  // No explicit init required: I2C is initialised in main setup() via Wire.begin().
  // Ensure buzzer is silent at startup.
  buzzerOff();
}

void hmi_audio_module_beep(int freq_hz, int duration_ms) {
  (void)freq_hz; // frequency not controllable on this hardware
  buzzerOn();
  delay((duration_ms > 0) ? duration_ms : 100);
  buzzerOff();
}
