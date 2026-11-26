## ADDED Requirements

### Requirement: Notice duration

The motherBoard SHALL show the notice from the moment a `hand_hold_until` value (epoch, ms) is applied. With a network clock (`PROTO_TIME_SOURCE_NTP` or `PROTO_TIME_SOURCE_NITZ`) it SHALL last until `hand_hold_until`, capped at 30 min. With any other clock source, or no time, it SHALL last a fixed 5 min from the moment the value arrives, and a value that arrives as the reply to the reconnect attribute request SHALL NOT start a notice. A `hand_hold_until` in the past SHALL be ignored. Deadlines SHALL be compared with unsigned subtraction so they survive `millis()` wrap-around.

#### Scenario: Trusted clock shows until the deadline
- **WHEN** the clock comes from the network and `hand_hold_until` is now + 120 s
- **THEN** the notice is active at 119 s and off at 121 s (motherBoard `[env:native]`, `test_trusted_clock_shows_until`)

#### Scenario: Trusted clock caps at 30 minutes
- **WHEN** the clock comes from the network and `hand_hold_until` is now + 2 h
- **THEN** the notice turns off at 1800 s + 1 (motherBoard `[env:native]`, `test_trusted_clock_caps_at_30_min`)

#### Scenario: Past deadline is ignored
- **WHEN** `hand_hold_until` is 1 s in the past
- **THEN** no notice is shown (motherBoard `[env:native]`, `test_past_until_ignored`)

#### Scenario: Untrusted clock uses a fixed window
- **WHEN** the clock source is not NTP or NITZ and `hand_hold_until` is now + 2 h
- **THEN** the notice lasts exactly 300 s (motherBoard `[env:native]`, `test_untrusted_clock_uses_fixed_window`)

#### Scenario: Reconnect request with an untrusted clock
- **WHEN** `hand_hold_until` arrives as the reply to the reconnect request and the clock is not from the network
- **THEN** no notice is shown (motherBoard `[env:native]`, `test_request_with_untrusted_clock_ignored`)

#### Scenario: Reconnect request with a trusted clock
- **WHEN** `hand_hold_until` arrives as the reply to the reconnect request, the clock is from the network and the value is in the future
- **THEN** the notice is shown (motherBoard `[env:native]`, `test_request_with_trusted_clock_shows`)

#### Scenario: Millis wrap-around
- **WHEN** a 5 min window starts at `millis()` = 0xFFFFF000 and the counter wraps
- **THEN** the notice is still active after the wrap (motherBoard `[env:native]`, `test_millis_wraparound`)

### Requirement: Duplicate and older deadlines

The motherBoard SHALL ignore any `hand_hold_until` equal to or older than the last one applied, because ThingsBoard re-sends the latest value on every connection. It SHALL record the value as applied even when it ignores it for an untrusted clock, so a later duplicate does not count either. A newer value received while the notice is active SHALL extend it.

#### Scenario: Duplicate after expiry
- **WHEN** the notice expired and ThingsBoard re-sends the same `hand_hold_until` on reconnection
- **THEN** the notice stays off (motherBoard `[env:native]`, `test_duplicate_until_ignored`)

#### Scenario: Newer deadline extends
- **WHEN** a greater `hand_hold_until` arrives while the notice is active
- **THEN** the notice takes the new deadline (motherBoard `[env:native]`, `test_newer_until_extends`)

### Requirement: Valid QR URL

The `family_qr_url` SHALL be valid only if it starts with `https://`, has 9 to 96 characters, contains only printable ASCII 0x21-0x7E and no comma. An invalid, empty or absent URL SHALL be treated as empty: no QR is shown, and the text-only notice is unaffected. In a live attribute update a missing `family_qr_url` key SHALL NOT change a URL already stored. In the reply to the reconnect attribute request, which is a full snapshot of the requested keys, a missing `family_qr_url` key SHALL clear the stored URL (a deleted attribute is simply absent there; the live deletion arrives as `{"deleted":[...]}`, which the key-filtered callback never sees). The server SHALL clear the code by writing `family_qr_url` = `""` and SHALL NOT delete the attribute, and SHOULD keep the URL at about 40 characters or less so the 105 px lock-screen QR gets at least 3 px per module.

#### Scenario: Plain http is rejected
- **WHEN** the URL is `http://a.org/f/1`
- **THEN** it is invalid (motherBoard `[env:native]`, `test_url_rejects_http`)

#### Scenario: Comma, space and control characters are rejected
- **WHEN** the URL is `https://a.org/f,1`, `https://a.org/ f` or ends in a tab
- **THEN** it is invalid (motherBoard `[env:native]`, `test_url_rejects_comma_space_ctrl`)

#### Scenario: Length limits
- **WHEN** the URL has 97 characters, or 96 characters
- **THEN** the first is invalid and the second is valid (motherBoard `[env:native]`, `test_url_rejects_too_long`)

#### Scenario: Bare scheme is rejected
- **WHEN** the URL is `https://`
- **THEN** it is invalid (motherBoard `[env:native]`, `test_url_rejects_bare_scheme`)

#### Scenario: A bad URL clears the QR
- **WHEN** a valid URL is stored and then `http://x` arrives
- **THEN** the stored URL becomes empty (motherBoard `[env:native]`, `test_bad_url_clears_qr`)

#### Scenario: Missing key in a live update keeps the URL
- **WHEN** a live attribute update carries `hand_hold_until` but not `family_qr_url`
- **THEN** the stored URL is kept (motherBoard `[env:native]`, `test_url_action_live_update`)

#### Scenario: Missing key in the reconnect reply clears the URL
- **WHEN** `family_qr_url` was deleted in ThingsBoard and the board reconnects, so the reply to the attribute request lacks the key
- **THEN** the stored URL is cleared and no QR is shown (motherBoard `[env:native]`, `test_url_action_request_missing_key_clears`; the wiring in `family_state.cpp` is manual verification on the bench)

#### Scenario: Server clears the code with an empty string
- **WHEN** the server writes `family_qr_url` = `""`
- **THEN** the stored URL is cleared at once, live (motherBoard `[env:native]`, `test_url_action_live_update` and `test_bad_url_clears_qr`)

### Requirement: Discharge clears the notice

When a `PROFILE_DISCHARGE` is confirmed on the motherBoard, it SHALL forget the family URL and switch the notice off at once.

#### Scenario: Discharge with an active notice
- **WHEN** a notice is active with a URL and the discharge is confirmed
- **THEN** the notice is off and the URL is empty (motherBoard `[env:native]`, `test_discharge_clears_all`; the wiring in `CommTask.cpp` is manual verification, no test environment)

### Requirement: CTRL,FAMILY line

The motherBoard SHALL send `CTRL,FAMILY,<0|1>,<url>` (the url may be empty: `CTRL,FAMILY,1,`) only when the visible state (notice active, URL) changes and on `HMI,UI_READY` or `HMI,REQ,STATE`; it SHALL NOT send it periodically (`known_issues.md` #2). The builder SHALL return 0 if the buffer is too small or the URL is neither empty nor valid. The parser SHALL discard a malformed line whole (bad flag, missing or extra field) leaving no notice and no URL, and the Display_HMI SHALL keep its last valid message when a line is discarded.

#### Scenario: Build with URL
- **WHEN** the message is notice on with `https://incutwin.org/f/7KQ2M9`
- **THEN** the line is `CTRL,FAMILY,1,https://incutwin.org/f/7KQ2M9\n` (motherBoard `[env:native]`, `test_build_with_url`)

#### Scenario: Build without URL
- **WHEN** the message is notice off with an empty URL
- **THEN** the line is `CTRL,FAMILY,0,\n` (motherBoard `[env:native]`, `test_build_without_url`)

#### Scenario: Buffer too small
- **WHEN** the buffer is 10 bytes
- **THEN** the builder returns 0 (motherBoard `[env:native]`, `test_build_too_small_returns_0`)

#### Scenario: Round trip
- **WHEN** a built line (without `\n`) is parsed
- **THEN** both fields match (motherBoard `[env:native]`, `test_roundtrip`)

#### Scenario: Malformed lines
- **WHEN** the line has flag `2` or `x`, lacks the URL field, or has an extra field
- **THEN** the parser rejects it and leaves `notice = false` and an empty URL (motherBoard `[env:native]`, `test_parse_rejects_bad_flag`, `test_parse_rejects_missing_field`, `test_parse_rejects_extra_field`)

#### Scenario: Sent only on change
- **WHEN** the state is queried twice without change, then again with `force`
- **THEN** only the first and the forced calls return a message (motherBoard `[env:native]`, `test_take_msg_only_on_change`)

#### Scenario: Expiry is a change
- **WHEN** the notice deadline passes
- **THEN** a message with `notice = 0` is due (motherBoard `[env:native]`, `test_take_msg_on_expiry`)

#### Scenario: Old and new boards
- **WHEN** an old display receives the line, or a new display talks to an old board
- **THEN** the old display discards it (log only) and the old board never sends it; nothing visible happens (manual verification on hardware)

### Requirement: Display forgets on board-link loss and asks again on recovery

When the board link is lost (`Display_IsBoardLinkLost()`), the Display_HMI SHALL forget the notice and the URL, including a `CTRL,FAMILY` line received but not yet applied, so a stale notice or a previous family's QR cannot come back by itself. When the link recovers it SHALL send `HMI,REQ,STATE` (display-to-board traffic that makes the board resend its state, `CTRL,FAMILY` included) up to 3 times, 3 s apart, and SHALL stop as soon as a `CTRL,FAMILY` line arrives. The cap exists because an old board never sends `CTRL,FAMILY`.

#### Scenario: Link lost forgets notice and URL
- **WHEN** a notice with a URL is stored and the board link is lost
- **THEN** the lock-screen notice and the Help "Familia" card disappear, and they do not come back until the board sends `CTRL,FAMILY` again (manual verification on hardware, no test environment)

#### Scenario: Link recovered asks for the state
- **WHEN** the board link recovers
- **THEN** the display sends `HMI,REQ,STATE` at once and, while no `CTRL,FAMILY` has arrived, again after 3 s and 6 s; it sends none after a `CTRL,FAMILY` arrives (manual verification on hardware, no test environment)

#### Scenario: Old board after recovery
- **WHEN** the link recovers with a board that never sends `CTRL,FAMILY`
- **THEN** the display sends exactly 3 `HMI,REQ,STATE` and stops (manual verification on hardware, no test environment)

### Requirement: Painting conditions on the lock screen

The Display_HMI SHALL paint the notice in the `ui_ScreenLock` bottom strip (792 x 117 px, background `0x054E92`, QR 105 px) only if a notice is on, the lock screen is the active screen, no alarm is active (`UI_IsAnyAlarmActive()`), the board link is alive (`!Display_IsBoardLinkLost()`), the pulse-oximeter panel (`ui_LockPulseOxFrame`) is hidden and the unlock slider (`ui_UnlockCont`) is hidden. With a valid URL it SHALL show text and QR; without one, text only. It SHALL return by itself when the blocking condition ends while the notice is still valid. It SHALL be drawn below the unlock slider in z-order. It SHALL NOT be modal, SHALL NOT be clickable, SHALL NOT make any sound and SHALL NOT use red, yellow, orange or cyan.

#### Scenario: All clear with URL
- **WHEN** all conditions hold and there is a URL
- **THEN** the mode is text and QR (Display_HMI `[env:native]`, `test_all_clear_with_url_shows_qr`)

#### Scenario: No URL
- **WHEN** all conditions hold and there is no URL
- **THEN** the mode is text only (Display_HMI `[env:native]`, `test_without_url_text_only`)

#### Scenario: No notice
- **WHEN** there is no notice, even with a URL
- **THEN** the notice is hidden (Display_HMI `[env:native]`, `test_no_notice_hidden`)

#### Scenario: Not on the lock screen
- **WHEN** another screen is active
- **THEN** the notice is hidden (Display_HMI `[env:native]`, `test_not_on_lock_hidden`)

#### Scenario: Alarm hides it, and it returns
- **WHEN** any alarm becomes active and later clears while the notice is still valid
- **THEN** the notice hides at once and returns afterwards (Display_HMI `[env:native]`, `test_alarm_hides_then_returns`)

#### Scenario: Board link lost
- **WHEN** the link with the board is lost
- **THEN** the notice is hidden (Display_HMI `[env:native]`, `test_link_lost_hides`)

#### Scenario: Pulse oximeter shown
- **WHEN** the pulse-oximeter panel is visible
- **THEN** the notice is hidden (Display_HMI `[env:native]`, `test_pulseox_hides`)

#### Scenario: Unlock slider shown
- **WHEN** the unlock slider is visible
- **THEN** the notice is hidden and a touch is never swallowed by the notice (Display_HMI `[env:native]`, `test_unlock_slider_hides`; the touch behaviour is manual verification)

#### Scenario: Visual and silent
- **WHEN** the notice is on screen
- **THEN** it uses `0x054E92` with no red, yellow, orange or cyan, is not clickable and makes no sound (manual verification on hardware, no test environment)

### Requirement: Help "Familia" card

The Help dialog SHALL offer a fourth "Familia" card (blue `0x054E92`, four cards of 178 px with 16 px gaps) showing a big QR of the stored URL whenever there is a valid URL and the board link is alive, whether or not a notice is running: the card is for the nurse to show the mother the QR at admission. Otherwise it SHALL show the three cards as before. The end of a notice SHALL NOT close the "Familia" view. If the URL disappears while the view is open, it SHALL return to the menu; if the URL changes, it SHALL redraw the QR with the new one. The Help QR SHALL NOT depend on whether the small lock-screen QR could be encoded; if its own 300 px encode failed, the view SHALL show the address as text instead of a QR.

#### Scenario: URL and link alive
- **WHEN** there is a URL and the link is alive
- **THEN** the card is offered (Display_HMI `[env:native]`, `test_help_entry`)

#### Scenario: URL without notice
- **WHEN** there is a URL, the link is alive and no notice is running
- **THEN** the lock-screen notice is hidden and the Help "Familia" card is offered (Display_HMI `[env:native]`, `test_url_without_notice_help_only`)

#### Scenario: Notice ends with the Familia view open
- **WHEN** the "Familia" view is open and the notice ends while the URL stays
- **THEN** the view stays open (manual verification on hardware, no test environment)

#### Scenario: No URL
- **WHEN** there is no URL
- **THEN** the card is not offered (Display_HMI `[env:native]`, `test_help_entry`)

#### Scenario: Link lost
- **WHEN** there is a URL but the link is lost
- **THEN** the card is not offered (Display_HMI `[env:native]`, `test_help_entry`)

#### Scenario: Layout and QR
- **WHEN** the card is offered and opened
- **THEN** four cards fit and the big QR encodes the stored URL (manual verification on hardware, no test environment)
