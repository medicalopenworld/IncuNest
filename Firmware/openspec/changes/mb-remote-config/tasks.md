Every phase is motherBoard-only and lands as its own commit per `.claude/rules/commits.md`. Phases 1 and 2 are TDD (red first) in `[env:native]`; phases 3 to 5 touch code with no test environment and are manual verification, documented as such.

Run `pio` from PowerShell, never from MSYS/Git Bash, when reading `pio test -e native` results: a run under MSYS reports a false green.

## 0. Baseline

- [ ] 0.1 Before touching code, build `dev` (`pio run -e IncuNest_V18` in `Firmware/motherBoard`) and record RAM/Flash: this is the reference for the size delta in 5.2.

## 1. Pure rules (host-tested, red first)

- [ ] 1.1 Write the failing Unity tests in `motherBoard/test/test_runtime_config/` for `runtime_config_lookup()`/`runtime_config_name()`, `runtime_config_validate()` (every parameter: accept, boundary, reject, clamp, NaN/Inf, non-integer), `runtime_config_parse_number()`, `runtime_config_json_number()` and `runtime_config_format_echo()`.
- [ ] 1.2 Add `motherBoard/src/modules/config/runtime_config_rules.{h,cpp}` (no `Arduino.h`; `ArduinoJson.h`, `math.h`, `stdio.h` only, plus `alarm_policy.h` from `shared/`), until the tests are green.
- [ ] 1.3 Extend `[env:native]`'s `build_src_filter` with `modules/config/runtime_config_rules.cpp` in the same commit as the tests (`.claude/rules/testing.md`).

## 2. Single application point (manual verification)

- [ ] 2.1 Add `motherBoard/src/modules/config/runtime_config.{h,cpp}`: `runtime_config_apply()`, `_apply_text()`, `_apply_json()`, the echo mask and builder, the log line. Reuses `KEY_*`/`NS_*` from `config/preferences_keys.h`.
- [ ] 2.2 MANUAL: compile `pio run -e IncuNest_V18`; no test environment covers NVS, `in3` or LEDC.

## 3. ThingsBoard wiring (manual verification)

- [ ] 3.1 GPRS client (`GPRS.cpp`): one-time `Shared_Attributes_Subscribe`, per-connection `Shared_Attributes_Request`, `runtime_config_echo_all()`, echo publish each pass while connected.
- [ ] 3.2 WiFi client (`Wifi_OTA.cpp`, `WIFI_TB_OTA()`): the same.
- [ ] 3.3 MANUAL on hardware and a real ThingsBoard server: hot change, change while off, rejected value, echo of a clamped value, reconnect without duplication, over WiFi and over GPRS.

## 4. Local paths (manual verification)

- [ ] 4.1 `POST /config` in `Wifi_OTA.cpp`: the nine parameters go through `runtime_config_apply_text(RCFG_ORIGIN_WEB, ...)`, `400` on rejection; `serial`, `reference_temp`, `set_time` untouched.
- [ ] 4.2 Console `/config,<param>,<value>` in `CommTask.cpp`: the eight console parameters go through `runtime_config_apply(RCFG_ORIGIN_USB, ...)`; `BABY_WIPE` untouched.
- [ ] 4.3 MANUAL: `curl -u ... -d air_tmax=45 /config` then `GET /get_config`; USB `/config,AIR_TMAX,45`.

## 5. Verify

- [ ] 5.1 `pio test -e native` in `Firmware/motherBoard`, run from PowerShell; check each suite's own SUMMARY line.
- [ ] 5.2 `pio run -e IncuNest_V18` green; record RAM/Flash and compare with 0.1.
- [ ] 5.3 `openspec validate --all` green.

## 6. Docs

- [ ] 6.1 `Firmware/docs/known_issues.md` #19: the cut-out is now correctable remotely.
- [ ] 6.2 `Firmware/docs/thingsboard_dashboards.md`: the nine shared attributes, the client-attribute echo, how to write them.
- [ ] 6.3 `Firmware/docs/transport_matrix.md`: attributes row next to the RPC table (both transports).

## 7. Archive

- [ ] 7.1 `openspec archive mb-remote-config`.
