Every phase below is motherBoard-only, and each is a self-contained commit per `.claude/rules/commits.md`. Phases 1 and 2 are TDD (red first) in `[env:native]`; phases 3 to 6 have no test environment and are manual verification, documented as such.

Run `pio` from PowerShell, never from MSYS/Git Bash — a run under MSYS reports a false green. When reading the `pio test -e native` summary, check each suite's own SUMMARY line: `test_sensorboard_frame` ERRORs with `0xC0000139` on this machine and has done so since before this change.

## 1. Dwell state machine (host-tested, red first)

- [ ] 1.1 Write the failing Unity tests for `wifi_dwell_update()` in motherBoard's native test tree: first association, same-day reassociation reporting no change, later-day increment, non-contiguous days, SSID change reset, 32-byte SSID stored whole, epoch 0 counting nothing, clock arriving after the association, SSID change with an unset clock still resetting and still reporting a change, and twenty same-day associations producing at most one change.
- [ ] 1.2 Add `motherBoard/src/modules/util/wifi_dwell.{h,cpp}` with the `WifiDwell` struct and `wifi_dwell_update()`, freestanding (`stdint.h`, `string.h` only — no `Arduino.h`, no `Preferences`).
- [ ] 1.3 Add the SSID sanitizer (bytes outside 0x20–0x7E become `?`) to the same module, with its own failing tests first: control bytes, high bytes, quote and backslash preserved as data, empty SSID, full 32-byte SSID.
- [ ] 1.4 Extend `build_src_filter` in `[env:native]` (`motherBoard/platformio.ini`) to include `modules/util/wifi_dwell.cpp`, and get `pio test -e native` green.

## 2. IP geolocation parser (host-tested, red first)

- [ ] 2.1 Write the failing Unity tests for `ip_geoloc_parse()`: a successful body, `"status":"fail"` with coordinates present, latitude 91.0, longitude -200.0, longitude missing, and a body truncated at every offset of a known-good response.
- [ ] 2.2 Add `motherBoard/src/modules/util/ip_geoloc.{h,cpp}` implementing `ip_geoloc_parse()`, requiring `"status":"success"`, range-checking both values, never reading past the terminating NUL, and leaving the caller's outputs untouched on any rejection.
- [ ] 2.3 Extend `[env:native]`'s `build_src_filter` for `modules/util/ip_geoloc.cpp` and get `pio test -e native` green.
- [ ] 2.4 Add a regression test asserting that a body carrying the offset but no coordinates still parses the timezone through the existing `tz_parse_ipapi_offset()`, so phase 4 cannot silently break the timezone.

## 3. NVS persistence and the association hook

- [ ] 3.1 Add the new `NS_WIFI` keys to `motherBoard/include/config/preferences_keys.h` (tracked SSID, first epoch, last day index, day count).
- [ ] 3.2 Load the dwell struct from NVS at boot alongside the other `NS_WIFI` reads, and hold it in the WiFi task's state.
- [ ] 3.3 Call `wifi_dwell_update()` from the `ARDUINO_EVENT_WIFI_STA_GOT_IP` handler (`motherBoard/src/tasks/Wifi_OTA.cpp:134-137`) with `WiFi.SSID()` and `time(nullptr)`, and write the struct back to NVS **only** when the call reports a change.
- [ ] 3.4 Confirm the handler stays short: no blocking call, no logging inside the event callback beyond what is already there. The NVS write must not run in the event context if that would block it — move it to the WiFi task loop behind a flag if measurement shows it does.
- [ ] 3.5 Manual verification: associate to a provisioned SSID, confirm the count reaches 1, power-cycle, confirm it is still 1 and does not increment on the same day; then change SSID and confirm the reset. Record what was flashed and what was observed.

## 4. Attribute publication

- [ ] 4.1 Add the attribute names to `motherBoard/include/config/telemetry_keys.h`: `wifi_ssid`, `wifi_is_default`, `wifi_dwell_days`, `wifi_dwell_since`, `wifi_dwell_span_d`.
- [ ] 4.2 Add a dirty flag mirroring `babyStore_attributesDirty()`, raised by a state change and by a ThingsBoard reconnection.
- [ ] 4.3 Build and publish the attribute payload through the existing `tb_wifi.sendAttributeJson()` site (`Wifi_OTA.cpp:1127-1131`), clearing the flag only on a successful publish, using the sanitizer from 1.3 for `wifi_ssid` and comparing against `WIFI_SSID` for `wifi_is_default`.
- [ ] 4.4 Confirm no GPRS-side equivalent is added, and that the payload contains no password and no BSSID.
- [ ] 4.5 Manual verification: watch the five attributes arrive in ThingsBoard with the expected values, drop and restore the broker connection and confirm they are republished, and inspect the payload for the absence of the password.

## 5. IP position in the telemetry

- [ ] 5.1 Extend the ip-api request in `ensureWifiTimeZoneSynced()` (`Wifi_OTA.cpp:1284`) to `fields=status,offset,lat,lon`, grow the fixed buffer from 256 to 384 bytes, and keep the existing 5-second deadline and header/body split unchanged.
- [ ] 5.2 Store the parsed position in module state with a validity flag; do not reset it when a later lookup fails, so a transient failure does not blank the map.
- [ ] 5.3 Add `TX_FEATURE_IP_GEOLOC_WIFI 1` to `motherBoard/include/config/transport_policy.h` with the rationale, leaving `TX_FEATURE_TRIANGULATION_WIFI` at 0, and add the `loc_source` key to `telemetry_keys.h`.
- [ ] 5.4 Apply the precedence rule at the WiFi location assembly (`Wifi_OTA.cpp:964-970`): cellular fix wins with `loc_source="gsm"`; otherwise a valid IP fix under the same `tri_*` keys with `tri_accuracy = 25000` and `loc_source="ip"`; otherwise no location keys.
- [ ] 5.5 Run `python tools/check_transport_matrix.py` and confirm it passes.
- [ ] 5.6 Manual verification: with the modem attached confirm `loc_source` is `gsm`; with the SIM out or deactivated confirm the IP position appears under `tri_*` with accuracy 25000 and `loc_source` `ip`, that the existing map widget still plots the unit, and that the timezone still resolves.

## 6. Documentation

- [ ] 6.1 Document the five attributes, the `loc_source` key and the two-source location channel in `Firmware/docs/transport_matrix.md`.
- [ ] 6.2 Add an operational runbook to `Firmware/docs/thingsboard_dashboards.md`: what the attributes mean, the ≥14-day and non-default criteria, the operator-maintained list of the organization's own networks, the mandatory 72-hour re-activation net and why it cannot be dropped, and the Onomondo API details (raw `authorization` header with no `Bearer`, ICCID usable directly as `{id}`).
- [ ] 6.3 State explicitly in that runbook that already-deployed units need 14 more days from the moment they take this firmware, and that the rule chain must not be armed until the attributes are visibly arriving and plausible across the fleet.

## 7. Server side (outside this OpenSpec root — tracked here so it is not forgotten)

- [ ] 7.1 Decide where the policy runs: a ThingsBoard rule chain with a REST API call node, or a scheduled job against the ThingsBoard REST API. This determines who holds the ICCID→device mapping.
- [ ] 7.2 Confirm with Onomondo whether billing is a monthly per-SIM fee or data-only, and whether re-activation is free and immediate. If billing is data-only, do not arm the policy — the firmware half still earns its place as fleet visibility.
- [ ] 7.3 Implement the deactivation rule, the organization's own-networks exclusion list, and the 72-hour re-activation net. Do not arm the deactivation half before the re-activation half works.
- [ ] 7.4 Dry-run the policy in report-only mode over the live fleet for at least one full dwell window, and check every unit it would have taken down against where it actually is.
