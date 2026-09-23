## ADDED Requirements

### Requirement: Bundle manifest as the ThingsBoard firmware package
The package assigned to the `motherBoard` device in ThingsBoard SHALL be a JSON **bundle manifest**, not an application image. The manifest SHALL declare a set version and, for each of the three boards (`mb`, `hmi`, `sb`), a firmware version, an HTTPS URL, a byte size, a lowercase hex SHA-256 digest, and a minimum transport (`gprs` or `wifi`). It SHALL be fetched through the existing `Firmware_Send_Info`/`Start_Firmware_Update` path on both the WiFi and GPRS clients, so the dashboard workflow documented in `Firmware/docs/thingsboard_dashboards.md` §8 is unchanged.

#### Scenario: Well-formed manifest is accepted
- **WHEN** the motherBoard receives a manifest declaring a `set` version and complete `mb`, `hmi` and `sb` entries
- **AND** every entry carries a version, url, size, sha256 and min_transport
- **THEN** the manifest parses into a `BundleManifest` with three populated board entries and no error
- (Verified via motherBoard `[env:native]`, `pio test -e native`.)

#### Scenario: Manifest arrives over either transport
- **WHEN** a bundle package is assigned to the motherBoard device and the unit is connected over WiFi, and separately when it is connected over GPRS only
- **THEN** the manifest is fetched and parsed identically on both paths
- (Manual verification against a real unit and a real ThingsBoard instance — no test environment covers the transports.)

### Requirement: Malformed manifests are rejected without side effects
Manifest parsing SHALL validate before it trusts, per `.claude/rules/security.md`. A manifest that is not valid JSON, is missing any required field, carries a size or digest that is not parseable, declares a size above the receiving board's app-slot capacity, or exceeds the manifest size ceiling SHALL be discarded in full. A rejected manifest SHALL NOT start any transfer, SHALL NOT partially apply, and SHALL leave the previously installed set recorded as current.

#### Scenario: Truncated manifest
- **WHEN** the fetched manifest is truncated mid-document
- **THEN** parsing fails, no transfer starts, and the recorded installed set is unchanged
- (Verified via motherBoard `[env:native]`.)

#### Scenario: Missing board entry
- **WHEN** a manifest declares `mb` and `hmi` but omits `sb`
- **THEN** the manifest is rejected as incomplete rather than installing a partial set
- (Verified via motherBoard `[env:native]`.)

#### Scenario: Declared size exceeds the target app slot
- **WHEN** a manifest declares an `hmi` image larger than the HMI's 5 MB app slot, or an `sb` image larger than the SensorBoard's 2 MB slot
- **THEN** the manifest is rejected before any byte is fetched
- (Verified via motherBoard `[env:native]`.)

#### Scenario: Non-numeric or malformed digest
- **WHEN** a manifest carries a `sha256` that is not 64 hexadecimal characters
- **THEN** the manifest is rejected
- (Verified via motherBoard `[env:native]`.)

### Requirement: Set-version comparison decides whether an install is needed
The motherBoard SHALL persist the set version it last installed successfully and compare it against the manifest's `set` before acting. A manifest whose set version equals the installed one SHALL be treated as already applied and SHALL NOT trigger any transfer, even if an individual board reports a differing version — that case is handled by the orchestrator's mismatch recovery, not by re-running the whole bundle.

#### Scenario: Manifest matching the installed set
- **WHEN** the manifest's `set` equals the persisted installed set version
- **THEN** no transfer is started and the manifest is reported as already applied
- (Verified via motherBoard `[env:native]`.)

#### Scenario: Manifest declaring a different set
- **WHEN** the manifest's `set` differs from the persisted installed set version
- **THEN** the orchestrator is offered the bundle for installation
- (Verified via motherBoard `[env:native]`.)

### Requirement: Per-image transport eligibility
Each board entry's `min_transport` SHALL gate delivery. An image declaring `min_transport=wifi` SHALL NOT be fetched while the active transport is GPRS. When one image in a set is ineligible on the active transport, the whole set SHALL be deferred rather than partially installed, and the reason SHALL be reported to the cloud.

#### Scenario: HMI image deferred on a GPRS-only unit
- **WHEN** a manifest declares `hmi.min_transport=wifi` and the active transport is GPRS
- **THEN** no transfer starts, the set is reported as deferred with a transport reason, and the SensorBoard image is not installed on its own
- (Verified via motherBoard `[env:native]` for the decision logic; the transport state itself is manual verification.)

### Requirement: Image integrity is verified against the manifest digest
Every fetched image SHALL be hashed incrementally as it is streamed and its SHA-256 compared against the manifest before that board's slot is allowed to become bootable. A digest mismatch SHALL abort that board's install, leave its inactive slot unbooted, and abort the whole set.

#### Scenario: Digest mismatch after a complete transfer
- **WHEN** an image transfers completely but its computed SHA-256 differs from the manifest value
- **THEN** the boot switch for that board is never issued, the set is aborted, and all three boards keep running their previous images
- (Incremental hashing verified via motherBoard `[env:native]`; the end-to-end abort is manual verification.)
