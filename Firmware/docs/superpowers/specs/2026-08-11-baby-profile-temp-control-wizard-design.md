# Baby Profile + Temperature Control Activation Wizard — Design Spec

**Date:** 2026-08-11
**Status:** Approved (Sub-project A of 3 — see "Related sub-projects" below)

## Overview

Today, activating AIR or SKIN temperature control on the incubator does not
ask anything about the baby. A separate, optional "AUTO AIR" popup
(`Display_HMI/src/tasks/UITask.cpp:901-1481`) lets the nurse type in
weight/gestational age/age to get a suggested air setpoint from an existing
Neutral Thermal Environment (NTE) table, but nothing is persisted, and SKIN
mode is completely disconnected from that calculation.

This feature makes the baby-data wizard mandatory on every activation of
either control mode, adds persistent baby profiles (owned by the
motherBoard), and lays the groundwork (shared NTE table) for a future
safety guardrail on SKIN mode (see "Related sub-projects").

**Trigger:** every OFF→ON transition of the AIR or SKIN control switch,
even if already triggered earlier in the same session.

---

## Section 1 — Data Model (motherBoard, source of truth)

Two storage tiers, both new, no changes to `partitions/ESP32S3_8MB.csv`
(20 KB NVS already has headroom; the existing 2 MB `spiffs`/LittleFS
partition — currently used only for transient crash/comm logs,
`DriveUpload.cpp`, `CrashReporter.cpp` — is mounted and mostly free):

**3 active slots — NVS (`Preferences`), new namespace `NS_BABY`:**
```cpp
struct BabyProfile {
  bool     slotUsed;
  uint32_t seq;             // monotonic creation counter — NOT wall-clock;
                             // used for FIFO eviction so it still works
                             // with no WiFi/GPRS time ever available
  char     name[24];        // no commas (protocol is comma-delimited CSV)
  uint8_t  gestWeeks;
  uint16_t weightGrams;     // last known weight; 0 = never provided (SKIP)
  uint32_t admissionEpoch;  // unix time at profile creation; 0 = unknown
  uint32_t dischargeEpoch;  // unix time of explicit discharge; 0 = not discharged
  uint8_t  outcome;         // 0=Unknown, 1=Survived, 2=Deceased, 3=Transferred
};
```
`dischargeEpoch`/`outcome` are carried into the `baby_history.log` audit
record too (Section 6), so the permanent record always reflects the final
outcome, not just the last known state while active.
Age in days is **never persisted** — always derived at question time from
`admissionEpoch` and the current synced time (Section 3). If no synced time
is available *at that moment* (even if one was available at creation), the
wizard asks for it manually for that activation only; `admissionEpoch` is
left untouched so auto-derivation resumes once time sync is available again.

When a 4th profile is created, the slot with the lowest `seq` is evicted
(FIFO by creation order, not by last-used) — **except** the profile
currently associated with an active AIR or SKIN control loop, which is
never FIFO-evicted no matter how low its `seq` is. This active association
(`NS_BABY`/`activeSeq`, `0` = none) is persisted in NVS so it survives a
motherBoard reboot mid-treatment, is set when the wizard's "Apply" step
(Section 4) completes for either mode, and is cleared when control is
switched OFF. FIFO eviction picks the lowest `seq` among the slots that
are *not* `activeSeq`; since only one control loop can be active at a
time, at least 2 of the 3 slots are always eligible, so this never blocks
creating a 4th profile in normal operation. (Defensive fallback, not
expected to trigger: if no eligible slot exists, creation is refused with
an error rather than evicting the active profile.)

**Permanent history — LittleFS, `/littlefs/baby_history.log`:**
Append-only ring buffer, same record layout (~40 bytes/entry), capped at
**1,000 records (~40 KB)**. When full, the oldest record is overwritten.
Every profile that gets evicted from the 3 active NVS slots is appended
here first. Read-only from the UI (audit/history screen, not selectable
in the wizard — only the 3 active slots are selectable).

**Weight history (evolution tracking) — LittleFS, one file per active profile:**
Every non-`SKIP` weight answered in the wizard (Section 4, step 3) is
appended as a `(timestamp uint32, weightGrams uint16)` point (6 bytes) to
`/littlefs/weight_active/<seq>.bin`, a fixed-size circular file capped at
**1,000 points (~6 KB/baby)**. A new point is appended only if it differs
from the most recently stored point for that `seq` — repeated wizard
activations with an unchanged weight do not create duplicate points.

Unlike `weightGrams` in `BabyProfile` (last-known value only), this file
is what feeds the weight-evolution chart for the currently active baby.

**Archived weight history — LittleFS, `/littlefs/weight_archive/<seq>.bin`:**
When a profile is evicted from an active NVS slot, its
`weight_active/<seq>.bin` file is moved (not discarded) to
`weight_archive/<seq>.bin`, alongside its `baby_history.log` audit record.
The archive is a **shared 600 KB byte budget** across however many
archived babies fit — unlike the fixed-per-record audit log, each baby's
archived file is a different size depending on how long it was active. An
index (small LittleFS file or NVS blob) tracks total bytes used per
archived `seq`.

**Unified retention/eviction**: a baby's `baby_history.log` audit record
and its `weight_archive/<seq>.bin` file are always evicted **together**,
for the oldest `seq`, the moment *either* cap is hit (1,000 audit records
**or** 600 KB of archived weight files) — never independently. This
guarantees a historical baby entry always has both its summary and its
weight curve, or neither; there is no partial state where one exists
without the other.

---

## Section 2 — Communication Protocol (motherBoard ↔ HMI)

The motherBoard holds synced time (WiFi NTP already exists in
`DriveUpload.cpp:253-274`; GPRS time sync is new — see Sub-project C) and
now owns the NTE range calculation (Section 3), so it computes age and
range; the HMI only drives wizard screens and displays results.

This **replaces** the undocumented `babyWeightGrams`/`babyGestWeeks`/
`babyAgeDays` fields currently appended to the recurring `HMI,...` line
(`Display_HMI/src/tasks/CommTask.cpp:90-96`) — a fix that also resolves the
existing drift between `PROTOCOL.md` and the real 13-field message.

New messages (ASCII CSV, no CRC — validate field count before indexing,
per project convention):

| Message | Direction | Fields |
|---|---|---|
| `HMI,PROFILE_LIST_REQ` | HMI → MB | — |
| `CTRL,PROFILE_LIST,<n>,{<seq>,<name>,<gestWeeks>,<weightGrams>}×n` | MB → HMI | `n` = 0-3 |
| `HMI,PROFILE_NEW,<name>,<gestWeeks>` | HMI → MB | new baby |
| `HMI,PROFILE_SELECT,<seq>` | HMI → MB | existing baby |
| `HMI,PROFILE_WEIGHT,<seq>,<grams\|SKIP>` | HMI → MB | |
| `CTRL,PROFILE_RANGE,<seq>,<ageKnown>,<ageDays>,<lo>,<hi>,<mid>,<estimated>` | MB → HMI | `ageKnown=0` ⇒ HMI must ask |
| `HMI,PROFILE_AGE_MANUAL,<seq>,<ageDays>` | HMI → MB | only if `ageKnown=0` above; MB recomputes and resends `CTRL,PROFILE_RANGE` |

`PROTOCOL.md` must be updated in the same change (documenting these plus
correcting the existing field-count drift).

**Cloud telemetry (ThingsBoard) — intentional, not a leak.** The legacy
fields already republished today (`weightGrams`, `gestWeeks`, age) keep
being published from `motherBoard/src/tasks/GPRS.cpp` /
`Wifi_OTA.cpp`, now sourced from `modules/baby_profile/`. This change
**adds `name` and `outcome`/`dischargeEpoch` to that same published set**
— explicitly requested so the Ministry of Health has access to this data
via ThingsBoard. This is a deliberate policy decision, not an oversight:
it is called out here so the implementation doesn't have to guess whether
including the baby's name and clinical outcome in cloud telemetry was
intended.

---

## Section 3 — Shared NTE Calculation Engine

`autoair_calculate_setpoint()` (`Display_HMI/src/tasks/UITask.cpp:901-1060+`)
is extracted to `Firmware/shared/include/nte_table.h` /
`shared/src/nte_table.cpp` as the single source of truth:

```cpp
struct NteRange { float lo, hi, mid; bool estimated; };
NteRange calculateNteRange(uint16_t weightGrams, uint8_t gestWeeks, uint16_t ageDays);
```

Used by:
- **motherBoard**: computes `CTRL,PROFILE_RANGE` (this feature), and later
  the SKIN-mode clamp/alarm guardrail (Sub-project B).
- **Display_HMI**: no longer computes locally — only renders what the
  motherBoard returns. (Keeps a single clinical table instead of two
  copies that could silently drift apart.)

If weight is `SKIP` (0), the range cannot be computed with confidence —
`CTRL,PROFILE_RANGE` returns `estimated=1` with `lo=hi=mid=-1`, meaning: no
auto range, AIR starts in plain manual mode, and SKIN mode stays blocked
until a future activation provides a weight (same blocking pattern already
used today for an invalid skin probe, `UITask.cpp:2074-2098`).

---

## Section 4 — HMI Wizard Flow

State machine, replacing the current AUTO AIR popup:

1. **List** — `HMI,PROFILE_LIST_REQ` → `CTRL,PROFILE_LIST`. Shows up to 3
   cards (name, gestational age, last known weight) + "New baby" button.
   Empty list or motherBoard timeout → skip straight to "New baby" with a
   notice (fail-safe: never blocks indefinitely on a missing response).
2. **New baby**: name (on-screen keyboard, comma key disabled) + gestational
   age (weeks stepper) → `HMI,PROFILE_NEW`. **Existing baby**: tap a card →
   `HMI,PROFILE_SELECT`.
3. **Weight**: grams stepper + SKIP button → `HMI,PROFILE_WEIGHT`.
4. Wait for `CTRL,PROFILE_RANGE`. If `ageKnown=0` → **Age in days** screen
   (stepper) → `HMI,PROFILE_AGE_MANUAL` → wait for the recomputed range.
5. **Summary**: shows `[lo–hi]` and midpoint.
   - With weight: "Apply" — AIR starts at the midpoint (still adjustable
     with ±0.1°C arrows afterwards, as today); SKIN starts fixed at
     **36.5°C, no manual arrows** (locked clinical standard), with
     `[lo,hi]` becoming the guardrail consumed later by Sub-project B.
   - Without weight (SKIP): forces plain manual AIR, no auto range, SKIN
     stays blocked (Section 3).
6. Closing the wizard before completing (back button) leaves the switch
   OFF — control is never activated without minimum data.
7. A critical alarm arriving while the wizard is open closes it and yields
   the screen to the alarm (safety takes priority over data entry).

---

## Section 5 — Edge Cases & Testing

- Duplicate names are allowed; cards disambiguate by admission date when
  known, "no date" otherwise.
- motherBoard reboot mid-wizard: nothing is persisted until
  `PROFILE_WEIGHT` is received, so a reboot mid-flow only loses the
  in-progress wizard, never corrupts a slot.
- Malformed/truncated protocol line during the wizard: silent discard +
  error log (per `security.md`); the HMI treats a missing response as a
  timeout, never as partial data.
- Wizard and new alarm text must go through the existing translation
  system (avoids reintroducing the language-desync bug in
  `known_issues.md` #3).
- `shared/nte_table.h`, the FIFO-by-`seq` eviction logic, the weight-point
  dedup rule (skip if unchanged from the last stored point), and the
  unified audit+weight-archive eviction trigger (either cap hit → evict
  oldest `seq` from both) are pure, hardware-free logic → new Unity tests
  in motherBoard's `[env:native]` (mandatory per `testing.md` when
  extending that coverage). Same for `PROFILE_*` message
  parsing/validation (field-count and numeric validation before indexing,
  per `security.md`).
- NVS/LittleFS persistence and the LVGL wizard itself: manual verification
  on real hardware — new/existing baby activation for AIR and SKIN, SKIP
  weight, no time sync (manual age), FIFO eviction on a 4th profile,
  history log after a rotation, weight chart for an active baby, archived
  weight file surviving eviction, unified eviction at either cap,
  cancelling the wizard mid-flow, and a serial disconnect during the flow.

---

## Section 6 — Discharge, Outcome, and the "Babies" History Screen

**Explicit discharge** (new, distinct from the FIFO-by-`seq` fallback in
Section 1): `HMI,PROFILE_DISCHARGE,<seq>,<outcome>` (outcome: 0=Unknown,
1=Survived, 2=Deceased, 3=Transferred). On receipt, motherBoard:
1. Sets `dischargeEpoch` (current synced time, or 0 if unavailable) and
   `outcome` on the profile.
2. Immediately runs the same archival routine used by FIFO eviction —
   appends to `baby_history.log`, moves `weight_active/<seq>.bin` to
   `weight_archive/<seq>.bin` (subject to the same unified 600 KB/1,000-
   record retention rule from Section 1) — and frees the NVS slot for
   reuse right away, instead of waiting for a 4th profile to force it out.

Discharge is **independent of AIR/SKIN control state** — discharging the
baby currently driving control does not turn control off; the two are
unrelated actions. A profile that reaches the FIFO fallback without ever
being explicitly discharged keeps `outcome=Unknown, dischargeEpoch=0` in
its permanent record — this is expected, not an error.

**New top-bar "Babies" button** opens a history screen:
- Active profiles (up to 3) first, each with a "Discharge" action (confirm
  + pick outcome) and tap-to-view-chart.
- Archived profiles below, paginated (`HMI,PROFILE_HISTORY_REQ,<page>`,
  10 entries/page, → `CTRL,PROFILE_HISTORY,<page>,<totalCount>,<n>,
  {<seq>,<name>,<gestWeeks>,<lastWeightGrams>,<admissionEpoch>,
  <dischargeEpoch>,<outcome>}×n`), each tap-to-view-chart too (the archive
  retention rule guarantees a chart exists whenever the audit entry does).

**Weight evolution chart**: `HMI,WEIGHT_HISTORY_REQ,<seq>` →
`CTRL,WEIGHT_HISTORY,<seq>,<n>,{<dayOffset>,<weightGrams>}×n`, rendered
with an LVGL `lv_chart`, X-axis = day of life since admission, Y-axis =
weight. To keep the response to a single bounded protocol line regardless
of how much history exists, motherBoard caps `n` at **50** — if more than
50 points are stored for that `seq`, it downsamples to 50 evenly-spaced
points rather than chunking the transfer. (Implementation detail to
confirm against the real serial RX buffer size, same
verify-then-fallback posture as the shared-library question in the
OpenSpec design.)

---

## Related sub-projects (not in this spec)

- **B — SKIN-mode NTE safety guardrail**: fixed 36.5°C skin setpoint,
  clamp the air setpoint at the NTE range limit when the PID needs to go
  beyond it to hold 36.5°C, and a new `SKIN_SERVO_RANGE_LIMIT_ALARM`
  (ID 11, Medical/Critical, documented in `docs/alarms.md`) alongside the
  existing skin-deviation `TEMPERATURE_ALARM`. Depends on Section 3's
  shared `nte_table.h` and the active profile from this spec.
- **C — Unified time service**: generalize the WiFi NTP sync that exists
  today only inside `DriveUpload.cpp` into a reusable system service, and
  add GPRS-based time sync (AT+CCLK or NTP over the PDP context — does not
  exist today) as a second source before falling back to manual age entry.
  Needed for `admissionEpoch`/auto-age in Section 1 to work without WiFi.
