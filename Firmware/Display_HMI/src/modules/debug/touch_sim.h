#pragma once
#include <stdint.h>

// Queue of simulated touches for POST /debug/touch (HMI_DEBUG_UI_AUTOMATION
// builds only). The LVGL read callback asks it, every read, whether a
// simulated finger is down and where; if so that point is reported pressed,
// otherwise the real touch controller is read as usual.
//
// Not thread-safe on its own: every call goes under LVGL_Lock, which the read
// callback already holds (it runs inside lv_timer_handler).
//
// Timing: a touch starts at the first read after it reaches the head of the
// queue and stays pressed for `ms` from then. After it, at least one read
// reports released, so two queued taps on the same point are two clicks.

constexpr uint8_t TOUCH_SIM_QUEUE_LEN = 8;
constexpr uint32_t TOUCH_SIM_DEFAULT_MS = 80;
constexpr uint32_t TOUCH_SIM_MAX_MS = 10000;

struct TouchSimPoint {
  bool pressed;
  int16_t x;
  int16_t y;
};

// Resets the queue (also for tests).
void touch_sim_reset();

// Queues a touch at (x, y) for ms milliseconds. false if the queue is full,
// ms is 0 or above TOUCH_SIM_MAX_MS, or the point is outside w x h.
bool touch_sim_push(int16_t x, int16_t y, uint32_t ms, int16_t w, int16_t h);

// Called from the LVGL read callback with the current time. pressed=false
// means "no simulated finger: read the panel".
TouchSimPoint touch_sim_poll(uint32_t now_ms);

// true while a touch is queued or pressed, or the release read after it is
// still due.
bool touch_sim_busy();
