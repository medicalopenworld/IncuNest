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

## 11. Control (or phototherapy) switches itself OFF milliseconds after being turned on (residual race of #10)

*   **Symptom**: flip temperature control or phototherapy ON and a few
    *milliseconds* later it goes back OFF on its own, as if OFF had been
    pressed. Intermittent — it depends on the exact instant of the tap.
*   **How it differs from #10**: #10 is the *baby wizard* shape and takes a
    second or two (the command's round trip). This one is immediate and hits a
    plain switch tap. The confirmation guard from #10 is present and correct;
    what failed is *when* it gets armed.
*   **Root cause**: the guard was armed only by `Comm_Task`'s 10 ms poll
    (`CommTask.cpp`, `trackLocalCmdGuard`), while the decision is taken by
    `UITask` inside `Display_ApplyCtrlState()`. Between the operator's tap
    (UITask writes `hmi_msg`) and the next poll there is a window of up to
    10 ms:

    ```
    t=0      Comm_Task parses a CTRL,STATE that predates the command
    t=+1ms   operator taps  -> UITask writes hmi_msg.actuation = 1
    t=+2ms   UITask reaches Display_ApplyCtrlState -> `pending` is still false
             -> the stale echo wins, hmi_msg is rewritten, switch paints OFF
    t=+10ms  Comm_Task polls -> sees 0, equal to lastSeen -> "no change"
             -> the guard is NEVER armed
    ```

    Because the guard never armed, the `<field> sin confirmar en N ms` line
    never fires either: the defect leaves **no trace in the log**, which is why
    it survived the #10 work.
*   **Fix (implemented)**: `Display_ApplyCtrlState()` now arms all seven guards
    itself, immediately before consulting them. Arming and deciding happen in
    the same task and the same instant, so the window is gone. `Comm_Task`'s
    poll stays: it is idempotent, and it is what leaves `changedAtMs` at the
    real instant of the change, which the `LOCAL_CMD_CONFIRM_TIMEOUT_MS` safety
    net hangs from.
*   **What did NOT change**: recovery after an HMI reboot (#4). `g_stateSynced`
    is set *after* `Display_ApplyCtrlState()` returns, so on the first frame the
    new arming call takes the `!g_stateSynced` branch and arms nothing — a
    freshly booted display still inherits the board's real state instead of
    imposing its start-up "everything off".
*   **Bench verification**: flip temperature control ON and OFF ~20 times in a
    row at a normal pace, then the same for phototherapy. Expected: every ON
    stays ON. If one still drops, capture both serial ports and check whether
    `sin confirmar en 10000 ms` appears — with the line it is the board
    refusing the command (a different defect); without it, the race is not
    fully closed.
*   **Bench result (2026-09-19, SN 353, HW18)**: verified working by the user
    on the fixed build — control and phototherapy no longer switch themselves
    back OFF. This issue is **closed**; #10's own bench verification, which had
    been left pending since 2026-09-11, is covered by the same run.

## 12. Re-flashing with the flasher tool erases the unit's identity (no ThingsBoard) — FIXED

*   **Symptom (2026-09-20)**: a unit re-flashed with `IncuNest_Flasher.exe`
    stopped connecting to ThingsBoard. Nothing in the flashing log looked
    wrong, the board booted normally, and no new device appeared in the
    server.
*   **Root cause**: `flasher_config.json` sets `force_serial_number: true`, so
    the tool asks for a serial on *every* motherBoard, including boards that
    already have firmware. Supplying a serial makes `flash_board()` write the
    image from `nvs_gen.generate_serial_nvs()`, which is the size of the whole
    NVS partition and carries a single key (`mb_cfg/serial`). Writing it wipes
    everything else in NVS: the ThingsBoard token and the `provisioned` flag
    (`mb_gprs`) and the WiFi credentials (`mb_wifi`).
*   **Why it looks like a server problem**: with no stored WiFi the unit falls
    back to the SSID compiled into `Credentials.h`, which usually does not
    exist outside the bench, so it never reaches the network. If it does reach
    it, the `IncuNest` device profile provisions with
    `ALLOW_CREATE_NEW_DEVICES`, which refuses a name that already exists; the
    firmware retries as `IncuNest-<n>_1`..`_3` (`PROVISION_MAX_RETRIES = 3`)
    and then gives up for good. On the server, serial 1 already has all four
    names taken, and 325, 327, 328, 331, 333, 334, 336, 337 and 353 have burnt
    at least one retry — each of those is a unit that was re-flashed and came
    back as a different device, losing its history.
*   **Fix (flasher)**: the tool now reads the serial already stored on the
    board (`flasher.read_device_serial()`, using the `parse_nvs_serial()` that
    was already there) and pre-fills the dialog with it. Accepting it unchanged
    writes no NVS at all, so the unit keeps its identity
    (`flasher.serial_to_write()`); changing it rewrites NVS and the dialog says
    so in red. Covered by `tests/test_serial_preserva_nvs.py`.
*   **Still open on the server side**: the duplicate `IncuNest-<n>_k` devices
    are not cleaned up, and serial 1 cannot provision again until somebody
    deletes them. Worth considering `CHECK_PRE_PROVISIONED_DEVICES` for the
    `IncuNest` profile so a re-provisioning unit recovers its own credentials
    instead of creating a twin.

## 13. `Reset due to task watchdog` a los 2-3 min de encender (TinyGSM) — FIXED

*   **Symptom (production, 2026-09-20)**: units 352, 358 and 359 reset with
    `RST_reason = 6` (`ESP_RST_TASK_WDT`) two to three minutes after power-on,
    once each, and then ran for hours without another reset. The operator had
    just switched phototherapy on, so phototherapy looked like the trigger.
*   **Phototherapy is not the trigger.** Its regulation loop
    (`sensors_module.cpp`) is rate-limited and never blocks, and the three
    units kept phototherapy on for 4.5 h afterwards with no further resets.
    What lines up with the timing is the **modem bring-up**.
*   **Root cause**: `TINY_GSM_YIELD()` is `delay(TINY_GSM_YIELD_MS)` and the
    library's default is `0`. In arduino-esp32 `delay(0)` is `vTaskDelay(0)`,
    which yields **only to tasks of equal or higher priority**. `loopTask`
    runs at priority 1 and is the only thing that feeds the 75 s task
    watchdog (`watchdogInit(WDT_TIMEOUT)`, `initHardware.cpp`), so while the
    GPRS task — priority 5 — sits inside a TinyGSM wait, the watchdog is
    never fed. And the waits are long: `modem.gprsConnect()` blocks for the
    whole attach handshake, which `GPRS.cpp:727` already warned "can by
    itself exceed GPRS_TIMEOUT on slow networks". Over 75 s on a slow 2G
    network and the board resets.
*   **Same family as #8**: that one was PubSubClient's 15 s active waits
    starving the same `loopTask`, fixed with `MQTT_SOCKET_TIMEOUT=2`. The
    project had already paid for this lesson once in another library.
*   **Fix**: `-DTINY_GSM_YIELD_MS=1` on both motherBoard environments. One
    tick of real `vTaskDelay` does yield to lower priorities. Costs at most
    1 ms per poll of the modem UART.
*   **Not verified on hardware yet**: the bench unit attaches quickly, so it
    does not reproduce the slow-attach case on demand. What can be checked is
    the absence of the reset; reproducing it needs a slow or marginal 2G
    network.

## 14. Every OTA ends in `PANIC`: the server re-triggers an update that can never succeed

*   **Symptom (production, 2026-09-21)**: the seven units of the batch (352,
    354-359) all reached 18.30 **and** all reported
    `RST_reason = 4` (`ESP_RST_PANIC`). Reproduced on the bench on 2026-09-22
    with 18.32 and again with 18.33.
*   **Why it looked like the update "worked"**: it applies on reboot, and the
    panic *is* the reboot. `updatedCallback()` (`GPRS.cpp`) and
    `WIFI_UpdatedCallback()` (`Wifi_OTA.cpp`) deliberately leave
    `esp_restart()` commented out — you do not reboot an incubator with a baby
    inside, the image is meant to boot at the next power-up. So a unit that
    updated *and* rebooted by itself had, by definition, crashed.
*   **Captured live** (`ota2_bucle_fantasma.log`, bench, 18.33). Right after
    `Progress 100.00%` and `[GPRS] -> Done, OTA will be implemented on next
    boot`, the port fills at ~7 lines/s with:

    ```
    [TB] Failed to initalize flash updater, ensure that the partition scheme has two app sections
    ```

    That message is `ERROR_UPDATE_BEGIN`, and it is only reachable while
    processing **chunk 0**. The finished download had been restarted from
    scratch, on an updater that was already closed. Control and the HMI link
    stayed alive throughout: it is not a CPU lock-up, it is a retry storm that
    keeps asking the server for chunks forever.
*   **Root cause — a clash of models, not a coding slip.** ThingsBoard
    considers an update finished when the device **reports** the new version.
    This device deliberately does not reboot, so it keeps reporting the old
    one. The server therefore still sees `target != current`, re-triggers the
    update, and `Start_Firmware_Update()` runs again on a partition that is
    already written and finalized — so Arduino's `Update.begin()` refuses,
    which is the `ERROR_UPDATE_BEGIN` above.

    From there the SDK never gives up: `Handle_Failure(RETRY_UPDATE)` calls
    `Request_First_Firmware_Packet()`, which **re-reads
    `m_retries = m_fw_callback->Get_Chunk_Retries()` on every lap**. The retry
    count never reaches zero, so the loop is unbounded by construction.
*   **Proof it is server-driven** (bench, 18.35, 00:05:45): unassigning the
    firmware package in ThingsBoard stopped the storm instantly — 488
    occurrences before, 488 thirty seconds later, zero new — and the unit
    logged `[TB] No new firmware assigned on the given device`. Nothing was
    changed on the device to achieve that.
*   **Fix** (`GPRS.cpp`, `Wifi_OTA.cpp`): a `g_otaPendingReboot` latch, set by
    the shared `updatedCallback()` on success and checked at both
    `Start_Firmware_Update()` call sites. It lives in RAM on purpose: the only
    thing that clears it is the reboot that actually applies the image. Both
    transports share `updatedCallback()`, so one flag covers 2G and WiFi.
*   **A false trail worth recording.** The first diagnosis was that the
    per-chunk timeout timer (`OTA_Handler::m_watchdog`) outlived the download
    and resurrected it, and the first patch was
    `if (m_fw_callback == nullptr) { return; }` in `Handle_Request_Timeout()`.
    That guard **can never fire**: on success `Firmware_OTA_Unsubscribe()` does
    `m_fw_callback = OTA_Update_Callback();`, which rebuilds
    *ThingsBoardSized's own member* rather than `OTA_Handler`'s pointer to it,
    and the only thing that nulls that pointer — `Stop_Firmware_Update()` —
    hangs off `Cleanup_Subscriptions()`, which this firmware never calls. The
    patch shipped inside 18.32 and the unit panicked anyway. What settled it
    was the serial capture: the loop starts **one second after** `Done` with no
    `Progress` line at all, so it is a *new* update being started, not a
    download being resumed.
*   **Still worth knowing** (not the cause, not patched): after
    `Firmware_OTA_Unsubscribe()` that pointer aims at a default-constructed
    object, and both `OTA_Update_Callback()` and `Callback()` are `= default`
    with no member initializers, so `m_message`, `m_fwTitel`, `m_updater`,
    `m_retries` and `m_timeout` are **indeterminate**. Anything that reaches
    `Handle_Request_Timeout()` after an update ends reads garbage, and
    `Logger::println(m_message)` in `Callback.h` would print from a wild
    `char *`. `patch_libdeps.py` patches 2 to 4 stop the timer and null the
    pointer at the three exits of the update so that patch 1's guard becomes
    reachable — defensive, and **not** verified to fix anything observed.
*   **Do not "fix" this by raising the timeout or the retry count.** The retry
    count is reset every lap; the defect is that the update is restarted at
    all.

## 15. Phototherapy timer lost or resurrected across a crash

Found by audit on 2026-09-23, not by a field report. Both faults need a reset
to show up, and both change how long a baby is irradiated, so they are written
down in full.

**What already worked.** State restore is sound: the rule is "restore on every
reset except `POWERON` and `BROWNOUT`" (`initHardware.cpp`), both boards use
it, and control state is written to NVS *at the moment it changes*
(`main.cpp:474` and `546`), so a crash does not lose it. `in3.phototherapy` is
part of that, which means **after a crash the lamp always comes back on**. The
only thing that decides whether it also comes back with its countdown is the
`"photo"` NVS namespace — and that is where both faults were.

**A — a crash in the first 60 s turned a timed session into an endless one.**
The remaining minutes were only persisted by a periodic save every 60 s
(`CommTask.cpp`). Before the first save the namespace is empty, so
`initEEPROM()` read `active=false`, `g_restore_photo_minutes` stayed 0 and the
timer was never re-armed. The lamp came back on with **no countdown and no
auto-off**. Over-treatment, with nothing on screen to suggest it.

**B — a cancelled timer could resurrect on top of a continuous session.** Only
natural expiry cleared the namespace. Stopping a timed session by hand left
`active=true, mins=N` behind. Start a **continuous** session afterwards, crash,
and the restore read those leftovers and re-armed the cancelled timer: the lamp
switched itself off N minutes later with nobody asking. Under-treatment, silent.

**Fix.** `photoTimerPersist()` / `photoTimerForget()` in `CommTask.cpp`, called
on *every* transition rather than some of them: persist when the session starts
(A) and on each periodic save, forget on operator stop, on natural expiry, and
on the edge that starts a continuous session (B). `photoTimerForget()` probes
before clearing, because it runs on every HMI command (~1 Hz) and an
unconditional `clear()` would be a flash write per second.

**The trap in this code — do not "simplify" the three branches into two.** The
HMI sends `photoMin=0` *with* `photo=1` during the **last minute** of a timed
session, because the seconds travel in their own field
(`Display_HMI/src/tasks/CommTask.cpp:1571`, and `photoMinutesRemaining` is
`(int)photoTimeRemaining` over an `MM.SS` value). So `mode=1, mins=0` is
ambiguous: it is either a continuous session or the final minute of a timed
one. Treating it as "continuous" stops a live timer and leaves the lamp on
forever — the exact fault A was meant to remove. The third branch tells them
apart by looking at `photoTimerActive`. The original two-branch shape was
deliberate; it just did not persist or clear enough.

**Verification.** The five transitions were modelled on the host and run
against the old and the new logic: the old one fails A and B (2 failures), the
new one passes all of them, and the last-minute case is unaffected by both.
That model is *not* in the repo: the logic lives inside `CommTask.cpp`, which
needs Arduino, FreeRTOS and Preferences and does not build in `[env:native]`,
so a copy of it kept as a unit test would drift from production in silence.
Making this properly testable means extracting the timer into
`modules/control/`, like `fan_guard` — worth doing, not done here.

**C — the fix for A uncovered a third fault: the HMI echo ratcheted the
countdown.** Verified on the bench (2026-09-23, 18.39): after a restored timer,
15 minutes were consumed in **26 seconds**. The board broadcasts the remaining
time as `MM.SS`, which just under a whole minute reads `14.59`; the HMI
truncates that to `14` and sends it back; the board compared it against
`photoTimerMinutes` — the duration it was *armed* with — saw `14 != 15`, took it
for a new duration and restarted the count from 14. Every round trip ate a
whole minute.

This was pre-existing and unreachable in production: it needs a *restored*
timer, and until A was fixed the timer was never restored. Left alone it would
have traded "the lamp never switches off" for "the lamp switches off in 30
seconds", which is not an improvement.

The fix compares against the **remaining** time, not the armed duration, and
only re-arms on a genuine change: a new session, or a difference of more than
one minute. One minute is exactly the truncation margin, so the steady-state
echo — always exactly one step below — is ignored, while a real operator change
(15 → 30, 15 → 5) still re-arms.

*Residual limitation:* the protocol has no way to distinguish "the operator set
a new duration" from "the HMI is echoing what we just sent", so the rule is a
heuristic on the size of the jump. An echo that arrived two or more minutes
stale would still be read as a change and would lose a minute. It would not
ratchet, and it has not been observed.

**Verified on hardware** (bench, 18.39 and 18.40, `/debug/inject` +
`/debug/crash?kind=null`):

| check | evidence |
|---|---|
| A: crash 1 s into a timed session | `reset=PANIC (4) restoreState=1` then `[RESTORE] photo timer resumed: 15 min` |
| C: single-step echo (`15` then `14`) | no second `timer started` line — no re-arm |
| C: two-step jump (`15` then `13`) | re-arms, as a real operator change should |

B was verified on the host model only: reproducing it on hardware needs the HMI
to hold a continuous session, and the HMI overwrites the injected command about
once a second.

## 16. Web endpoints leaked patient data and unit identifiers without a password

Found by audit on 2026-09-23. Every endpoint that **writes** was already
authenticated — `/config` POST, `/update`, all of `/debug/*` (the last with a
second gate on debug mode). The reads were not.

*   `/get_config` returned `skin_temp_val` — **the baby's current skin
    temperature** — plus the serial number, `heater_amps` and the `air_tmax` /
    `skin_tmax` thermal cutoffs. Anyone on the hospital LAN could read it with
    no credentials, while the POST that writes those same parameters asked for
    them.
*   `/get_ccid` returned the unit's SIM CCID.
*   Both are now authenticated. Neither user flow changes: they are fetched by
    AJAX from `/config` and `/serverIndex`, which are themselves authenticated,
    so the browser sends the credentials it has already cached.

**Left open on purpose, do not "fix" these:** `/get_fw_version` (both boards)
and `/get_freq` (HMI) are the flasher's discovery path — it sweeps the subnet
with 50 threads before it has any credential, and uses `/get_freq` returning
200 vs 404 to tell an HMI from a motherBoard. They publish only version, serial,
board type and the LCD write frequency. Adding a field to either of them
publishes it to the whole network unauthenticated.

**Still open — the transport.** All of this is HTTP Basic over plain HTTP, so
the credentials travel base64 on the wire, and that same password is the only
thing guarding `/update`, which accepts an arbitrary binary. Capturing one
authenticated request is enough to flash anything onto the unit. Fixing it
means TLS on a board with ~11 KB of free internal heap (see the OOM notes), so
it is a design decision, not a patch.

**Credentials fallback is no longer silent.** `Credentials_public.h` fell back
to the repository's public dummy values with no diagnostic; the only thing that
caught it was the factory test failing to provision. It now emits a `#warning`
always, and `-DREQUIRE_REAL_CREDENTIALS` turns it into an `#error` — that flag
belongs in any build destined for a real unit.
