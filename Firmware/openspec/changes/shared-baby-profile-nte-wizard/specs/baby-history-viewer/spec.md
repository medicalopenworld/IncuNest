## ADDED Requirements

### Requirement: Top-bar "Babies" button opens the history screen
Display_HMI SHALL show a new "Babies" button in the top bar. Tapping it
SHALL open a history screen, independent of the AIR/SKIN activation wizard
(`temp-control-activation-wizard`) — it does not require either control
mode to be active or being activated.

#### Scenario: Babies button opens the history screen from the main UI
- **WHEN** the nurse taps the "Babies" top-bar button
- **THEN** the history screen opens, showing active profiles first (see
  below)
- (Manual verification on real hardware — no test env exists for
  Display_HMI.)

### Requirement: Active profiles are listed first, each with a Discharge action
The history screen SHALL request `HMI,PROFILE_LIST_REQ` and list the
returned active profiles (up to 3) first, each with a "Discharge" action
and a tap-to-view-chart affordance.

#### Scenario: Active profile shows a Discharge action
- **WHEN** the history screen displays an active profile card
- **THEN** a "Discharge" action is available on that card, alongside the
  existing name/gestational-age/weight display
- (Manual verification on real hardware.)

### Requirement: Discharge requires confirmation and an outcome selection
Tapping "Discharge" on an active profile SHALL show a confirmation dialog
that requires selecting an outcome (Unknown/Survived/Deceased/Transferred)
before sending `HMI,PROFILE_DISCHARGE,<seq>,<outcome>`. Dismissing the
dialog without confirming SHALL send nothing and leave the profile active.

#### Scenario: Confirming discharge with a selected outcome sends the request
- **WHEN** the nurse taps "Discharge", selects "Survived", and confirms
- **THEN** `HMI,PROFILE_DISCHARGE,<seq>,1` is sent
- (Manual verification on real hardware.)

#### Scenario: Dismissing the discharge dialog sends nothing
- **WHEN** the nurse opens the discharge dialog and dismisses it without
  confirming an outcome
- **THEN** no `PROFILE_DISCHARGE` message is sent and the profile remains
  active and listed
- (Manual verification on real hardware.)

### Requirement: Discharge is independent of AIR/SKIN control state in the UI
Discharging the baby currently driving active AIR or SKIN control SHALL be
allowed and SHALL NOT alter that control state; the UI SHALL NOT block or
warn against discharging based on control state.

#### Scenario: Discharging the baby driving active control does not stop control
- **WHEN** the nurse discharges the profile currently associated with
  active AIR or SKIN control
- **THEN** the discharge proceeds normally and AIR/SKIN control continues
  running unchanged, with no blocking dialog or warning shown
- (Manual verification on real hardware.)

### Requirement: Archived profiles are listed below active ones, paginated 10 per page
Below the active profiles, the history screen SHALL list archived profiles
using `HMI,PROFILE_HISTORY_REQ,<page>` / `CTRL,PROFILE_HISTORY`, 10 entries
per page, with pagination controls (next/previous page) reflecting the
response's `<totalCount>`. Each archived entry SHALL show name,
gestational age, last known weight, admission date, discharge date, and
outcome, plus a tap-to-view-chart affordance. Archived profiles SHALL NOT
show a Discharge action (already discharged/archived).

#### Scenario: Opening the history screen requests the first page of archived profiles
- **WHEN** the history screen is opened
- **THEN** `HMI,PROFILE_HISTORY_REQ,0` is sent and the returned entries are
  shown below the active profiles
- (Manual verification on real hardware.)

#### Scenario: Paging forward requests the next page
- **WHEN** the nurse taps "next page" and `<totalCount>` indicates more
  entries exist beyond the current page
- **THEN** `HMI,PROFILE_HISTORY_REQ,<page+1>` is sent and the screen
  updates with the returned entries
- (Manual verification on real hardware.)

#### Scenario: Archived entries have no Discharge action
- **WHEN** the history screen displays an archived profile entry
- **THEN** no "Discharge" action is shown for that entry
- (Manual verification on real hardware.)

### Requirement: Tapping any listed baby opens its weight-evolution chart
Tapping any baby card — active or archived — SHALL send
`HMI,WEIGHT_HISTORY_REQ,<seq>` and open a chart screen once
`CTRL,WEIGHT_HISTORY` is received, rendered with an LVGL `lv_chart` widget:
X-axis = day of life (`dayOffset` since admission), Y-axis = weight in
grams.

#### Scenario: Tapping an active baby opens its chart
- **WHEN** the nurse taps an active profile's card (not the Discharge
  action)
- **THEN** `HMI,WEIGHT_HISTORY_REQ,<seq>` is sent and, on response, the
  weight-evolution chart screen opens for that baby
- (Manual verification on real hardware.)

#### Scenario: Tapping an archived baby opens its chart
- **WHEN** the nurse taps an archived profile's card
- **THEN** `HMI,WEIGHT_HISTORY_REQ,<seq>` is sent and, on response, the
  weight-evolution chart screen opens for that baby, using its archived
  weight history
- (Manual verification on real hardware — relies on `baby-profile-storage`'s
  unified retention guarantee that an archived audit entry always has a
  corresponding weight-archive file.)

### Requirement: Chart screen renders whatever bounded point set it receives
The chart screen SHALL render exactly the `<n>` points (n <= 50, per
`baby-profile-protocol`'s bound) received in `CTRL,WEIGHT_HISTORY`, without
performing any additional client-side downsampling or point-count
validation beyond the existing malformed-line field-count check — all
downsampling is the motherBoard's responsibility (`baby-profile-storage`).

#### Scenario: Chart renders a downsampled 50-point response as-is
- **WHEN** `CTRL,WEIGHT_HISTORY,<seq>,50,{...}` is received
- **THEN** the chart plots exactly those 50 points, with no further
  reduction on the HMI side
- (Manual verification on real hardware.)

#### Scenario: A malformed WEIGHT_HISTORY response is treated as no data, not a crash
- **WHEN** a `CTRL,WEIGHT_HISTORY` line arrives with a field count that does
  not match its declared `<n>`
- **THEN** the line is discarded per the malformed-line convention (see
  `baby-profile-protocol`) and the chart screen shows an empty/no-data
  state rather than partial or garbled points
- (Manual verification on real hardware — no automated test env for
  Display_HMI.)
