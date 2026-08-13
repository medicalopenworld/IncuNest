## ADDED Requirements

### Requirement: Pure, hardware-free NTE range calculation
`Firmware/shared/include/nte_table.h` / `shared/src/nte_table.cpp` SHALL
expose `NteRange calculateNteRange(uint16_t weightGrams, uint8_t gestWeeks,
uint16_t ageDays)` as a pure function (no I/O, no global mutable state, no
hardware access) so it can run identically on both `motherBoard` and
`Display_HMI` and be covered by motherBoard's `[env:native]` Unity tests.

#### Scenario: Deterministic output for identical inputs
- **WHEN** `calculateNteRange()` is called twice with the same
  `weightGrams`, `gestWeeks`, and `ageDays`
- **THEN** both calls return an identical `NteRange` (`lo`, `hi`, `mid`,
  `estimated`)
- (Verified via motherBoard `[env:native]`, `pio test -e native`.)

#### Scenario: Known weight produces a bounded, ordered range
- **WHEN** `calculateNteRange()` is called with a non-zero `weightGrams`
  within the clinical table's covered range
- **THEN** it returns `estimated=false` and `lo <= mid <= hi`, with `mid`
  the midpoint of `[lo, hi]`
- (Verified via motherBoard `[env:native]`.)

#### Scenario: SKIP weight yields the unestimated sentinel range
- **WHEN** `calculateNteRange()` is called with `weightGrams=0` (the SKIP
  sentinel)
- **THEN** it returns `estimated=true` and `lo=hi=mid=-1`, meaning no
  auto-computed range is available
- (Verified via motherBoard `[env:native]`.)

### Requirement: Out-of-table inputs fail safe, never crash
`calculateNteRange()` SHALL return a safe, well-defined result for any `weightGrams`/`gestWeeks`/`ageDays` input, including values outside the clinical table's covered range — it SHALL NOT read out of bounds, divide by zero, or crash, per `.claude/rules/security.md`'s fail-safe convention.

#### Scenario: Weight/gestational-age combination outside the table's covered range
- **WHEN** `calculateNteRange()` is called with a `weightGrams`/`gestWeeks`
  combination outside the table's covered rows/columns
- **THEN** it returns `estimated=true` with `lo=hi=mid=-1` (same sentinel
  as SKIP) instead of reading out of bounds or crashing
- (Verified via motherBoard `[env:native]`.)

### Requirement: Single shared implementation, no per-board duplicate
Neither `motherBoard` nor `Display_HMI` SHALL contain its own copy of the
NTE lookup/interpolation logic; both SHALL consume
`shared/include/nte_table.h`. `Display_HMI` SHALL NOT compute an NTE range
locally — it only renders the `CTRL,PROFILE_RANGE` values it receives from
`motherBoard`.

#### Scenario: Display_HMI has no local NTE calculation after this change
- **WHEN** the codebase is searched for NTE table lookup/interpolation logic
  after this change lands
- **THEN** the only implementation is `shared/src/nte_table.cpp`; the
  former `Display_HMI/src/tasks/UITask.cpp:901-1060+` logic has been
  removed, not duplicated
- (Manual verification — code review / grep, not a runtime test case.)
