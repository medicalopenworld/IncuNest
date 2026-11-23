#pragma once

// Diagnostics of the AFE4490 sample stream, for the spikes of the pleth.
//
// Why: the pleth on the HMI shows bursts of isolated spikes, a few seconds
// long, 66 or 99 s apart (always a multiple of ~33 s). Measured on the bench on
// 2026-11-18: the serial link is clean while they happen, so the motherBoard
// sends those values, and they come straight from the library's ppg_disp.
// 33 s is what two clocks 30 ppm apart take to slide 1 ms: the AFE4490's own
// clock (a sample every 2 ms) against this processor's 1 ms tick.
//
// What this answers, looking at every one of the 500 samples a second and not
// at the one in ten that goes to the HMI:
//   - Does the chip already deliver a wrong raw code (led1, led2, aled1,
//     aled2), or are the raw codes clean and only ppg_disp jumps?
//   - Are samples lost (fewer than 500 in a second)?
//   - Does it happen while the library reports an RF change of its automatic
//     gain control (diag_code bit 15)?
// It keeps the samples around the first events, to see their shape.
//
// It only observes. Read with GET /debug/afe.

#include <stddef.h>
#include <stdint.h>

struct AfeDiagSample {
  int32_t led1, led2, aled1, aled2;  // raw ADC codes
  float ppg_disp;                    // [-1, +1]
  uint32_t diag_code;
  bool applied;                      // probe_state == PROBE_APPLIED
};

void afe_diag_init(void);
// Every sample the library delivers, in order.
void afe_diag_feed(const AfeDiagSample *s, uint32_t now_ms);
// Once per turn of SPO2_Task: how many samples that turn took from the queue.
void afe_diag_turn(int drained, uint32_t now_ms);
// JSON with the counters, the last events and the captures. Returns the length.
size_t afe_diag_json(char *out, size_t out_len, uint32_t now_ms);
