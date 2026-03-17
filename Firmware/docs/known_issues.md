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
