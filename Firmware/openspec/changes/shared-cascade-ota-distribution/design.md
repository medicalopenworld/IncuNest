## Context

Three ESP32-S3 boards, three independent update stories:

| Board | Link to motherBoard | Update path today | App slots | Current image |
|---|---|---|---|---|
| `motherBoard` | — | ThingsBoard OTA over WiFi **and** GPRS (`Start_Firmware_Update`, title `IncuNest`) | 2 × 2.625 MB | 1.48 MB |
| `Display_HMI` | UART 115200 8N1, `motherBoard` GPIO15/16 ↔ HMI **UART0** | Own ThingsBoard device (title `IncuNest_HMI`) + HTTP `Update.h` endpoint | 2 × 5 MB | 2.53 MB |
| `SensorBoard_v2` | Native USB CDC-ACM, motherBoard is USB **host** | none | 2 × 2 MB | 356 KB |

Three constraints shape everything below, and none of them are negotiable:

1. **The HMI link is the HMI's UART0** (`Display_HMI/include/tasks/CommTask.h:19` → `Serial`; confirmed from the other end by `motherBoard/src/tasks/CommTask.cpp:413-417`, which buffers the HMI's ROM panic dumps off that same wire). Log text, ROM boot banners and panic backtraces are interleaved with protocol traffic by construction. Any transfer framing has to tolerate that, not assume a clean pipe.
2. **The motherboard cannot stage an image.** With the inherited 8 MB flash its LittleFS is 2.625 MB and already holds baby profiles and weight history; the HMI image alone is 2.53 MB. Bytes have to flow network → wire without ever landing on the motherboard's filesystem.
3. **A ThingsBoard device has one `fw_*` slot.** Three images cannot ride the native firmware mechanism on a single registered device.

The clinical context matters as much as the technical one. The SensorBoard's SHT40s are the air/humidity variable of the PID and the source of the thermal cut-offs; the HMI is the only visual alarm indicator. Updating either one takes a safety function offline for the duration.

## Goals / Non-Goals

**Goals:**

- One ThingsBoard device per incubator (the motherboard), one assignable package, one version number describing all three boards.
- Every board reachable in the field, including SensorBoards on units that only ever see GPRS.
- Structural enforcement of the motherBoard↔HMI protocol coupling that `PROTOCOL.md` currently enforces only by convention.
- No loss of observability: the `hmi_*` diagnostic keys keep flowing, and per-board versions become visible in the fleet view.
- A bad image is always recoverable without a service visit.

**Non-Goals:**

- ROM download mode / esptool emulation from the motherboard (no EN/IO0 control exists, and `known_issues.md` issue 5 is the scar tissue from that area).
- Baud escalation on the HMI link.
- Delivering the HMI image over GPRS.
- Store-and-forward staging (blocked on the flash-size measurement).
- Removing the HMI's WiFi stack.
- Image signing / secure boot.

## Decisions

### D1 — Application-level OTA on every board, orchestrated by the motherboard

Each slave already owns a bootloader, two OTA slots and `otadata`. The motherboard streams an image; the slave writes it to its inactive slot, verifies, and switches boot.

*Alternative rejected:* the motherboard acting as esptool over the wire. It cannot assert the HMI's IO0 at reset — no DTR/RTS is wired to it and `board.h` has no reset GPIO — and the SensorBoard's ROM download mode would need its BOOT pin too. The upside would have been not needing receiver code in the slaves; the cost is a hardware change plus re-entering the exact failure domain of `known_issues.md` issue 5.

*Consequence to accept openly:* this has a bootstrap problem. Any unit already in the field needs one last update by cable or by each board's own WiFi before the cascade works. It is cheap now — no unit is deployed (the same window that justified the 2026-09-06 partition rework) — and expensive later.

### D2 — The ThingsBoard package is a manifest, not an image

The package assigned to the motherboard is a JSON document of a few hundred bytes:

```json
{ "set": "2.3.0",
  "boards": {
    "mb":  { "version": "18.3", "url": "https://…/motherboard_firmware.bin", "size": 1481072, "sha256": "…", "min_transport": "gprs" },
    "hmi": { "version": "18.3", "url": "https://…/display_hmi_firmware.bin", "size": 2653184, "sha256": "…", "min_transport": "wifi" },
    "sb":  { "version": "2.1",  "url": "https://…/sensorboard_firmware.bin", "size": 364512, "sha256": "…", "min_transport": "gprs" } } }
```

It arrives through the already-proven `Firmware_Send_Info`/`Start_Firmware_Update` path, so the dashboard flow in `docs/thingsboard_dashboards.md` §8 is untouched, `ota_progress` keeps working, and both transports keep the code path they have today. The three `.bin` are then fetched by HTTPS with `Range`, which the ThingsBoard chunked-MQTT transfer cannot resume.

*Alternatives considered:*
- **`sw_*` slot for a second image.** Gets you two, not three, and abuses "software" to mean "the display's firmware".
- **One concatenated bundle artifact in `fw_*`.** Attractive on the server side — one version really is one artifact. Rejected because 4.3 MB would arrive as an unrewindable MQTT chunk stream with nowhere to stage it (constraint 2): a failed slave write mid-stream means restarting the whole set, and there is no `Range` to resume with.
- **URLs in plain shared attributes, no package.** Works, but loses the assign-a-package UX, `fw_checksum`, and the *FW state* column that the dashboard already leans on.

*Trade-off accepted:* ThingsBoard's native firmware state tracks the **set** version reported by the motherboard, not each slave. Per-board versions are ordinary attributes (`hmi_fw_version`, `sb_fw_version`), so a slave stuck one version behind after a partial install shows up as an attribute mismatch, not as *Not synced*. D5's recovery path is what actually catches that, not the dashboard column.

The `.bin` are hosted where `flasher_tool/flasher/updater.py` already gets them: GitHub Releases. Reusing that means the release process gains one artifact (the manifest) and one missing asset (`sensorboard_firmware.bin`, absent from `_ASSET_MAP` today), not a new distribution channel.

### D3 — Stream, never stage

Network bytes are decoded, hashed incrementally, and pushed to the slave as they arrive. The motherboard holds one chunk plus a rolling SHA-256 state, not an image.

The uncomfortable consequence: the SHA-256 can only be **confirmed at the end**, after the slave has already written everything. That is acceptable because the slave writes to its *inactive* slot and the boot switch is a separate, later step — a corrupt transfer is discarded before it can ever boot. But it does mean a corrupted download costs a full retransfer.

*Alternative deferred, not rejected:* if `esptool.py flash_id` shows a 16 MB module, the filesystem becomes 7.875 MB (`partitions/ESP32S3_16MB.csv` is already written) and the motherboard could download, verify, and only then touch the slave — strictly better. It is an Open Question below because the measurement has not been taken.

### D4 — HMI transfer: ASCII line-framed base64, ACK-gated, telemetry quiesced

`FWUP_*` messages stay inside the existing line-oriented ASCII protocol. Payload is base64, 768 raw bytes per chunk → 1024 characters, which fits the HMI's 1280-byte `rxBuffer` (`Display_HMI/include/main.h:210`) with room for the header and CRC. One chunk outstanding at a time; the HMI ACKs with the sequence number it just committed.

*Why not the binary TLV protocol* that already exists in `Display_HMI/include/protocol/display_comms.h` (preamble `AA 55`, CRC16-CCITT, described in `docs/communication.md` §5)? It is the technically superior frame — 33 % less overhead — but it is dormant, unused by either board, and it assumes a clean byte stream. Constraint 1 says this wire is not one: a panic dump or a stray `ESP_LOG` line lands in the middle of a frame with no way to resynchronize except a timeout. Line framing degrades gracefully — an unrecognized line is discarded, which is exactly what both parsers already do. Migrating the whole link to TLV is a worthwhile separate change; doing it *as part of* an OTA path means a bug in the new framing can brick the very mechanism meant to fix bugs.

*Why base64 and not raw binary on a line protocol:* raw bytes can contain `\n` and `\r`, which is the frame terminator. Escaping is a subtler parser than base64 decoding, and the 33 % costs about 90 seconds here.

Rough budget: 2.53 MB ÷ 768 B ≈ 3450 chunks; ~1070 bytes on the wire each ≈ 93 ms at 115200, plus ACK and flash time ≈ **5–6 minutes**. The HMI buffers chunks to 4 KB before calling `Update.write()` so flash writes align with the erase page instead of doing 3450 partial-page writes.

**Telemetry is quiesced for the duration** — `CTRL,TEL` and the 1 Hz `CTRL,STATE` stop. This is a direct answer to `known_issues.md` issue 2: that link has already collapsed once from a `CTRL,TEL` burst saturating the HMI's LVGL/DMA path, and this transfer is that burst sustained for five minutes. **Alarm traffic is exempt and never suppressed**; an alarm during a transfer aborts it rather than being delayed.

### D5 — Install order, two-phase commit, and what a split commit looks like

Order: **SensorBoard → HMI → motherBoard**, all three written to inactive slots with no boot switch. Only then does the motherboard issue commit-and-reset to both slaves and reboot itself.

The motherboard goes last because it is the orchestrator: it must still be running the old, known-good image while it drives the two transfers. Writing all three before committing any is what gives version-set semantics — a network failure at 90 % leaves three boards running exactly what they ran before, with three inactive slots holding garbage that nothing will ever boot.

*The residual window is real and must be specified, not hand-waved:* a power cut between the two slave commits and the motherboard's own reboot can leave, say, a new HMI against an old motherboard — precisely the mismatch `PROTOCOL.md` warns about. Mitigation: every board reports its version, the motherboard compares the trio against the installed set on every boot, and a mismatch re-enters the orchestrator to finish the set. Until it does, the HMI holds a "finishing update" screen rather than trying to speak a protocol version its peer may not have.

### D6 — A maintenance gate, because both slaves carry safety functions

The orchestrator refuses to start, with a distinct reason code per condition, unless: AIR and SKIN control are off, no baby profile is active, heater and humidifier are idle, and the unit is on mains.

The SensorBoard case is the sharp one. Its SHT40s are the PID's air/humidity variable and feed the thermal cut-offs (`modules/sensorboard_comm/sensorboard_comm.h` is explicit that this is not auxiliary telemetry), and `sensorboard_apply_room_sensor()` deliberately stops refreshing the freshness stamp when data goes stale so that `ALARM_AIR_SENSOR_FAULT` fires. A SensorBoard rebooting into a new image *is* that condition. Rather than special-casing the alarm — which would mean teaching a safety path to stay quiet, the wrong direction entirely — the gate guarantees there is no thermal control running to protect. The alarm is allowed to fire; it is simply not clinically meaningful with the heater off and no patient.

The HMI case: a rebooting display is an incubator with no visual alarm indication. Audio survives (the buzzer belongs to the motherboard), but 60601-1-8 expects both. Same answer — the gate, not a suppression.

### D7 — Rollback: the motherboard judges, the slave acts

A slave's new image is *provisional* until the motherboard sees proof of life: `HMI,UI_READY` from the HMI, a heartbeat from the SensorBoard, within a bounded window.

The two boards need different mechanisms, and this is the part most likely to be got wrong:

- **SensorBoard (ESP-IDF)** has the real thing: `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` plus `esp_ota_mark_app_valid_cancel_rollback()`, called only after the motherboard confirms the link. The bootloader reverts automatically otherwise. (Specified in `SensorBoard_v2/openspec/`, `sb-ota-receiver`.)
- **HMI (Arduino-ESP32)** does not — that Kconfig is not enabled in the prebuilt framework, so `Update.h` gives no pending-verify state. Rollback therefore has to be application-level: the new image increments a boot-attempt counter in NVS before initializing, clears it once it has received a valid `CTRL,STATE`, and on the third failed attempt calls `esp_ota_set_boot_partition()` back to the other slot and reboots. This covers both "boots but cannot talk" and "crashes on boot", which a link-timeout check alone would not.

If both mechanisms fail, the HTTP `Update.h` endpoint (D8) and the cable remain.

### D8 — One cloud identity, but the HMI keeps its recovery path

The HMI loses its ThingsBoard client: provisioning, token, `WIFI_TB_OTA()`, `WIFICheckOTA()`, and the `PROVISION_DEVICE_KEY`/`SECRET` from its build. It **keeps** WiFi STA, mDNS and the HTTP `Update.h` endpoint.

That asymmetry is deliberate. Removing the TB client is what achieves the goal (one device, one version set) and it removes credential material from a board with an exposed USB port and a touchscreen. Removing WiFi entirely would free flash the pending Amharic font work wants — but it is also `flasher_tool/flasher/wifi_flasher.py`'s transport on the factory floor and, once the motherboard is the only updater, the only way back from an HMI image that will not talk. Cutting both in one change would trade a real recovery path for a flash saving.

Observability is preserved by relay: a new `HMI,DIAG` line carries the seven keys the HMI publishes today, and the motherboard republishes them **under the same key names** on its own device, so most existing widgets need only re-pointing, not rewriting.

### D9 — Resume granularity

Two different answers, because the two links fail differently. The **network** side resumes byte-exactly via HTTP `Range` — a GPRS or WiFi drop mid-download is common and 2.5 MB is expensive. The **wire** side does not: a failed slave transfer restarts that image from chunk zero. A UART or CDC transfer that breaks means the slave rebooted or the link is unhealthy, in which case its partially written slot is untrustworthy anyway; per-chunk resume state would be complexity bought for a case where restarting is the honest response.

## Risks / Trade-offs

| Risk | Mitigation |
|---|---|
| Five minutes of quiesced telemetry is five minutes of a display not showing live values | The gate (D6) means nothing clinical is running; the HMI shows an explicit updating screen with progress, not a frozen normal screen. Alarms remain exempt. |
| The motherboard becomes the single point of failure for all updates | The cable and the HMI's HTTP endpoint are retained as independent recovery paths (D8). The SensorBoard's only fallback is the cable — accepted, it is a serviceable connector. |
| A `FWUP_*` bug bricks the update mechanism itself | Line framing over the proven parser rather than the dormant TLV path (D4); the receiver only ever writes to the inactive slot; and the first field of every chunk is validated for field count and numeric parseability before indexing, per `.claude/rules/security.md`. |
| Power cut between slave commits leaves a mismatched trio | Version comparison on every boot re-enters the orchestrator; HMI holds a neutral screen while mismatched (D5). |
| `ALARM_AIR_SENSOR_FAULT` fires every time the SensorBoard updates | Intended. The gate guarantees no thermal control is running; the alarm is left honest rather than suppressed (D6). |
| Interleaved HMI log/panic text corrupts a chunk | Unrecognized lines are discarded, as both parsers already do; per-chunk CRC plus sequence numbers catch anything that survives; terminal SHA-256 catches the rest before any boot switch. |
| Orphaned `IncuNest-Display-<sn>` devices and broken dashboard widgets | Same key names on the motherboard's device (D8) plus a documented migration; one-way, and called out as BREAKING in the proposal. |
| A corrupted 2.5 MB download costs a full retransfer (no staging) | Accepted at 8 MB flash; the 16 MB path (D3) removes it if the measurement allows. |
| Bootstrap: this cannot update a fleet that does not already have it | No units are deployed. This is the cheapest it will ever be, and the reason to land the receiver code early even if the orchestrator follows later. |

## Migration Plan

1. Land the SensorBoard receiver (`SensorBoard_v2/openspec/`, `sb-ota-receiver`) — its first `esp_ota_*` code path.
2. Land the HMI receiver and `HMI,DIAG` **before** removing the HMI's ThingsBoard client, so there is a working alternative at every moment.
3. Land the motherboard orchestrator, the manifest fetch, and the `installBundle` RPC in **both** transport lists (`GPRS.cpp:190` and `Wifi_OTA.cpp:1221` are independent arrays; `docs/thingsboard_dashboards.md` records that `checkOta` was once registered in only one and silently did nothing in the other).
4. Add `sensorboard_firmware.bin` and the manifest to the release artifacts / `updater.py` `_ASSET_MAP`.
5. Flash all three boards by cable once, to seed the receivers.
6. Verify a full bundle install on the bench, then a deliberately interrupted one (network drop, then power cut mid-commit) to exercise D5's recovery.
7. Remove the HMI's ThingsBoard client; re-point dashboard widgets to the motherboard's device; delete the `IncuNest_HMI` packages and the orphaned devices.

Rollback of the *change itself*: the HMI's TB client is the last thing removed, so every step before that is revertible by reflashing the previous images with the existing tools.

## Open Questions

1. **Is the motherboard's flash 8 MB or 16 MB?** `esptool.py flash_id` answers it. 16 MB turns D3 from streaming into verify-then-write, which is strictly safer. Worth measuring before implementation starts.
2. **Who authorizes an install?** Assigning the package from the dashboard, or the dashboard offering it and a nurse confirming on the HMI? The second fits a medical device better and makes the D6 gate explainable to the operator; it also means the manifest can sit pending for days.
3. **Firmware title for the bundle.** A new title (`IncuNest_Bundle`) is cleaner but leaves the existing `IncuNest` title meaning "motherboard app only" in historical records. Reusing `IncuNest` avoids a rename but changes what an assigned package *is* halfway through the device's history.
4. **Chunk size 768 B.** Chosen to fit the current 1280-byte `rxBuffer` with headroom. Larger chunks with a bigger buffer would cut the transfer time; worth one bench measurement rather than a guess.
5. **Does the manifest need a minimum-version field?** Skipping intermediate sets is fine for app images, but not if a future set depends on an NVS or LittleFS layout migration performed by an intermediate version.
