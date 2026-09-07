## ADDED Requirements

### Requirement: Maintenance gate before any install
The orchestrator SHALL refuse to start a bundle install unless all of the following hold: AIR control off, SKIN control off, no active baby profile, heater idle, humidifier idle, and the unit powered from mains. Each unmet condition SHALL produce a distinct, reportable refusal reason. The gate exists because the SensorBoard's sensors are the air/humidity variable of the PID and the source of the thermal cut-offs, and because a rebooting HMI leaves the incubator with no visual alarm indication.

#### Scenario: Install refused while AIR control is active
- **WHEN** an install is requested and AIR control is on
- **THEN** the orchestrator refuses with the AIR-active reason and no transfer starts
- (Verified via motherBoard `[env:native]` for the decision logic.)

#### Scenario: Install refused with a baby profile active
- **WHEN** an install is requested and `NS_BABY/activeSeq` is non-zero
- **THEN** the orchestrator refuses with the active-patient reason
- (Verified via motherBoard `[env:native]`.)

#### Scenario: Install refused on battery
- **WHEN** an install is requested and the unit is not on mains
- **THEN** the orchestrator refuses with the power reason
- (Decision logic verified via motherBoard `[env:native]`; the power-source reading is manual verification.)

#### Scenario: All preconditions met
- **WHEN** an install is requested with control off, no active profile, actuators idle and mains present
- **THEN** the orchestrator proceeds to the transfer phase
- (Verified via motherBoard `[env:native]`.)

### Requirement: Fixed install order, write-then-commit
The orchestrator SHALL install in the order SensorBoard, then Display_HMI, then motherBoard. All three images SHALL be written to their boards' inactive slots **without** switching any boot partition. Only after all three writes have completed and verified SHALL the orchestrator issue commit-and-reset to both slaves and then reboot itself. The motherBoard is last because it must keep running its known-good image while it drives both transfers.

#### Scenario: Order is honored
- **WHEN** a bundle install runs to completion
- **THEN** the SensorBoard image is transferred first, the HMI image second, and the motherBoard image last, and no boot partition is switched until all three writes have verified
- (Sequencing logic verified via motherBoard `[env:native]`; the end-to-end run is manual verification on the bench.)

#### Scenario: Failure before commit leaves everything on the old images
- **WHEN** any transfer or verification fails at any point before the commit phase
- **THEN** no boot partition is switched on any board, the set is aborted, and all three boards continue running their previous images
- (Manual verification: deliberately interrupt a bench install.)

### Requirement: Progress and outcome are reported to the cloud
The orchestrator SHALL publish `ota_progress` with a per-target dimension identifying which board is transferring, and SHALL publish the outcome of every install attempt (success, refusal reason, or failure reason). `ota_progress` SHALL only be published while a transfer is in flight and SHALL be cleared when the bundle finishes, with or without success, so no stale percentage remains in the fleet table.

#### Scenario: Progress identifies the target board
- **WHEN** the HMI image is at 40 % of its transfer
- **THEN** the published progress carries both the percentage and the `hmi` target
- (Manual verification against a real ThingsBoard dashboard.)

#### Scenario: Progress cleared after a failed install
- **WHEN** a bundle install aborts at 87 %
- **THEN** `ota_progress` is cleared rather than left at 87
- (Verified via motherBoard `[env:native]` for the clearing logic.)

### Requirement: Version mismatch on boot re-enters the orchestrator
On every boot the motherBoard SHALL compare its own version, the HMI's reported version and the SensorBoard's reported version against the persisted installed set. A trio that does not match the recorded set — the state a power cut between commits can produce — SHALL re-enter the orchestrator to finish the set rather than being left to run mismatched.

#### Scenario: Power cut between slave commit and motherBoard reboot
- **WHEN** the HMI and SensorBoard have committed the new set but the motherBoard is still running the previous image
- **THEN** on its next boot the motherBoard detects the mismatch and resumes the install to completion
- (Mismatch-detection logic verified via motherBoard `[env:native]`; the power-cut scenario is manual verification.)

#### Scenario: All three match the recorded set
- **WHEN** all three reported versions equal the persisted installed set
- **THEN** no install is started
- (Verified via motherBoard `[env:native]`.)

### Requirement: Rollback arbitration for both slaves
After a slave commits and reboots, the orchestrator SHALL require proof of life within a bounded window: `HMI,UI_READY` from the Display_HMI, a heartbeat frame from the SensorBoard. Absence of proof within the window SHALL be treated as a failed image and SHALL trigger that board's revert to its previous slot. The motherBoard SHALL NOT mark the set as installed until both slaves have proven alive on their new images.

#### Scenario: HMI does not report ready after commit
- **WHEN** the HMI commits a new image and no `HMI,UI_READY` arrives within the window
- **THEN** the orchestrator records the image as failed and the HMI reverts to its previous slot
- (Manual verification with a deliberately broken HMI image.)

#### Scenario: SensorBoard heartbeat resumes after commit
- **WHEN** the SensorBoard commits a new image and its heartbeat resumes within the window
- **THEN** the orchestrator confirms the image, allowing the SensorBoard to cancel its pending rollback
- (Manual verification on the bench.)

### Requirement: Network interruption resumes, wire interruption restarts
An interrupted HTTPS fetch SHALL resume byte-exactly using an HTTP `Range` request rather than restarting the download. An interrupted transfer to a slave (UART or USB CDC) SHALL restart that image from its first chunk; per-chunk resume across a slave link failure SHALL NOT be attempted, because a broken slave link means the slave rebooted or the link is unhealthy and its partially written slot is untrustworthy.

#### Scenario: WiFi drops mid-download
- **WHEN** the network drops at 60 % of an image download and reconnects
- **THEN** the fetch resumes from the byte offset already received
- (Manual verification — no test environment covers the network stack.)

#### Scenario: HMI link drops mid-transfer
- **WHEN** the UART transfer to the HMI fails at chunk 2000
- **THEN** that image restarts from chunk 0 on the next attempt, and the HMI's inactive slot is re-erased before rewriting
- (Restart decision verified via motherBoard `[env:native]`; the link failure is manual verification.)

### Requirement: The motherBoard never stages an image on its filesystem
Because the motherBoard's LittleFS partition holds baby profiles and weight history and is smaller than the HMI image, image bytes SHALL be streamed from the network to the target board without being written to the motherBoard's filesystem at any point.

#### Scenario: LittleFS is untouched during a bundle install
- **WHEN** a full bundle install runs
- **THEN** no image data is written to the motherBoard's LittleFS partition and the free space on it is unchanged before and after
- (Manual verification on the bench.)
