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
    trigger). No reset **on the HMI**: `CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1`
    is off in the framework's sdkconfig, so a long core-1 stall is benign for
    the TWDT *there*.
*   **The same conclusion does NOT transfer to the motherBoard — there the
    identical stall reboots the board.** The asymmetry is that the HMI
    subscribes no task to the TWDT and therefore inherits the framework's
    configuration, while the motherBoard installs its own:
    `initHardware()` ends in `watchdogInit(WDT_TIMEOUT)`
    (`system/initHardware.cpp`), i.e. `esp_task_wdt_init(75, true)` plus
    `esp_task_wdt_add(NULL)` called from `setup()` — which subscribes
    Arduino's **`loopTask`** to a 75 s TWDT with panic, fed once a second by
    `watchdogReload()` in `loop()`. That `loopTask` runs at **priority 1 on
    core 1** (`CONFIG_ARDUINO_RUNNING_CORE=1`), the lowest of every motherBoard
    task on that core (OTA 4, GPRS 5, buzzer 6, comm 7, sensors 8, security 9),
    so the very busy-loops described above starve it. Five consecutive waits of
    the library's default `MQTT_SOCKET_TIMEOUT` (15 s) are exactly the 75 s
    budget. Observed on the bench on 2026-09-10: IncuNest-353_1 lost WiFi at
    16:54 and reset with `RST_reason = 6` (`TASK_WDT`) about 50 minutes later,
    having stopped publishing at 17:46. `-D MQTT_SOCKET_TIMEOUT=2` is now in
    the motherBoard's `IncuNest_V18`/`IncuNest_V17` blocks too (the `_factory`
    variants inherit it); with 2 s it would take 38 consecutive waits.
    Still a strong hypothesis rather than a closed case: the CrashReporter
    dump naming the starved task is what would confirm it.
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

## 9. Phantom `BOARD LINK LOST` of a few ms on every unlock (regression of #8)

*   **Problem Description**: on the bench (2026-09-10), **every** screen unlock
    produced the same sequence: the maintenance pop-up opened, the link-lost
    banner appeared with its audible pattern for a few milliseconds, the pop-up
    closed itself, and everything went back to normal. The motherBoard's alarm
    log held **no** `ALARM_HMI_LINK_LOST` entry, so unlike #8 the HMI's
    keepalive never stopped: the board could still hear the display, and the
    display was the only one claiming the link was gone.
*   **Reason**: an unsigned underflow in the very discount added as layer 3 of
    #8's mitigation. The accounting block at the top of `Comm_Task()` measures
    `gap` from the **header of the previous pass**, but incoming lines are
    stamped into `g_lastCtrlLineMs` **inside** that pass, by
    `ReceiveMessageFromOtherESP()`. When the UI task (priority 5) preempts Comm
    (priority 3) mid-pass and does not hand the CPU back for hundreds of ms,
    the stamp ends up *later* than the header it will be compared against, so
    `g_lastCtrlLineMs += gap` double-counts a window already contained in the
    stamp and pushes it into the **future**. The old code asserted the opposite
    in a comment ("gap se mide contra la pasada anterior, `g_lastCtrlLineMs`
    nunca adelanta a `nowPass`"), which is true only if nothing is stamped
    during the pass. With a future stamp, `now - g_lastCtrlLineMs` wraps around
    in `Display_IsBoardLinkLost()` and reads as ~49 days of silence, so the
    detector fires instantly — and clears again as soon as the next `CTRL,TEL`
    or `CTRL,STATE` (1 Hz each) re-stamps a sane value. Hence "a few ms".
    The unlock is what makes it reproducible rather than random: repainting the
    screen and then building the maintenance pop-up (QR render included) are
    two long UI passes back to back, which is exactly the preemption shape
    required. The pop-up then closed itself correctly — `mustYield()` in
    `MaintenanceDialog.cpp` cedes to any active alarm or to a lost link — so
    the closing pop-up was a symptom, never a second bug.
*   **Mitigation (implemented)**, in two layers:
    1.  *Root cause*: the shifted stamp is clamped to `nowPass`, so
        `g_lastCtrlLineMs` can never move ahead of the pass doing the shifting.
        The discount still forgives blindness shorter than
        `BOARD_LINK_TIMEOUT_MS`, exactly as #8 intended.
    2.  *Defence in depth*: `Display_IsBoardLinkLost()` computes both ages with
        **signed** arithmetic saturated at 0, so a stamp ahead of `millis()`
        reads as "just seen" rather than as a wrap-around silence. This
        detector decides whether a medical device declares its readings dead;
        it must not depend on every other site in the file being right to the
        millisecond.
*   **What did NOT change**: a genuinely silent board is still declared at
    `BOARD_LINK_TIMEOUT_MS`, and a hung or dead Comm task still raises the
    banner through the `unheard > BOARD_LINK_TIMEOUT_MS` branch, which the
    clamp does not touch.
*   **Bench verification**: with the reminder due (or `Maintenance_SetEnabled`
    left on with the daily level overdue), lock and unlock the screen several
    times. Expected after the fix: the pop-up opens and **stays** open, no
    banner, no beep, no blanked readings. `[COMM] N ms sin drenar el cable` may
    still appear with N between `COMM_RX_TIMEOUT_MS` and `BOARD_LINK_TIMEOUT_MS`
    — that line reports the UI stall, which is real and unchanged; what it must
    no longer do is trigger the alarm.

## 10. Temperature control (or phototherapy) switches itself back OFF right after the baby wizard

*   **Symptom**: select a baby, enter the weight, press APLICAR on the proposed
    air temperature. Temperature control goes ON and, a second or two later,
    goes back OFF on its own. The same shape was reported for phototherapy.
*   **Why it is a regression of the phototherapy echo race (`a77bd1b`)**: the
    HMI is the only place that can turn these switches off spontaneously.
    `Display_ApplyCtrlState()` resyncs `actuation` / `controlMode` /
    `phototherapyMode` / `muteAlarm` / `skinModeEnabled` from the
    motherBoard's echoed `CTRL,STATE` on every frame — needed so a rebooted
    HMI inherits the board's real state (#4). A frame that was already in
    flight still carries the value from *before* the board processed the
    command; adopting it does not just repaint the switch, it writes
    `hmi_msg`, and the HMI's next 1 Hz heartbeat then genuinely commands the
    board OFF.
*   **Why the old fix was not enough**: `a77bd1b` protected a just-changed
    field for a fixed 2.5 s grace window. That is a bet that the round trip
    fits inside it, and this link does not guarantee that — the HMI Comm task
    can lose the CPU for whole seconds (#7, #9) and the UART RX ring drops the
    *newest* bytes when it fills, so what gets parsed after a stall is
    precisely the stale line. Once the window expires without confirmation the
    stale echo wins, and the resulting OFF is irreversible. The baby wizard
    makes the shape easy to hit: activation now happens at the end of a long
    modal, in a single heavy UI pass, right after a burst of `HMI,PROFILE_*`
    traffic.
*   **Fix (implemented)**: the guard is no longer a timer but a
    **confirmation**. A locally changed field stays authoritative until the
    board echoes back that same value; the 1 Hz heartbeat keeps resending the
    intent meanwhile, so lost frames and multi-second stalls no longer matter.
    `LOCAL_CMD_CONFIRM_TIMEOUT_MS` (10 s) survives only as a safety net for a
    board that never confirms, and logs `<campo> sin confirmar en N ms` when it
    fires — that log line is the discriminator between "stale echo" and "the
    board is refusing the command".
*   **Also fixed here**: the setpoints (`desiredAirTemperature`,
    `desiredSkinTemperature`) had **no** guard at all — they were overwritten
    from every echo. Applying the wizard's proposed temperature could therefore
    be silently undone by the next `CTRL,STATE`, which still carried the
    previous setpoint. They now use the same confirmation guard.
*   **What did NOT change**: recovery after an HMI reboot (#4). No guard arms
    before the first `CTRL,STATE` has been applied (`g_stateSynced`), nor on
    the first value observed for a field, so a freshly booted display never
    imposes its start-up "everything off" on a board that is actually
    regulating.
*   **Bench verification**: select a baby, enter a weight, press APLICAR.
    Expected: temperature control stays ON and the target temperature stays at
    the proposed value (not the previous setpoint) for at least 30 s. Repeat
    for SKIN and for phototherapy. Unplug the HMI↔MB cable for ~5 s while
    control is ON and reconnect: control must still be ON afterwards.

## 11. HMI boot loop on units with saved WiFi credentials (the "OTA server that was never there")

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
*   **Partial verification on the port branch (2026-09-14, unit 353)**: the
    display on the bench **does** have saved credentials (`Connecting to SSID
    from Preferences: in3wifi`), which is the configuration said to reproduce
    the loop, and on this branch it boots clean: a single `rst:` in the whole
    capture, `RGB panel initialized OK` with `bounce=19200 px` — the full 24
    lines, no ladder degradation — and `[SRAM DMA] libre=193212
    mayor_bloque=139264`. That covers the ordering barrier on THIS unit. It
    does **not** close the item above: sn 317 is a different unit and the
    field figures still come from @acuesta-mow's branch, not from here.

## 12. HMI shows "MB: connected" to ThingsBoard while the unit never appears on the platform (OPEN)

*   **Symptom** (bench, unit sn 353, IDF port on `dev` at `8d07c11`,
    2026-09-16): the WiFi screen paints `MB: ...` green and the heading
    indicator is green with 4 bars, but the device is not active on
    `mon.medicalopenworld.org` and no telemetry arrives by any transport.
*   **The display is not lying on its own**: it only repeats the
    `serverCommStatus` field of `CTRL,STATE` (`UITask.cpp`,
    `wifi_link_status_update()`), and the board is sending `4`
    (`COMM_STATUS_WIFI_SERVER`). The fault is on the motherBoard.
*   **Root cause of the false indicator** (confirmed in code and log):
    `motherBoard/src/tasks/Wifi_OTA.cpp`, `WIFI_TB_OTA()` around line 1904.
    After `tb_wifi.connect(...)` returns `true`, the code sets
    `Wifi_TB.serverConnectionStatus = true`, subscribes the RPC callbacks and
    requests the OTA **immediately**. With `Espressif_MQTT_Client`, `connect()`
    only calls `esp_mqtt_client_start()` (or `esp_mqtt_client_reconnect()` on
    retries) and returns `ESP_OK` as soon as the client task starts: it says
    nothing about the session. The flag is never lowered while WiFi stays
    associated (`WIFIIsConnectedToServer()` only checks the flag plus
    `WIFIIsConnected()`), and every 30 s retry sets it to `true` again.
    **The GPRS twin already has the correct code** (`GPRS.cpp` lines ~1340
    and ~1370-1391: lower the flag when `!tb.connected()`, wait for
    `tb.connected()` up to `GPRS_MQTT_SESSION_TIMEOUT` before subscribing or
    declaring `+SERVIDOR`). That fix was item 3 of the 2G OTA chain and was
    never ported to the WiFi side.
*   **Log evidence** (COM32, two consecutive boots, identical shape):

    ```
    I (40855) tb_mqtt: creando cliente MQTT: bufer 4352 B x2, bloque mayor 20480 B, libre 30964 B
    E (41275) mqtt_client: esp_mqtt_handle_transport_read_error: transport_read(): EOF
    E (41290) mqtt_client: mqtt_process_receive: mqtt_message_receive() returned -2
    I (42446) COMM_HOST: Sending state to HMI: CTRL,STATE,...,0,0,4,0.00,...   <- serverCommStatus=4
    E (72251) esp-tls: couldn't get hostname for :mon.medicalopenworld.org: getaddrinfo() returns 202
    W (86276) mqtt_client: Publish: Losing qos0 data when client not connected
    [TB] Subscribing the given topic (v1/devices/me/rpc/request/+) failed    x3
    [TB] Preparing for OTA firmware updates failed, attributes might be NULL
    E (69657) DIAG: LOW RESOURCES heap_int=11064 heap_int_min=100 heap_int_largest=3456
    ```

    The `Subscribing ... failed` / `Losing qos0` lines are the WiFi side acting
    on a session that does not exist. The GPRS side never publishes because
    `GPRS.cpp` (~line 1495) hands ThingsBoard to WiFi whenever
    `WIFIIsConnected()`; so with WiFi associated but MQTT down, **neither
    transport** delivers telemetry. `heap_int_min=100` is issue
    [mb-muere-por-oom-en-comm-task-rx] (memory), not new.
*   **Why the MQTT session over WiFi actually fails — two stages**:
    1.  First attempt (~1 s after `GOT_IP`, before PPP): TCP connects and the
        peer closes it 0.4-2.4 s later **before any CONNACK**. Cause not
        established, see below.
    2.  Every later attempt fails at DNS (`getaddrinfo() returns 202`): once
        the modem's PPP negotiates IPCP it overwrites lwIP's global DNS while
        the default route stays on WiFi. Known; candidate fix is
        `CONFIG_ESP_NETIF_SET_DNS_PER_DEFAULT_NETIF=y` in the MB
        `sdkconfig.defaults` (being handled on `feat/mb-heap-diag`).
*   **Ruled out for stage 1**: the broker closing without CONNACK. A raw MQTT
    CONNECT from the PC to `mon.medicalopenworld.org:1883` with a bogus token
    gets `CONNACK rc=5` in 0.12 s and `provision` gets `rc=0`; the server
    answers, it does not silently drop. The HMI kicking the MB's session is
    also unlikely: the display provisions as `IncuNest-Display-<sn>` with its
    own provision key, so tokens differ.
*   **Two hypotheses left for stage 1, undecided**:
    1.  The WiFi side is **not provisioned** in NVS (`NS_GPRS`/`KEY_PROVISIONED`
        read in `WIFI_TB_Init()`), so that first connection is the
        `provision` request. ThingsBoard closes the connection right after the
        provision response, which is exactly an EOF with no further CONNACK.
        Fits the timing. If so, expect an `IncuNest-353` or `IncuNest-353_N`
        device on the platform that never goes active (the retry logic in
        `WIFIProvisionResponse()` appends `_N` when the name exists).
    2.  The path through the Windows Mobile Hotspot (`in3wifi`, gateway
        192.168.137.1 = the bench PC, NAT to the home router) resets the
        session. Could not be checked: `pktmon` needs an elevated shell.
*   **Bench notes for whoever picks this up**:
    *   `logI()` is compiled out (`LOG_INFORMATION false` in `main.h`), so
        `[WIFI] -> Connecting ... with token`, `Provisioning as:` and the
        `PUBLISH ... SUCCESS/FAIL` lines **never reach the UART**. Either flip
        it for the session or promote the four decisive lines to `ESP_LOGI`.
    *   Opening COM32 with default DTR/RTS **resets the motherBoard** (FTDI
        auto-reset). Capture with `dtr=False`, `rts=False` set before
        `open()`, or you will chase reboots that are your own.
    *   NVS is at `0x9000`, size `0x5000` (`partitions/ESP32S3_8MB.csv`).
        `esptool --chip esp32s3 --port COM32 read-flash 0x9000 0x5000 nvs.bin`
        plus a strings dump gives token and provisioned flag; then a raw
        CONNECT from the PC with that token decides hypothesis 1 vs 2
        (`rc=0` = the device exists and the token is good).
*   **Plan**:
    1.  Port the GPRS session wait to `WIFI_TB_OTA()`: after `connect()`,
        poll `tb_wifi.connected()` up to a `WIFI_MQTT_SESSION_TIMEOUT`
        (5000 ms like GPRS) before subscribing, publishing config, requesting
        OTA or raising `serverConnectionStatus`; set the flag to `false` in
        the `!tb_wifi.connected()` branch. Change `WIFIIsConnectedToServer()`
        to also require `tb_wifi.connected()`. After this the HMI will show
        `MB: sin servidor`, which is the truth, and the real fault becomes
        visible to the operator.
    2.  Consider letting the GPRS side publish when WiFi is associated but
        `!tb_wifi.connected()` for longer than N minutes, instead of gating on
        `WIFIIsConnected()` alone. Today a WiFi network without reachable
        MQTT silences the cellular path too.
    3.  Close stage 1 with the NVS read described above, then either fix
        provisioning persistence or document the hotspot as unsupported for
        bench MQTT tests.
    4.  Land the DNS-per-netif change (stage 2) and re-verify with WiFi and
        PPP up at the same time; the reconnect storm is also what pushes
        `heap_int_min` to 100 B.
