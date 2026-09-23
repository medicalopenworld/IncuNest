## ADDED Requirements

### Requirement: OTA frames over the existing USB CDC framing
The motherBoard SHALL carry the SensorBoard image over the `usb_comm` binary framing already in use (Magic 2 B + Type 1 B + Length 4 B LE + Payload + CRC16-CCITT), adding new frame types for offer, accept/refuse, payload chunk, chunk acknowledgement, commit and abort. Payload SHALL be sent as raw bytes — this link is a framed binary channel with a length field and a CRC, so no base64 encoding is needed, unlike the HMI's line-oriented UART.

#### Scenario: Offer accepted and image transferred
- **WHEN** the motherBoard offers an image whose declared size fits the SensorBoard's 2 MB app slot
- **THEN** the SensorBoard accepts, receives all chunks, and confirms the terminal digest
- (Manual verification on the bench — the SensorBoard has no host test environment for this path.)

#### Scenario: Frame with a bad CRC is dropped
- **WHEN** a chunk frame arrives with a CRC16 that does not match its payload
- **THEN** it is dropped and retransmission of that sequence is requested, and nothing is written to flash
- (CRC and sequence logic verified via motherBoard `[env:native]` on the motherBoard side.)

### Requirement: `sensorboard_comm` remains the single owner of the device handle
All OTA traffic to the SensorBoard SHALL be transmitted by the existing `modules/sensorboard_comm/` task, which is already documented as the only owner that opens, closes and transmits on the CDC device handle. The orchestrator SHALL request a transfer and observe its progress; it SHALL NOT open, write to, or close the device itself.

#### Scenario: Orchestrator requests rather than transmits
- **WHEN** the orchestrator starts a SensorBoard image transfer
- **THEN** the bytes are transmitted by the `sensorboard_comm` task and no other task touches the device handle
- (Verified by code review and manual bench verification; ownership is a structural invariant, not a runtime assertion.)

#### Scenario: Transfer requested while the link is down
- **WHEN** a SensorBoard transfer is requested and `sensorboard_comm_connected()` is false
- **THEN** the request is refused with a link reason and the bundle is aborted before any byte is sent
- (Verified via motherBoard `[env:native]` for the decision logic.)

### Requirement: The air-sensor fault alarm is left honest during a SensorBoard update
While the SensorBoard is committing and rebooting into a new image it stops publishing the readings that feed `in3.temperature[]`/`in3.humidity[]`, so `sensorboard_apply_room_sensor()` stops refreshing the freshness stamp and `ALARM_AIR_SENSOR_FAULT` fires by the normal 5-second mechanism. This change SHALL NOT suppress, mask or special-case that alarm. Safety is provided by the orchestrator's maintenance gate — no thermal control is running and no patient is present — not by teaching a safety path to stay quiet.

#### Scenario: Sensor fault alarm fires during the SensorBoard reboot
- **WHEN** the SensorBoard reboots into its new image and stops publishing for longer than the freshness window
- **THEN** `ALARM_AIR_SENSOR_FAULT` becomes active through the existing mechanism, and the OTA path contains no code that suppresses it
- (Manual verification on the bench; absence of suppression is verified by code review.)

#### Scenario: Alarm clears when the new image publishes
- **WHEN** the SensorBoard's new image resumes publishing valid readings
- **THEN** the freshness stamp refreshes and the alarm clears through the existing mechanism
- (Manual verification on the bench.)

### Requirement: SensorBoard sensor source is respected
The SensorBoard OTA path SHALL only be exercised when boot detection selected the SensorBoard sensor source. On a unit whose sensors are on the I2C path and which has no SensorBoard, the manifest's `sb` entry SHALL be treated as not applicable and SHALL NOT block or fail the rest of the bundle.

#### Scenario: Unit without a SensorBoard installs the other two images
- **WHEN** a bundle is installed on a unit whose sensor source is not the SensorBoard
- **THEN** the `sb` entry is skipped as not applicable, the motherBoard and HMI images install normally, and the set is recorded as installed
- (Verified via motherBoard `[env:native]` for the applicability decision; end to end is manual verification.)

### Requirement: Commit is confirmed before the SensorBoard cancels its rollback
The motherBoard SHALL send an explicit confirmation once it has observed a heartbeat from the SensorBoard's new image. The SensorBoard's own pending-verify state SHALL only be resolved on that confirmation, so an image that boots but cannot talk to the motherBoard is reverted by its bootloader. The SensorBoard-side receiver, its `esp_ota_*` calls and its rollback handling are specified in `SensorBoard_v2/openspec/`, change `sb-ota-receiver`.

#### Scenario: Heartbeat observed, confirmation sent
- **WHEN** the SensorBoard's new image sends a heartbeat within the orchestrator's window
- **THEN** the motherBoard sends the confirmation frame that allows the pending-verify state to be resolved
- (Manual verification on the bench.)

#### Scenario: No heartbeat, no confirmation
- **WHEN** no heartbeat arrives within the window
- **THEN** the motherBoard sends no confirmation and reports the image as failed, leaving the SensorBoard's bootloader to revert
- (Manual verification with a deliberately broken SensorBoard image.)
