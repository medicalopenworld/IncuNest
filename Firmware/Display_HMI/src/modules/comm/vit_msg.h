#pragma once
// HMI side of CTRL,VIT (fused vitals, 1 Hz while the SpO2 probe is applied).
// No LVGL and no Arduino, so the parser can be tested on the host; what is
// shown for each value is decided in UITask.cpp. Contract in PROTOCOL.md.
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint16_t hr;    // bpm; 0 = no value. uint16_t: the library goes up to 260
  uint8_t  spo2;  // %; 0 = no value
  float    pi;    // %; NaN ("nan" on the wire) = no value; 0.0 if not sent
} VitMsg;

// "CTRL,VIT,<hr>,<spo2>[,<pi>]". hr and spo2 are required; pi is optional
// (older motherBoards sent two fields) and extra trailing fields are
// ignored. Returns false and leaves `out` untouched otherwise. No range
// checks: the motherBoard sends 0 for "no value" and the UI treats it so.
bool vit_parse(const char *line, VitMsg *out);
