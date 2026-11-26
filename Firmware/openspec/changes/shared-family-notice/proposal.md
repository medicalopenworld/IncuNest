## Why

Boards affected: **motherBoard** (decides when a notice is due, validates the URL, sends it) and **Display_HMI** (paints it on the lock screen and in Help), plus **shared** (the new `CTRL,FAMILY` line format and its parser, consumed by both).

IncuTwin is the family-facing twin of the incubator, and partners can hold the baby's hand from it ("Coge mi mano" / hold my hand). When someone does, ThingsBoard writes the shared attribute `hand_hold_until` on the IncuNest. The baby's mother is usually sitting next to the incubator doing skin-to-skin, so a small notice on the lock screen ("Someone is holding your baby's hand"), with a QR to see it, reaches her without anybody calling her. The nurse operating the incubator must never be interrupted: the notice is silent, not modal, not clickable and gives way to anything clinical.

The QR comes from a second shared attribute, `family_qr_url`. The board receives the complete URL, not a code, so the server decides the domain without touching firmware.

## What Changes

- **Two shared attributes** read by the motherBoard (WiFi and GPRS tasks, same pattern as `runtime_config_tb.h`): `hand_hold_until` (number, epoch in ms, already written by ThingsBoard) and `family_qr_url` (text; empty or absent = no QR). In a live update a missing key leaves its own half untouched; in the reply to the reconnect request (a full snapshot) a missing `family_qr_url` clears the URL. Server contract: clear the code by writing `family_qr_url` = `""`, never delete the attribute; keep the URL at about 40 characters or less.
- **Notice duration.** With a network clock (`PROTO_TIME_SOURCE_NTP` or `PROTO_TIME_SOURCE_NITZ`): until `hand_hold_until`, capped at 30 min. With any other clock source or no time: a fixed 5 min from the moment the value arrives, and never from the reply to the reconnect attribute request. A `hand_hold_until` equal to or older than the last applied one is always ignored (ThingsBoard re-sends the last value on every connection).
- **URL validation.** Valid = starts with `https://`, 9 to 96 characters, printable ASCII 0x21-0x7E only, **no comma** (it travels in a CSV line). Invalid = same as empty: no QR, and the text-only notice still works.
- **Discharge.** A confirmed `PROFILE_DISCHARGE` on the IncuNest forgets the URL and switches the notice off at once.
- **New protocol line `CTRL,FAMILY,<0|1>,<url>`** (`shared/family_proto.*`, url may be empty: `CTRL,FAMILY,1,`). Sent **only on change** and on `HMI,UI_READY` / `HMI,REQ,STATE`; never periodic (`known_issues.md` #2, UART flooding). A malformed line is discarded whole. **Not breaking in either direction**: an old board never sends the line, an old display discards the unknown line (log only).
- **Lock-screen notice** in `ui_ScreenLock`, bottom strip 792 x 117 (the pulse-oximetry frame's place), background `0x054E92`, QR 105 px black on white. Painted only if there is a notice, no alarm (`UI_IsAnyAlarmActive()`), the board link is alive (`!Display_IsBoardLinkLost()`), the pulse-oximeter panel is hidden and the unlock slider is hidden. Not modal, not clickable, no sound.
- **Help dialog: fourth "Familia" card** with a big QR, whenever there is a valid URL and the link is alive, with or without a running notice (the nurse shows it to the mother at admission); otherwise the three cards stay as today.
- **Board-link loss and recovery on the display.** On link loss the display forgets notice and URL; on recovery it sends `HMI,REQ,STATE` (up to 3 times, 3 s apart, until a `CTRL,FAMILY` arrives) so the board resends its state.
- Strings in `i18n_strings.def` (es/en/fr/pt, ASCII only). Versions: motherBoard `18.68.2` -> `18.69.0`, Display_HMI `4.13.1` -> `4.14.0`.

## Risk

The one risk that matters: the notice hiding or delaying an alarm. Mitigations, each with a test:

- It is never modal, never clickable (so it cannot steal the touch that wakes the unlock slider), and makes no sound.
- It uses no red, yellow, orange or cyan (IEC 60601-1-8 alarm colours); background `0x054E92`.
- It hides at once on any alarm, on board-link loss, when the pulse-oximeter panel is shown and when the unlock slider is shown, and returns only if still valid afterwards.
- It lives only on the lock screen; it does not exist on any other screen.
- It does not touch control, alarms or telemetry. The only new traffic is one short UART line on change, plus one on `UI_READY` / `REQ,STATE`, and from the display at most 3 `HMI,REQ,STATE` after each board-link recovery (no flooding, `known_issues.md` #2).
- A hostile or broken `family_qr_url` (comma, `http://`, whitespace, 200 characters) can neither draw a QR nor break the CSV line.
- A manually set clock (MANUAL or RTC source) is not UTC: the notice lasts a fixed 5 min and does not trust `hand_hold_until`.

## Capabilities

### New Capabilities
- `family-notice`: silent lock-screen notice and Help card for the baby's family, driven by the shared attributes `hand_hold_until` and `family_qr_url` over the `CTRL,FAMILY` line.
