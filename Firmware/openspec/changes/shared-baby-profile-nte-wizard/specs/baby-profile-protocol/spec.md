## ADDED Requirements

### Requirement: PROFILE_LIST_REQ / PROFILE_LIST exchange
`HMI,PROFILE_LIST_REQ` (HMI -> MB, no fields) SHALL cause the motherBoard to
respond with `CTRL,PROFILE_LIST,<n>,{<seq>,<name>,<gestWeeks>,<weightGrams>}x n`
where `n` is 0-3, listing the currently active NVS profiles.

#### Scenario: List request with active profiles
- **WHEN** `HMI,PROFILE_LIST_REQ` arrives and 2 active profiles exist
- **THEN** `CTRL,PROFILE_LIST,2,<seq1>,<name1>,<gestWeeks1>,<weightGrams1>,<seq2>,<name2>,<gestWeeks2>,<weightGrams2>` is sent
- (Verified via motherBoard `[env:native]` for the response-building logic.)

#### Scenario: List request with no active profiles
- **WHEN** `HMI,PROFILE_LIST_REQ` arrives and no active profiles exist
- **THEN** `CTRL,PROFILE_LIST,0` is sent
- (Verified via motherBoard `[env:native]`.)

### Requirement: PROFILE_NEW creates a profile
`HMI,PROFILE_NEW,<name>,<gestWeeks>` SHALL create a new active profile with
`weightGrams=0` and `admissionEpoch` set from the current synced time (0 if
unknown), per `baby-profile-storage`'s creation/eviction rules.

#### Scenario: Valid new-baby request
- **WHEN** `HMI,PROFILE_NEW,Baby A,32` arrives
- **THEN** a new active profile is created with `name="Baby A"`,
  `gestWeeks=32`, `weightGrams=0`, and a fresh `seq`
- (Verified via motherBoard `[env:native]`.)

### Requirement: PROFILE_SELECT selects an existing profile
`HMI,PROFILE_SELECT,<seq>` SHALL select the active profile matching `<seq>`
as the one being activated in the current wizard flow.

#### Scenario: Selecting an unknown seq is discarded
- **WHEN** `HMI,PROFILE_SELECT,<seq>` arrives with a `<seq>` that does not
  match any active profile
- **THEN** the line is discarded silently (per the malformed-line rule
  below) and no profile is selected
- (Verified via motherBoard `[env:native]`.)

### Requirement: PROFILE_WEIGHT sets or skips the weight
`HMI,PROFILE_WEIGHT,<seq>,<grams|SKIP>` SHALL update the selected profile's
`weightGrams` (0 if `SKIP`) and is the point at which the profile becomes
persisted (see `baby-profile-storage`'s "nothing persisted before
PROFILE_WEIGHT" requirement).

#### Scenario: SKIP sets weightGrams to 0
- **WHEN** `HMI,PROFILE_WEIGHT,<seq>,SKIP` arrives for the selected profile
- **THEN** `weightGrams` is set to 0 for that profile
- (Verified via motherBoard `[env:native]`.)

#### Scenario: Numeric weight is stored as-is
- **WHEN** `HMI,PROFILE_WEIGHT,<seq>,2450` arrives for the selected profile
- **THEN** `weightGrams` is set to 2450 for that profile
- (Verified via motherBoard `[env:native]`.)

### Requirement: PROFILE_RANGE is computed via the shared NTE engine
The motherBoard SHALL compute `CTRL,PROFILE_RANGE` using
`nte-calculation-engine`'s `calculateNteRange()` after receiving
`PROFILE_WEIGHT` (or a subsequent `PROFILE_AGE_MANUAL`), responding with
`CTRL,PROFILE_RANGE,<seq>,<ageKnown>,<ageDays>,<lo>,<hi>,<mid>,<estimated>`.
`ageKnown=0` SHALL mean the HMI must ask for manual age (per
`temp-control-activation-wizard`).

#### Scenario: Age known — range computed immediately
- **WHEN** `PROFILE_WEIGHT` is received and synced time is available to
  derive age from `admissionEpoch`
- **THEN** `CTRL,PROFILE_RANGE` is sent with `ageKnown=1`, the derived
  `ageDays`, and the `lo`/`hi`/`mid`/`estimated` from `calculateNteRange()`
- (Verified via motherBoard `[env:native]`.)

#### Scenario: Age unknown — range response asks the HMI to collect it
- **WHEN** `PROFILE_WEIGHT` is received and no synced time is available at
  that moment
- **THEN** `CTRL,PROFILE_RANGE` is sent with `ageKnown=0`; `ageDays` is not
  used by the HMI in this case
- (Verified via motherBoard `[env:native]`.)

### Requirement: PROFILE_AGE_MANUAL triggers a recomputed PROFILE_RANGE
`HMI,PROFILE_AGE_MANUAL,<seq>,<ageDays>` SHALL only be sent (and only be
accepted) when a prior `CTRL,PROFILE_RANGE` for the same `<seq>` had
`ageKnown=0`. On receipt, the motherBoard SHALL recompute the range using
the provided `<ageDays>` and resend `CTRL,PROFILE_RANGE` (now with
`ageKnown=1`).

#### Scenario: Manual age recomputes and resends the range
- **WHEN** `HMI,PROFILE_AGE_MANUAL,<seq>,14` arrives following a
  `CTRL,PROFILE_RANGE,<seq>,0,...` response
- **THEN** a new `CTRL,PROFILE_RANGE,<seq>,1,14,<lo>,<hi>,<mid>,<estimated>`
  is sent, computed via `calculateNteRange()` with `ageDays=14`
- (Verified via motherBoard `[env:native]`.)

### Requirement: PROFILE_DISCHARGE archives a profile immediately, independent of control state
`HMI,PROFILE_DISCHARGE,<seq>,<outcome>` SHALL, for an active `<seq>`, set
`dischargeEpoch`/`outcome` and immediately archive the profile via the
shared archive routine (per `baby-profile-storage`'s "Explicit discharge"
requirement), freeing its active slot right away. This message SHALL have
no effect on AIR/SKIN control state.

#### Scenario: Valid discharge request archives and frees the slot
- **WHEN** `HMI,PROFILE_DISCHARGE,<seq>,1` arrives for an active `<seq>`
- **THEN** the profile is archived with `outcome=1` (Survived) and its
  active slot becomes free immediately
- (Verified via motherBoard `[env:native]`.)

#### Scenario: Discharge for an unknown seq is discarded
- **WHEN** `HMI,PROFILE_DISCHARGE,<seq>,<outcome>` arrives with a `<seq>`
  that does not match any active profile
- **THEN** the line is discarded silently (per the malformed-line rule
  below) and no archival occurs
- (Verified via motherBoard `[env:native]`.)

#### Scenario: Discharge with an out-of-range outcome value is discarded
- **WHEN** `HMI,PROFILE_DISCHARGE,<seq>,<outcome>` arrives with an
  `<outcome>` value outside `0-3`
- **THEN** the line is discarded silently and no archival occurs
- (Verified via motherBoard `[env:native]`.)

### Requirement: PROFILE_HISTORY_REQ / PROFILE_HISTORY paginated exchange
`HMI,PROFILE_HISTORY_REQ,<page>` SHALL cause motherBoard to respond with
`CTRL,PROFILE_HISTORY,<page>,<totalCount>,<n>,{<seq>,<name>,<gestWeeks>,
<lastWeightGrams>,<admissionEpoch>,<dischargeEpoch>,<outcome>}xn`, where
`<totalCount>` is the current count of valid (non-tombstoned) archived
records, `<page>` is 0-indexed, 10 records per page, most-recently-archived
first (per design decision 12), and `<n>` is the number of records actually
returned for that page (0-10).

#### Scenario: First page of a history with more than 10 archived babies
- **WHEN** `HMI,PROFILE_HISTORY_REQ,0` arrives and 25 valid archived
  records exist
- **THEN** `CTRL,PROFILE_HISTORY,0,25,10,{...}` is sent with the 10
  most-recently-archived records
- (Verified via motherBoard `[env:native]` for the pure pagination-slicing
  logic.)

#### Scenario: Last, partially-filled page
- **WHEN** `HMI,PROFILE_HISTORY_REQ,2` arrives and 25 valid archived
  records exist (pages 0 and 1 fully cover 20)
- **THEN** `CTRL,PROFILE_HISTORY,2,25,5,{...}` is sent with the remaining 5
  records
- (Verified via motherBoard `[env:native]`.)

#### Scenario: Page beyond the available records returns zero entries
- **WHEN** `HMI,PROFILE_HISTORY_REQ,<page>` arrives with a `<page>` past the
  last page of available records
- **THEN** `CTRL,PROFILE_HISTORY,<page>,<totalCount>,0` is sent (no records,
  not an error)
- (Verified via motherBoard `[env:native]`.)

#### Scenario: Tombstoned records are excluded from totalCount and pagination
- **WHEN** `HMI,PROFILE_HISTORY_REQ,<page>` arrives and some
  `baby_history.log` records are marked `valid=false` (per the unified
  eviction rule in `baby-profile-storage`)
- **THEN** those records are excluded from both `<totalCount>` and the
  paginated results
- (Verified via motherBoard `[env:native]`.)

### Requirement: WEIGHT_HISTORY_REQ / WEIGHT_HISTORY exchange, bounded to 50 points
`HMI,WEIGHT_HISTORY_REQ,<seq>` SHALL cause motherBoard to respond with
`CTRL,WEIGHT_HISTORY,<seq>,<n>,{<dayOffset>,<weightGrams>}xn`, where `<seq>`
may refer to either an active or an archived profile, `<dayOffset>` is days
since `admissionEpoch`, and `<n>` is at most 50 (per `baby-profile-storage`'s
downsampling requirement).

#### Scenario: Active baby with a small weight history
- **WHEN** `HMI,WEIGHT_HISTORY_REQ,<seq>` arrives for an active baby with 12
  stored weight points
- **THEN** `CTRL,WEIGHT_HISTORY,<seq>,12,{...}` is sent with all 12 points,
  undownsampled
- (Verified via motherBoard `[env:native]`.)

#### Scenario: Archived baby with a large weight history is downsampled
- **WHEN** `HMI,WEIGHT_HISTORY_REQ,<seq>` arrives for an archived baby whose
  `weight_archive/<seq>.bin` holds 800 points
- **THEN** `CTRL,WEIGHT_HISTORY,<seq>,50,{...}` is sent with exactly 50
  downsampled points
- (Verified via motherBoard `[env:native]` for the pure downsampling and
  response-building logic; actual LittleFS read is manual verification.)

#### Scenario: Unknown seq (neither active nor archived) is discarded
- **WHEN** `HMI,WEIGHT_HISTORY_REQ,<seq>` arrives for a `<seq>` that matches
  neither an active profile nor an archived history record
- **THEN** the line is discarded silently and no response is sent
- (Verified via motherBoard `[env:native]`.)

### Requirement: Malformed or truncated PROFILE and WEIGHT_HISTORY lines are discarded silently
Per `.claude/rules/security.md`, any `PROFILE_*`/`CTRL,PROFILE_*`/`WEIGHT_HISTORY_*`/`CTRL,WEIGHT_HISTORY` line SHALL have its field count
validated, and every numeric field validated as parseable, before any field
is indexed or used. A line failing either check SHALL be discarded silently
(logged at error level with the discard reason, never treated as partial
data).

#### Scenario: Line with fewer fields than expected
- **WHEN** a `HMI,PROFILE_NEW` line arrives with fewer comma-separated
  fields than the protocol expects
- **THEN** the line is discarded silently and no profile is created or
  modified
- (Verified via motherBoard `[env:native]`.)

#### Scenario: Non-numeric value in a numeric field
- **WHEN** a `HMI,PROFILE_WEIGHT,<seq>,<grams>` line arrives with a
  non-numeric, non-"SKIP" value in the weight field
- **THEN** the line is discarded silently and `weightGrams` is left
  unchanged
- (Verified via motherBoard `[env:native]`.)

#### Scenario: Non-numeric page number in a history request
- **WHEN** a `HMI,PROFILE_HISTORY_REQ,<page>` line arrives with a
  non-numeric `<page>` value
- **THEN** the line is discarded silently and no `CTRL,PROFILE_HISTORY`
  response is sent
- (Verified via motherBoard `[env:native]`.)

### Requirement: Legacy baby fields are removed from the recurring HMI line
The undocumented `babyWeightGrams`/`babyGestWeeks`/`babyAgeDays` fields currently appended to the recurring `HMI,...` line (`Display_HMI/src/tasks/CommTask.cpp:90-96`) SHALL be removed (**BREAKING**). The
motherBoard consumer that maps those line fields onto `hmi_cmd_msg`
(`motherBoard/src/tasks/CommTask.cpp:413-418`) SHALL be updated to source
baby data from the `NS_BABY` active-profile storage instead. (For the
GPRS/WiFi cloud-telemetry consumers of the *old* `hmi_cmd_msg.baby*`
fields, see "Cloud telemetry republishes baby-profile fields, now
including name and outcome" below — they are updated, not removed, and
their published field set grows rather than staying the same.)

#### Scenario: Recurring HMI line no longer carries legacy baby fields
- **WHEN** the recurring `HMI,...` line is built by `Display_HMI` after this
  change
- **THEN** its field count matches the corrected `PROTOCOL.md` definition
  with no `babyWeightGrams`/`babyGestWeeks`/`babyAgeDays` fields present
- (Verified via motherBoard `[env:native]` field-count check on the parser
  side; the HMI-side line-building change is manual verification, no test
  env exists for Display_HMI.)

### Requirement: Cloud telemetry republishes baby-profile fields, now including name and outcome
The GPRS (`motherBoard/src/tasks/GPRS.cpp:800-806`) and WiFi (`motherBoard/src/tasks/Wifi_OTA.cpp:893-899`) ThingsBoard telemetry publishers SHALL continue publishing `weightGrams`/`gestWeeks`/`ageDays` for the active baby profile, now sourced from `NS_BABY` active-profile storage instead of the removed `hmi_cmd_msg.baby*` fields, using the same "publish once per pending-flag" pattern already in place (`newBabyDataForTelemetry`).
This change ADDS `name`, `outcome`, and
`dischargeEpoch` to that same published field set on **both** the GPRS and
WiFi paths. This is an explicit, deliberate product decision — the
Ministry of Health has access to this data via ThingsBoard — documented so
implementers do not omit `name`/`outcome`/`dischargeEpoch` from the
published set on privacy-minimization grounds; doing so would be a
regression against this requirement, not a fix.

#### Scenario: GPRS telemetry includes name, outcome, and discharge epoch
- **WHEN** GPRS telemetry is published while a pending baby-data change
  exists for the active profile
- **THEN** the published payload includes `weightGrams`, `gestWeeks`, and
  `ageDays` (as before this change) plus `name`, `outcome`, and
  `dischargeEpoch`, all sourced from `NS_BABY` active-profile storage
- (Manual verification on real hardware with a provisioned GPRS
  connection — `GPRS.cpp` is not covered by `[env:native]`.)

#### Scenario: WiFi telemetry includes name, outcome, and discharge epoch
- **WHEN** WiFi telemetry is published while a pending baby-data change
  exists for the active profile
- **THEN** the published payload includes the same expanded field set as
  the GPRS path
- (Manual verification on real hardware with a provisioned WiFi
  connection — `Wifi_OTA.cpp` is not covered by `[env:native]`.)

### Requirement: PROTOCOL.md documents the new messages and fixes the drift
`Firmware/PROTOCOL.md` SHALL document every message in this capability
(`PROFILE_LIST_REQ`, `PROFILE_LIST`, `PROFILE_NEW`, `PROFILE_SELECT`,
`PROFILE_WEIGHT`, `PROFILE_RANGE`, `PROFILE_AGE_MANUAL`) and SHALL correct
the existing drift between the documented and the real recurring `HMI,...`
line field count.

#### Scenario: PROTOCOL.md field count matches the implementation
- **WHEN** `Firmware/PROTOCOL.md` is reviewed against the recurring
  `HMI,...` line's implementation after this change
- **THEN** the documented field count matches exactly
- (Manual verification — documentation conformance, not a runtime test
  case.)
