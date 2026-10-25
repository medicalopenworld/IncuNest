## ADDED Requirements

### Requirement: CTRL,ENV line format

The motherBoard SHALL report the SensorBoard environment to the display as one ASCII line `CTRL,ENV,<link>,<door>,<lux>,<dbA>\n`, where `link` is `1` or `0`, `door` is `0` (closed), `1` (open), `2` (fault) or `-1` (unknown), and `lux` and `dbA` are non-negative integers or `-1` (unavailable). The line SHALL NOT exceed 39 characters plus the terminating newline.

#### Scenario: Healthy SensorBoard
- **WHEN** the link is up, the door is known closed and fresh, lux is 320.4 and dBA is 61.6
- **THEN** the line is `CTRL,ENV,1,0,320,62\n`

Covered by motherBoard `[env:native]`.

#### Scenario: Door open
- **WHEN** the link is up and the door is known open and fresh
- **THEN** the `door` field is `1`

Covered by motherBoard `[env:native]`.

### Requirement: Unavailable values use sentinels, never zero

The motherBoard SHALL send `-1` for any value it does not currently know, and SHALL NOT send `0` in place of a missing measurement. When the SensorBoard link is down it SHALL send `link=0` and `-1` for `door`, `lux` and `dbA`, and SHALL NOT send the last good readings.

#### Scenario: Link down
- **WHEN** `link_ok` is false while the snapshot still holds earlier readings
- **THEN** the line is `CTRL,ENV,0,-1,-1,-1\n`

Covered by motherBoard `[env:native]`.

#### Scenario: Stale sound reading
- **WHEN** the link is up but the last sound level is older than `SB_SOUND_STALE_MS`
- **THEN** `dbA` is `-1` and the other fields are unaffected

Covered by motherBoard `[env:native]`.

#### Scenario: Sensor not calibrated or invalid
- **WHEN** the snapshot marks lux (or dBA) as not valid
- **THEN** that field is `-1`

Covered by motherBoard `[env:native]`.

### Requirement: Door fault is reported as its own state

The motherBoard SHALL report `door=2` whenever the door state is flagged faulty (the same condition that raises `ALARM_SENSORBOARD_DOOR_FAULT`), regardless of the last open/closed reading, and SHALL NOT report `0` or `1` in that condition.

#### Scenario: Faulty hall with a last known state
- **WHEN** the link is up, `door_faulty` is true and the last known state was open
- **THEN** the `door` field is `2`

Covered by motherBoard `[env:native]`.

### Requirement: Send cadence

The motherBoard SHALL send `CTRL,ENV` immediately when `link` or `door` differs from the last value sent, SHALL send it at the latest `PROTO_ENV_PERIOD_MS` (5000 ms) after the previous send otherwise, SHALL send it the first time it is evaluated, and SHALL NOT send it more often. A change in `lux` or `dbA` alone SHALL NOT trigger an immediate send. The interval comparison SHALL be correct across the wrap of the 32-bit millisecond counter.

#### Scenario: First evaluation
- **WHEN** nothing has been sent yet
- **THEN** a send is due

Covered by motherBoard `[env:native]`.

#### Scenario: Nothing changed within the period
- **WHEN** the last send was 4999 ms ago and the report is identical
- **THEN** no send is due

Covered by motherBoard `[env:native]`.

#### Scenario: Period elapsed
- **WHEN** the last send was 5000 ms ago and the report is identical
- **THEN** a send is due

Covered by motherBoard `[env:native]`.

#### Scenario: Door changes
- **WHEN** the door goes from closed to open 100 ms after the last send
- **THEN** a send is due immediately

Covered by motherBoard `[env:native]`.

#### Scenario: Only the sound level changes
- **WHEN** only `dbA` differs from the last send and the period has not elapsed
- **THEN** no send is due

Covered by motherBoard `[env:native]`.

#### Scenario: Millisecond counter wraps
- **WHEN** the last send happened at 4294967000 ms and now is 4800 ms (wrapped)
- **THEN** the elapsed time is computed as 5096 ms and a send is due

Covered by motherBoard `[env:native]`.

### Requirement: Units without a SensorBoard stay silent

The motherBoard SHALL NOT emit `CTRL,ENV` when the room sensor source is not the SensorBoard.

#### Scenario: Legacy unit with I2C sensors
- **WHEN** the unit booted with `SENSOR_SOURCE_I2C`
- **THEN** no `CTRL,ENV` line is ever sent

Verified manually: the gating is a one-line check in `CommTask.cpp`, which has no host test environment.
