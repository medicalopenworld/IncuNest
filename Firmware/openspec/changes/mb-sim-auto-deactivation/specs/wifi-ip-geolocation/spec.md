## ADDED Requirements

### Requirement: Coarse position derived from the existing daily IP lookup

The motherBoard SHALL obtain a coarse position from the ip-api.com lookup it already performs once a day for the timezone, by extending that request's `fields=` list to include latitude and longitude. It SHALL NOT add an HTTP request, a host, a dependency or a polling interval; the response buffer SHALL remain fixed-size and bounded in time, as it is today.

#### Scenario: Position parsed from a successful response

- **WHEN** the lookup returns a body containing `"status":"success"` together with a latitude and a longitude
- **THEN** both values are parsed and reported as valid

Covered by motherBoard `[env:native]`.

#### Scenario: The timezone keeps working unchanged

- **WHEN** the extended lookup returns a body containing the offset and the position
- **THEN** the timezone offset is resolved exactly as before, and a body that carries the offset but no position still resolves the timezone

Covered by motherBoard `[env:native]`.

#### Scenario: No additional network traffic

- **WHEN** a unit runs for 24 hours on WiFi
- **THEN** the number of requests made to ip-api.com is the same as before this change

Manual verification on real hardware.

### Requirement: The IP response is parsed as untrusted input

The lookup travels over cleartext HTTP because the free tier offers no TLS, so the response is attacker-shaped. The parser SHALL require `"status":"success"`, SHALL reject a latitude outside [-90, 90] or a longitude outside [-180, 180], SHALL not read past the terminating NUL of a truncated body, and SHALL leave the caller's output values untouched on any rejection.

#### Scenario: A failed lookup is rejected

- **WHEN** the body reports `"status":"fail"` but still carries latitude and longitude fields
- **THEN** the parser reports failure and the caller's values are unchanged

Covered by motherBoard `[env:native]`.

#### Scenario: Out-of-range coordinates are rejected

- **WHEN** the body reports a latitude of 91.0, or a longitude of -200.0
- **THEN** the parser reports failure and the caller's values are unchanged

Covered by motherBoard `[env:native]`.

#### Scenario: A truncated body is rejected without overrun

- **WHEN** the body is cut off mid-number, mid-key or mid-string at any offset
- **THEN** the parser reports failure, reads no byte past the terminating NUL, and does not crash

Covered by motherBoard `[env:native]`.

#### Scenario: Missing fields are rejected

- **WHEN** the body reports success and a latitude but no longitude
- **THEN** the parser reports failure

Covered by motherBoard `[env:native]`.

### Requirement: One location channel, GSM authoritative, source named

The published position SHALL continue to use the existing `tri_latitud`, `tri_longitud` and `tri_accuracy` keys so that dashboards bound to them keep working, and SHALL declare its origin in a new `loc_source` key. A cellular fix SHALL take precedence over an IP-derived one. An IP-derived position SHALL be published with an accuracy of 25000 metres, so it can never be mistaken for a cellular or satellite fix.

#### Scenario: The modem has a fix

- **WHEN** the WiFi telemetry is assembled and the modem reports a non-zero latitude or longitude
- **THEN** the cellular position and its own accuracy are published, and `loc_source` is `"gsm"`

Manual verification on real hardware.

#### Scenario: The SIM is deactivated and the modem has no fix

- **WHEN** the WiFi telemetry is assembled, the modem reports no position, and a valid IP-derived position is available
- **THEN** the IP position is published under the same `tri_latitud` and `tri_longitud` keys, `tri_accuracy` is 25000, and `loc_source` is `"ip"`

Manual verification on real hardware.

#### Scenario: No position at all

- **WHEN** the WiFi telemetry is assembled with neither a cellular nor a valid IP position
- **THEN** no location key is published, exactly as before this change

Manual verification on real hardware.

#### Scenario: A unit stays on the fleet map after deactivation

- **WHEN** a unit's SIM is deactivated and the unit keeps reporting over WiFi
- **THEN** the existing map widget still shows it, with no dashboard edit

Manual verification against ThingsBoard.

### Requirement: The IP position is advisory only

Because it arrives over cleartext HTTP and is spoofable, the IP-derived position SHALL reach the two telemetry keys and nothing else. It SHALL NOT influence the PID, any alarm, any actuator, the clock, the timezone or any stored patient record.

#### Scenario: A manipulated response changes only a dashboard value

- **WHEN** the lookup returns an arbitrary in-range latitude and longitude
- **THEN** the only observable effect is the published position, and control, alarms, actuators and the clock are unaffected

Manual verification by inspection of the change.

### Requirement: The transport matrix records the new capability

The capability SHALL be declared in `motherBoard/include/config/transport_policy.h` as its own feature flag, distinct from the cell-tower triangulation flag, which remains false for WiFi because WiFi still cannot produce a tower fix. The drift check SHALL keep passing.

#### Scenario: Declared and consistent

- **WHEN** `python tools/check_transport_matrix.py` is run after the change
- **THEN** it passes, the IP-geolocation flag is set for WiFi only, and `TX_FEATURE_TRIANGULATION_WIFI` is still 0

Manual verification by running the drift check.
