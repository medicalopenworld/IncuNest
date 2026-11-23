#pragma once

// Diagnostics of the board -> display serial link.
//
// Why: the pleth on the lock screen shows bursts of isolated spikes, a few
// seconds long, about once a minute (bench, 2026-11-18). The link has no
// checksum and the parser draws any "CTRL,PPG,<n>" it can read, so a damaged
// line and a line the board really sent look the same on screen. This module
// tells them apart: it counts what the UART driver reports, what the line
// reader throws away and what the parser would accept although it is not a
// well-formed line, and it keeps the raw text around every spike.
//
// It only observes. Nothing here changes what is parsed or drawn.
//
// Read with GET /debug/rx.

#include <stddef.h>
#include <stdint.h>

// A UART driver event: the value of hardwareSerial_error_t.
void rx_diag_uart_error(int err, uint32_t now_ms);
// A complete line that starts with the expected prefix, before it is parsed.
void rx_diag_line(const char *line, uint32_t now_ms);
// A complete line that does not start with the expected prefix (discarded).
void rx_diag_no_prefix(const char *line, uint32_t now_ms);
// The line reader dropped a half-received line after a pause.
void rx_diag_rx_timeout(uint32_t now_ms);
// The line reader dropped a line longer than its buffer.
void rx_diag_line_too_long(uint32_t now_ms);
// JSON with the counters and the last events. Returns the length written.
size_t rx_diag_json(char *out, size_t out_len, uint32_t now_ms);
