#include "modules/debug/afe_diag.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace {

// A raw code is an outlier when it stands this far from the mean of its two
// neighbours while the neighbours agree with each other. The second difference
// of a clean signal is ~100 counts (captures of this board), so the first bin
// is already more than ten times the noise.
constexpr int32_t kRawBins[3] = {1500, 15000, 150000};
// ppg_disp moves at most ~0.06 per sample on a 160 bpm pulse at full scale.
constexpr float kPpgStep = 0.12f;
// The test the HMI's diagnostics apply to the bytes it draws, on the same
// spacing (one sample in ten): a point 24 counts of 127 away from the two
// points 20 ms before and after it, which agree within 20 counts.
constexpr float kBumpDev = 24.0f / 127.0f, kBumpNeighbours = 20.0f / 127.0f;
constexpr int kBumpLag = 10;
constexpr uint32_t kBumpGapMs = 100;  // one count per bump, not one per phase
constexpr uint32_t kSettlingBit = 0x00008000UL;  // RSQM_DIAG_SWITCHED_RC_SETTLING

constexpr int kEvents = 32;
constexpr int kCaptures = 5;
constexpr int kCapLen = 64;       // samples in a capture (128 ms)
constexpr int kCapPost = 30;      // collected after the trigger
constexpr uint32_t kCapGapMs = 5000;  // one capture per burst
constexpr int kDrainBins = 14;    // 0..12 and "13 or more"

struct Rec {
  int32_t led1, led2, aled1, aled2;
  float ppg;
  uint16_t diag;  // low 16 bits: the register and the settling flag
};

struct Event {
  uint32_t ms;
  uint8_t mask;       // bit 0..3: led1, led2, aled1, aled2; bit 4: ppg step;
                      // bit 5: ppg bump, as the HMI would see it
  int32_t dev[4];     // distance to the neighbours' mean
  float step;         // ppg_disp[n] - ppg_disp[n-1]
  uint16_t diag;
};

struct Capture {
  uint32_t ms;
  uint8_t mask;
  Rec s[kCapLen];
};

SemaphoreHandle_t s_lock = nullptr;

// Counters: written only by SPO2_Task, read without the lock (32-bit loads).
volatile uint32_t s_samples = 0, s_turns = 0, s_applied = 0, s_skipped = 0;
volatile uint32_t s_drain[kDrainBins] = {};
volatile uint32_t s_rawCount[4][3] = {};
volatile int32_t s_rawMax[4] = {};
volatile uint32_t s_ppgSteps = 0, s_ppgBumps = 0;
volatile float s_ppgMaxStep = 0.0f;
volatile uint32_t s_settlingSamples = 0, s_settlingEpisodes = 0, s_settlingLastMs = 0;
volatile uint32_t s_diagOr = 0;
volatile uint32_t s_secMin = 0xFFFFFFFFu, s_secMax = 0, s_secLast = 0;

// Only SPO2_Task.
Rec s_ring[kCapLen];
uint32_t s_ringN = 0;          // samples ever written to the ring
int s_appliedRun = 0;          // consecutive samples with the probe applied
bool s_wasSettling = false;
uint32_t s_secStartMs = 0, s_secCount = 0;
int s_capPending = -1;         // samples still to collect for the capture, or -1
uint32_t s_capMs = 0, s_lastCapMs = 0;
uint8_t s_capMask = 0;
uint32_t s_lastBumpMs = 0;

// Under s_lock.
Event s_events[kEvents];
uint32_t s_eventCount = 0;
Capture s_caps[kCaptures];
int s_capCount = 0;

inline int32_t iabs(int32_t v) { return v < 0 ? -v : v; }

const Rec &back(int k) {  // k = 0: the newest sample
  return s_ring[(s_ringN - 1 - (uint32_t)k) % kCapLen];
}

}  // namespace

void afe_diag_init(void) {
  if (s_lock == nullptr) {
    s_lock = xSemaphoreCreateMutex();
  }
}

void afe_diag_turn(int drained, uint32_t now_ms) {
  (void)now_ms;
  s_turns = s_turns + 1;
  const int b = drained < 0 ? 0 : (drained >= kDrainBins ? kDrainBins - 1 : drained);
  s_drain[b] = s_drain[b] + 1;
}

void afe_diag_feed(const AfeDiagSample *s, uint32_t now_ms) {
  s_samples = s_samples + 1;
  s_diagOr = s_diagOr | s->diag_code;

  // Samples per second, by this processor's clock: 500, give or take one.
  if (s_secStartMs == 0) s_secStartMs = now_ms;
  if ((uint32_t)(now_ms - s_secStartMs) >= 1000u) {
    s_secLast = s_secCount;
    if (s_secCount < s_secMin) s_secMin = s_secCount;
    if (s_secCount > s_secMax) s_secMax = s_secCount;
    s_secStartMs += 1000u;
    if ((uint32_t)(now_ms - s_secStartMs) >= 1000u) s_secStartMs = now_ms;  // after a pause
    s_secCount = 0;
  }
  s_secCount++;

  // RF changes of the library's automatic gain control.
  const bool settling = (s->diag_code & kSettlingBit) != 0;
  if (settling) {
    s_settlingSamples = s_settlingSamples + 1;
    if (!s_wasSettling) {
      s_settlingEpisodes = s_settlingEpisodes + 1;
      s_settlingLastMs = now_ms;
    }
  }
  s_wasSettling = settling;

  Rec r;
  r.led1 = s->led1;
  r.led2 = s->led2;
  r.aled1 = s->aled1;
  r.aled2 = s->aled2;
  r.ppg = s->ppg_disp;
  r.diag = (uint16_t)(s->diag_code & 0xFFFFu);
  s_ring[s_ringN % kCapLen] = r;
  s_ringN++;

  s_appliedRun = s->applied ? s_appliedRun + 1 : 0;
  if (s->applied) s_applied = s_applied + 1;

  // The tests need three samples in a row with the probe applied: n-2, n-1, n.
  uint8_t mask = 0;
  int32_t dev[4] = {0, 0, 0, 0};
  float step = 0.0f;
  if (s_appliedRun >= 3 && s_ringN >= 3) {
    const Rec &a = back(2), &m = back(1), &c = back(0);
    const int32_t av[4] = {a.led1, a.led2, a.aled1, a.aled2};
    const int32_t mv[4] = {m.led1, m.led2, m.aled1, m.aled2};
    const int32_t cv[4] = {c.led1, c.led2, c.aled1, c.aled2};
    for (int ch = 0; ch < 4; ch++) {
      const int32_t d = mv[ch] - (av[ch] / 2 + cv[ch] / 2);
      const int32_t nb = iabs(cv[ch] - av[ch]);
      dev[ch] = d;
      if (iabs(d) > kRawBins[0] && nb < iabs(d) / 2) {
        const int bin = iabs(d) > kRawBins[2] ? 2 : (iabs(d) > kRawBins[1] ? 1 : 0);
        s_rawCount[ch][bin] = s_rawCount[ch][bin] + 1;
        if (iabs(d) > s_rawMax[ch]) s_rawMax[ch] = iabs(d);
        mask |= (uint8_t)(1u << ch);
      }
    }
    if (isfinite(c.ppg) && isfinite(m.ppg)) {
      step = c.ppg - m.ppg;
      if (fabsf(step) > kPpgStep) {
        s_ppgSteps = s_ppgSteps + 1;
        if (fabsf(step) > s_ppgMaxStep) s_ppgMaxStep = fabsf(step);
        mask |= 0x10;
      }
    }
    if (s_appliedRun > 2 * kBumpLag && s_ringN > (uint32_t)(2 * kBumpLag)) {
      const float pa = back(2 * kBumpLag).ppg, pm = back(kBumpLag).ppg, pc = c.ppg;
      if (isfinite(pa) && isfinite(pm) && isfinite(pc) &&
          fabsf(pm - (pa + pc) / 2.0f) > kBumpDev && fabsf(pc - pa) < kBumpNeighbours &&
          ((pm > pa && pm > pc) || (pm < pa && pm < pc)) &&
          (uint32_t)(now_ms - s_lastBumpMs) > kBumpGapMs) {
        s_lastBumpMs = now_ms;
        s_ppgBumps = s_ppgBumps + 1;
        mask |= 0x20;
      }
    }
  }

  const bool capDone = s_capPending == 0;
  if (s_capPending > 0) s_capPending--;
  const bool startCap = mask != 0 && s_capPending < 0 &&
                        (s_lastCapMs == 0 || (uint32_t)(now_ms - s_lastCapMs) > kCapGapMs);
  if (mask == 0 && !capDone) return;

  // Never wait here: this is the task that feeds the display. If the reader
  // holds the lock, this event is counted as skipped and nothing else.
  if (s_lock == nullptr || xSemaphoreTake(s_lock, 0) != pdTRUE) {
    if (mask != 0) s_skipped = s_skipped + 1;
    return;
  }
  if (mask != 0) {
    Event &e = s_events[s_eventCount % kEvents];
    e.ms = now_ms;
    e.mask = mask;
    memcpy(e.dev, dev, sizeof(dev));
    e.step = step;
    e.diag = r.diag;
    s_eventCount++;
    if (startCap && s_capCount < kCaptures) {
      s_capPending = kCapPost;
      s_capMs = now_ms;
      s_capMask = mask;
      s_lastCapMs = now_ms;
    }
  }
  if (capDone) {
    if (s_capCount < kCaptures) {
      Capture &c = s_caps[s_capCount];
      c.ms = s_capMs;
      c.mask = s_capMask;
      for (int i = 0; i < kCapLen; i++) c.s[i] = back(kCapLen - 1 - i);  // oldest first
      s_capCount++;
    }
    s_capPending = -1;
  }
  xSemaphoreGive(s_lock);
}

size_t afe_diag_json(char *out, size_t out_len, uint32_t now_ms) {
  if (out == NULL || out_len == 0) return 0;
  size_t n = 0;
#define AJ(...)                                                                \
  do {                                                                         \
    if (n < out_len) {                                                         \
      const int w = snprintf(out + n, out_len - n, __VA_ARGS__);               \
      if (w > 0) n += ((size_t)w < out_len - n) ? (size_t)w : out_len - n - 1; \
    }                                                                          \
  } while (0)

  AJ("{\"now_ms\":%lu,\"samples\":%lu,\"applied\":%lu,\"turns\":%lu,\"skipped\":%lu",
     (unsigned long)now_ms, (unsigned long)s_samples, (unsigned long)s_applied,
     (unsigned long)s_turns, (unsigned long)s_skipped);
  AJ(",\"per_s\":{\"min\":%lu,\"max\":%lu,\"last\":%lu}",
     (unsigned long)(s_secMin == 0xFFFFFFFFu ? 0 : s_secMin), (unsigned long)s_secMax,
     (unsigned long)s_secLast);
  AJ(",\"drain\":[");
  for (int i = 0; i < kDrainBins; i++) AJ("%s%lu", i ? "," : "", (unsigned long)s_drain[i]);
  AJ("],\"gain\":{\"settling_samples\":%lu,\"episodes\":%lu,\"last_ms\":%lu}"
     ",\"diag_or\":\"0x%lX\"",
     (unsigned long)s_settlingSamples, (unsigned long)s_settlingEpisodes,
     (unsigned long)s_settlingLastMs, (unsigned long)s_diagOr);
  static const char *const kCh[4] = {"led1", "led2", "aled1", "aled2"};
  AJ(",\"raw\":{");
  for (int ch = 0; ch < 4; ch++) {
    AJ("%s\"%s\":[%lu,%lu,%lu,%ld]", ch ? "," : "", kCh[ch],
       (unsigned long)s_rawCount[ch][0], (unsigned long)s_rawCount[ch][1],
       (unsigned long)s_rawCount[ch][2], (long)s_rawMax[ch]);
  }
  AJ("},\"ppg\":{\"steps\":%lu,\"max_step\":%.3f,\"bumps\":%lu}",
     (unsigned long)s_ppgSteps, (double)s_ppgMaxStep, (unsigned long)s_ppgBumps);

  if (s_lock != nullptr && xSemaphoreTake(s_lock, pdMS_TO_TICKS(200)) == pdTRUE) {
    const uint32_t total = s_eventCount;
    const uint32_t have = total < (uint32_t)kEvents ? total : (uint32_t)kEvents;
    AJ(",\"events_total\":%lu,\"events\":[", (unsigned long)total);
    for (uint32_t i = 0; i < have; i++) {
      const Event &e = s_events[(total - have + i) % kEvents];
      AJ("%s{\"ms\":%lu,\"m\":%u,\"d\":[%ld,%ld,%ld,%ld],\"st\":%.3f,\"dg\":%u}",
         i ? "," : "", (unsigned long)e.ms, (unsigned)e.mask, (long)e.dev[0],
         (long)e.dev[1], (long)e.dev[2], (long)e.dev[3], (double)e.step,
         (unsigned)e.diag);
    }
    // Each capture: kCapLen samples, oldest first; "trig" is the index of the
    // sample that raised it (a raw outlier is the sample before it, the point
    // of a bump kBumpLag samples before it). A row is [led1, led2, aled1,
    // aled2, ppg_disp x 10000, diag].
    AJ("],\"trig\":%d,\"caps\":[", kCapLen - 2 - kCapPost);
    for (int k = 0; k < s_capCount; k++) {
      const Capture &c = s_caps[k];
      AJ("%s{\"ms\":%lu,\"m\":%u,\"s\":[", k ? "," : "", (unsigned long)c.ms,
         (unsigned)c.mask);
      for (int i = 0; i < kCapLen; i++) {
        const Rec &r = c.s[i];
        AJ("%s[%ld,%ld,%ld,%ld,%d,%u]", i ? "," : "", (long)r.led1, (long)r.led2,
           (long)r.aled1, (long)r.aled2,
           (int)(isfinite(r.ppg) ? lroundf(r.ppg * 10000.0f) : 99999),
           (unsigned)r.diag);
      }
      AJ("]}");
    }
    AJ("]");
    xSemaphoreGive(s_lock);
  }
  AJ("}");
#undef AJ
  return n;
}
