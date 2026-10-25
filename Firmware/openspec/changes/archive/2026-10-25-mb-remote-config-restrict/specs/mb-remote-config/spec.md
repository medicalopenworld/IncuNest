## MODIFIED Requirements

### Requirement: Lista blanca de parametros configurables

The motherBoard SHALL know nine configurable parameters by these exact, case-sensitive names: `fan_supply_pwm`, `fan_ctl_pwm`, `fan_pid_en`, `heater_amps`, `air_tmax`, `skin_tmax`, `gprs_act`, `gprs_photo`, `gprs_stby`. A ThingsBoard origin (`tb-gprs`, `tb-wifi`) SHALL be allowed to change only `gprs_act`, `gprs_photo` and `gprs_stby` while `RCFG_REMOTE_SAFETY_PARAMS` is 0; the other six SHALL be local-only (web, USB). Any other name, including `serial`, `reference_temp` and `set_time`, SHALL NOT be applied through ThingsBoard. A shared attribute for a safety parameter received while the flag is 0 SHALL be ignored, logged as a warning and reported through the client attribute `cfg_rejected`. The USB console SHALL accept only the exact upper-case names (`AIR_TMAX`), and the web and ThingsBoard only the lower-case ones.

#### Scenario: A name outside the whitelist is not applied

- **WHEN** a shared-attribute update contains `serial`, `reference_temp`, `set_time` or an unknown key such as `foo`
- **THEN** none of `in3`, NVS or any actuator changes
- **AND** the update does not produce an error toward ThingsBoard

Lookup by name covered by motherBoard `[env:native]`; the absence of effect on a live unit is manual verification.

#### Scenario: Remote list with the safety flag at 0

- **GIVEN** `RCFG_REMOTE_SAFETY_PARAMS` is 0
- **WHEN** the remote key list is requested
- **THEN** it contains exactly `gprs_act`, `gprs_photo` and `gprs_stby`
- **AND** with the flag at 1 it contains all nine

Covered by motherBoard `[env:native]`.

#### Scenario: A safety attribute is ignored remotely

- **GIVEN** the flag is 0
- **WHEN** a shared-attribute update carries `air_tmax = 38` together with a period
- **THEN** `air_tmax` is unchanged, a warning is logged and `cfg_rejected` is published

Manual verification against a real ThingsBoard server.

#### Scenario: Case sensitivity per origin

- **WHEN** `AIR_TMAX` is looked up as a web or ThingsBoard name, or `air_tmax` as a console name
- **THEN** neither resolves, while `AIR_TMAX` resolves for the console and `air_tmax` for web and ThingsBoard

Covered by motherBoard `[env:native]`.

### Requirement: Interpretacion estricta de valores de entrada

A textual value (web argument, USB console field) SHALL be parsed strictly: the whole string must be one finite decimal number with optional surrounding whitespace; empty strings, trailing garbage, hexadecimal, `nan` and `inf` SHALL fail. The USB console SHALL pass the raw field to this parser. A JSON value SHALL be accepted only if it is a number, and a boolean (`true` = 1, `false` = 0) only for `fan_pid_en`; strings, null, arrays, objects and booleans for any other parameter SHALL be refused.

#### Scenario: Text parsing

- **WHEN** the texts `"130"`, `" 38.5 "`, `"abc"`, `""`, `"12x"`, `"0x10"` and `"1e999"` are parsed
- **THEN** the first two give 130 and 38.5 and the rest fail

Covered by motherBoard `[env:native]`.

#### Scenario: Booleans only for fan_pid_en

- **WHEN** the JSON value `true` is extracted for a parameter other than `fan_pid_en`
- **THEN** it is refused, while `1` and `38.5` are accepted as numbers

Covered by motherBoard `[env:native]`.

### Requirement: Suscripcion a atributos compartidos en ambos transportes

Each ThingsBoard client (GPRS and WiFi) SHALL register, once per process, a shared-attribute update subscription for the effective remote keys using `Shared_Attribute_Callback` and `Shared_Attributes_Subscribe`, and SHALL, after each successful connection, issue one `Shared_Attributes_Request` for the same keys with a distinct `Attribute_Request_Callback`. The update callback SHALL treat a delivered value as a new remote write; the request callback SHALL apply a value only under the reapply rule. The callbacks SHALL NOT publish; publication happens in the owning task. The SDK SHALL remain at v0.13.0.

#### Scenario: Hot change while connected

- **GIVEN** a unit connected to ThingsBoard over WiFi or GPRS
- **WHEN** the tenant writes the shared attribute `gprs_stby = 1800`
- **THEN** the unit applies 1800 within one loop pass and logs origin, name, requested and applied value

Manual verification against a real ThingsBoard server.

#### Scenario: Change made while the unit was off

- **GIVEN** `gprs_stby = 1800` was written while the unit was powered off
- **WHEN** the unit connects
- **THEN** after the request it holds and persists 1800

Manual verification against a real ThingsBoard server.

#### Scenario: Reconnection does not duplicate the subscription

- **WHEN** the connection drops and is re-established several times
- **THEN** one attribute update causes exactly one application

Manual verification (observed in the log across a forced reconnection).

### Requirement: Eco del valor efectivo

After applying a parameter from any origin, and once after each connection for all nine, the unit SHALL publish the effective value as a **client** attribute with the same name, `fan_pid_en` as a JSON boolean, together with `cfg_src_<name>` holding the origin and, when the clock is synchronised, the epoch of the last change (`tb-gprs@1730000000`). A rejected remote value SHALL leave the effective value echoed. A failed publish SHALL be retried no sooner than 5 s later; a successful one SHALL clear only the entries it carried, and an entry changed while the message was in flight SHALL be echoed again. The echo SHALL fit a 704 B static buffer and SHALL NOT be sent from inside an SDK callback.

#### Scenario: Building the echo with origin and epoch

- **WHEN** the echo is built for a subset of parameters with origin and epoch for some of them
- **THEN** the JSON contains exactly those keys, `fan_pid_en` as `true`/`false`, `cfg_src_<name>` only where an origin is known, and it fits the buffer with every key at its maximum

Covered by motherBoard `[env:native]`.

#### Scenario: Echo after a local change

- **WHEN** `POST /config` changes `gprs_act` while connected
- **THEN** the client attribute `gprs_act` shows the new value on the next pass

Manual verification against a real ThingsBoard server.

## ADDED Requirements

### Requirement: Reglas del origen remoto con los parametros de seguridad habilitados

With `RCFG_REMOTE_SAFETY_PARAMS` at 1, a ThingsBoard origin SHALL be subject to `runtime_config_validate_remote()`: the strict base rule; a cut-out or `heater_amps` above the compiled factory value rejected; `air_tmax` and `skin_tmax` outside 34-40 C rejected instead of clamped; a cut-out at or below the active setpoint with control running rejected; `fan_supply_pwm`, `fan_ctl_pwm` and `fan_pid_en` rejected when the unit has no tachometer or the heater is acting; and a fan PWM below `RCFG_FAN_PWM_REMOTE_MIN` rejected. The USB console SHALL also reject cut-outs outside 34-40 C; the web SHALL keep clamping them. These rules SHALL be implemented and tested even while the flag is 0.

#### Scenario: A cut-out can only go down

- **GIVEN** a factory air cut-out of 38 C
- **WHEN** the remote value is 36, 38, 38.5 and 40
- **THEN** 36 and 38 are accepted and 38.5 and 40 are rejected as above the factory value

Covered by motherBoard `[env:native]`.

#### Scenario: Cut-outs outside 34-40 are rejected

- **WHEN** a remote or USB `air_tmax` is 45 or 30, or `skin_tmax` 41
- **THEN** the value is rejected and not clamped
- **AND** the same 45 from `POST /config` is clamped to 40

Covered by motherBoard `[env:native]`.

#### Scenario: A cut-out below the active setpoint

- **GIVEN** air control running at 37 C
- **WHEN** a remote `air_tmax` of 36 or 37 is validated
- **THEN** it is rejected, and 37.5 is accepted

Covered by motherBoard `[env:native]`.

#### Scenario: Fan rules

- **WHEN** a remote fan parameter is validated on a unit without tachometer, or with the heater acting, or a PWM below the remote minimum
- **THEN** it is rejected with the matching reason

Covered by motherBoard `[env:native]`. The minimum of 80 has not been measured on a bench.

### Requirement: Aplazamiento y ritmo de los cambios remotos

A remote change SHALL be applied at most once per key every 10 s; one arriving earlier SHALL be kept pending and applied when the interval has elapsed. A remote change to a safety parameter SHALL stay pending while therapy (control or phototherapy) is active and be applied at standby. The rate check SHALL tolerate the wrap of the 32-bit millisecond counter.

#### Scenario: One change per key every 10 seconds

- **WHEN** the interval since the last change of a key is 9.999 s, 10 s and 50 s
- **THEN** only the first is refused, and a key never changed is always allowed, also across a counter wrap

Covered by motherBoard `[env:native]`.

#### Scenario: Deferral with therapy active

- **WHEN** therapy is active
- **THEN** safety parameters are deferred and the GPRS periods are not

Covered by motherBoard `[env:native]`.

### Requirement: Cambio local y reaplicado al reconectar

For each key the unit SHALL persist the last remote value seen, a local-override flag and the origin and epoch of the last effective change. The response to the post-connect request SHALL apply a value only if it differs from the last remote value seen and no local override is set; an unchanged resend SHALL do nothing. A local change that alters the value SHALL set the override; a local write of the same value SHALL NOT. Only a new remote write delivered through the subscription SHALL clear the override. A deferred remote value SHALL NOT be recorded as seen. Deleting a shared attribute on the server SHALL NOT revert any value.

#### Scenario: A resend after reconnection does nothing

- **GIVEN** the last remote value seen for `gprs_act` is 120
- **WHEN** the request response again carries 120
- **THEN** nothing is applied or written

Manual verification on hardware.

#### Scenario: A local change survives reconnection

- **GIVEN** `gprs_act` was set remotely to 120 and then locally to 90
- **WHEN** the unit reconnects and the response carries 120
- **THEN** the unit keeps 90 and logs that the local change wins

Manual verification on hardware.

#### Scenario: A new remote write lifts the override

- **GIVEN** a local override on `gprs_act`
- **WHEN** the tenant writes the attribute while the unit is connected
- **THEN** the value is applied and the override cleared

Manual verification against a real ThingsBoard server.

### Requirement: Auditoria de cambios de configuracion

Every effective change of a parameter, from any origin, SHALL produce one telemetry event `cfg_change` with `key`, `old`, `new`, `origin` and `epoch`, formatted without dynamic JSON, queued (four events, oldest dropped) and published by the owning task on both transports, and SHALL produce a warning in the `CFG` log. A re-application of the same value SHALL NOT produce an event.

#### Scenario: Audit event content

- **WHEN** `air_tmax` goes from 38 to 40 from `tb-gprs` at epoch 1730000000
- **THEN** the event is `{"cfg_change":{"key":"air_tmax","old":38.00,"new":40.00,"origin":"tb-gprs","epoch":1730000000}}`

Covered by motherBoard `[env:native]`; the publication is manual verification.

#### Scenario: A rejected remote value

- **WHEN** a rejected value is reported
- **THEN** `cfg_rejected` carries `name: reason` with no character that could break the JSON

Covered by motherBoard `[env:native]`.

### Requirement: Parches al SDK de ThingsBoard

`patch_libdeps.py` SHALL apply, idempotently at build time, a fix reserving `size + 1` bytes in `Attributes_Request()` (the final NUL of `strncat` overflowed by one byte) and a cap of 64 key/value pairs on the heap reservation of every incoming JSON document. The SDK version SHALL NOT change.

#### Scenario: Patches apply on a clean dependency tree

- **WHEN** the ThingsBoard library is reinstalled and the firmware is built
- **THEN** both patches are reported as applied and a second build reports nothing to apply

Manual verification (done on 2026-10-25 with a clean libdeps build).
