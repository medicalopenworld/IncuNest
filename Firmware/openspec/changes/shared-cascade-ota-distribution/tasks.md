> Phases are ordered so each one lands as its own atomic commit and never mixes
> two boards, per `Firmware/.claude/rules/commits.md`. Phase 0 must be answered
> before Phase 2 starts. The SensorBoard receiver lives in
> `SensorBoard_v2/openspec/` (change `sb-ota-receiver`) and is a hard dependency
> of Phase 7.

## 0. Prerequisites and open questions

- [ ] 0.1 **Manual**: run `esptool.py --port COMx flash_id` on the motherBoard and record whether the module is 8 MB or 16 MB. If 16 MB, decide whether to adopt `partitions/ESP32S3_16MB.csv` and switch design decision D3 from streaming to verify-then-write before writing any orchestrator code
- [ ] 0.2 Decide who authorizes an install: dashboard assignment alone, or dashboard offer plus operator confirmation on the HMI (design Open Question 2). This changes the orchestrator's entry points and the HMI screens in Phases 3 and 6
- [ ] 0.3 Decide the ThingsBoard firmware title for the bundle — new (`IncuNest_Bundle`) versus reusing `IncuNest` (design Open Question 3)
- [ ] 0.4 Decide whether the manifest needs a minimum-version field for sets that carry NVS/LittleFS migrations (design Open Question 5)

## 1. shared/ — bundle and transfer types

- [ ] 1.1 Add `shared/include/ota_bundle.h`: `BundleManifest`, per-board `BundleImage` (version, url, size, sha256, min_transport), transfer status/result enums, and the `FWUP_*` field-layout constants both boards parse against
- [ ] 1.2 If any `.cpp` is added to `shared/`, update `shared/library.json`, both `platformio.ini` files, and extend motherBoard's `[env:native]` `build_src_filter` in the **same commit**, per `.claude/rules/testing.md`
- [ ] 1.3 Unity tests in motherBoard `[env:native]` for the manifest parser: well-formed input, truncated JSON, missing board entry, non-numeric size, malformed 64-char digest, size exceeding each target's app slot, manifest above the size ceiling

## 2. motherBoard — manifest fetch and set bookkeeping

- [ ] 2.1 Fetch the manifest through the existing `Firmware_Send_Info`/`Start_Firmware_Update` path on both the WiFi and GPRS clients, without disturbing the current motherBoard self-OTA behavior
- [ ] 2.2 Persist the installed set version in NVS and implement set-version comparison (already-applied versus offer)
- [ ] 2.3 Implement per-image transport eligibility (`min_transport`), including deferring the whole set when one image is ineligible on the active transport
- [ ] 2.4 Implement HTTPS image fetch with `Range` resume and incremental SHA-256 over the stream, writing nothing to LittleFS
- [ ] 2.5 Unity tests in `[env:native]`: set-version comparison, transport-eligibility decisions, incremental hashing over chunk boundaries, `Range` offset arithmetic

## 3. motherBoard — orchestrator state machine

- [ ] 3.1 Implement the maintenance gate with a distinct reason code per unmet condition (AIR off, SKIN off, no active baby profile, heater idle, humidifier idle, mains present)
- [ ] 3.2 Implement install ordering (SensorBoard → HMI → motherBoard), write-to-inactive-slot with no boot switch, and the separate commit-and-reset phase
- [ ] 3.3 Implement rollback arbitration: proof-of-life windows for both slaves, confirmation to the SensorBoard, failure recording for the HMI
- [ ] 3.4 Implement boot-time version comparison of the trio against the persisted set and re-entry into the orchestrator on mismatch
- [ ] 3.5 Add `ota_progress` per-target dimension and outcome reporting, clearing progress when a bundle ends with or without success
- [ ] 3.6 Register the `installBundle` RPC in **both** `RPC_Callback` arrays (`GPRS.cpp:190` and `Wifi_OTA.cpp:1221`) — they are independent lists
- [ ] 3.7 Unity tests in `[env:native]`: gate decisions per condition, install ordering, abort-before-commit invariants, mismatch detection, progress clearing, RPC refusal-reason mapping

## 4. motherBoard — HMI transfer (sender side)

- [ ] 4.1 Implement `FWUP_*` sender: offer, chunk emission, ACK handling with one chunk outstanding, retry budget, commit, abort
- [ ] 4.2 Implement base64 encoding and chunk sizing against `COMM_RX_BUFFER_SIZE` (768 raw bytes → 1024 encoded), with per-chunk sequence and CRC
- [ ] 4.3 Quiesce `CTRL,TEL` and periodic `CTRL,STATE` for the transfer window; keep alarm traffic exempt and make an alarm abort the transfer
- [ ] 4.4 Parse `HMI,DIAG` with field-count and numeric validation, discarding malformed lines silently
- [ ] 4.5 Unity tests in `[env:native]`: base64 round-trip at non-multiple-of-3 and final-short-chunk boundaries, encoded-line-fits-buffer, chunk CRC, out-of-sequence rejection, retry accounting, `HMI,DIAG` field validation

## 5. motherBoard — SensorBoard transfer (sender side)

- [ ] 5.1 Add OTA frame types to `modules/sensorboard_comm/` (offer, accept/refuse, chunk, ACK, commit, abort, confirm) over the existing Magic+Type+Len+CRC16 framing, raw payload
- [ ] 5.2 Route all transmission through the `sensorboard_comm` task so it remains the single owner of the device handle; the orchestrator requests and observes only
- [ ] 5.3 Implement sensor-source applicability: skip the `sb` entry as not-applicable on units without a SensorBoard, without failing the set
- [ ] 5.4 Verify by code review that no code path added here suppresses, masks or special-cases `ALARM_AIR_SENSOR_FAULT`
- [ ] 5.5 Unity tests in `[env:native]`: link-down refusal, applicability decision, chunk sequence/CRC logic

## 6. Display_HMI — OTA receiver and diagnostics relay

- [ ] 6.1 Implement the `FWUP_*` receiver: offer accept/refuse against slot capacity, slot erase, chunk validation, base64 decode, ACK by sequence
- [ ] 6.2 Buffer payload to a flash-page-aligned size before each `Update.write()` instead of one partial-page write per chunk
- [ ] 6.3 Implement terminal SHA-256 verification and the commit path, leaving the slot unbooted on any mismatch
- [ ] 6.4 Add the dedicated updating screen with progress, shown for the whole transfer instead of a frozen normal screen
- [ ] 6.5 Implement application-level rollback: NVS boot-attempt counter incremented before init, cleared on the first valid `CTRL,STATE`, `esp_ota_set_boot_partition()` back to the other slot on the third consecutive failure
- [ ] 6.6 Send `HMI,DIAG` with the five diagnostic values plus version and serial
- [ ] 6.7 **Manual verification** (no test environment exists for Display_HMI): full transfer against a real motherBoard, chunk corruption forced by injecting log output mid-transfer, a deliberately broken image to exercise both rollback branches, and the updating screen's behavior when an alarm arrives

## 7. End-to-end bring-up

- [ ] 7.1 Land `sb-ota-receiver` from `SensorBoard_v2/openspec/` (hard dependency — the SensorBoard's first `esp_ota_*` code path)
- [ ] 7.2 Add `sensorboard_firmware.bin` and the bundle manifest to the release artifacts and to `flasher_tool/flasher/updater.py` `_ASSET_MAP`
- [ ] 7.3 **Manual**: flash all three boards by cable once to seed the receivers, then run a full bundle install on the bench
- [ ] 7.4 **Manual**: interrupt a bench install at the network layer (drop WiFi mid-download) and confirm `Range` resume
- [ ] 7.5 **Manual**: interrupt a bench install between the slave commits and the motherBoard reboot (power cut) and confirm the boot-time mismatch recovery finishes the set
- [ ] 7.6 **Manual**: confirm LittleFS free space is unchanged before and after a full install (nothing staged)
- [ ] 7.7 **Manual**: confirm `installBundle` answers over WiFi **and** over GPRS, and that a refusal returns its specific reason

## 8. Cloud identity migration (last — it removes the fallback)

- [ ] 8.1 Republish the relayed `hmi_*` keys under the motherBoard's device with unchanged names, omitting them when stale rather than publishing last-known values
- [ ] 8.2 Publish `hmi_fw_version`, `sb_fw_version` and the installed set version, omitting `sb_fw_version` on units without a SensorBoard
- [ ] 8.3 Remove the HMI's ThingsBoard client, provisioning, device token, `WIFI_TB_OTA()`, `WIFICheckOTA()` and its provisioning keys — keeping WiFi STA, mDNS and the HTTP `Update.h` endpoint intact
- [ ] 8.4 **Manual**: confirm `flasher_tool/flasher/wifi_flasher.py` still discovers and flashes the HMI after 8.3
- [ ] 8.5 **Manual**: re-point dashboard widgets to the motherBoard device, then delete the orphaned `IncuNest-Display-<sn>` devices and withdraw the `IncuNest_HMI` OTA packages

## 9. Documentation

- [ ] 9.1 Update `Firmware/PROTOCOL.md` with the `FWUP_*` and `HMI,DIAG` field layouts and a minor version bump
- [ ] 9.2 Update `Firmware/docs/communication.md` with the transfer protocol and the telemetry-quiescing rule
- [ ] 9.3 Update `Firmware/docs/thingsboard_dashboards.md` with the single-device model, the bundle manifest, the `installBundle` RPC and the migration
- [ ] 9.4 Update `Firmware/docs/transport_matrix.md` with the bundle-check cadence and which images each transport may deliver
- [ ] 9.5 Add an ADR under `Firmware/docs/adr/` recording why application-level OTA was chosen over esptool emulation, and why the manifest rides the `fw_*` slot instead of the images
