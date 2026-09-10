# Known Issues & Edge Cases

Throughout the development, redesign, and intensive testing of the dual firmware of IncuNest (Motherboard + HMI), various anomalous scenarios of communication, asynchrony, and hardware were fenced and handled. This list compiles typical failures, how they were architecturally mitigated, and exotic resolved case scenarios.

## 1. Auditory Desync / Alarm "Visual Glitch" ("The Phantom Alarms")

*   **Problem Description**: Generally occurred when the Motherboard turned off an alarm (`CTRL,ALM,ID,Title,Desc,0`) but exactly at that microsecond an EMI microcut (Electromagnetic Interference) occurred on the RX/TX cables. The HMI screen never received the "Turn off alarm X" command, maintaining it visually on causing unfounded panic to the user.
*   **Mitigation (Implemented)**: The hex variable flag `alarmBitmask` was embedded within the regular 1Hz telemetry heartbeat (`CTRL,STATE`). By packaging the pure binary truth every second (e.g. `0x02` indicates that *only* ID 2 is alive), the HMI sweeps and purges via a function (`Display_ApplyCtrlState`) all remaining graphic slots not figured within the imposed mask, eradicating "phantoms" automatically.

## 2. Cyclic UART Flooding (UART Flooding Overflow)

*   **Problem Description**: During rapid pre-heating, actuators caused probes to rise in value at dizzying speeds, triggering very long queues of `CTRL,TEL` towards the HMI's LVGL controller. The HMI's DMA got saturated redrawing numeric labels 30 times a second, collapsing the RX buffer.
*   **Mitigation (Implemented)**: The HMI introduces a "Differences Filter". The graphic label only invokes the heavy `lv_label_set_text()` function if the conversion from double `airTempValueDetected` to its numeric format (truncated to 1 decimal) is logically higher/lower than the previously printed string.

## 3. Language Dissociation (Broken Bilingual HMI)

*   **Problem Description**: The nurse selected "English" in the Screen Menu. The screen switched to English. Half an hour later a fan alarm triggered and, to the crew's astonishment, the red text crossed the screen in "Spanish".
*   **Reason**: Alarms are spawned, decided, and sent text-preformatted *from* the Motherboard board (`alarmIDtoString()`). If the Motherboard did not keep memory that the HMI commanded to switch the Set to English, it would use its default.
*   **Mitigation (Implemented)**: The incoming HMI payload (`HMI,...lang...`) now explicitly overwrites `in3.language` on the Motherboard. Additionally, when the HMI reconnects its USB or asks for `HMI,REQ,STATE`, it verifies the lang aligns.

## 4. Astray Phototherapy Timer (Continuous vs Timed Clash)

*   **Problem Description**: If phototherapy was turned on by the HMI setting 15 minutes, and after 5 minutes someone violently restarted **only** the HMI. Upon powering back and redrawing the view, who governed the time?
*   **Architectural Solution (Current Base)**: The Motherboard (`CommTask.cpp` on Motherboard) is the mathematical owner (Source of Truth) for `photoTimerStartMs`. It continually broadcasts in its `STATE` the purged `photoTimeRem` variable formatted `Minutes.Seconds`. The HMI powering from 0, receives that floating value, rebuilds its cache memory mathematically pushing back its zero instant so that it suits the received Delta and both proceed peeling off the remainder peacefully.

## 5. Bootload "Pile-Up" (CH340 Critical Startup Sequence)

*   **Classic ESP32 Problem Description**: Because both boards communicate via a Virtual COM Port converter (USB-Host to USB-Device), it critically depended on the voltages on the Transmission lines (TX/RX) and RTS/DTR physical states for the Host to recognize the sub-device profile without muting and injecting spurious voltage hanging the *First-Stage* bootloader and leaving the Motherboard catatonic in "Waiting Download Mode".
*   **Robust Solution**: The `VCP_CH34x` Driver on Motherboard is modified to be "Pacing-Oriented". It uses precise Mutexes (`vcp_mux`) and inserts lazy retries (`vTaskDelay`) after a false `set_control_line_state()` so any transient line instability thermally quiets down before the "115200 8N1" handshake. Meanwhile, the Display suppresses its massive LVGL debug `print`s to not trample the Motherboard's attention as soon as it boots.

## 6. AUDIO PAUSED indicator not shown after silencing (OPEN)

*   **Symptom**: silencing an alarm works — the buzzer stops and the motherBoard
    logs `estado=3` (SILENCED). But on the display the row button stays amber
    `SILENCIAR` instead of turning green `REANUDAR`, the bell-with-dashed-X
    symbol (IEC 60417-5576) is not drawn, and the countdown next to it never
    decrements. The symbol *is* drawn when the HMI boots with an alarm that was
    already silenced, which is the clue: the state is painted once and never
    refreshed.
*   **Status**: **open**. Everything below has been ruled out with evidence, so
    do not repeat it.

**Ruled out — do not re-investigate:**

| Hypothesis | How it was eliminated |
|---|---|
| motherBoard does not silence | Bench log: `[ALARM] ALM_SILENCE id=3 on=1 -> estado=3 bitmask=0x8` |
| Wrong `CTRL,STATE` format or field count | The display's exact `sscanf` was compiled and run on the host against a real 21-field line: `result=21`, `silencedBitmask=0x20` |
| Line truncated by a buffer | The line is 97 bytes; the motherBoard buffer is 192 |
| `%c`/`%s` argument mismatch | `HW_REVISION` is `'A'` (char), `FWversion` is `"18.2"` (4-char literal) |
| Wrong bit index on the display | `processReceivedAlarm()` stores `alarmList[id].id = id`; the row tests `1u << id` |
| Something overwriting the struct | Only one writer of `ctrl_state_msg` in the whole display |
| Symbol clipped by the image transform | Fixed: `lv_img_set_zoom()` transforms around the *source* image centre, which fell outside a smaller object box. Now drawn at native 48 px |
| `CTRL,STATE` not periodic | Fixed: it was request-only and the display stops requesting after sync, so every field froze at boot values. Now 1 Hz |

**Where to look next.** The two fixes above were necessary but not sufficient —
the symptom survived both. The remaining suspects, in order:

1.  Confirm on the bench whether `CTRL,STATE` now actually arrives at 1 Hz. The
    cheapest probe is the countdown: if it still does not decrement, the line is
    still not reaching the display and the problem is in transport, not in the UI.
2.  The display's only UART is the protocol link, so `COMM_LOG` output travels
    down the same wire the motherBoard parses. Any HMI-side tracing has to be
    painted on screen or routed elsewhere — budget for that before starting.
3.  `AlarmCenter_Poll()` repaints the row from a signature comparison
    (`viewSignature()`). A previous XOR collision there was already fixed, but
    the repaint path is still the least directly observed part of the chain.

**Verify the flashed build first.** Two rounds of this investigation were spent
on a board that had not been reflashed. The HMI's information screen now shows
the compiler's `__DATE__`/`__TIME__` next to its version — check it matches the
build before trusting any observation.

## 7. Wrong board's firmware pushed over WiFi OTA (violet screen, link lost)

*   **Symptom** (2026-09-08): a Display HMI flashed over USB with the flasher
    tool works. The same unit flashed over **WiFi** comes back with a violet
    screen, no touch, no protocol link — and the motherBoard raising its
    display-link alarm. Only a USB reflash recovers it.
*   **Cause**: the flasher's WiFi tab derived the board type from the mDNS
    hostname. The HMI advertises `IncuNest-Display-<sn>` (`WIFI_NAME` in
    `Display_HMI/include/main.h`), but the parser only knew the underscore
    spelling `IncuNest_Display`, so every HMI fell through to the generic
    `IncuNest` branch and was labelled — and flashed — as a motherBoard.
    Nothing downstream objected: both boards are ESP32-S3, the motherBoard
    image (1.5 MB) fits in the HMI's 5 MB app slot, and `esp_ota` only checks
    the chip and the size. The HMI then boots motherBoard firmware, which never
    creates the RGB panel.
*   **Why the app descriptor cannot catch it**: in Arduino/PlatformIO builds
    `esp_app_desc_t.project_name` is `arduino-lib-builder` on *both* boards
    (inherited from the precompiled core), so it distinguishes nothing.
*   **Mitigation (implemented)**, in three layers:
    1.  *Tool*: the board type now comes from the device itself
        (`"board"` in `/get_fw_version`), not from its hostname; the hostname
        is only a fallback when the device cannot be reached.
    2.  *Declared intent*: the tool sends `X-IncuNest-Board` (and `?board=`)
        with every `/update`. The device compares it with its own identity and
        refuses before opening `Update` — nothing is written to flash.
    3.  *Content check (the one that survives a stale tool)*: every image
        carries a board marker in `.rodata`
        (`IncuNestFW:display_hmi` / `IncuNestFW:motherboard`, see
        `shared/include/fw_image_tag.h`). Both the HTTP `/update` handler and
        the ThingsBoard updater (`shared/include/fw_guarded_updater.h`, used on
        the HMI's WiFi OTA and on the motherBoard's WiFi *and* GPRS OTA) scan
        the incoming stream and abort the write if the image carries another
        board's marker. `otadata` is left untouched, so the unit keeps running
        the firmware it already had.
*   **Deliberate gap**: an image with **no** marker is accepted. Every build
    before this change is unmarked, and rejecting them would leave the fleet
    with no way back over the air. Both sides therefore only protect each other
    once both carry a marked build — until then, the tool fix is the only
    barrier.
*   **Recovery**: reflash over USB. A unit running the wrong firmware will not
    reconnect to WiFi with the other board's credential layout, so there is no
    over-the-air way back.


## 8. Phantom `BOARD LINK LOST` when the WiFi signal drops

*   **Problem Description**: occasionally — and always correlated with losing
    WiFi coverage — the HMI painted the `BOARD LINK LOST` banner, blanked every
    reading and sounded the link-lost pattern, for a few seconds, then
    recovered on its own. The motherBoard had never stopped talking. The same
    window also produced phantom `ALARM_HMI_LINK_LOST` entries in the
    motherBoard's alarm log, because the HMI's 1 Hz keepalive stopped too.
*   **Reason**: a priority inversion, not a communication fault.
    `Display_IsBoardLinkLost()` measures *when the Comm task last saw a line*,
    not when the board last spoke, and the Comm task was the lowest-priority of
    the three tasks pinned to core 1 (UI 5, OTA/WiFi 4, **Comm 3**).
    PubSubClient waits for bytes with **busy loops**: `readByte()` spins on
    `while(!available()) yield();` and `connect()` spins without yielding at
    all, both for up to `MQTT_SOCKET_TIMEOUT` (15 s by default). `yield()` on
    Arduino-ESP32 is `taskYIELD()`, which does **not** yield to lower-priority
    tasks, and `WiFiClient::available()` is a non-blocking `ioctl`, so there is
    no real blocking point where the CPU is released. A radio drop leaves the
    TCP socket half-open with a half-received MQTT packet — exactly the input
    that makes those loops spin — so the OTA task (called every 50 ms from
    `WifiOTAHandler()`) starved the Comm task for seconds at a time. UART0's
    1 KB RX ring fills in ~1.5 s, so whole protocol lines were lost as well,
    alarm lines included (same sink as the factory-test lines, but with a new
    trigger). No reset: `CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1` is off in
    the framework's sdkconfig, so a long core-1 stall is benign for the TWDT.
*   **Mitigation (implemented)**, in three layers:
    1.  *Order of priorities*: `OTA_TASK_PRIORITY` 4 -> **2**, below
        `COMM_TASK_PRIORITY` (3). The link with the board cannot yield to
        network housekeeping; this is the order the motherBoard already used
        (its `Communication_Task` at 7, its OTA at 4). Cost: a web OTA upload
        is somewhat slower while the UI is busy.
    2.  *Bounded busy-wait*: `-D MQTT_SOCKET_TIMEOUT=2` in the three
        `build_flags` blocks, so a single wait can no longer outlast the 5 s
        `BOARD_LINK_TIMEOUT_MS` window even if the ordering regresses.
    3.  *Honest detector*: `Display_IsBoardLinkLost()` now **discounts** the
        time in which the Comm task did not run at all — nobody was reading the
        UART then, so that silence belongs to the display, not to the board.
        The Comm task stamps every pass, and after a stall longer than
        `COMM_RX_TIMEOUT_MS` it pushes `g_lastCtrlLineMs` forward by the gap
        (the residue case, where the ring overflowed and no complete line is
        left to stamp). The discount **stops at `BOARD_LINK_TIMEOUT_MS`**: past
        that the readings are equally dead whoever is to blame, and forgiving it
        would leave a blind display swearing everything is fine — the very
        danger the detector exists to prevent. So a hung or dead Comm task
        still raises the banner; what no longer does is a half-second hiccup,
        and a real silence with a healthy Comm task is still declared at
        `BOARD_LINK_TIMEOUT_MS` exactly as before.
*   **Bench verification**: with the board connected, power the AP off. Before
    the fix: `[COMM] anillo RX 7xx/1024 B: la tarea Comm no esta drenando`, a
    gap of >5 s with no lines, the banner, and an entry in the board's alarm
    log. After: only the `wifiInit()` retries with their backoff, and at most
    the new `[COMM] N ms sin drenar el cable` line. `-e wifi_off_test` builds
    without the WiFi/OTA task at all, as the A/B control.
*   **Measured (bench, 2026-09-08, 13 min)**: the instrument is the HMI's own
    1 Hz keepalive, which the same Comm task emits — its cadence *is* the
    measurement, and it comes out of the HMI's USB port. 850 keepalives, median
    **1000 ms**, p99 1191 ms, worst **1239 ms**, **zero** gaps >= 1500 ms and
    zero >= the 5 s window. The decisive stretch is 23:20-23:24, associated
    with an IP but unable to reach ThingsBoard: seven consecutive
    `TB disconnected, reconnecting...`, i.e. seven runs of the busy-wait in
    `PubSubClient::connect()`, with no effect on the cadence at all. The link
    was verifiably alive throughout: `HMI,REQ,STATE` appears 3 times in the
    first 2 s and never again, so `Display_StateSync_Service()` got its answer
    from the board. Residual jitter is ~200 ms and correlates with `wifiInit()`
    and the 5 s telemetry publish, not with the OTA task (which can no longer
    preempt Comm) — the LVGL mutex and the `arduino_events` task at priority 19
    on core 1 are the remaining coupling, 4x under the alarm window.


## 9. HMI boot loop on units with saved WiFi credentials (the "OTA server that was never there")

*   **Problem Description**: a deployed Display comes up with a blank screen and
    reboots roughly every 1.2 s (49 reboots/min measured over serial on unit
    sn 317). WiFi appears intermittent and the OTA web server is unreachable.
    Reported from the field; never reproduced on the bench.
*   **Reason**: startup order. `setup()` used to create the OTA/WiFi task
    before the UI. With an SSID stored in NVS, WiFi associates at ~300 ms and
    the WiFi/lwIP stack fragments internal RAM. When `UI_Task` then calls
    `esp_lcd_new_rgb_panel()` at ~340 ms there is no longer a contiguous
    internal DMA block for the bounce buffers (two of 38.4 KB), so it returns
    `ESP_ERR_NO_MEM`, `ESP_ERROR_CHECK` aborts, and the unit loops.
    Log line: `lcd_rgb_panel_alloc_frame_buffers(185): no mem for bounce buffer`.
    The largest contiguous internal DMA block drops from ~164 KB at the top of
    `setup()` to ~86 KB once WiFi is up; the 76.8 KB the panel needs fit by
    9 KB, so any new static buffer tipped it over. **Free total says nothing
    here** — there were 133 KB free on a unit that would not boot. Always read
    `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA)`.
*   **Why the bench never saw it**: with no WiFi configured, `GOT_IP` does not
    arrive before the panel is created and the unit boots fine. It only fires
    on Displays with saved credentials — that is, on deployed units.
*   **This is what the Display's "OTA failures" since June 2026 actually were**
    (diagnosis by @acuesta-mow on PR #28). The update server was not at fault:
    the Display died before it could serve anything. Any future report of "the
    Display does not answer over the network" should check the boot loop
    first — the symptom looks like a connectivity problem and is not one.
*   **Fix** (`50293a0`, on `dev` since 2026-09-07): `setup()` creates
    `CreateUITask()` first, then `CreateCommTask()` (its rings are static, so
    it asks for no large blocks, and starting it early avoids losing the
    `CTRL,*` lines the board emits while the panel is built), then waits on
    `UI_IsLcdPanelReady()` up to `LCD_READY_TIMEOUT_MS` (3000 ms; worst real
    case before the panel is ~740 ms because of the STC8 backlight I2C
    retries) before `CreateOTATask()`. The timeout is a safety net: a panel
    that never initialises must not leave the unit without communication to
    the board. `TelemetryHistory` also moved to PSRAM (-17.3 KB of internal
    `.bss`), and panel creation stopped aborting outright — it now walks a
    ladder of 24 -> 16 -> 12 -> 8 -> 0 bounce lines. Margin: 9 KB -> ~87 KB.
*   **What is verified, and what is not**: on COM62, `50293a0` boots with the
    full 24 bounce lines, `LCD_DIAG` steady at 43.5 fps and touch working —
    that is the PSRAM move and the ladder doing the work. The `setup()`
    barrier itself was **compiled but never flashed**; its own commit message
    records that the port was absent at the time. The field figures (49
    reboots/min before, 0 reboots and panel ready at 240 ms after, on
    CrowPanel 7.0 sn 317) come from @acuesta-mow's equivalent variant on the
    branch of PR #28, **not** from the build now on `dev`.
*   **Still open**:
    *   `dev`'s ordering barrier is pending a flash on a unit with saved
        credentials, which is the only configuration that reproduces the loop.
        @acuesta-mow has that unit (sn 317) and is verifying it.
    *   The bounce ladder is only visible over UART
        (`RGB panel initialized OK bounce=N lineas`). `g_lcd_bounce_lines` is
        exposed neither in Settings nor in the factory test, so a deployed
        unit running with degraded bounce buffers goes unnoticed.
