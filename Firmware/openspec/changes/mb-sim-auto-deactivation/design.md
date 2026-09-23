## Context

Board affected: **motherBoard** only.

The firmware half of this feature is deliberately small: it observes the WiFi association, keeps a durable count of how long it has lasted, and publishes that as ThingsBoard client attributes. Everything that decides anything — the 14-day threshold, the list of the organization's own networks, the `PATCH /sims/{iccid}` call and the re-activation net — lives on the server. See `proposal.md` for why the split falls there.

Current state that the design has to fit into:

- The SSID is chosen at `motherBoard/src/tasks/Wifi_OTA.cpp:430-447` from three sources in order: `pendingSSID` (set by the `setWifi` RPC), NVS `NS_WIFI`/`KEY_SSID`, and the compiled-in `WIFI_SSID` default. Only the third one means "nobody has provisioned this unit".
- The only trustworthy signal that the link is really up is `s_staHasIp`, maintained by the `ARDUINO_EVENT_WIFI_STA_GOT_IP` / `..._DISCONNECTED` handlers (`Wifi_OTA.cpp:117-140`), because `WiFi.status()` alone lies through the core's `AUTH_EXPIRE` bug. `WIFIIsConnected()` (`:686`) already combines the three criteria.
- Attributes already have a publish path: `tb_wifi.sendAttributeJson(json)` at `:1129`, driven by a dirty flag owned by `baby_profile_store` (`babyStore_attributesDirty()`).
- The timezone lookup at `:1240-1320` already fetches `http://ip-api.com/json/?fields=status,offset` once a day over **plain HTTP** (the free tier has no TLS) into a fixed 256-byte buffer, and hands the body to the pure parser `tz_parse_ipapi_offset()` in `modules/util/tz_source.cpp`.
- `[env:native]` exists and covers `modules/control/` via `build_src_filter` in `motherBoard/platformio.ini`. `modules/util/tz_source.cpp` is the closest precedent for what this change adds: a small module of pure logic with its own state, no Arduino headers, host-testable.

## Goals / Non-Goals

**Goals:**

- Publish enough evidence for a server to decide, on its own, that a unit has settled on a hospital network: which SSID, whether it is the factory default, when it was first seen, and on how many distinct days it has been seen since.
- Survive reboots, power cuts and WiFi flapping without either losing the count or grinding NVS.
- Keep the deactivation criterion (14 days) out of the firmware entirely, so tuning it is a server-side edit and not an OTA.
- Stop the fleet from going position-blind when a SIM is taken down, without adding a network request or a dependency.
- Keep all new arithmetic in pure functions that `pio test -e native` can cover.

**Non-Goals:**

- Evaluating the policy on the device, or any field-side contact with `api.onomondo.com`. `ONOMONDO_API_KEY` stays confined to the factory build.
- Re-activating the SIM from the device — impossible by construction once the modem has no subscription.
- Publishing the WiFi password, the BSSID, or a scan of neighbouring networks.
- Using position, hours of use or link reliability as part of the trigger. Position and hours are published; the server may add them later with no firmware change.
- Any change to `sim_act` or to factory-floor activation.

## Decisions

### 1. A pure dwell state machine plus a thin NVS shim, in `modules/util/`

New `motherBoard/src/modules/util/wifi_dwell.{h,cpp}`, modelled on `tz_source`: no `Arduino.h`, no `Preferences`, so `[env:native]` can compile it. The state is one plain struct and one function that mutates it:

```c
typedef struct {
  char     ssid[33];      // 32 bytes of SSID + NUL (802.11 maximum)
  uint32_t firstEpoch;    // first association with a valid clock; 0 = unknown
  uint32_t lastDayIndex;  // UTC day index of the last counted day
  uint16_t days;          // distinct UTC days seen on this SSID
} WifiDwell;

// Applies an association to `ssid` observed at `nowEpoch` (0 = clock unset).
// Returns true when the state changed and the caller must persist it.
bool wifi_dwell_update(WifiDwell *st, const char *ssid, uint32_t nowEpoch);
```

The caller — the `GOT_IP` handler in `Wifi_OTA.cpp` — loads the struct from NVS at boot, calls this on every association, and writes it back **only when the function returns true**.

*Why a struct the caller persists, rather than a module that owns its own NVS:* it is the only shape that is testable on the host without a Preferences fake, and NVS access stays where every other NVS access on this board already is. `tz_source` sets the precedent of pure-logic-plus-caller-owned-side-effects.

*Alternative rejected:* deriving dwell from `millis()` uptime. It resets on every reboot and cannot distinguish "12 days plugged in" from "12 reboots".

### 2. Distinct UTC days, not elapsed time

`dayIndex = nowEpoch / 86400`. On an association: if the SSID differs from `st->ssid`, reset everything to the new SSID; then, if the clock is valid and `dayIndex != st->lastDayIndex`, increment `days` and store the index.

*Why distinct days and not `now - firstEpoch`:* elapsed time counts a unit that was boxed and unplugged for two weeks after a single association. Distinct days require the unit to actually have been powered and associated on 14 separate days. Both numbers are published anyway (`days` and `firstEpoch`), so the server can require a span *and* a count, and this change does not have to guess which it will want.

*Why UTC and not local:* the day boundary only has to be consistent, and `tz_source` may resolve the offset hours after the first association or never. UTC is the one boundary that never moves under the counter.

### 3. Counting requires a valid clock; the SSID reset does not

A day is counted only when `nowEpoch >= 1609459200` (2021-01-01), the same validity threshold `ftest_sim_activation.cpp` uses for its timestamps. Before the clock is set — SNTP takes seconds after association, and a unit with neither WiFi-internet nor NITZ may never get there — `days` stays put and `firstEpoch` stays 0, to be filled in by the first association that does have a clock.

The SSID-change reset is applied **unconditionally**, clock or no clock. Otherwise a unit moved to a new network before its clock came up would keep accumulating days credited to the old SSID, which is precisely the failure this feature exists to avoid.

### 4. Publication: latest-value attributes, on the same dirty-flag pattern already in use

Five client attributes, published with `tb_wifi.sendAttributeJson()`:

| Attribute | Type | Meaning |
|---|---|---|
| `wifi_ssid` | string | The associated SSID, sanitized (see below) |
| `wifi_is_default` | bool | True when it is the compiled-in `WIFI_SSID` |
| `wifi_dwell_days` | int | Distinct UTC days on this SSID |
| `wifi_dwell_since` | int | Epoch of first association with a valid clock; 0 if unknown |
| `wifi_dwell_span_d` | int | `(now - wifi_dwell_since) / 86400`, or 0 when `since` is 0 |

Attributes, not telemetry: the server wants the current value, not a time series, and attributes do not consume the `THINGSBOARD_FIELDS_AMOUNT` budget the telemetry documents are boxed into. A module-level dirty flag, mirroring `babyStore_attributesDirty()`, is raised whenever `wifi_dwell_update()` reports a change and on every ThingsBoard (re)connection, and the existing publish site clears it on success.

WiFi transport only. There is no GPRS path: by definition the interesting case is a unit sitting on WiFi, and a unit on cellular has nothing to report.

### 5. The SSID is published in clear, sanitized to printable ASCII

*Why in clear:* the operator's list of the organization's own networks — the workshop, the training room — is a list of names. A hash would make that list unreadable and unmaintainable, and an SSID is broadcast in the open by the AP anyway. The password is never published, and is not in the struct.

*Sanitizing:* an SSID is 32 arbitrary **bytes**, not a string. Anything outside printable ASCII (0x20–0x7E) is replaced with `?` before it goes into the JSON. ArduinoJson escapes quotes and backslashes correctly on its own, but it does not fix invalid UTF-8, and an invalid sequence in an MQTT payload is a broker-side problem rather than a firmware-side one. Sanitizing at the source is cheaper than diagnosing that later.

### 6. IP geolocation rides the existing daily lookup

`ensureWifiTimeZoneSynced()`'s request becomes `fields=status,offset,lat,lon`, and the fixed buffer grows from 256 to 384 bytes to keep the "response is tiny, the buffer is fixed" property that its comment relies on. No new request, no new host, no change of cadence — the once-a-day refresh already there is far more often than a hospital incubator moves.

A new pure parser, `motherBoard/src/modules/util/ip_geoloc.{h,cpp}`:

```c
// Parses lat/lon out of an ip-api.com body. Requires "status":"success" and
// both values in range. Returns false on anything else, leaving *lat/*lon
// untouched.
bool ip_geoloc_parse(const char *json, float *lat, float *lon);
```

It must require `"status":"success"`, must reject a latitude outside [-90, 90] or a longitude outside [-180, 180], and must not read past the NUL of a truncated body — the same discipline `.claude/rules/security.md` imposes on the HMI line parser, for the same reason: the input is attacker-shaped. This one arrives over cleartext HTTP, so it is *advisory only*. It reaches two telemetry keys and nothing else; it never touches the PID, the alarms, the clock or any actuator. The worst a manipulated response achieves is a wrong dot on a dashboard.

### 7. One location channel, two sources, with the source named

The GSM fix stays authoritative. In the WiFi telemetry assembly (`Wifi_OTA.cpp:964-970`):

- `GPRS.latitud || GPRS.longitud` non-zero → publish as today, plus `loc_source = "gsm"`.
- otherwise, a valid IP fix → publish it under the same `tri_latitud`/`tri_longitud` keys with `tri_accuracy = 25000` (metres, i.e. "somewhere in this city") and `loc_source = "ip"`.
- otherwise → publish no location keys at all, as today.

*Why reuse the `tri_*` keys instead of adding `ip_*` ones:* the map widgets in `Firmware/docs/thingsboard_dashboards.md` are bound to `tri_latitud`/`tri_longitud`. Reusing them means a unit whose SIM has just been deactivated stays on the map with no dashboard edit, which is the whole point of adding this. `tri_accuracy` already exists to say how good a fix is, and `loc_source` removes any ambiguity for a query that cares.

*Alternative rejected:* raising `TX_FEATURE_TRIANGULATION_WIFI` from 0 to 1. That flag means "this transport can produce a cell-tower fix", which stays false. A new `TX_FEATURE_IP_GEOLOC_WIFI` flag documents the real capability, and `python tools/check_transport_matrix.py` keeps working.

### 8. No threshold, no tunable, no new RPC in the firmware

The firmware carries no `14`, no `72`, and no denylist. It publishes facts. This is what makes the policy a server-side edit rather than a fleet-wide OTA, and it is also what keeps the device incapable of taking its own SIM down by mistake.

## Risks / Trade-offs

- **A commissioning or training stay longer than 14 days on one network takes the SIM down early** → The server-side 72-hour re-activation net bounds the damage to at most three days of silence, and the operator-maintained list of the organization's own SSIDs removes the common case outright. Both are specified as requirements of the attribute contract in `specs/wifi-dwell-reporting/`, not left to whoever wires the rule chain.
- **Two different networks can share an SSID** ("Guest", a hospital chain's standard name), so a unit moved between them looks settled → Accepted. The BSSID would disambiguate but changes whenever an AP is swapped or the unit roams between APs of the same network, which would reset the counter constantly and is the worse failure. The server can cross-check the published position, which is exactly why §7 exists.
- **Re-provisioning a unit to a different SSID at the hospital restarts the 14 days** → Accepted, and it errs in the safe direction.
- **The IP position is spoofable over cleartext HTTP** → Confined to two telemetry keys, never used for anything, and its quality is declared in `tri_accuracy`. Same risk already accepted for the timezone at `Wifi_OTA.cpp:1254-1260`, for the same reasons.
- **The IP position is only city-accurate, and a hospital behind a national carrier's NAT can geolocate to the wrong city entirely** → It is a replacement for *nothing*, not for the GSM fix: the alternative after deactivation is no position at all. `tri_accuracy = 25000` stops anyone reading it as a GPS fix.
- **After deactivation, a unit whose WiFi then dies has no telemetry, no GSM position and no clock source on its next boot** (NITZ and `AT+CNTP` both need the SIM) → This is the irreducible cost of the feature and the reason the re-activation net is mandatory rather than optional.
- **NVS wear** → A write happens only when the state changes: at most once per UTC day per SSID, plus one per network change. Roughly 400 writes a year against a wear-levelled partition. WiFi flapping produces associations, not state changes, so it produces no writes.
- **`build_src_filter` drift** → Adding two `.cpp` files to `[env:native]` widens what the host build links. Both are freestanding (`stdint.h`, `string.h`, `stdlib.h`) and neither pulls in Arduino, so the risk is a link error at test time, not a silent behaviour change.

## Migration Plan

No NVS migration and no protocol version bump. The new `NS_WIFI` keys are absent on every existing unit and default to an empty SSID with `days = 0`, so the first association after the update starts the count. **Every already-deployed unit therefore needs 14 more days from the moment it takes this firmware** — which is the correct behaviour, since the firmware has no way to know how long it was on that network before.

Rollback is a plain firmware downgrade: the extra `NS_WIFI` keys are simply ignored, and the stale attributes in ThingsBoard stop being refreshed. The server-side policy must therefore treat a stale `wifi_dwell_days` as insufficient evidence, which the 72-hour silence net already forces it to do.

Order of deployment matters: the firmware must be in the field, and the attributes visibly arriving and plausible, **before** anyone arms the rule chain that actually calls Onomondo. Arming the policy against a fleet that is not yet reporting is the one way to get a wrong deactivation out of a correct implementation.

## Open Questions

- Does Onomondo bill a monthly fee per active SIM, or only for data? If it is data-only, the fleet is already near zero on WiFi (`GPRS.cpp:1314`) and the whole feature saves nothing. This does not block the firmware half, which is useful on its own for fleet visibility, but it decides whether the server-side policy is ever worth arming.
- Is re-activation after deactivation free and immediate on the Onomondo side? The 72-hour safety net assumes it is. If re-activation carries a fee or a delay, the threshold should move up rather than the net being dropped.
- Where does the server-side policy run — a ThingsBoard rule chain with a REST API call node, or a scheduled job against the ThingsBoard REST API? Out of this OpenSpec root either way, but it determines who holds the ICCID→device mapping.
