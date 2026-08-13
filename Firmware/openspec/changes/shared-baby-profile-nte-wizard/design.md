## Context

Full behavioral spec is already approved and lives at
`Firmware/docs/superpowers/specs/2026-08-11-baby-profile-temp-control-wizard-design.md`
(data model, protocol, wizard flow, edge cases, testing strategy). This
document does not restate that content — it covers the technical decisions
needed to execute it as an OpenSpec change across three independently
buildable PlatformIO projects (`motherBoard`, `Display_HMI`, `shared`),
where the project convention (`.claude/rules/commits.md`) requires one
board per commit except for genuinely shared code.

## Goals / Non-Goals

**Goals:**
- Decide how the 5 capabilities (`nte-calculation-engine`,
  `baby-profile-storage`, `baby-profile-protocol`,
  `temp-control-activation-wizard`, `baby-history-viewer`) map to buildable
  phases that can each land as an atomic, board-scoped commit.
- Resolve the mechanical gaps the design doc leaves implicit: how `shared/`
  goes from header-only to header+source without breaking either board's
  PlatformIO build, how the FIFO `seq` counter survives a reboot, and how
  the LittleFS "ring buffer" is actually laid out on a filesystem that has
  no native circular-append primitive.
- Keep every intermediate phase compiling green on both boards (`pio run
  -e main` on each), even before the whole feature is done.

**Non-Goals:**
- Sub-project B (SKIN NTE guardrail/alarm) and C (unified time service +
  GPRS time sync) — not designed here, only referenced where this change
  must leave a clean extension point for them (see the source design doc's
  "Related sub-projects").
- Backward compatibility with the legacy `babyWeightGrams`/`babyGestWeeks`/
  `babyAgeDays` fields — this is a matched-pair firmware (both boards are
  always flashed together by the same person), so no dual-protocol shim is
  designed; both boards move to the new protocol in the same release.

## Decisions

1. **Capability-to-phase mapping, ordered shared -> motherBoard -> Display_HMI.**
   `nte-calculation-engine` (shared) lands first since both boards depend on
   it. `baby-profile-storage` + `baby-profile-protocol` (motherBoard) land
   second — motherBoard can implement, store, and respond to `PROFILE_*`
   messages before any HMI code sends them, keeping `pio run -e main` green
   on motherBoard throughout. `temp-control-activation-wizard`
   (Display_HMI) lands next, replacing the AUTO AIR popup and switching to
   the new protocol only once motherBoard already speaks it.
   `baby-history-viewer` (Display_HMI) lands last, since its "Babies"
   button/screens are additive UI that does not gate AIR/SKIN activation —
   it can be its own commit even though it ships in the same `hmi` phase
   window as the wizard, because it depends on the discharge/history/
   weight-history protocol messages (also motherBoard-phase) but not on the
   wizard's own code. This preserves one-board-per-commit for the last
   phases; the first phase is genuinely `shared/`-scoped per
   `.claude/rules/embedded-shared.md`.
   *Alternative considered*: a single cross-board commit — rejected, it
   violates `.claude/rules/commits.md` for no benefit (the phases are
   independently testable).

2. **`shared/` gains its first `.cpp` file; no `library.json` change
   expected.** `shared/library.json` already declares `shared/` as a
   PlatformIO library (name/version/frameworks/platforms) with the default
   `include/` + `src/` layout, and both boards reference it via
   `lib_extra_dirs = ../shared` with PlatformIO's default chain-mode
   Library Dependency Finder. Adding `shared/src/nte_table.cpp` alongside
   the existing `shared/include/*.h` should be picked up automatically by
   both boards' builds and by motherBoard's native LDF once
   `nte_table.h` is `#include`d from test/production code — **verify this
   with a real `pio run -e main` on both boards and `pio test -e native`
   before assuming it**; if LDF does not pick it up, the fallback is an
   explicit `"build": {"srcDir": "src"}` in `library.json` (documented here
   so it's a one-line fix, not a surprise mid-implementation).
   *Alternative considered*: keep `calculateNteRange()` header-only
   (`inline`/`constexpr`-free templates in the `.h`) to avoid touching the
   library layout at all — rejected, the function has enough branching
   logic (table lookup + interpolation) that a `.cpp` is more maintainable
   and matches how `alarm_machine.cpp`/`pid_wrapper.cpp` are already
   structured as testable pure `.cpp` units.

3. **Monotonic `seq` counter persists across reboots as its own NVS key**
   (`NS_BABY`/`nextSeq`, `uint32_t`), incremented and saved every time a
   profile is created, never decremented on eviction. Without this, a
   reboot would reset `seq` to whatever the 3 in-memory slots already show,
   breaking FIFO ordering the first time two profiles share a `seq` after
   restart. This is the one storage detail the source design doc leaves
   implicit (Section 1 says `seq` is "monotonic," not where the next value
   lives).

4. **LittleFS history is a fixed-size, preallocated circular file with a
   header cursor**, not a variable-length append-and-truncate log. Layout:
   an 8-byte header (`uint32_t writeIndex`, `uint32_t count`) followed by
   1,000 fixed ~40-byte record slots, preallocated at first use. Appending
   a record writes to `writeIndex`, increments it mod 1000, and increments
   `count` up to 1000; once full, the write at `writeIndex` naturally
   overwrites the oldest record without ever rewriting the rest of the
   file. Reading for the audit/history screen walks `count` records
   starting from the oldest (`writeIndex - count`, wrapped).
   *Alternative considered*: true append (`O_APPEND`) with a periodic
   "drop oldest N records by rewriting the file" compaction — rejected,
   LittleFS has no in-place truncate-from-front, so this would mean
   rewriting up to ~40 KB on every rotation once full, which is both slower
   and a worse flash-wear pattern than fixed-slot overwrite.

5. **New motherBoard module: `modules/baby_profile/`** (storage: NVS slots
   + LittleFS audit history + LittleFS weight-evolution active/archive
   files + `seq`/eviction logic, all pure/testable where possible) kept
   separate from `modules/comm/` (protocol line parsing/building for
   `PROFILE_*`/`CTRL,PROFILE_*`) per
   `.claude/rules/embedded-motherboard.md`'s existing boundary ("no mixes
   sensor interpretation with protocol parsing in the same file" — same
   principle applied to storage vs. protocol here). `comm/` calls into
   `baby_profile/` and into the shared `nte_table.h`; it does not implement
   eviction or persistence itself.

6. **No CRC/versioning added to the new protocol messages.** Consistent
   with `.claude/rules/security.md`: the real protocol has no CRC by
   design, and adding one here would be an undocumented, localized
   deviation. Every new line follows the same field-count validation +
   silent discard convention already used elsewhere.

7. **Weight-evolution history uses one real LittleFS file per baby
   (`weight_active/<seq>.bin`, `weight_archive/<seq>.bin`), not a
   fixed-slot layout like the audit log (decision 4).** The archive is a
   shared 600 KB *byte budget* across a variable number of babies (each
   file's size depends on how long that baby was active, up to the
   1,000-point/6 KB active cap) — the source design doc explicitly rejects
   a fixed max-N-archived-babies approach in favor of this budget, which
   only makes sense with real, independently-sized files. Eviction from an
   active slot is then a plain LittleFS move (`weight_active/<seq>.bin` ->
   `weight_archive/<seq>.bin`), not a copy into a shared region. Files are
   keyed by the baby's permanent `seq`, never by the 0-2 active-slot index,
   because the slot index is immediately reusable by the next created baby
   while `seq` is not.
   *Alternative considered*: key weight files by active-slot index (0-2)
   instead of `seq` — rejected, a new baby could be created in a
   just-freed slot before the outgoing baby's archive move completes,
   causing the new baby's active weight file to collide with the old one's
   filename.

8. **Unified audit+weight-archive eviction is implemented with a per-record
   1-byte `valid` tombstone flag added to `baby_history.log`'s ~40-byte
   record layout (~41 bytes/record), not by shifting/compacting the
   circular buffer.** The two caps (1,000 audit records vs. 600 KB weight
   archive) can each be hit first depending on how much weight data
   accumulates before a baby is evicted from an active slot. When the
   audit log's own circular buffer (decision 4) naturally overwrites the
   oldest record because the 1,000-record cap is hit, that same oldest
   `seq`'s `weight_archive/<seq>.bin` is deleted in the same operation —
   no extra bookkeeping needed, the two are already in sync. When the
   600 KB weight-archive budget is hit *first* (a likely case, since babies
   with rich weight history consume archive budget faster than they fill
   1,000 audit slots), the oldest archived baby's `weight_archive/<seq>.bin`
   is deleted per the byte-budget index, and its `baby_history.log` record
   — which may not yet be at the circular buffer's natural overwrite
   position — is marked `valid=false` immediately instead. The
   audit/history UI read path skips `valid=false` records; `writeIndex`/
   `count` cursor mechanics from decision 4 are otherwise unchanged, so a
   tombstoned slot is still eventually physically overwritten once the
   cursor reaches it, which is harmless since it was already invalid.
   *Alternative considered*: shift/compact `baby_history.log` to physically
   remove the tombstoned record immediately — rejected for the same
   flash-wear/no-in-place-truncate-from-front reason decision 4 already
   rejects for the plain 1,000-record cap case.

9. **One internal `archiveProfile(BabyProfile, reason)` routine, called from
   two entry points: FIFO auto-evict (slot-full) and explicit discharge
   (`PROFILE_DISCHARGE`).** Both paths must do exactly the same three
   things — append the audit record to `baby_history.log`, move
   `weight_active/<seq>.bin` to `weight_archive/<seq>.bin` if it exists, and
   run the unified-eviction check from decision 8 (either cap hit evicts the
   oldest archived `seq`) — so they share one implementation instead of two
   copies that could silently drift (the same anti-duplication reasoning as
   decision 5's storage/protocol boundary). The two entry points differ only
   in *when* they run and what they set beforehand: FIFO auto-evict sets
   nothing new on the outgoing profile (it keeps whatever `dischargeEpoch`/
   `outcome` it already had — `0`/`Unknown` if never explicitly discharged,
   a valid and expected terminal state), while explicit discharge sets
   `dischargeEpoch` (current synced time, or `0` if unavailable) and
   `outcome` on the profile immediately before calling `archiveProfile()`,
   and additionally marks the slot free right away instead of waiting for a
   4th profile. Because the unified-eviction check in decision 8 runs after
   *every* call to `archiveProfile()` regardless of which entry point
   triggered it, a burst of explicit discharges can now trigger the 600 KB
   weight-archive-budget eviction just as easily as a burst of FIFO
   evictions could before — the check was already trigger-agnostic, so no
   new logic is needed there, only the second caller.
   *Alternative considered*: give explicit discharge its own separate
   archive/retention implementation since it is UI-triggered and
   FIFO-eviction is protocol-triggered — rejected, the actual archival
   side effects are identical and a second implementation is exactly the
   kind of drift decision 5 already exists to prevent.

10. **`outcome` is a plain `uint8_t` (0-3) defined only in motherBoard**
    (`modules/baby_profile/`), not promoted into `shared/` alongside
    `nte_table.h`. Unlike `calculateNteRange()` (decision 2), there is no
    calculation to keep in sync between boards — the HMI only needs to map
    the wire integer to a translated display label (`Unknown`/`Survived`/
    `Deceased`/`Transferred`) via the existing translation system, the same
    kind of local lookup it already does for other status codes. Promoting
    a 4-value enum to `shared/` for a lookup-only consumer would grow the
    shared library's surface for no drift-prevention benefit (`shared/` is
    reserved for genuinely shared *logic*, per decision 2's rationale).
    `PROTOCOL.md` is the single source of truth for the 0-3 mapping instead
    of a shared header.
    *Alternative considered*: a `shared/include/baby_outcome.h` enum —
    rejected as unnecessary indirection for 4 constants with no behavior
    attached.

11. **Weight-history downsampling to a maximum of 50 points is a pure,
    deterministic, evenly-spaced index-selection function**, living
    alongside the other pure `modules/baby_profile/` logic (decision 5):
    given `count` stored points and a target of `maxOut=50`, pick indices
    `idx(i) = round(i * (count - 1) / (maxOut - 1))` for `i` in
    `[0, maxOut)`; if `count <= maxOut`, return all points unchanged
    (no-op passthrough, never pads or duplicates). This keeps `n` in
    `CTRL,WEIGHT_HISTORY` bounded to 50 regardless of how much history a
    long-stay baby has accumulated (up to the 1,000-point active cap or
    however much survived archiving), so the response always fits one
    protocol line instead of needing multi-line chunking. Whether 50 points
    actually fits comfortably within the real serial RX line buffer on both
    boards is **not yet confirmed against hardware** — flagged the same way
    decision 2 flags the LDF question: verify with a real
    `HMI,WEIGHT_HISTORY_REQ` round-trip at max point count before assuming
    it, fallback (if it doesn't fit) is lowering `maxOut` or chunking the
    response, both cheap changes localized to this one function and its
    protocol builder.
    *Alternative considered*: send weight history across multiple
    `CTRL,WEIGHT_HISTORY` lines (one chunk per line) — rejected as the
    default design, since the whole point of downsampling is to avoid the
    added complexity of multi-line reassembly/ordering on the HMI side; only
    fall back to it if the single-line size is confirmed to be a real
    problem.

12. **`CTRL,PROFILE_HISTORY` pagination is stateless (page-sliced, not
    cursor-based) and lists archived profiles most-recently-archived-first.**
    Each `HMI,PROFILE_HISTORY_REQ,<page>` independently computes the current
    valid (non-tombstoned, decision 8) record count as `totalCount` and
    returns records `[page*10, page*10+10)` from that ordering — there is no
    server-side pagination cursor to keep in sync across requests. Most-
    recent-first means a nurse opening the history screen sees recently
    discharged/evicted babies on page 0 without paging through up to 990
    older entries first. Because pagination is stateless, a page requested
    after a concurrent eviction (FIFO or discharge, decision 9) can shift by
    at most one entry between the two requests — acceptable, since the
    screen already re-requests the current page on any local list-refreshing
    action, and this is the same trade-off already accepted for the
    3-active-slot list (`PROFILE_LIST`) reflecting live state rather than a
    frozen snapshot.
    *Alternative considered*: oldest-first ordering (matches the physical
    circular-buffer read order used internally for `baby_history.log`,
    decision 4) — rejected for the UX reason above; the internal read order
    and the externally-paginated order are allowed to differ since the
    pagination layer already has to skip tombstoned records regardless of
    direction.

13. **`NS_BABY/activeSeq` (uint32_t, `0` = none) is a fourth, independent NVS
    key** — alongside the 3 profile slots (decision 1's data model) and
    `nextSeq` (decision 3) — that records which `seq`, if any, is currently
    driving an active AIR or SKIN control loop. It is set the moment
    AIR/SKIN control actually turns on following a completed wizard Apply
    step, using the `seq` already tracked as "selected for the current
    wizard flow" (set by `PROFILE_NEW`/`PROFILE_SELECT`, per
    `baby-profile-protocol`) — this happens when motherBoard processes the
    pre-existing AIR/SKIN control-on command (already part of the protocol,
    unrelated to the new `PROFILE_*` messages and out of scope here). It is
    cleared back to `0` when motherBoard processes the corresponding
    control-off command for whichever mode was driving it. `activeSeq` is
    persisted synchronously (like `nextSeq`, decision 3) so it survives a
    reboot mid-treatment. The FIFO eviction rule (decision-free, directly
    from the source design doc's Section 1) now excludes the slot whose
    `seq == activeSeq` from the eviction candidates on a 4th-profile
    creation — it picks the lowest `seq` among the remaining slots. Since
    only one control loop can be active at a time, at least 2 of the 3
    slots are always eligible in normal operation. Defensive fallback (not
    expected to trigger): if `activeSeq` is set but, due to some
    inconsistency, no eligible slot exists, 4th-profile creation is
    rejected with an error rather than evicting the active profile — this
    is intentionally a hard rejection, not a silent override, because
    silently evicting the profile driving live control would be a
    clinical-data-loss regression, not a cosmetic one.
    *Alternative considered*: no protection at all, accepting that an
    active profile could in theory be FIFO-evicted if 3 other profiles are
    created faster than the active one is discharged — rejected as an
    unacceptable clinical-data-loss risk for a baby currently under active
    temperature control, raised explicitly during design review.

14. **Cloud telemetry to ThingsBoard including `name`/`outcome`/`dischargeEpoch`
    is a deliberate product decision, not a privacy oversight, and is
    documented here specifically so implementation does not "fix" it by
    omission.** `motherBoard/src/tasks/GPRS.cpp:800-806` and
    `motherBoard/src/tasks/Wifi_OTA.cpp:893-899` already publish
    `weightGrams`/`gestWeeks`/`ageDays` to ThingsBoard today (sourced, before
    this change, from the now-removed `hmi_cmd_msg.baby*` fields); this
    change re-sources those same three fields from `modules/baby_profile/`
    and **adds** `name`, `outcome`, and `dischargeEpoch` to the same
    published set on both the GPRS and WiFi paths, following the existing
    "publish once per pending-flag" pattern (`newBabyDataForTelemetry`)
    already in place in both files. The stated reason (from the user,
    product owner) is that the Ministry of Health has access to this data
    via ThingsBoard and needs the baby's name and clinical outcome for
    oversight/reporting. Because minimizing PII in telemetry is the
    general instinct for this kind of field, this decision is called out
    explicitly so a future implementer or reviewer does not quietly drop
    `name`/`outcome`/`dischargeEpoch` from the published set as an assumed
    privacy fix.
    *Alternative considered*: keep `name`/`outcome` motherBoard/HMI-only and
    publish only the legacy three fields to the cloud — rejected per the
    explicit Ministry-access requirement; this would otherwise have been
    this PM's default privacy-conservative recommendation absent that
    requirement.

## Risks / Trade-offs

- **[Risk]** LDF might not auto-discover `shared/src/nte_table.cpp` in one
  or both boards depending on PlatformIO version/cache state → **Mitigation**:
  decision 2's fallback (`"build": {"srcDir": "src"}`) is pre-agreed, and
  the shared-phase task list includes an explicit `pio run -e main` check
  on *both* boards, not just motherBoard, before moving to phase 2.
- **[Risk]** A motherBoard reboot between incrementing `nextSeq` in RAM and
  persisting it to NVS could in theory skip or (much less likely) reuse a
  `seq` value → **Mitigation**: `nextSeq` is written to NVS synchronously
  before the new `BabyProfile` slot is marked `slotUsed`, so a reboot mid-
  write loses at most the in-progress profile creation (consistent with the
  source design doc's existing "reboot mid-wizard" edge case — nothing is
  persisted until `PROFILE_WEIGHT`), never corrupts ordering of already
  -committed profiles.
- **[Risk]** Fixed 1,000-record circular file wastes space if the device
  never accumulates history (preallocated ~40 KB from day one) →
  **Mitigation**: acceptable, LittleFS partition already has multi-MB
  headroom (per the source design doc) and this is far simpler/safer than
  dynamic growth on a medical device's storage.
- **[Risk]** The 600 KB weight-archive byte-budget index (bytes-used per
  archived `seq`) could drift from the actual on-disk file sizes if a
  crash/power-loss happens between deleting a `weight_archive/<seq>.bin`
  file and updating the index → **Mitigation**: rebuild the index from a
  directory scan of `weight_archive/` at boot (sum of real file sizes,
  keyed by filename `seq`) instead of trusting only the persisted index,
  same defensive pattern as re-deriving age from `admissionEpoch` rather
  than trusting a stale cache.
- **[Trade-off]** Splitting into 4 capabilities/phases means the feature is
  only end-to-end testable on hardware once all 3 phases land — mitigated
  by keeping shared and motherBoard phases compiling and unit-testable
  (`pio test -e native`) independently, so integration risk is pushed as
  late as possible but build risk is not.

## Migration Plan

1. Land `nte-calculation-engine` in `shared/` (commit scope `shared`).
2. Land `baby-profile-storage` (including the `weight_active`/
   `weight_archive` per-baby files, the 600 KB archive budget index, the
   unified audit+weight-archive eviction tombstone logic, the
   `dischargeEpoch`/`outcome` fields, the `NS_BABY/activeSeq` set/clear/
   FIFO-exclusion logic (decision 13), the shared `archiveProfile()`
   routine with its two entry points, and the weight-history
   downsampling-to-50 function) + `baby-profile-protocol` (including
   `PROFILE_DISCHARGE`, `PROFILE_HISTORY_REQ`/`PROFILE_HISTORY`, and
   `WEIGHT_HISTORY_REQ`/`WEIGHT_HISTORY`) in `motherBoard` (commit scope
   `motherboard`), including the `PROTOCOL.md` update, removal of the
   legacy baby fields from `CommTask.cpp`, and updating (not removing)
   `GPRS.cpp`/`Wifi_OTA.cpp` to re-source the legacy telemetry fields from
   `modules/baby_profile/` plus publish the new `name`/`outcome`/
   `dischargeEpoch` fields (decision 14).
3. Land `temp-control-activation-wizard` in `Display_HMI` (commit scope
   `hmi`), replacing the AUTO AIR popup and removing local NTE calculation
   and the legacy baby fields from `Display_HMI/src/tasks/CommTask.cpp`.
4. Land `baby-history-viewer` in `Display_HMI` (commit scope `hmi`), once
   motherBoard already speaks `PROFILE_DISCHARGE`/`PROFILE_HISTORY_REQ`/
   `WEIGHT_HISTORY_REQ` (phase 2): the "Babies" top-bar button, the
   active+archived paginated history screen with the discharge dialog, and
   the `lv_chart` weight-evolution screen.
5. No feature flag / staged rollout: this is a solo-developer, matched-pair
   firmware — both boards are reflashed together once phase 4 lands.
   Rollback is `git revert` of the relevant phase commit(s), no
   on-device migration of persisted data is needed since `NS_BABY` and the
   history log are new (nothing pre-existing to migrate).

## Open Questions

- Confirm during phase 1 whether `shared/library.json` needs the explicit
  `srcDir` override (decision 2) — resolve before starting phase 2.
- On-screen keyboard comma-key disabling (`BabyProfile.name`) is a
  Display_HMI/LVGL UI detail belonging to the `temp-control-activation-wizard`
  spec/tasks, not a cross-cutting design decision — deferred to that
  phase's tasks.md.
- Confirm during the motherBoard phase whether 50 downsampled weight points
  actually fits the real serial RX line buffer on both boards (decision
  11) — resolve before starting the `baby-history-viewer` HMI phase, since
  its chart screen's rendering approach depends on receiving the full
  response in one line rather than needing chunk-reassembly.
