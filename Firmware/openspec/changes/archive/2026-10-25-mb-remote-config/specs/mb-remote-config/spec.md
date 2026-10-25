## ADDED Requirements

### Requirement: Lista blanca de parametros configurables

The motherBoard SHALL accept configuration changes only for these nine parameters, by these exact names: `fan_supply_pwm`, `fan_ctl_pwm`, `fan_pid_en`, `heater_amps`, `air_tmax`, `skin_tmax`, `gprs_act`, `gprs_photo`, `gprs_stby`. Any other name, including `serial`, `reference_temp` and `set_time`, SHALL NOT be applied through the ThingsBoard path. A shared attribute outside the list SHALL be ignored without effect and without error.

#### Scenario: A name outside the whitelist is not applied

- **WHEN** a shared-attribute update contains `serial`, `reference_temp`, `set_time` or an unknown key such as `foo`
- **THEN** none of `in3`, NVS or any actuator changes
- **AND** the update does not produce an error toward ThingsBoard

Lookup by name covered by motherBoard `[env:native]`; the absence of effect on a live unit is manual verification.

#### Scenario: Every whitelisted name resolves

- **WHEN** each of the nine names is looked up
- **THEN** it resolves to its own distinct identifier and `runtime_config_name()` returns the same name

Covered by motherBoard `[env:native]`.

### Requirement: Regla de validacion por parametro

Every origin SHALL validate a value with the same rule before anything is changed. The rule per parameter is: `fan_supply_pwm` and `fan_ctl_pwm` integers in 1..255; `fan_pid_en` exactly 0 or 1; `heater_amps` a finite number in 1.0..12.0; `air_tmax` and `skin_tmax` finite numbers, clamped by `alarm_clamp_air_cutout()` and `alarm_clamp_skin_cutout()` respectively; `gprs_act`, `gprs_photo` and `gprs_stby` integers in 10..86400. A value that is NaN or infinite, outside a reject range, or non-integral for an integer parameter SHALL be rejected, and a rejection SHALL leave `in3`, NVS and the hardware untouched. Only `air_tmax` and `skin_tmax` SHALL be clamped rather than rejected.

#### Scenario: In-range integer is accepted unchanged

- **WHEN** `fan_ctl_pwm` is validated with 130
- **THEN** the status is OK and the output value is 130

Covered by motherBoard `[env:native]`.

#### Scenario: PWM out of range is rejected

- **WHEN** `fan_supply_pwm` or `fan_ctl_pwm` is validated with 0, -1, 256 or 300
- **THEN** the status is a range rejection and the output value is not written

Covered by motherBoard `[env:native]`.

#### Scenario: Non-integer for an integer parameter is rejected

- **WHEN** `fan_ctl_pwm` is validated with 130.5, or `gprs_act` with 60.25
- **THEN** the status is a not-integer rejection

Covered by motherBoard `[env:native]`.

#### Scenario: Period boundaries

- **WHEN** `gprs_act` is validated with 9, 10, 86400 and 86401
- **THEN** 10 and 86400 are accepted and 9 and 86401 are rejected as out of range
- **AND** the same holds for `gprs_photo` and `gprs_stby`

Covered by motherBoard `[env:native]`.

#### Scenario: Non-finite values are rejected for every parameter

- **WHEN** any of the nine parameters is validated with NaN, +Inf or -Inf
- **THEN** the status is a not-finite rejection and the output value is not written

Covered by motherBoard `[env:native]`.

#### Scenario: Temperature cut-outs are clamped, not rejected

- **WHEN** `air_tmax` is validated with 45.0 and with 30.0, and `skin_tmax` with 45.0
- **THEN** the outputs are 40.0, 34.0 and 40.0 and the status is OK-clamped
- **AND** `air_tmax` with 38.0 returns 38.0 with status OK (not clamped)

Covered by motherBoard `[env:native]`.

#### Scenario: The fan PID flag accepts only 0 or 1

- **WHEN** `fan_pid_en` is validated with 0, 1, 2 and 0.5
- **THEN** 0 and 1 are accepted and 2 and 0.5 are rejected as not boolean

Covered by motherBoard `[env:native]`.

#### Scenario: heater_amps limits

- **WHEN** `heater_amps` is validated with 0.0, 0.99, 1.0, 9.5, 12.0 and 12.01
- **THEN** 1.0, 9.5 and 12.0 are accepted and the others are rejected as out of range

Covered by motherBoard `[env:native]`.

### Requirement: Interpretacion estricta de valores de entrada

A textual value (web argument, console field) SHALL be parsed strictly: the whole string must be one finite decimal number, with optional surrounding whitespace; an empty string or trailing garbage SHALL fail. A JSON value (shared attribute) SHALL be accepted only if it is a number or a boolean (`true` = 1, `false` = 0); strings, null, arrays and objects SHALL be refused.

#### Scenario: Text parsing

- **WHEN** the texts `"130"`, `" 38.5 "`, `"abc"`, `""`, `"12x"` and `"1e999"` are parsed
- **THEN** the first two give 130 and 38.5 and the rest fail

Covered by motherBoard `[env:native]`.

#### Scenario: JSON value types

- **WHEN** the JSON values `38`, `38.5`, `true`, `false`, `"38"`, `null`, `[1]` and `{}` are extracted
- **THEN** the first four give 38, 38.5, 1 and 0 and the rest are refused

Covered by motherBoard `[env:native]`.

### Requirement: Un unico punto de aplicacion

Applying a validated parameter SHALL be done only by `runtime_config_apply()` (and its text/JSON front-ends), for the four origins `usb`, `web`, `tb-gprs` and `tb-wifi`. The application SHALL set the `in3` field, persist the value in NVS under the key that `recapVariables()` reads, and run the parameter's side effect, as listed in the design. If the validated value equals the value already held, NVS SHALL NOT be written. `fan_ctl_pwm` SHALL only write the LEDC channel while the fan is commanded on, the fan PID is not in AUTOMATIC mode and the factory test is not active. `POST /config` and the console `/config,<param>,<value>` SHALL call this function for the nine parameters, keeping `serial`, `reference_temp`, `set_time` and `BABY_WIPE` where they are.

#### Scenario: Applying a changed value persists it and runs the side effect

- **GIVEN** a unit with `air_tmax` at 38.0 in NVS
- **WHEN** `air_tmax` is applied with 40.0 from any origin
- **THEN** `in3.airTemperatureSetMax` and `maxDesiredTemp[CONTROL_AIR]` become 40.0
- **AND** after a reboot `recapVariables()` loads 40.0

Manual verification on hardware (no test environment for NVS or `in3`).

#### Scenario: Applying the same value does not write flash

- **WHEN** `gprs_stby` is applied with the value already in force
- **THEN** the call succeeds and no NVS write is issued

Manual verification on hardware.

#### Scenario: A rejected value leaves the unit untouched

- **WHEN** `fan_ctl_pwm` is applied with 300 from `POST /config`
- **THEN** the response is HTTP 400 naming the reason and the previous value remains in `in3`, NVS and the LEDC channel

Manual verification on hardware.

#### Scenario: Local paths keep their unrelated parameters

- **WHEN** `POST /config` carries `serial`, `reference_temp` or `set_time`, or the console receives `BABY_WIPE,1234`
- **THEN** they are handled exactly as before this change

Manual verification on hardware.

### Requirement: Suscripcion a atributos compartidos en ambos transportes

Each ThingsBoard client (GPRS and WiFi) SHALL register, once per process, a shared-attribute update subscription for the nine keys using `Shared_Attribute_Callback` and `Shared_Attributes_Subscribe`, and SHALL, after each successful connection to ThingsBoard, issue one `Shared_Attributes_Request` for the same keys using `Attribute_Request_Callback`. Both callbacks SHALL apply the keys present in the delivered object through the single application point, tagging the origin `tb-gprs` or `tb-wifi`. The callbacks SHALL NOT publish; publication happens in the owning task. The SDK SHALL remain at v0.13.0.

#### Scenario: Hot change while connected

- **GIVEN** a unit connected to ThingsBoard over WiFi (or GPRS)
- **WHEN** the tenant writes the shared attribute `air_tmax = 38`
- **THEN** the unit applies 38.0 within one ThingsBoard loop pass
- **AND** a log line records origin `tb-wifi` (or `tb-gprs`), name, requested and applied value

Manual verification against a real ThingsBoard server (no test environment).

#### Scenario: Change made while the unit was off

- **GIVEN** the shared attribute `gprs_stby = 1800` was written while the unit was powered off
- **WHEN** the unit boots and connects to ThingsBoard
- **THEN** after the post-connect request it holds `gprs_stby = 1800` and has persisted it

Manual verification against a real ThingsBoard server.

#### Scenario: Reconnection does not duplicate the subscription

- **WHEN** the ThingsBoard connection drops and is re-established several times
- **THEN** the shared-attribute update callback is still registered exactly once and one attribute update causes exactly one application per parameter

Manual verification (observed in the log across a forced reconnection).

#### Scenario: A rejected remote value changes nothing

- **WHEN** the tenant writes `fan_ctl_pwm = 999`
- **THEN** the unit keeps its previous value, logs the rejection with origin and reason, and echoes the previous value

Manual verification against a real ThingsBoard server.

### Requirement: Eco del valor efectivo

After applying a parameter from any origin, and once after each ThingsBoard connection for all nine, the unit SHALL publish the effective value as a **client** attribute with the same name as the parameter, with `fan_pid_en` as a JSON boolean. A failed publish SHALL be retried on a later pass; a successful one SHALL clear exactly the entries it carried. The echo SHALL fit in a statically sized buffer and SHALL NOT be sent from inside an SDK callback.

#### Scenario: The echo carries the clamped value

- **WHEN** the tenant writes the shared attribute `skin_tmax = 45`
- **THEN** the client attribute `skin_tmax` becomes 40.0, while the shared attribute still reads 45

Manual verification against a real ThingsBoard server.

#### Scenario: Building the echo

- **WHEN** the echo is built with a subset of parameters pending
- **THEN** the JSON contains exactly those keys with their current values, `fan_pid_en` as `true`/`false`, and it fits the 224 B buffer even with every key at its maximum

The JSON formatting of values is covered by motherBoard `[env:native]` if kept in the pure module; the publish itself is manual verification.

#### Scenario: Echo after a local change

- **WHEN** `POST /config` changes `gprs_act` while the unit is connected to ThingsBoard
- **THEN** the client attribute `gprs_act` shows the new value on the next publish pass

Manual verification against a real ThingsBoard server.

### Requirement: Registro de cada aplicacion

Each application attempt SHALL be logged with origin (`usb`, `web`, `tb-gprs`, `tb-wifi`), name, requested value and applied value, or the rejection reason. Rejections SHALL be visible even when `logI` is compiled out.

#### Scenario: Log line content

- **WHEN** `air_tmax` is requested at 45.0 from `tb-gprs`
- **THEN** the log contains origin `tb-gprs`, name `air_tmax`, requested 45.00 and applied 40.00

Manual verification on the serial console.
