## ADDED Requirements

### Requirement: Up to 3 active baby profiles in NVS
`motherBoard` SHALL persist up to 3 active `BabyProfile` records in NVS
(`Preferences`, namespace `NS_BABY`), each with `slotUsed`, monotonic `seq`,
`name[24]` (no commas), `gestWeeks`, `weightGrams` (0 = never provided),
`admissionEpoch` (0 = unknown), `dischargeEpoch` (0 = not discharged, see
below), and `outcome` (0 = Unknown, see below).

#### Scenario: Creating a profile while a free slot exists
- **WHEN** fewer than 3 active profiles exist and a new profile is created
- **THEN** it is written to a free NVS slot with `slotUsed=true` and a new
  `seq` value strictly greater than any previously issued `seq`
- (Verified via motherBoard `[env:native]`, `pio test -e native`.)

### Requirement: Monotonic seq counter survives reboot
The `seq` counter SHALL be persisted in NVS (its own key in `NS_BABY`,
independent of the 3 profile slots) and SHALL be incremented and saved
before a new profile is marked `slotUsed`, so that `seq` values are never
reused across a motherBoard reboot.

#### Scenario: seq is not reused after a reboot
- **WHEN** the motherBoard reboots after creating profiles with `seq` values
  up to N, and a new profile is then created
- **THEN** the new profile's `seq` is strictly greater than N, never a
  reused or repeated value
- (Verified via motherBoard `[env:native]` for the counter logic; the
  reboot itself is manual verification on real hardware.)

### Requirement: Active-control profile is protected from FIFO eviction via activeSeq
`motherBoard` SHALL persist a `NS_BABY/activeSeq` NVS key (`uint32_t`,
`0` = no active association), independent of the 3 profile slots and the
`nextSeq` counter, and SHALL survive a motherBoard reboot. `activeSeq`
SHALL be set to the `seq` of the profile associated with the current wizard
flow the moment AIR or SKIN control actually turns on following that
flow's completed Apply step, and SHALL be cleared back to `0` the moment
that control mode (AIR or SKIN, whichever was driving it) is switched off.

#### Scenario: activeSeq is set when AIR control turns on after the wizard
- **WHEN** AIR control turns on immediately after the wizard's Apply step
  completes for a profile with a given `seq`
- **THEN** `NS_BABY/activeSeq` is set to that `seq`
- (Verified via motherBoard `[env:native]` for the pure set logic; the
  end-to-end trigger from the actual AIR-on command is manual verification
  on real hardware.)

#### Scenario: activeSeq is set when SKIN control turns on after the wizard
- **WHEN** SKIN control turns on immediately after the wizard's Apply step
  completes for a profile with a given `seq`
- **THEN** `NS_BABY/activeSeq` is set to that `seq`
- (Verified via motherBoard `[env:native]` for the pure set logic; the
  end-to-end trigger is manual verification on real hardware.)

#### Scenario: activeSeq is cleared when control is switched off
- **WHEN** the AIR or SKIN control mode currently associated with
  `activeSeq` is switched off
- **THEN** `NS_BABY/activeSeq` is cleared back to `0`
- (Verified via motherBoard `[env:native]` for the pure clear logic; the
  end-to-end trigger from the actual control-off command is manual
  verification on real hardware.)

#### Scenario: activeSeq survives a motherBoard reboot
- **WHEN** the motherBoard reboots while `activeSeq` is set to a non-zero
  value
- **THEN** `activeSeq` still holds that same value after the reboot,
  continuing to protect that profile from FIFO eviction
- (Manual verification on real hardware — reboot persistence is not
  reproducible in `[env:native]`.)

### Requirement: FIFO eviction by creation order on a 4th profile, excluding the active profile
The slot holding the **lowest `seq` among slots that are not `activeSeq`** SHALL be evicted (FIFO by creation order, never by last-used order) when a 4th profile is created while all 3 slots are `slotUsed` (see "Active-control profile is protected from FIFO eviction" below for `activeSeq`).
Since only one AIR or
SKIN control loop can be active at a time, at least 2 of the 3 slots are
always eligible for eviction in normal operation. As a defensive fallback
(not expected to trigger in normal operation), if no eligible (non-active)
slot exists, the 4th-profile creation SHALL be rejected with an error
instead of evicting the active profile.

#### Scenario: 4th profile evicts the oldest-created slot when none is active
- **WHEN** all 3 slots are occupied (seqs S1 < S2 < S3), none of them is
  `activeSeq`, and a 4th profile is created
- **THEN** the slot holding `seq=S1` is evicted and replaced, regardless of
  which slot was most recently selected/used
- (Verified via motherBoard `[env:native]`.)

#### Scenario: 4th profile eviction skips the active slot even if it holds the lowest seq
- **WHEN** all 3 slots are occupied (seqs S1 < S2 < S3), `activeSeq == S1`,
  and a 4th profile is created
- **THEN** the slot holding `seq=S2` (the lowest **eligible** seq) is
  evicted and replaced; the slot holding `seq=S1` is never evicted
- (Verified via motherBoard `[env:native]`.)

#### Scenario: Defensive fallback — creation is rejected if no eligible slot exists
- **WHEN** all 3 slots are occupied and, due to an inconsistent state, no
  slot is eligible for eviction (none can be identified as safely
  non-active)
- **THEN** the 4th-profile creation SHALL be rejected with an error and no
  slot SHALL be evicted or overwritten
- (Verified via motherBoard `[env:native]` for the pure decision logic;
  this state is not expected to occur in normal operation since only one
  control loop can be active at a time.)

### Requirement: Evicted profiles are appended to permanent LittleFS history
Before an active-slot profile is overwritten by eviction, its record SHALL
be appended to `/littlefs/baby_history.log`, a fixed-size circular file of
1,000 ~41-byte record slots (40-byte record + 1-byte `valid` flag, see
"Unified eviction of the oldest audit record and its archived weight
history" below) with an 8-byte header (`writeIndex`, `count`). The history
log is read-only from the UI (audit/history screen) and is not selectable
in the wizard. If the outgoing profile has a `/littlefs/weight_active/<seq>.bin`
file, it SHALL be moved (not discarded) to `/littlefs/weight_archive/<seq>.bin`
as part of the same eviction operation (see the weight-history requirements
below).

#### Scenario: Eviction appends the outgoing profile to history
- **WHEN** a profile is evicted from an active NVS slot (per the FIFO rule
  above)
- **THEN** its record is present in `baby_history.log` at the position
  indicated by the header's `writeIndex` prior to the write (with
  `valid=true`), and `count`/`writeIndex` are updated accordingly
- (Verified via motherBoard `[env:native]` for the pure record-encoding and
  cursor-advance logic; actual LittleFS file I/O is manual verification on
  real hardware.)

#### Scenario: Eviction moves the outgoing profile's active weight file to the archive
- **WHEN** a profile with an existing `/littlefs/weight_active/<seq>.bin`
  file is evicted from an active NVS slot
- **THEN** the file is moved to `/littlefs/weight_archive/<seq>.bin` (same
  content, new path) and no longer exists at the `weight_active/` path
- (Verified via motherBoard `[env:native]` for the pure decision of
  which file to move and the archive byte-budget bookkeeping; the actual
  LittleFS move is manual verification on real hardware.)

#### Scenario: Eviction of a profile that never recorded a weight
- **WHEN** a profile that never received a non-`SKIP` `PROFILE_WEIGHT` (no
  `weight_active/<seq>.bin` file was ever created) is evicted
- **THEN** the audit record is still appended to `baby_history.log` as
  usual, and no file-move or archive-budget change occurs since there was
  no active weight file to move
- (Verified via motherBoard `[env:native]`.)

### Requirement: History log wraps at 1,000 records without file rewrite
Once the history log holds 1,000 records, appending a new record SHALL
overwrite the oldest record in place (via the circular `writeIndex`)
without rewriting any other part of the file. This overwrite is one of the
two triggers of the unified eviction rule below: when it happens, the
outgoing record's paired `weight_archive/<seq>.bin` (if any) SHALL be
deleted in the same operation.

#### Scenario: 1,001st history record overwrites the oldest
- **WHEN** the history log already holds exactly 1,000 records and a new
  record is appended
- **THEN** the record that was previously at `writeIndex` (the oldest) is
  overwritten, `count` stays at 1,000, and all other 999 records are
  unchanged
- (Verified via motherBoard `[env:native]` for the pure cursor logic;
  confirmed with a real rotation on hardware — manual.)

### Requirement: A record-layout change resets stored profiles instead of misreading them
Neither the active-slot NVS blobs nor the audit log carries a version field,
so a firmware whose `BabyProfile` layout or `BABY_HISTORY_RECORD_SIZE` differs
from the one that wrote the data MUST NOT reinterpret those bytes: decoding a
clinical record at the wrong stride would invent patient data. At init, a slot
blob whose stored size differs from `sizeof(BabyProfile)` SHALL be reset to
empty, and an audit log whose file size differs from
`header + min(count, cap) * BABY_HISTORY_RECORD_SIZE` SHALL be deleted along
with the weight archives it is the only index for. Both SHALL be logged at
warning level, because losing an active profile is visible to the ward and
must not look like a glitch.

#### Scenario: Audit log written by an older record layout
- **WHEN** `babyStore_init()` finds `/baby_history.log` whose size does not
  match its own header count times the current record size
- **THEN** the log is deleted, `weight_archive/` is emptied, a warning naming
  both the actual and the expected size is logged, and the store starts with
  an empty history rather than decoding the old bytes
- (Manual verification on hardware: flash a build with the previous record
  size, create and discharge a baby, then flash the new build.)

#### Scenario: Active slot blob written by an older struct layout
- **WHEN** a slot blob's stored size differs from `sizeof(BabyProfile)`
- **THEN** that slot starts empty with a warning, and the other slots are
  evaluated independently
- (Verified on hardware alongside the scenario above — manual.)

### Requirement: Non-SKIP weight answers append a deduplicated point to the active weight-history file
A non-`SKIP` weight received via `PROFILE_WEIGHT` (see `baby-profile-protocol`) SHALL be appended as a `(timestamp uint32, weightGrams uint16)` point
(6 bytes) to `/littlefs/weight_active/<seq>.bin` for that profile's `seq`,
**unless** the new value is identical to the most recently stored point for
that `seq`, in which case no new point is appended (dedup is a plain value
comparison against the last stored point only — no time-based logic).

#### Scenario: A changed weight is appended as a new point
- **WHEN** a non-`SKIP` weight is received for `seq` and it differs from
  the most recently stored point for that `seq` (or no point exists yet)
- **THEN** a new `(timestamp, weightGrams)` point is appended to
  `weight_active/<seq>.bin`
- (Verified via motherBoard `[env:native]` for the pure dedup-compare and
  point-encoding logic; actual LittleFS file I/O is manual verification on
  real hardware.)

#### Scenario: An unchanged weight does not create a duplicate point
- **WHEN** a non-`SKIP` weight is received for `seq` and it is identical to
  the most recently stored point's `weightGrams` for that `seq`
- **THEN** no new point is appended; the file's point count is unchanged
- (Verified via motherBoard `[env:native]`.)

### Requirement: Active weight-history file caps at 1,000 points via circular overwrite
`/littlefs/weight_active/<seq>.bin` SHALL be a fixed-size circular file
capped at 1,000 points (~6 KB/baby). Once full, appending a new point SHALL
overwrite the oldest point in place, following the same circular-cursor
pattern as `baby_history.log` (no file rewrite on rotation).

#### Scenario: 1,001st weight point overwrites the oldest
- **WHEN** a baby's `weight_active/<seq>.bin` already holds exactly 1,000
  points and a new, non-duplicate point is appended
- **THEN** the oldest point is overwritten in place and the file still
  holds exactly 1,000 points
- (Verified via motherBoard `[env:native]` for the pure cursor logic;
  confirmed with a real rotation on hardware — manual, may use a reduced
  cap for practicality.)

### Requirement: Archived weight history is limited to a shared 600 KB byte budget
`/littlefs/weight_archive/` SHALL be limited to a shared 600 KB total byte
budget across all archived babies' `<seq>.bin` files (variable size per
baby, not a fixed count of archived babies). A byte-usage index (per
archived `seq`) SHALL track total bytes used and be rebuildable from a
directory scan of `weight_archive/` (defensive recovery if the index and
on-disk state ever drift, e.g. after a crash between a file delete and an
index update).

#### Scenario: Archiving a file within budget updates the index
- **WHEN** a profile is evicted and its `weight_active/<seq>.bin` file is
  moved to `weight_archive/<seq>.bin`, and the resulting total archive
  usage is still within 600 KB
- **THEN** the move succeeds and the byte-usage index reflects the new
  file's size for that `seq`
- (Verified via motherBoard `[env:native]` for the pure budget-accounting
  logic; actual LittleFS I/O is manual verification.)

#### Scenario: Index is rebuilt from a directory scan after a mismatch
- **WHEN** the persisted byte-usage index does not match the real on-disk
  contents of `weight_archive/` (e.g. after an unclean shutdown)
- **THEN** the index is rebuilt by summing the real file sizes found in
  `weight_archive/`, keyed by each file's `seq`
- (Manual verification on real hardware — requires simulating an unclean
  shutdown, not reproducible in `[env:native]`.)

### Requirement: Unified eviction of the oldest audit record and its archived weight history
A baby's `baby_history.log` audit record and `weight_archive/<seq>.bin` file SHALL always be evicted **together**, for the **oldest `seq`** among
archived babies, the moment **either** the 1,000-record audit cap or the
600 KB weight-archive budget is reached — never independently.

When the audit cap is the trigger, the audit log's natural circular
overwrite (above) already removes the oldest record; the paired
`weight_archive/<seq>.bin` (if any) is deleted in the same operation. When
the weight-archive budget is the trigger (reached before the audit cap),
the oldest archived baby's `weight_archive/<seq>.bin` is deleted
immediately and its `baby_history.log` record is marked `valid=false`
immediately, even though the circular buffer has not yet reached that
record's slot. A record with `valid=false` SHALL be treated as absent by
the audit/history UI read path.

#### Scenario: Audit cap reached first evicts both together
- **WHEN** the 1,000-record audit cap is reached and the oldest record is
  about to be overwritten by the circular buffer
- **THEN** the oldest `seq`'s `weight_archive/<seq>.bin` (if it exists) is
  deleted in the same eviction operation, and the byte-usage index is
  updated accordingly
- (Verified via motherBoard `[env:native]` for the pure trigger/decision
  logic; actual LittleFS deletion is manual verification.)

#### Scenario: Weight-archive budget reached first evicts both together
- **WHEN** the 600 KB weight-archive budget is reached before the audit
  log's 1,000-record cap, and the oldest archived `seq` must be evicted to
  make room
- **THEN** that `seq`'s `weight_archive/<seq>.bin` is deleted, and its
  `baby_history.log` record is marked `valid=false`, even though its
  circular-buffer slot has not yet been naturally overwritten
- (Verified via motherBoard `[env:native]` for the pure trigger/decision
  logic; actual LittleFS deletion is manual verification.)

#### Scenario: No historical entry ever exists with only one of the two parts
- **WHEN** the audit/history screen reads `baby_history.log` after any
  eviction (by either trigger)
- **THEN** every `valid=true` record's `seq` has a corresponding
  `weight_archive/<seq>.bin` file (if that baby ever recorded a weight),
  and no `weight_archive/<seq>.bin` file exists for a `seq` whose audit
  record is `valid=false` or absent
- (Manual verification on real hardware — a cross-file consistency
  guarantee, not practical to assert end-to-end in `[env:native]` without a
  real filesystem.)

### Requirement: Age in days is derived, never persisted
`admissionEpoch` (unix time at profile creation, 0 = unknown) SHALL be the
only time-related field persisted per profile. Age in days SHALL always be
computed at question time from `admissionEpoch` and the current synced
time, never stored.

#### Scenario: Age is recomputed after a reboot using stored admissionEpoch
- **WHEN** a profile with a known `admissionEpoch` is read after a
  motherBoard reboot, with synced time available
- **THEN** age in days is derived fresh from `admissionEpoch` and the
  current synced time; no persisted age value is read
- (Verified via motherBoard `[env:native]` for the derivation function;
  reboot behavior confirmed manually on hardware.)

#### Scenario: No synced time available at question time
- **WHEN** a profile's `admissionEpoch` is known but no synced time is
  available at the moment age is needed
- **THEN** age is reported as unknown (`ageKnown=0` in `CTRL,PROFILE_RANGE`,
  see `baby-profile-protocol`) for that occasion only; `admissionEpoch`
  itself is left untouched so auto-derivation resumes once time sync
  returns
- (Verified via motherBoard `[env:native]` for the derivation function's
  unknown-time branch.)

### Requirement: Active profiles record discharge epoch and outcome
Each `BabyProfile` record (active NVS slot and `baby_history.log` audit record) SHALL additionally carry `dischargeEpoch` (uint32, unix time of
explicit discharge, `0` = not discharged) and `outcome` (uint8, `0`=Unknown,
`1`=Survived, `2`=Deceased, `3`=Transferred). Both fields default to `0`
(not discharged / Unknown) for a newly created profile and are only ever
set together, by explicit discharge (see below).

#### Scenario: A newly created profile has no discharge data
- **WHEN** a new profile is created via `PROFILE_NEW`/`PROFILE_SELECT` +
  `PROFILE_WEIGHT`
- **THEN** its `dischargeEpoch` is `0` and its `outcome` is `0` (Unknown)
- (Verified via motherBoard `[env:native]`.)

### Requirement: Explicit discharge archives the profile immediately via the shared archive routine
On receiving a discharge request for an active `seq` (see `PROFILE_DISCHARGE` in `baby-profile-protocol`), motherBoard SHALL set `dischargeEpoch` to the
current synced time (`0` if unavailable) and `outcome` to the requested
value on that profile, then immediately run the same internal archive
routine used by FIFO eviction (append to `baby_history.log`, move
`weight_active/<seq>.bin` to `weight_archive/<seq>.bin` if present, apply
the unified 600 KB/1,000-record retention rule below), and free the active
NVS slot right away — **without** waiting for a 4th profile to force
eviction. Explicit discharge SHALL NOT depend on or alter the AIR/SKIN
control state in any way.

#### Scenario: Explicit discharge frees the slot immediately with fewer than 3 active profiles
- **WHEN** an active profile is explicitly discharged while fewer than 3
  active slots are in use
- **THEN** the profile is archived (audit record appended, weight file
  moved if present) and its slot is immediately marked free, without
  waiting for a 4th profile to be created
- (Verified via motherBoard `[env:native]` for the pure archive-and-free
  decision logic; actual NVS/LittleFS I/O is manual verification on real
  hardware.)

#### Scenario: Discharge sets outcome and discharge epoch on the archived record
- **WHEN** an active profile is explicitly discharged with a given
  `outcome` value and synced time is available
- **THEN** the resulting `baby_history.log` record for that `seq` has
  `dischargeEpoch` set to the current synced time and `outcome` set to the
  requested value
- (Verified via motherBoard `[env:native]`.)

#### Scenario: Discharge does not change AIR/SKIN control state
- **WHEN** a profile currently driving active AIR or SKIN control is
  explicitly discharged
- **THEN** AIR/SKIN control continues running unchanged; discharge only
  affects profile storage
- (Manual verification on real hardware — cross-subsystem behavior not
  reproducible in `[env:native]`.)

### Requirement: FIFO-evicted profiles without a prior explicit discharge keep Unknown outcome
A profile that reaches FIFO eviction without ever having received an explicit discharge SHALL be archived with `dischargeEpoch=0` and `outcome=0` (Unknown) — this is an expected, valid terminal state for a permanent
history record, not an error condition (per the "FIFO eviction by creation
order" requirement above).

#### Scenario: FIFO-evicted profile is archived with Unknown outcome
- **WHEN** a profile that was never explicitly discharged is evicted by the
  FIFO rule (4th profile created while all 3 slots are full)
- **THEN** its `baby_history.log` record has `dischargeEpoch=0` and
  `outcome=0` (Unknown), and this is not treated as an error or logged as a
  failure
- (Verified via motherBoard `[env:native]`.)

### Requirement: Weight-history responses are downsampled to at most 50 points
If a baby's stored weight-history points (from `weight_active/<seq>.bin` or `weight_archive/<seq>.bin`) exceed 50, they SHALL be downsampled to exactly
50 evenly-spaced points via deterministic index selection (`idx(i) =
round(i * (count-1) / (49))` for `i` in `[0, 50)`) rather than truncated or
chunked across multiple lines, when building the point list for
`CTRL,WEIGHT_HISTORY` (see `baby-profile-protocol`). If 50 or fewer points
are stored, all of them SHALL be returned unchanged.

#### Scenario: Fewer than 50 stored points are returned unchanged
- **WHEN** a baby's weight-history file holds 30 points and its history is
  requested
- **THEN** all 30 points are returned, in their original order, with no
  downsampling applied
- (Verified via motherBoard `[env:native]`.)

#### Scenario: More than 50 stored points are downsampled to exactly 50
- **WHEN** a baby's weight-history file holds 400 points and its history is
  requested
- **THEN** exactly 50 points are returned, selected via the deterministic
  evenly-spaced index formula above, preserving chronological order
- (Verified via motherBoard `[env:native]`.)

#### Scenario: Downsampling is deterministic for identical input
- **WHEN** the downsampling function is called twice with the same stored
  point array
- **THEN** both calls return an identical sequence of 50 points
- (Verified via motherBoard `[env:native]`.)

### Requirement: Nothing is persisted before PROFILE_WEIGHT is received
No NVS slot or history record SHALL be created or modified for an
in-progress wizard until a `PROFILE_WEIGHT` message is received for that
flow (per `baby-profile-protocol`). A motherBoard reboot mid-wizard SHALL
lose only the in-progress wizard state, never corrupt an existing slot.

#### Scenario: Reboot before PROFILE_WEIGHT leaves storage untouched
- **WHEN** the motherBoard reboots after `PROFILE_NEW`/`PROFILE_SELECT` but
  before `PROFILE_WEIGHT` is received for that wizard flow
- **THEN** the 3 active NVS slots and the history log are unchanged from
  their state before the wizard started
- (Manual verification on real hardware — reboot timing during an
  in-progress protocol exchange is not reproducible in `[env:native]`.)
