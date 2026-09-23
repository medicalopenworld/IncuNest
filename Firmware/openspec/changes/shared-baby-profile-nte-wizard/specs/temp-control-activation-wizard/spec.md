## ADDED Requirements

### Requirement: Wizard is mandatory on every OFF->ON transition of AIR or SKIN
The baby-data wizard SHALL open on every OFF->ON transition of the AIR
control switch or the SKIN control switch, even if the wizard was already
completed earlier in the same session. It replaces the current AUTO AIR
popup (`Display_HMI/src/tasks/UITask.cpp:901-1481`), which SHALL be
removed.

#### Scenario: Turning AIR on always opens the wizard
- **WHEN** the AIR control switch transitions from OFF to ON
- **THEN** the wizard opens, regardless of whether it was already completed
  earlier in the current session
- (Manual verification on real hardware — no test env exists for
  Display_HMI.)

#### Scenario: Turning SKIN on always opens the wizard
- **WHEN** the SKIN control switch transitions from OFF to ON
- **THEN** the wizard opens under the same rule as AIR
- (Manual verification on real hardware.)

### Requirement: Phototherapy and humidity also gate on the wizard
The baby-data wizard SHALL also open on every OFF->ON transition of the
phototherapy switch and the humidity switch, subject to the identified-baby
shortcut below. Neither a lamp nor a humidifier has an NTE range, so those
runs SHALL stop at identifying the baby: no weight step, no age-in-days step,
no range proposal and no summary screen. Identifying the baby is still
mandatory because both therapies accumulate per-baby exposure minutes
(`phototherapyMin` / `humidityMin` in `CTRL,PROFILE_LIST`), which cannot be
attributed to anyone without it.

#### Scenario: Turning humidity on opens the wizard
- **WHEN** the humidity switch transitions from OFF to ON with no therapy
  already running
- **THEN** the wizard opens, the switch stays OFF until the wizard finishes,
  and no weight/age/summary screen is shown
- (Manual verification on real hardware — no test env exists for
  Display_HMI.)

#### Scenario: Cancelling the humidity wizard leaves humidity OFF
- **WHEN** the wizard opened by the humidity switch is cancelled
- **THEN** humidity stays OFF and any thermal control already running is left
  untouched
- (Manual verification on real hardware.)

#### Scenario: Phototherapy keeps the safety popup last
- **WHEN** the wizard opened by the phototherapy switch completes
- **THEN** the ISO 7010 M025 eye-protection popup is shown next, immediately
  before the lamp turns on
- (Manual verification on real hardware.)

### Requirement: The identified-baby shortcut lasts only while care is live
The wizard MAY reuse the baby it already identified — skipping the picker and
re-sending `HMI,PROFILE_SELECT,<seq>` — only while some therapy is actually
running (temperature, humidity or phototherapy). Once the incubator has gone
fully idle the care session is over, and the next activation SHALL identify
the baby again, even though the profile is still remembered and still offered
by the picker. This SHALL NOT depend on what the baby-exit dialog was answered
(kangaroo, "not now", or never shown because an alarm owned the screen): that
answer governs the clinical record, never who the picker offers.

#### Scenario: Adding a therapy mid-care does not re-ask
- **WHEN** temperature control is running for an identified baby and
  phototherapy is switched ON (or vice versa)
- **THEN** the baby picker is not shown again for that second therapy
- (Manual verification on real hardware.)

#### Scenario: Re-activating after a full stop asks again
- **WHEN** every control has been switched OFF and the temperature switch is
  then switched ON again
- **THEN** the wizard shows the baby picker, with the previously used baby
  listed as an existing profile
- (Manual verification on real hardware.)

#### Scenario: Kangaroo exit still ends the identity shortcut
- **WHEN** the baby-exit dialog is answered "with the mother" (profile stays
  active, not discharged) and a control is later switched ON
- **THEN** the wizard shows the baby picker rather than silently reusing the
  remembered baby
- (Manual verification on real hardware.)

### Requirement: List screen fails safe on empty list or motherBoard timeout
The wizard's first screen SHALL send `HMI,PROFILE_LIST_REQ` and show up to 3
profile cards plus a "New baby" button. An empty list or a motherBoard
response timeout SHALL skip straight to the "New baby" screen with a
notice, never block indefinitely.

#### Scenario: Empty profile list skips to New baby
- **WHEN** `CTRL,PROFILE_LIST,0` is received
- **THEN** the wizard shows the "New baby" screen directly with a notice,
  without an intermediate empty-list screen
- (Manual verification on real hardware.)

#### Scenario: motherBoard response timeout skips to New baby
- **WHEN** no `CTRL,PROFILE_LIST` response arrives within the wizard's
  timeout window after `HMI,PROFILE_LIST_REQ`
- **THEN** the wizard proceeds to the "New baby" screen with a notice
  instead of waiting indefinitely
- (Manual verification on real hardware — timing-dependent, not
  reproducible in an automated test env.)

### Requirement: New baby and existing baby entry paths
Selecting "New baby" SHALL collect a name (on-screen keyboard, comma key
disabled) and gestational age (weeks stepper), then send
`HMI,PROFILE_NEW,<name>,<gestWeeks>`. Tapping an existing profile card SHALL
send `HMI,PROFILE_SELECT,<seq>`.

#### Scenario: Comma key is disabled on the name keyboard
- **WHEN** the on-screen keyboard is shown for the baby name field
- **THEN** the comma key is disabled/absent, preventing a name that would
  break the comma-delimited protocol
- (Manual verification on real hardware — LVGL UI behavior.)

#### Scenario: Selecting an existing profile card
- **WHEN** the nurse taps an existing profile card showing `seq=S`
- **THEN** `HMI,PROFILE_SELECT,S` is sent and the wizard advances to the
  Weight screen
- (Manual verification on real hardware.)

### Requirement: Weight screen with SKIP
The Weight screen SHALL offer a grams stepper and a SKIP button, sending
`HMI,PROFILE_WEIGHT,<seq>,<grams|SKIP>` and then waiting for
`CTRL,PROFILE_RANGE`.

#### Scenario: SKIP sends the SKIP sentinel
- **WHEN** the nurse taps SKIP on the Weight screen
- **THEN** `HMI,PROFILE_WEIGHT,<seq>,SKIP` is sent
- (Manual verification on real hardware.)

### Requirement: Age-in-days screen shown only when age is unknown
When `CTRL,PROFILE_RANGE` arrives with `ageKnown=0`, the wizard SHALL show
an Age in days screen (stepper), send `HMI,PROFILE_AGE_MANUAL,<seq>,<ageDays>`,
and wait for the recomputed `CTRL,PROFILE_RANGE`. When `ageKnown=1`, this
screen SHALL be skipped entirely.

#### Scenario: ageKnown=0 shows the manual age screen
- **WHEN** `CTRL,PROFILE_RANGE` arrives with `ageKnown=0`
- **THEN** the wizard shows the Age in days screen before proceeding to
  Summary
- (Manual verification on real hardware.)

#### Scenario: ageKnown=1 skips straight to Summary
- **WHEN** `CTRL,PROFILE_RANGE` arrives with `ageKnown=1`
- **THEN** the wizard proceeds directly to the Summary screen, without
  showing the Age in days screen
- (Manual verification on real hardware.)

### Requirement: Summary screen applies the correct activation mode
The Summary screen SHALL show `[lo-hi]` and the midpoint. With a known
weight (`estimated=false`), applying SHALL start AIR at the midpoint
(still adjustable by +/-0.1°C afterwards) or start SKIN fixed at 36.5°C
with no manual adjustment arrows. With `SKIP`/unknown weight
(`estimated=true`), applying SHALL force plain manual AIR (no auto range)
and SHALL keep SKIN blocked.

#### Scenario: Known weight — AIR starts at the midpoint, adjustable
- **WHEN** the nurse taps Apply on the Summary screen after a range with
  `estimated=false` and AIR was the switch being activated
- **THEN** AIR control starts at `mid` and remains adjustable by
  +/-0.1°C arrows
- (Manual verification on real hardware.)

#### Scenario: Known weight — SKIN starts fixed at 36.5C, locked
- **WHEN** the nurse taps Apply on the Summary screen after a range with
  `estimated=false` and SKIN was the switch being activated
- **THEN** SKIN control starts fixed at 36.5°C with no manual adjustment
  arrows shown
- (Manual verification on real hardware.)

#### Scenario: SKIP weight — AIR forced to plain manual mode
- **WHEN** the nurse taps Apply on the Summary screen after a `SKIP`/
  `estimated=true` response and AIR was the switch being activated
- **THEN** AIR control starts in plain manual mode with no auto range
  applied
- (Manual verification on real hardware.)

#### Scenario: SKIP weight — SKIN stays blocked
- **WHEN** the nurse reaches the Summary screen after a `SKIP`/
  `estimated=true` response and SKIN was the switch being activated
- **THEN** SKIN control remains blocked from activating, using the same
  blocking pattern already used for an invalid skin probe
  (`Display_HMI/src/tasks/UITask.cpp:2074-2098`)
- (Manual verification on real hardware.)

### Requirement: Cancelling the wizard leaves the switch OFF
Closing the wizard (back button) before completing it SHALL leave the AIR
or SKIN switch OFF — control SHALL never be activated without the minimum
required data.

#### Scenario: Back button before Summary leaves control OFF
- **WHEN** the nurse presses the back button at any screen before reaching
  and completing the Summary screen
- **THEN** the AIR/SKIN switch that triggered the wizard remains OFF
- (Manual verification on real hardware.)

### Requirement: A critical alarm interrupts the wizard
A critical alarm arriving while the wizard is open SHALL close the wizard
and yield the screen to the alarm, per the project's safety-over-data-entry
priority.

#### Scenario: Critical alarm during the wizard closes it
- **WHEN** a critical alarm is raised while any wizard screen is open
- **THEN** the wizard closes immediately and the alarm screen is shown; the
  switch that triggered the wizard remains OFF (per the cancellation
  requirement above)
- (Manual verification on real hardware.)

### Requirement: Wizard and new alarm text use the existing translation system
All new wizard screen text and any new alarm text introduced by this change SHALL go through the existing translation system, to avoid reintroducing the language desync bug documented in `Firmware/docs/known_issues.md` #3 ("Language Dissociation").

#### Scenario: Wizard text follows the active language setting
- **WHEN** the nurse has selected a non-default language and opens the
  wizard
- **THEN** every wizard screen's text is shown in the selected language via
  the existing translation system, with no hardcoded-default-language
  strings
- (Manual verification on real hardware — no automated test env for
  Display_HMI; this specifically re-verifies known_issues.md #3 is not
  reintroduced.)
