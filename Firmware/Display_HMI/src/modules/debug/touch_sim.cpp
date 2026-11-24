#include "touch_sim.h"

namespace {

struct Pending {
  int16_t x;
  int16_t y;
  uint32_t ms;
};

Pending s_queue[TOUCH_SIM_QUEUE_LEN];
uint8_t s_head = 0;  // index of the oldest pending touch
uint8_t s_count = 0; // pending touches, not counting the active one

bool s_active = false;  // a simulated finger is down
bool s_release = false; // the touch just ended: report one released read
int16_t s_x = 0, s_y = 0;
uint32_t s_start = 0;
uint32_t s_ms = 0;

} // namespace

void touch_sim_reset() {
  s_head = 0;
  s_count = 0;
  s_active = false;
  s_release = false;
}

bool touch_sim_push(int16_t x, int16_t y, uint32_t ms, int16_t w, int16_t h) {
  if (ms == 0 || ms > TOUCH_SIM_MAX_MS) return false;
  if (x < 0 || y < 0 || x >= w || y >= h) return false;
  if (s_count >= TOUCH_SIM_QUEUE_LEN) return false;
  const uint8_t tail = (uint8_t)((s_head + s_count) % TOUCH_SIM_QUEUE_LEN);
  s_queue[tail] = {x, y, ms};
  s_count++;
  return true;
}

TouchSimPoint touch_sim_poll(uint32_t now_ms) {
  if (s_active) {
    // Unsigned difference: correct across the millis() wrap.
    if (now_ms - s_start < s_ms) return {true, s_x, s_y};
    s_active = false;
    s_release = true;
  }
  if (s_release) {
    // One released read between touches, so LVGL sees a click end.
    s_release = false;
    return {false, s_x, s_y};
  }
  if (s_count == 0) return {false, 0, 0};
  const Pending &p = s_queue[s_head];
  s_head = (uint8_t)((s_head + 1) % TOUCH_SIM_QUEUE_LEN);
  s_count--;
  s_active = true;
  s_x = p.x;
  s_y = p.y;
  s_ms = p.ms;
  s_start = now_ms;
  return {true, s_x, s_y};
}

bool touch_sim_busy() { return s_active || s_release || s_count > 0; }
