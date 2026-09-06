## ADDED Requirements

### Requirement: The motherBoard is the only ThingsBoard device
An incubator SHALL present exactly one device to ThingsBoard: the motherBoard. The Display_HMI SHALL NOT provision itself, SHALL NOT hold a device token, and SHALL NOT connect a ThingsBoard client. Its provisioning keys SHALL be removed from its build so no ThingsBoard credential material ships on a board with an exposed USB port and a touchscreen.

#### Scenario: HMI does not appear as a device
- **WHEN** a unit boots and connects to WiFi
- **THEN** no `IncuNest-Display-<sn>` device is created or updated in ThingsBoard, and the HMI opens no MQTT connection to the server
- (Manual verification against a real ThingsBoard instance.)

#### Scenario: No provisioning credentials in the HMI build
- **WHEN** the HMI firmware is built
- **THEN** it contains no ThingsBoard provisioning key or secret and no stored device token is read or written
- (Verified by code review and by inspecting the built image.)

### Requirement: HMI diagnostics keep flowing under the motherBoard's device
The motherBoard SHALL republish the diagnostic values relayed by `HMI,DIAG` under its own device using the **same key names** the HMI publishes today (`hmi_heap_int_b`, `hmi_heap_int_min_b`, `hmi_heap_psram_b`, `hmi_stack_ui_b`, `hmi_stack_comm_b`), so existing dashboard widgets need re-pointing to a different device but not rewriting against different keys.

#### Scenario: Keys preserved after the migration
- **WHEN** the motherBoard publishes telemetry after the HMI's ThingsBoard client is removed
- **THEN** the five `hmi_*` diagnostic keys appear under the motherBoard's device with unchanged names and units
- (Manual verification against a real dashboard.)

#### Scenario: Stale values are not published
- **WHEN** no `HMI,DIAG` line has been received within the staleness window (for example the HMI is disconnected or updating)
- **THEN** the motherBoard omits those keys rather than republishing the last known values as if they were current
- (Staleness logic verified via motherBoard `[env:native]`.)

### Requirement: Per-board versions are visible in the fleet view
The motherBoard SHALL publish `hmi_fw_version` and `sb_fw_version` alongside its own version and the installed set version. ThingsBoard's native firmware-state tracking follows only the set version reported by the motherBoard; per-board versions are ordinary attributes, and a board lagging the set is surfaced by the orchestrator's mismatch recovery rather than by the *FW state* column.

#### Scenario: All three versions published
- **WHEN** the motherBoard is connected and both slaves have reported their versions
- **THEN** its own version, `hmi_fw_version`, `sb_fw_version` and the installed set version are all published
- (Manual verification against a real dashboard.)

#### Scenario: SensorBoard absent
- **WHEN** the unit's sensor source is not the SensorBoard
- **THEN** `sb_fw_version` is omitted rather than published as an empty or zero value
- (Verified via motherBoard `[env:native]` for the omission logic.)

### Requirement: The install trigger RPC exists on both transports
The `installBundle` RPC SHALL be registered in **both** the GPRS and the WiFi `RPC_Callback` arrays. These two lists are independent, and `Firmware/docs/thingsboard_dashboards.md` records a case where `checkOta` was registered only on the GPRS side and silently did not exist over WiFi, with the dashboard reporting that the unit did not respond while showing it connected.

#### Scenario: RPC answered over WiFi
- **WHEN** `installBundle` is invoked from the dashboard on a unit connected over WiFi
- **THEN** the unit answers and the orchestrator evaluates the maintenance gate
- (Manual verification against a real unit.)

#### Scenario: RPC answered over GPRS
- **WHEN** `installBundle` is invoked on a unit connected over GPRS only
- **THEN** the unit answers with the same semantics as over WiFi
- (Manual verification against a real unit.)

#### Scenario: Refusal is reported, not silent
- **WHEN** `installBundle` is invoked while the maintenance gate is not satisfied
- **THEN** the RPC response carries the specific refusal reason rather than a generic failure or no response
- (Verified via motherBoard `[env:native]` for the reason mapping.)

### Requirement: The HMI keeps its WiFi recovery path
Removing the HMI's ThingsBoard client SHALL NOT remove its WiFi station mode, its mDNS advertisement, or its HTTP `Update.h` upload endpoint. That endpoint is the transport `flasher_tool/flasher/wifi_flasher.py` uses on the factory floor and, once the motherBoard is the only updater, the only remaining recovery route for an HMI image that will not talk over the link.

#### Scenario: Factory tool still finds and flashes the HMI
- **WHEN** `wifi_flasher.py` scans the network after this change
- **THEN** the HMI is still discoverable by mDNS and still accepts a firmware upload over HTTP
- (Manual verification with the real flasher tool.)

## REMOVED Requirements

### Requirement: Display_HMI self-updates from its own ThingsBoard device
**Reason**: An incubator now presents a single ThingsBoard device (the motherBoard) carrying a single version set, which makes the motherBoard↔HMI protocol coupling documented in `Firmware/PROTOCOL.md` structural instead of procedural, and gives GPRS-only units an HMI update path they never had. The HMI's ThingsBoard client, provisioning, device token, `WIFI_TB_OTA()`, `WIFICheckOTA()` and the `IncuNest_HMI` firmware title are removed.

**Migration**: Existing `IncuNest-Display-<sn>` devices in ThingsBoard become orphaned and must be deleted after the fleet is migrated; any dashboard widget bound to them must be re-pointed to the corresponding motherBoard device, where the same `hmi_*` key names are republished. `IncuNest_HMI` OTA packages must be withdrawn so nobody assigns one to a device that no longer reports. Units must be flashed once by cable or by the HMI's own HTTP endpoint to seed the receiver before the cascade can take over. This is a one-way migration: an HMI running post-change firmware will not re-provision itself if the motherBoard's orchestrator is unavailable — the cable and the HMI's HTTP endpoint are the fallbacks.

*(Note: this requirement was never formalized as an OpenSpec capability under `openspec/specs/`; it is recorded here as REMOVED so the behavior being retired is explicit in the change record rather than implied.)*
