## ADDED Requirements

### Requirement: CTRL,ENV parsing is strict and tolerant of the unknown

The display SHALL parse `CTRL,ENV,<link>,<door>,<lux>,<dbA>` only when all four fields are present and numeric, with `link` in `{0,1}` and `door` in `{-1,0,1,2}`, and SHALL discard any other line without changing its stored state. A `lux` or `dbA` below zero SHALL be stored as unavailable. A display that does not know the message SHALL ignore the line.

#### Scenario: Well-formed line
- **WHEN** the line `CTRL,ENV,1,1,320,62` is parsed
- **THEN** link is up, door is open, lux is 320 and dBA is 62

Covered by Display_HMI `[env:native]`.

#### Scenario: Truncated or malformed line
- **WHEN** the line is `CTRL,ENV,1,1,32`, `CTRL,ENV,1,x,320,62`, `CTRL,ENV,2,0,1,1` or `CTRL,ENV,1,7,1,1`
- **THEN** the parser rejects it and the previous stored state is unchanged

Covered by Display_HMI `[env:native]`.

#### Scenario: Unavailable sentinels
- **WHEN** the line `CTRL,ENV,0,-1,-1,-1` is parsed
- **THEN** link is down and door, lux and dBA are unavailable

Covered by Display_HMI `[env:native]`.

### Requirement: Noise is shown as a qualitative level, never as a number

The display SHALL classify the received dBA into exactly three levels (LOW, MID, HIGH) and SHALL NOT show a decibel or lux figure to the user. The thresholds are provisional, defined on the raw (uncalibrated) sensor scale as LOW below 65, MID from 65 to 74 and HIGH from 75, and SHALL be defined in a single place. An unavailable dBA SHALL classify as no level.

#### Scenario: Resting level
- **WHEN** the dBA is 60 (the measured resting value of the uncalibrated microphone)
- **THEN** the level is LOW

Covered by Display_HMI `[env:native]`.

#### Scenario: Threshold edges
- **WHEN** the dBA is 64, 65, 74 and 75
- **THEN** the levels are LOW, MID, MID and HIGH respectively

Covered by Display_HMI `[env:native]`.

#### Scenario: Unavailable
- **WHEN** the dBA is unavailable
- **THEN** there is no level

Covered by Display_HMI `[env:native]`.

### Requirement: The heading paints door and noise only when they are trustworthy

The display SHALL show the open-door indicator only when the last `CTRL,ENV` is fresh, its link is up and its door state is open. The display SHALL show the noise level only when the last `CTRL,ENV` is fresh, its link is up and the noise level is known. The display SHALL paint nothing when no `CTRL,ENV` has ever been received, when the last one is older than 15 s, or when the motherBoard link is lost. The display SHALL NOT paint a door indicator for a door fault, because `ALARM_SENSORBOARD_DOOR_FAULT` already signals it. The display SHALL NOT paint the closed state.

#### Scenario: Unit without SensorBoard
- **WHEN** no `CTRL,ENV` line has ever been received
- **THEN** neither indicator is shown

Covered by Display_HMI `[env:native]`.

#### Scenario: Door open and noise known
- **WHEN** a fresh `CTRL,ENV,1,1,-1,70` is held
- **THEN** the door-open indicator is shown and the noise level is MID

Covered by Display_HMI `[env:native]`.

#### Scenario: Door fault
- **WHEN** a fresh `CTRL,ENV,1,2,-1,70` is held
- **THEN** the door indicator is not shown and the noise level is still MID

Covered by Display_HMI `[env:native]`.

#### Scenario: SensorBoard link down
- **WHEN** a fresh `CTRL,ENV,0,-1,-1,-1` is held
- **THEN** neither indicator is shown

Covered by Display_HMI `[env:native]`.

#### Scenario: Stale data
- **WHEN** the last `CTRL,ENV` is 15001 ms old
- **THEN** neither indicator is shown

Covered by Display_HMI `[env:native]`.

#### Scenario: motherBoard link lost
- **WHEN** a fresh `CTRL,ENV,1,1,-1,80` is held but the display reports the board link as lost
- **THEN** neither indicator is shown

Covered by Display_HMI `[env:native]`.

#### Scenario: Staleness across the millisecond wrap
- **WHEN** the last line was received at 4294967000 ms and now is 4000 ms (wrapped)
- **THEN** the age is 4296 ms and the data is fresh

Covered by Display_HMI `[env:native]`.

### Requirement: Placement and languages

The display SHALL draw the indicators inside the clock button of the Main heading, to the right of the time and date, and a replica in the heading of the lock screen, without moving any existing heading widget or touch area. The labels SHALL exist in Spanish, English, French and Portuguese, using only ASCII characters. The indicator in Main SHALL be hidden while the clock shows its "no time" text.

#### Scenario: Touching the indicator
- **WHEN** the operator touches the area of the indicator inside the clock button
- **THEN** the touch reaches the clock button (the indicator is not clickable)

Verified manually on the bench: LVGL has no host test environment.

#### Scenario: Language change
- **WHEN** the language is changed while the indicator is visible
- **THEN** its labels are redrawn in the new language

Verified manually on the bench.
