#include "modules/debug/rx_diag.h"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr int kEvents = 24;
constexpr int kTextLen = 64;
// A spike: one sample this far from both neighbours, on the same side, while
// the neighbours agree with each other. 24 counts are ~10 px of the chart.
constexpr int kSpikeStep = 24;
constexpr int kSpikeNeighbours = 20;

struct Event {
  uint32_t ms;
  char kind[8];
  char text[kTextLen];
};

portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
Event s_events[kEvents];
uint32_t s_eventCount = 0;  // total ever, the ring keeps the last kEvents

volatile uint32_t s_lines = 0, s_ppg = 0, s_ppgBad = 0, s_spikes = 0;
volatile uint32_t s_garbled = 0, s_noPrefix = 0, s_rxTimeout = 0, s_tooLong = 0;
volatile uint32_t s_uart[6] = {0, 0, 0, 0, 0, 0};  // by hardwareSerial_error_t

// The last three pleth lines: value and raw text. Only the Comm task writes.
int s_val[3] = {-1, -1, -1};
char s_raw[3][20] = {"", "", ""};

// Printable ASCII only, and nothing that would break the JSON string.
void sanitize(char *dst, size_t cap, const char *src) {
  size_t i = 0;
  for (; src[i] != '\0' && i + 1 < cap; i++) {
    const unsigned char c = (unsigned char)src[i];
    dst[i] = (c < 0x20 || c > 0x7E || c == '"' || c == '\\') ? '?' : (char)c;
  }
  dst[i] = '\0';
}

void addEvent(const char *kind, const char *text, uint32_t now_ms) {
  Event e;
  e.ms = now_ms;
  snprintf(e.kind, sizeof(e.kind), "%s", kind);
  sanitize(e.text, sizeof(e.text), text);
  portENTER_CRITICAL(&s_mux);
  s_events[s_eventCount % kEvents] = e;
  s_eventCount++;
  portEXIT_CRITICAL(&s_mux);
}

bool hasOddChars(const char *line) {
  for (const char *p = line; *p != '\0'; p++) {
    const unsigned char c = (unsigned char)*p;
    if (c < 0x20 || c > 0x7E) return true;
  }
  return false;
}

// "CTRL,PPG," followed by one to three digits and nothing else, 0..255.
bool strictPpg(const char *line, int *value) {
  const char *p = line + 9;
  int n = 0, v = 0;
  while (p[n] >= '0' && p[n] <= '9' && n < 4) {
    v = v * 10 + (p[n] - '0');
    n++;
  }
  *value = v;
  return n >= 1 && n <= 3 && p[n] == '\0' && v <= 255;
}

}  // namespace

void rx_diag_uart_error(int err, uint32_t now_ms) {
  static const char *const kNames[6] = {"none", "break", "full",
                                        "fifo_ovf", "frame", "parity"};
  if (err < 0 || err > 5) return;
  s_uart[err] = s_uart[err] + 1;
  addEvent("uart", kNames[err], now_ms);
}

void rx_diag_line(const char *line, uint32_t now_ms) {
  s_lines = s_lines + 1;
  if (hasOddChars(line)) {
    s_garbled = s_garbled + 1;
    addEvent("garbled", line, now_ms);
  }
  if (strncmp(line, "CTRL,PPG,", 9) != 0) return;
  s_ppg = s_ppg + 1;

  int v = 0;
  const bool strict = strictPpg(line, &v);
  if (!strict) {
    s_ppgBad = s_ppgBad + 1;
    addEvent("ppg_bad", line, now_ms);
    // What the parser draws is sscanf("%d") cast to a byte: keep that value so
    // that the spike test sees what the screen sees.
    int drawn = 0;
    if (sscanf(line, "CTRL,PPG,%d", &drawn) != 1) return;
    v = drawn & 0xFF;
  }

  s_val[0] = s_val[1];
  s_val[1] = s_val[2];
  s_val[2] = v;
  memcpy(s_raw[0], s_raw[1], sizeof(s_raw[0]));
  memcpy(s_raw[1], s_raw[2], sizeof(s_raw[0]));
  sanitize(s_raw[2], sizeof(s_raw[2]), line);
  if (s_val[0] < 0) return;

  const int up = s_val[1] - s_val[0], down = s_val[1] - s_val[2];
  const int gap = s_val[2] - s_val[0];
  const bool spike = ((up > kSpikeStep && down > kSpikeStep) ||
                      (up < -kSpikeStep && down < -kSpikeStep)) &&
                     gap <= kSpikeNeighbours && gap >= -kSpikeNeighbours;
  if (spike) {
    s_spikes = s_spikes + 1;
    char text[kTextLen];
    snprintf(text, sizeof(text), "%s|%s|%s", s_raw[0], s_raw[1], s_raw[2]);
    addEvent("spike", text, now_ms);
  }
}

void rx_diag_no_prefix(const char *line, uint32_t now_ms) {
  s_noPrefix = s_noPrefix + 1;
  addEvent("noprefix", line, now_ms);
}

void rx_diag_rx_timeout(uint32_t now_ms) {
  s_rxTimeout = s_rxTimeout + 1;
  addEvent("timeout", "", now_ms);
}

void rx_diag_line_too_long(uint32_t now_ms) {
  s_tooLong = s_tooLong + 1;
  addEvent("toolong", "", now_ms);
}

size_t rx_diag_json(char *out, size_t out_len, uint32_t now_ms) {
  if (out == NULL || out_len == 0) return 0;
  size_t n = 0;
#define RXJ(...)                                                    \
  do {                                                              \
    if (n < out_len) {                                              \
      const int w = snprintf(out + n, out_len - n, __VA_ARGS__);    \
      if (w > 0) n += ((size_t)w < out_len - n) ? (size_t)w : out_len - n - 1; \
    }                                                               \
  } while (0)

  RXJ("{\"now_ms\":%lu,\"lines\":%lu,\"ppg\":%lu,\"ppg_bad\":%lu"
      ",\"spikes\":%lu,\"garbled\":%lu,\"no_prefix\":%lu,\"rx_timeout\":%lu"
      ",\"too_long\":%lu",
      (unsigned long)now_ms, (unsigned long)s_lines, (unsigned long)s_ppg,
      (unsigned long)s_ppgBad, (unsigned long)s_spikes,
      (unsigned long)s_garbled, (unsigned long)s_noPrefix,
      (unsigned long)s_rxTimeout, (unsigned long)s_tooLong);
  RXJ(",\"uart\":{\"break\":%lu,\"full\":%lu,\"fifo_ovf\":%lu,\"frame\":%lu"
      ",\"parity\":%lu}",
      (unsigned long)s_uart[1], (unsigned long)s_uart[2],
      (unsigned long)s_uart[3], (unsigned long)s_uart[4],
      (unsigned long)s_uart[5]);

  // A copy of the ring, oldest first, taken under the lock.
  static Event copy[kEvents];
  uint32_t total;
  portENTER_CRITICAL(&s_mux);
  total = s_eventCount;
  memcpy(copy, s_events, sizeof(copy));
  portEXIT_CRITICAL(&s_mux);
  const uint32_t have = total < (uint32_t)kEvents ? total : (uint32_t)kEvents;
  RXJ(",\"events_total\":%lu,\"events\":[", (unsigned long)total);
  for (uint32_t i = 0; i < have; i++) {
    const Event &e = copy[(total - have + i) % kEvents];
    RXJ("%s{\"ms\":%lu,\"k\":\"%s\",\"t\":\"%s\"}", i ? "," : "",
        (unsigned long)e.ms, e.kind, e.text);
  }
  RXJ("]}");
#undef RXJ
  return n;
}
