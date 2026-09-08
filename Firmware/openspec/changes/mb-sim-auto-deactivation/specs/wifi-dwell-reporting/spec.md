## ADDED Requirements

### Requirement: WiFi association dwell tracking

The motherBoard SHALL maintain, for the WiFi network it is currently associated with, the SSID, the epoch of the first association observed with a valid clock, and the number of distinct UTC days on which an association with that SSID has been observed. The day boundary SHALL be UTC (`epoch / 86400`), never local time.

#### Scenario: First association with a provisioned network

- **WHEN** the state is empty and an association to `"HospitalWiFi"` is observed at a valid epoch
- **THEN** the SSID is stored as `"HospitalWiFi"`, the first-association epoch is set to that epoch, and the distinct-day count becomes 1
- **AND** the update reports that the state changed

Covered by motherBoard `[env:native]`.

#### Scenario: Repeated association on the same UTC day

- **WHEN** an association to the already-tracked SSID is observed at an epoch falling on the same UTC day as the last counted day
- **THEN** the distinct-day count, the first-association epoch and the stored SSID are all unchanged
- **AND** the update reports that the state did NOT change

Covered by motherBoard `[env:native]`.

#### Scenario: Association on a later UTC day

- **WHEN** an association to the already-tracked SSID is observed at an epoch falling on a later UTC day than the last counted day
- **THEN** the distinct-day count is incremented by exactly 1 and the last-counted-day index becomes that day
- **AND** the first-association epoch is unchanged

Covered by motherBoard `[env:native]`.

#### Scenario: Non-contiguous days still count

- **WHEN** associations are observed on day N, then on day N+9, then on day N+30, with the unit unpowered in between
- **THEN** the distinct-day count is 3

Covered by motherBoard `[env:native]`.

#### Scenario: The network changes

- **WHEN** an association to an SSID different from the tracked one is observed
- **THEN** the tracked SSID is replaced, the distinct-day count is reset to 1 if the clock is valid or 0 if it is not, and the first-association epoch is reset to the new epoch or to 0 if the clock is not valid

Covered by motherBoard `[env:native]`.

#### Scenario: A 32-byte SSID is stored without truncation or overrun

- **WHEN** an association to an SSID of exactly 32 bytes is observed
- **THEN** all 32 bytes are stored, NUL-terminated, and comparison against the tracked SSID on the next association reports a match

Covered by motherBoard `[env:native]`.

### Requirement: Day counting requires a valid clock, the network-change reset does not

A day SHALL be counted only when the supplied epoch is at or after 1609459200 (2021-01-01T00:00:00Z), the same validity threshold used elsewhere on this board. The reset triggered by a change of SSID SHALL be applied regardless of clock validity, so that days are never credited to a network the unit is no longer on.

#### Scenario: Association before the clock is set

- **WHEN** an association is observed with an epoch of 0 and the tracked SSID is unchanged
- **THEN** the distinct-day count and the first-association epoch are unchanged

Covered by motherBoard `[env:native]`.

#### Scenario: The clock arrives after the association

- **WHEN** an association to a new SSID is observed with an epoch of 0, and a later association to that same SSID is observed with a valid epoch
- **THEN** the first-association epoch is set to that later valid epoch and the distinct-day count becomes 1

Covered by motherBoard `[env:native]`.

#### Scenario: The network changes while the clock is unset

- **WHEN** an association to an SSID different from the tracked one is observed with an epoch of 0
- **THEN** the tracked SSID is replaced and the distinct-day count is 0
- **AND** the update reports that the state changed, so the reset is persisted

Covered by motherBoard `[env:native]`.

### Requirement: Persistence bounded to actual state changes

The dwell state SHALL be persisted in NVS under the `mb_wifi` namespace and reloaded at boot, and SHALL be written back only when an update actually changed it — at most once per UTC day per SSID plus once per network change. A repeated association that changes nothing SHALL cause no NVS write.

#### Scenario: A flapping link produces no writes

- **WHEN** twenty associations to the tracked SSID are observed within the same UTC day
- **THEN** exactly one of them at most reports a state change, and the remaining ones report none

Covered by motherBoard `[env:native]`.

#### Scenario: The count survives a power cut

- **WHEN** a unit has accumulated several distinct days on a provisioned SSID and is power-cycled
- **THEN** after boot the reported distinct-day count and first-association epoch are the values from before the power cut, and a same-day reassociation does not increment the count

Manual verification on real hardware.

### Requirement: Default and provisioned networks are distinguished

The motherBoard SHALL report whether the associated SSID is the compiled-in `WIFI_SSID` default or a provisioned one, since only a provisioned network can indicate a destination site.

#### Scenario: Associated with the compiled-in default

- **WHEN** the unit is associated with the SSID equal to the compiled-in `WIFI_SSID`
- **THEN** the published default flag is true

Covered by motherBoard `[env:native]` for the comparison; the published value is verified manually.

#### Scenario: Associated with a provisioned network

- **WHEN** the unit is associated with an SSID that came from NVS or from the `setWifi` RPC and differs from `WIFI_SSID`
- **THEN** the published default flag is false

Covered by motherBoard `[env:native]` for the comparison; the published value is verified manually.

### Requirement: Dwell attributes published to ThingsBoard over the WiFi transport

The motherBoard SHALL publish `wifi_ssid`, `wifi_is_default`, `wifi_dwell_days`, `wifi_dwell_since` and `wifi_dwell_span_d` as ThingsBoard **client attributes** over the WiFi transport, whenever the dwell state changes and on every ThingsBoard reconnection. It SHALL NOT publish them over the GPRS transport, and SHALL NOT publish the WiFi password, the BSSID, or any scan of neighbouring networks.

#### Scenario: A new day is published

- **WHEN** the distinct-day count increments
- **THEN** the five attributes are published with the new values and the dirty flag is cleared only on a successful publish

Manual verification on real hardware.

#### Scenario: Attributes are restored after a broker reconnection

- **WHEN** the ThingsBoard connection drops and is re-established with no change to the dwell state
- **THEN** the five attributes are published again

Manual verification on real hardware.

#### Scenario: The password is never published

- **WHEN** the attribute payload is inspected on the broker
- **THEN** it contains no WiFi password, no BSSID and no neighbouring-network list

Manual verification on real hardware.

#### Scenario: Nothing is published over cellular

- **WHEN** the unit is running on GPRS with no WiFi association
- **THEN** no dwell attribute is published on that transport

Manual verification on real hardware.

### Requirement: The published SSID is sanitized to printable ASCII

An SSID is 32 arbitrary bytes, not a string. Every byte outside the printable ASCII range 0x20–0x7E SHALL be replaced with `?` before the SSID is published, so that a hostile or merely non-UTF-8 SSID cannot produce an invalid MQTT payload.

#### Scenario: Control bytes and non-UTF-8 bytes are replaced

- **WHEN** the associated SSID contains a byte of 0x00 through 0x1F, or a byte above 0x7E
- **THEN** each such byte is published as `?` and the remaining bytes are published unchanged

Covered by motherBoard `[env:native]`.

#### Scenario: Quotes and backslashes survive as data

- **WHEN** the associated SSID contains a double quote or a backslash
- **THEN** the published attribute value contains those characters and the payload remains valid JSON

Covered by motherBoard `[env:native]` for the sanitizer; payload validity is verified manually.

### Requirement: No deactivation policy and no Onomondo credential in the field firmware

The firmware SHALL NOT evaluate the SIM-deactivation policy, SHALL NOT contain the dwell threshold, the silence timeout or any list of the organization's own networks, and SHALL NOT contact `api.onomondo.com` outside the `sim_act` factory test. `ONOMONDO_API_KEY` SHALL remain confined to the factory build, because it is a fleet-wide credential and the release binaries are published.

#### Scenario: No threshold constant exists in the firmware

- **WHEN** the motherBoard sources are searched for a dwell threshold, a silence timeout or an SSID exclusion list
- **THEN** none is found, and the only Onomondo endpoint reference is the one in `modules/factory_test/ftest_sim_activation.cpp`

Manual verification by inspection of the change.

#### Scenario: A field build carries no usable Onomondo key

- **WHEN** a field build is produced and its binary inspected
- **THEN** no real Onomondo API key is present in it

Manual verification on the produced binary.

### Requirement: Attribute contract for the server-side deactivation policy

These attributes exist to be consumed by a server-side policy, and the contract SHALL be recorded here so the two halves cannot drift apart. The policy SHALL deactivate a unit's SIM only when the unit reports `wifi_is_default` false and `wifi_dwell_days` at or above 14; SHALL never deactivate while the reported `wifi_ssid` appears on an operator-maintained list of the organization's own networks; and SHALL re-activate the SIM automatically if the unit reports nothing for more than 72 hours after a deactivation. The re-activation net is not optional: a unit whose SIM has been deactivated has no transport with which to restore it.

#### Scenario: A settled unit is taken down

- **WHEN** a unit reports `wifi_is_default` false, `wifi_dwell_days` of 14 or more, and an SSID not on the organization list
- **THEN** the policy issues `PATCH /sims/{iccid}` with `{"activated":false}` and records the outcome against the device

Verified outside the firmware, against the server-side policy.

#### Scenario: A unit on one of the organization's own networks is never taken down

- **WHEN** a unit reports 40 distinct days on an SSID that appears on the operator-maintained list
- **THEN** the policy takes no action, whatever the day count

Verified outside the firmware, against the server-side policy.

#### Scenario: A unit that falls silent gets its SIM back

- **WHEN** a unit's SIM has been deactivated and the unit then reports nothing for more than 72 hours
- **THEN** the policy issues `PATCH /sims/{iccid}` with `{"activated":true}` without human intervention

Verified outside the firmware, against the server-side policy.

#### Scenario: A stale report is not evidence

- **WHEN** a unit's `wifi_dwell_days` has not been refreshed for longer than the policy's freshness window, for instance after a firmware downgrade
- **THEN** the policy treats the evidence as insufficient and takes no deactivation action

Verified outside the firmware, against the server-side policy.
