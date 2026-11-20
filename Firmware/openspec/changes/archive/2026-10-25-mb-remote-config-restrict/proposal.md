## Why

Board affected: **motherBoard** only. This change corrects the `mb-remote-config` spec to what the firmware does after the security review of 2026-10-25 (commits `18274697`, `fb675112`). It touches the safety limits of `Firmware/docs/known_issues.md` #19.

The MQTT link of the board is unencrypted (port 1883, no TLS), so anyone on the path can publish shared attributes as if it were the server. The original spec let ThingsBoard set nine parameters, including the thermal cut-outs, the heater current limit and the fan. The decision (taken by the coordinator, reversible by the user) is that until TLS with certificate verification exists, ThingsBoard may change only the three GPRS reporting periods.

## What Changes

- **Remote whitelist reduced** to `gprs_act`, `gprs_photo`, `gprs_stby`. The six safety parameters stay local (web, USB) behind `RCFG_REMOTE_SAFETY_PARAMS = 0`; a safety attribute received is ignored, logged and reported as `cfg_rejected`.
- **Remote rules, implemented but inactive** while the flag is 0: factory-value ceiling, cut-outs outside 34-40 C rejected, cut-out above the active setpoint rejected, fan only with tachometer and heater not acting, minimum remote fan PWM, deferral of safety changes until standby.
- **Rate limit** of one remote change per key every 10 s.
- **Booleans only for `fan_pid_en`**; the USB console passes the raw field, rejects cut-outs outside range and is case-sensitive.
- **Local override and reapply**: per-key NVS state; the post-connect request applies only a new remote value and never overrides a later local change.
- **Audit**: `cfg_change` telemetry per change, echo with `cfg_src_<key>` origin and epoch.
- **SDK patches** 5 and 6 in `patch_libdeps.py` (one-byte overflow in `Attributes_Request`, cap on the incoming JSON heap reservation).

## Capabilities

### New Capabilities
(none)

### Modified Capabilities
- `mb-remote-config`: whitelist, strict value interpretation, subscription and echo are modified; remote rules, deferral and rate, local override and reapply, audit, and SDK patches are added.

## Impact

- **Code**: `motherBoard/src/modules/config/` (`runtime_config_policy.h`, `_rules`, `runtime_config`, `_tb.h`), `CommTask.cpp`, `Wifi_OTA.cpp`, `GPRS.cpp`, `EEPROM.cpp`, `Actuators.cpp`, `patch_libdeps.py`. Already implemented; this change only records it.
- **Testing**: rules covered by `[env:native]` (`test_runtime_config`); everything with NVS, radio or ThingsBoard is manual verification and marked so.
- **Out of scope**: TLS, HMI notice of a remote change (technical debt), measuring `RCFG_FAN_PWM_REMOTE_MIN` on a bench.
