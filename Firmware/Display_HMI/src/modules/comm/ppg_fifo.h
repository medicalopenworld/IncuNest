#pragma once
// CTRL,PPG samples on their way from the parser (CommTask) to the lock-screen
// chart (UITask). No LVGL and no Arduino, so it can be tested on the host.
//
// Single-producer / single-consumer ring buffer. Until 4.7.0 this was a
// one-slot mailbox (ppg + updated flag): whenever lv_timer_handler() took
// longer than one sample period the next sample overwrote the previous one,
// and repaints of more than 40 ms are documented in UITask.cpp. Now UITask
// drains everything that arrived since its last pass.
//
// head and tail are uint8_t counters that wrap freely, each written by one
// side only, so no lock is needed; the length divides 256 so the index stays
// right across the wrap. `dropped` counts samples lost to a full buffer
// (bench diagnostics, DIAG log).
#include <stdbool.h>
#include <stdint.h>

#define PPG_FIFO_LEN 32

typedef struct {
  volatile uint8_t  buf[PPG_FIFO_LEN];
  volatile uint8_t  head;     // written by the producer only
  volatile uint8_t  tail;     // written by the consumer only
  volatile uint16_t dropped;  // written by the producer only
} PpgFifo;

// Producer. false = full: the sample is dropped and counted.
bool ppg_fifo_push(PpgFifo *f, uint8_t v);

// Consumer. false = empty: `v` is not touched.
bool ppg_fifo_pop(PpgFifo *f, uint8_t *v);
