## ADDED Requirements

### Requirement: `FWUP_*` message set on the motherBoard↔HMI link
The link SHALL carry a new `FWUP_*` message family inside the existing line-oriented ASCII protocol, terminated by `\n`: an offer from the motherBoard (image size, SHA-256, target version), an accept or refuse from the HMI, numbered payload chunks, per-chunk acknowledgements, a commit instruction, and an abort usable from either side. `Firmware/PROTOCOL.md` SHALL be updated in the same change with the full field layout and a minor version bump.

#### Scenario: Offer accepted and transfer begins
- **WHEN** the motherBoard sends a `FWUP` offer with a size that fits the HMI's inactive slot
- **THEN** the HMI erases its inactive slot, replies with an accept, and the motherBoard begins sending chunk 0
- (Manual verification against real hardware — no test environment exists for Display_HMI.)

#### Scenario: Offer refused because the image does not fit
- **WHEN** the offer declares a size larger than the HMI's app slot
- **THEN** the HMI refuses with a size reason and no chunk is sent
- (Field validation verified via motherBoard `[env:native]`; the HMI side is manual verification.)

### Requirement: Chunk framing survives interleaved log and ROM text
The HMI's protocol UART is its UART0, which also carries `ESP_LOG` output, ROM boot banners and panic backtraces — the motherBoard already buffers HMI panic dumps off this same wire. Every `FWUP_*` line SHALL therefore be independently parseable: field count and numeric parseability validated before any field is indexed, unrecognized lines discarded silently, and no parser state carried across an unrecognized line. A chunk SHALL carry a sequence number and a CRC so that corruption by interleaved text is detected rather than written to flash.

#### Scenario: Log line lands between two chunks
- **WHEN** an `ESP_LOG` line from the HMI or a ROM banner appears between two chunk lines
- **THEN** it is discarded by the receiving parser, the transfer continues, and no chunk is lost or misattributed
- (Line-parser validation verified via motherBoard `[env:native]`; the interleaving itself is manual verification.)

#### Scenario: Chunk fails its CRC
- **WHEN** a chunk arrives with a CRC that does not match its payload
- **THEN** the HMI does not write it to flash and requests retransmission of that sequence number
- (CRC logic verified via motherBoard `[env:native]`; the HMI behavior is manual verification.)

#### Scenario: Out-of-sequence chunk
- **WHEN** a chunk arrives with a sequence number other than the one expected next
- **THEN** it is rejected rather than written at the wrong offset
- (Sequence validation verified via motherBoard `[env:native]`.)

### Requirement: Base64 payload sized to the existing receive buffer
Chunk payloads SHALL be base64-encoded so that no payload byte can be mistaken for the `\n` frame terminator. Raw payload size per chunk SHALL be chosen so the encoded line plus header and CRC fits the HMI's existing `COMM_RX_BUFFER_SIZE` (1280 bytes) with headroom; 768 raw bytes → 1024 encoded characters SHALL be the initial value. The HMI SHALL buffer received payload to a flash-page-aligned size before calling `Update.write()` rather than issuing one partial-page write per chunk.

#### Scenario: Encoded chunk fits the buffer
- **WHEN** a chunk carries the maximum raw payload
- **THEN** the complete encoded line including header, sequence, CRC and terminator is shorter than `COMM_RX_BUFFER_SIZE`
- (Verified via motherBoard `[env:native]`.)

#### Scenario: Base64 boundary lengths round-trip
- **WHEN** payloads whose length is not a multiple of 3, and a final short chunk, are encoded and decoded
- **THEN** the decoded bytes are identical to the input with no padding artifacts
- (Verified via motherBoard `[env:native]`.)

#### Scenario: Writes are page-aligned
- **WHEN** an image transfer runs
- **THEN** the HMI accumulates payload to a flash-page-aligned buffer before each `Update.write()` call
- (Manual verification via HMI logging of write sizes.)

### Requirement: One chunk outstanding, acknowledged by sequence
The motherBoard SHALL send at most one unacknowledged chunk at a time and SHALL wait for the HMI's acknowledgement carrying the sequence number just committed. A missing acknowledgement within a timeout SHALL retransmit that chunk a bounded number of times before aborting the image. This ACK-gated flow control exists to prevent the receive-buffer collapse recorded as `Firmware/docs/known_issues.md` issue 2.

#### Scenario: Acknowledgement carries the committed sequence
- **WHEN** the HMI buffers chunk N successfully
- **THEN** it acknowledges N, and only then does the motherBoard send N+1
- (Manual verification on the bench.)

#### Scenario: Retry budget exhausted
- **WHEN** a chunk is retransmitted up to its retry limit without acknowledgement
- **THEN** the image is aborted, the HMI's inactive slot is left unbooted, and the orchestrator is told the transfer failed
- (Retry accounting verified via motherBoard `[env:native]`.)

### Requirement: Telemetry quiesced during transfer, alarms exempt
For the duration of an HMI image transfer the motherBoard SHALL stop sending `CTRL,TEL` and the 1 Hz `CTRL,STATE`. It SHALL NOT suppress alarm traffic: an alarm becoming active during a transfer SHALL be transmitted immediately and SHALL abort the transfer rather than be delayed behind it. The HMI SHALL display an explicit updating screen with progress for the duration, not a frozen normal screen.

#### Scenario: Telemetry stops for the transfer window
- **WHEN** an HMI image transfer is in progress
- **THEN** no `CTRL,TEL` or periodic `CTRL,STATE` line is emitted until the transfer ends
- (Manual verification by capturing the link.)

#### Scenario: Alarm during transfer aborts it
- **WHEN** an alarm becomes active mid-transfer
- **THEN** the alarm line is sent immediately and the transfer is aborted, leaving the inactive slot unbooted
- (Abort decision verified via motherBoard `[env:native]`; the end-to-end behavior is manual verification.)

#### Scenario: Updating screen is shown
- **WHEN** the HMI accepts an offer
- **THEN** it shows a dedicated updating screen with transfer progress and does not present normal operating values as if they were live
- (Manual verification on real hardware.)

### Requirement: HMI application-level rollback
Arduino-ESP32's prebuilt framework does not enable `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`, so `Update.h` provides no bootloader pending-verify state. The HMI's newly committed image SHALL therefore protect itself at application level: increment a boot-attempt counter in NVS before initialization, clear it once a valid `CTRL,STATE` has been received from the motherBoard, and on the third consecutive failed attempt call `esp_ota_set_boot_partition()` back to the other slot and reboot. This covers both "boots but cannot talk" and "crashes before it can talk".

#### Scenario: New image boots but never hears the motherBoard
- **WHEN** the new HMI image boots three times without receiving a valid `CTRL,STATE`
- **THEN** it switches the boot partition back to the previous slot and reboots into it
- (Counter logic is HMI-side with no test environment — manual verification, forced by disconnecting the link.)

#### Scenario: New image crashes on boot
- **WHEN** the new HMI image panics before clearing the counter, three times
- **THEN** the fourth boot lands on the previous slot
- (Manual verification with a deliberately broken image.)

#### Scenario: Healthy image clears the counter
- **WHEN** the new HMI image receives a valid `CTRL,STATE` after booting
- **THEN** the boot-attempt counter is cleared so a later unrelated reboot does not trigger a revert
- (Manual verification on real hardware.)

### Requirement: `HMI,DIAG` relays the HMI's diagnostics to the motherBoard
The HMI SHALL send an `HMI,DIAG` line carrying the diagnostic values it publishes to ThingsBoard today — internal heap free, internal heap minimum, PSRAM free, UI task stack high-water mark, comm task stack high-water mark, and its firmware version — so that removing its ThingsBoard client loses no observability. The line SHALL follow the same field-count and numeric validation conventions as every other message on this link and SHALL be discarded silently when malformed.

#### Scenario: Diagnostics reach the motherBoard
- **WHEN** the HMI sends a well-formed `HMI,DIAG` line
- **THEN** the motherBoard parses all six values and makes them available for republication
- (Verified via motherBoard `[env:native]` for the parser.)

#### Scenario: Malformed diagnostics line
- **WHEN** an `HMI,DIAG` line arrives with a missing field or a non-numeric value
- **THEN** it is discarded silently and no stale or partial value is published
- (Verified via motherBoard `[env:native]`.)
