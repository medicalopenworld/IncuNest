# IncuNest Communication Protocol

This document describes in detail the communication logic, protocols, and synchronization mechanisms between the Motherboard (Main MCU) and the Display HMI.

## 1. Physical Layer and Architecture Specifications

*   **Physical Medium**: Asynchronous bidirectional UART bus bridged via the external **CH340C** chip (or equivalent).
*   **Baudrate**: 115200 baud.
*   **Frame Format**: 8N1 (8 data bits, no parity, 1 stop bit).
*   **Frame Terminator**: Newline character `\n` (`0x0A`).
*   **Logical Architecture**: Event-based and asynchronous. The Motherboard sends the real-world state at a constant frequency (1 Hz), while the HMI reacts and sends commands upon human interactions. The control board assimilates the HMI packets in its available processing time.

## 2. Message Types: Motherboard → HMI

The Motherboard sends strict prefixes so the HMI knows what to parse. Every outgoing message typically begins with the `CTRL,` header.

### A. Full State (`CTRL,STATE`)
Sent every 1 second (minimum) or upon forced request from the HMI (`HMI,REQ,STATE`). Primary consolidation to recover variables that might have been lost on the noisy UART bus.
*   **Format**:
    `CTRL,STATE,<act>,<mode>,<airSet>,<skinSet>,<humSet>,<photo>,<mute>,<sn>,<hwNum>,<hwRev>,<fwVer>,<numAlarms>,<skinE>,<commStatus>,<photoTimeRem>,<lang>,<alarmBitmask>`
*   **Key Fields**:
    *   `act`: Power state bitmask of main modules (bit 0: Heaters/Motors, etc).
    *   `mode`: Active control mode (`1` = Air, `0` = Skin).
    *   `airSet`, `skinSet`, `humSet`: Current setpoints (targets) for temperatures and humidity.
    *   `photo`, `photoTimeRem`: State of the UV treatment and the minutes.seconds (MM.SS) remaining for auto shut-off.
    *   `lang`: Forced system language (0=ES, 1=EN, 2=FR).
    *   `alarmBitmask`: Crucial variable (e.g., `0x60`). Defines active IDs in binary (`(1 << alarmID)`).

### B. Fast Telemetry (`CTRL,TEL`)
Triggered at a minimum frequency of 1 Hz but usually interleaved between the STATE to refresh the graphics. Its byte size is smaller, optimizing LVGL DMA processing.
*   **Format**:
    `CTRL,TEL,<airDet>,<skinDet>,<humDet>,<serverStatus>`
*   **Key Fields**:
    *   `airDet`, `skinDet`: Direct floating measurements (Detected/Sensed).
    *   `serverStatus`: GPRS/WiFi IoT cloud connectivity flags.

### C. Reactive Alarm System (`CTRL,ALM`)
Triggered **spontaneously** by the Motherboard at the precise instant (mathematical rising or falling edge) of an arrest or extinction of a hardware and software alarm (PID offset, thermal, etc.).
*   **Format**:
    `CTRL,ALM,<id>,<short_text>,<long_text>,<active>`
*   **Key Fields**:
    *   `id`: `1` to `9` (Maps to pre-compiled indices like `FAN_ISSUE_ALARM`).
    *   `active`: Boolean `1` or `0` (New Trigger vs Extinction).

## 3. Message Types: HMI → Motherboard

Every send originating from the screen is preceded by the fundamental sub-header `HMI,`.

### A. User Commands (`HMI,...`)
When the interface receives tactile validation of changes (PID slider modified, language changed) and the user closes menus or saves, it packages its local cache and sends it to the main MCU to make it official.
*   **Format**:
    `HMI,<act>,<skinE>,<mode>,<airSet>,<skinSet>,<humSet>,<photo>,<mute>,<lang>,<photoMin>`

### B. Logical Initialization and Handshake (`HMI,UI_READY` & `HMI,REQ,STATE`)
*   **The Problem**: Upon energizing the combined machine, both boards take different times to be functional. The Motherboard (pure RTOS) usually boots in milliseconds and dispatches early initial alarms. LVGL/TFT usually takes 2 to 6 seconds loading the *assets* into the Display's dynamic RAM.
*   **The Solution**: The Motherboard will save any alarm "silently". When the graphics framework draws the first actual HMI frame successfully, it issues a single universal proof: `HMI,UI_READY`.
*   **Result**: Upon receiving the READY (or a RESYNC with `REQ,STATE`), the control board dumps its pending alarm log (`sendAlarmUSB()`) and floods the serial connection ensuring immaculate audiovisual synchrony. When a WiFi SSID is received in `HMI,WIFI...`, it triggers the internal IoT connection routine on the Motherboard.

## 4. Robustness Strategies on Disconnection

In a noisy medical RF environment or upon accidental loss/drop of the Flex with the Display:
1.  **Motherboard Host Resets**: Detects transmission failures ("TX failed:...") and restarts the Virtual Com Port driver (VCP), manipulating RTS/DTR lines at calculated times for the CH340C chip to perform a *Power Cycle*.
2.  **HMI Display Heartbeat**: If an alarm got "stuck hung" in the UI and the control board no longer reports it in the bitmask (`alarmBitmask`) attached to its recurrent 1 Hz telemetry, the HMI performs visual auto-cleaning of expired IDs forcing their `state` to inactive.

## 5. Alternative Binary Protocol (TLV)

An alternative binary protocol is available in the `display_comms.h` driver, prepared for future migration to a more robust format.

### 5.1 Frame Structure
`[0x AA][0x55][Ver][MsgType][Seq:2][PayloadLen:2][TLVs...][CRC16:2]`

| Field | Bytes | Description |
|---|---|---|
| Preamble | 2 | 0xAA, 0x55 |
| Version | 1 | Protocol version (current: 1) |
| MsgType | 1 | Message Type |
| Seq | 2 | Sequence number LE |
| PayloadLen | 2 | Payload length LE |
| Payload | N | TLVs (Type:2, Len:2, Value:N) |
| CRC16 | 2 | CRC16-CCITT (X.25) over bytes [2..end-3] |

### 5.2 Reliability Parameters
*   `DC_ACK_TIMEOUT_MS`: 50 ms
*   `DC_MAX_RETRIES`: 3 attempts
*   `DC_HEARTBEAT_PERIOD_MS`: 1000 ms
*   `DC_RX_IDLE_TIMEOUT_MS`: 200 ms
*   `DC_MAX_FRAME_SIZE`: 1024 bytes
