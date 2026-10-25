## Why

Board affected: **motherBoard** only. `Display_HMI` and `shared/` are untouched (`alarm_clamp_*_cutout()` in `shared/` is reused, not modified). The change touches the alarm-adjacent safety limits (`air_tmax`, `skin_tmax`) that `Firmware/docs/known_issues.md` #19 documents, so it keeps the existing clamps (`alarm_clamp_air_cutout()` / `alarm_clamp_skin_cutout()`, 34-40 C) as the only authority on those two limits; it neither raises nor bypasses them.

Today the safety and cadence parameters of a unit can only be changed from two places, both requiring someone physically at the unit:

- `POST /config` on the local WiFi web server (`motherBoard/src/tasks/Wifi_OTA.cpp`, handler `wifiServer.on("/config", HTTP_POST, ...)`).
- The service console on the USB / inter-board cable, `/config,<param>,<value>` (`motherBoard/src/tasks/CommTask.cpp`).

`known_issues.md` #19 records the consequence: units already in the field carry the old 38 C air cut-out in NVS (`recapVariables()` deliberately does not migrate a safety threshold on its own) and nobody can correct it without a site visit. The same goes for the GPRS publication cadence (`gprs_act`, `gprs_photo`, `gprs_stby`), which is the SIM-cost lever of the fleet. ThingsBoard already reaches every unit with RPC (`restart`, `setWifi`, `checkOta`, ...) over both transports, so the authority exists; what is missing is a way to carry *configuration* over it.

## What Changes

- **New module `runtime_config`** (`motherBoard/src/modules/config/`): the single point where a configuration parameter is validated, applied to `in3`, persisted in NVS under the existing keys and propagated to its side effect. It is split in a pure part (`runtime_config_rules`: whitelist, per-parameter rule, number parsing, JSON value extraction; host-testable) and a thin Arduino part (`runtime_config`: `in3`, NVS, `ledcWrite`, `setFanPidEnabled`, `maxDesiredTemp[]`, logging, echo bookkeeping).
- **ThingsBoard shared attributes carry the configuration.** Both ThingsBoard clients (GPRS `tb`, WiFi `tb_wifi`) subscribe once to shared-attribute updates for exactly nine keys (`fan_supply_pwm, fan_ctl_pwm, fan_pid_en, heater_amps, air_tmax, skin_tmax, gprs_act, gprs_photo, gprs_stby`) for hot changes, and issue one shared-attribute request after each (re)connection to pick up values changed while the unit was off. Both use the same names as the arguments of `POST /config`.
- **Effective-value echo.** After applying, the value actually in force (after clamping) is published as a *client* attribute with the same name, so ThingsBoard shows what the board has and not what somebody typed. The echo is also sent after a local change (web, USB) and once per connection for all nine keys.
- **`POST /config` and `/config,<param>,<value>` are refactored** to call the module. Their parameter names, success message and the `serial`, `reference_temp`, `set_time` and `BABY_WIPE` handling are unchanged; they gain the shared validation (see Impact for the three deliberate differences).
- **Logging.** Every application logs origin (`tb-gprs`, `tb-wifi`, `web`, `usb`), name, requested value and applied value.
- **Explicitly NOT exposed remotely:** `serial` (identity), `reference_temp` (calibration that depends on a local measurement), `set_time` (clock), `BABY_WIPE` (clinical data deletion; see the comment above `rpc_callbacks[]` in `GPRS.cpp`). Any attribute key outside the nine is ignored.
- Not breaking for ThingsBoard or the HMI protocol: no UART line, no telemetry key and no NVS key changes.

## Capabilities

### New Capabilities
- `mb-remote-config`: the whitelist of remotely settable parameters, the per-parameter validation rule (clamp versus reject), the single application point and its persistence/side-effect contract, the shared-attribute subscription and post-connect request on both transports, the effective-value echo and the logging contract.

### Modified Capabilities
(none: the existing specs in `openspec/specs/` do not describe `/config` or ThingsBoard attributes.)

## Impact

- **Affected code - motherBoard**: new `src/modules/config/runtime_config_rules.{h,cpp}` and `runtime_config.{h,cpp}`; `src/tasks/Wifi_OTA.cpp` (`POST /config` body, shared-attribute subscription and request in `WIFI_TB_OTA()`, echo publish); `src/tasks/CommTask.cpp` (the `/config,<param>,<value>` branch); `src/tasks/GPRS.cpp` (subscription, request and echo in `GPRSPost()`); `platformio.ini` (`[env:native]` `build_src_filter`); new `test/test_runtime_config/`.
- **Deliberate behaviour differences on the local paths** (the validation is now shared, so these are the price of one rule set):
  1. PWM and period values are *rejected* when out of range (PWM 1..255, periods 10..86400 s) or not integers; before, `POST /config` stored whatever `toInt()` returned (`abc` became 0) and the boot code silently reverted it. `POST /config` answers `400` with the reason for a rejected value; the console logs it.
  2. `heater_amps` is rejected outside 1.0..12.0 A (before: any value, with only `<= 0` repaired at the next boot).
  3. `fan_ctl_pwm` no longer calls `ledcWrite` unconditionally; it only writes the channel while the fan is commanded on, the fan PID is not driving it and the factory test is not running (before, a `POST` could spin the control channel of a stopped unit and, with the PID closed, stomp its output until the next `turnFans()`).
- **Authority and security**: shared attributes are written by the ThingsBoard tenant, the same authority that already sends `restart` and `setWifi` RPC. No new authentication is added; the protection is the strict whitelist, type check (numbers and booleans only, strings are refused) and range rule, applied identically to every origin. A rejected value changes nothing and is logged.
- **Memory**: the board runs at ~6 KB of internal heap with WiFi+PPP. The module uses only static storage (a 9-entry table, a 16-bit pending mask, `StaticJsonDocument<192>` for the echo, small stack buffers); the SDK, built with `THINGSBOARD_ENABLE_DYNAMIC=1`, keeps its own small vectors (one `Shared_Attribute_Callback` with 9 key pointers per client). Subscription happens **once per client** (the SDK re-subscribes the MQTT topics on reconnect by itself). The post-connect request is one ~120 B publish and one short response per connection. Flash/RAM deltas against `dev` are recorded in `tasks.md`.
- **Data cost over GPRS**: one request (~120 B) and its response plus one ~150 B echo per ThingsBoard (re)connection, and one echo per change. Reconnections already cost an RPC subscribe and a telemetry publish; this adds a few hundred bytes per connection.
- **Affected docs**: `Firmware/docs/known_issues.md` #19, `Firmware/docs/thingsboard_dashboards.md`, `Firmware/docs/transport_matrix.md`.
- **Testing**: the rules, the number parser and the JSON value extraction are pure and run in `[env:native]` with Unity (TDD, red first). Everything involving NVS, the radio and a real ThingsBoard is **manual verification** and is marked as such in the spec; there is no test environment for it.
- **Out of scope (Non-goals)**: remote `serial`, `reference_temp`, `set_time`, `BABY_WIPE`; a server-side ThingsBoard rule chain or dashboard widget for the attributes; fixing the pre-existing duplicate `RPC_Subscribe` on every reconnect in both clients (noted as technical debt); raising the 40 C hard ceiling of `alarm_policy.h`; the Display_HMI.
