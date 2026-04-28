#!/usr/bin/env python3
"""
motherboard_simulator.py — IncuNest Motherboard UART Simulator

Simulates the Motherboard-side ASCII protocol (v1.5.0) for testing
the Display HMI firmware without physical hardware.

Usage:
    python3 motherboard_simulator.py --port /dev/ttyUSB0 --scenario normal
    python3 motherboard_simulator.py --port COM3 --scenario overtemp

Scenarios:
    normal      — Sends normal telemetry at 1 Hz, no alarms
    overtemp    — After 10s, sends overtemperature alarm
    skin_fault  — Skin sensor disconnected, switches to AIR mode
    comm_loss   — Stops sending after 30s (tests comm-lost alarm)
    stress      — Max rate messages for stress testing

Protocol Reference: Firmware/PROTOCOL.md (ASCII v1.5.0)

TODO: Fase 5 — Implement full simulator
"""

import argparse
import serial
import time
import threading

# Protocol constants from PROTOCOL.md
MSG_SEP = ','
MSG_END = '\n'
PREFIX_CTRL = 'CTRL'

# Message types
MSG_TEL   = 'TEL'    # Telemetry: temp + humidity
MSG_STATE = 'STATE'  # Full state sync (sent on request or periodic)
MSG_ALM   = 'ALM'    # Individual alarm notification

# Alarm IDs (from alarms.md)
ALM_HUMIDITY      = 1
ALM_TEMPERATURE   = 2
ALM_THERMAL_CUT   = 3
ALM_AIR_SENSOR    = 4
ALM_SKIN_SENSOR   = 5
ALM_SKIN_ISSUE    = 6
ALM_FAN_ISSUE     = 7
ALM_BATTERY       = 8
ALM_POWER_SUPPLY  = 9


def build_tel(air_temp, skin_temp, humidity):
    """Build CTRL,TEL message."""
    return f"{PREFIX_CTRL}{MSG_SEP}{MSG_TEL}{MSG_SEP}{air_temp:.2f}{MSG_SEP}{skin_temp:.2f}{MSG_SEP}{humidity:.1f}{MSG_END}"


def build_state(air_set, skin_set, hum_set, mode, alm_bitmask=0x0000):
    """Build CTRL,STATE message (simplified — 12 fields)."""
    return (f"{PREFIX_CTRL}{MSG_SEP}{MSG_STATE}{MSG_SEP}"
            f"{air_set:.2f}{MSG_SEP}{skin_set:.2f}{MSG_SEP}{hum_set:.1f}{MSG_SEP}"
            f"{'1' if mode else '0'}{MSG_SEP}0{MSG_SEP}0{MSG_SEP}0{MSG_SEP}"
            f"0{MSG_SEP}0{MSG_SEP}{alm_bitmask:#06x}{MSG_END}")


def build_alm(alm_id, active):
    """Build CTRL,ALM message."""
    return f"{PREFIX_CTRL}{MSG_SEP}{MSG_ALM}{MSG_SEP}{alm_id}{MSG_SEP}{'1' if active else '0'}{MSG_END}"


def scenario_normal(ser):
    """Normal operation — temperature 36.0°C, humidity 55%, no alarms."""
    print("[SIM] Scenario: NORMAL")
    t = 0
    while True:
        # Telemetry at 1 Hz
        msg = build_tel(36.0 + 0.1 * (t % 3), 36.5, 55.0)
        ser.write(msg.encode())
        print(f"[TX] {msg.strip()}")

        # State every 10s
        if t % 10 == 0:
            msg = build_state(36.0, 36.5, 55.0, True)
            ser.write(msg.encode())
            print(f"[TX] {msg.strip()}")

        time.sleep(1.0)
        t += 1


def scenario_overtemp(ser):
    """Overtemperature after 10 seconds."""
    print("[SIM] Scenario: OVERTEMP (alarm after 10s)")
    t = 0
    alarm_active = False
    while True:
        air_temp = 36.0 if t < 10 else 38.8  # Spike after 10s
        msg = build_tel(air_temp, 36.5, 55.0)
        ser.write(msg.encode())
        print(f"[TX] {msg.strip()}")

        if t == 10 and not alarm_active:
            alm_msg = build_alm(ALM_TEMPERATURE, True)
            ser.write(alm_msg.encode())
            print(f"[TX] {alm_msg.strip()}")
            alarm_active = True

        time.sleep(1.0)
        t += 1


def rx_thread(ser):
    """Read and log messages from HMI."""
    while True:
        try:
            line = ser.readline().decode('utf-8', errors='replace').strip()
            if line:
                print(f"[RX] {line}")
        except Exception as e:
            print(f"[RX ERROR] {e}")
            break


def main():
    parser = argparse.ArgumentParser(description='IncuNest Motherboard Simulator')
    parser.add_argument('--port', default='/dev/ttyUSB0', help='Serial port')
    parser.add_argument('--baud', type=int, default=115200, help='Baud rate')
    parser.add_argument('--scenario', default='normal',
                        choices=['normal', 'overtemp', 'skin_fault', 'comm_loss', 'stress'],
                        help='Simulation scenario')
    args = parser.parse_args()

    print(f"[SIM] IncuNest Motherboard Simulator v0.1")
    print(f"[SIM] Port: {args.port} @ {args.baud} baud")
    print(f"[SIM] Scenario: {args.scenario}")

    # TODO: Fase 5 — Add all scenarios (skin_fault, comm_loss, stress)
    # For now only normal and overtemp are implemented

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.1)
        time.sleep(0.5)  # Port settle

        # Start RX monitor thread
        t = threading.Thread(target=rx_thread, args=(ser,), daemon=True)
        t.start()

        # Send initial STATE to simulate boot sync
        time.sleep(1.0)
        state_msg = build_state(36.0, 36.5, 55.0, True)
        ser.write(state_msg.encode())
        print(f"[TX] Initial STATE: {state_msg.strip()}")

        # Run scenario
        scenarios = {
            'normal':   scenario_normal,
            'overtemp': scenario_overtemp,
        }
        scenarios.get(args.scenario, scenario_normal)(ser)

    except serial.SerialException as e:
        print(f"[ERROR] Could not open port {args.port}: {e}")
        print("        Make sure the device is connected and the port is correct.")
        print("        On Linux: sudo chmod a+rw /dev/ttyUSB0")
    except KeyboardInterrupt:
        print("\n[SIM] Stopped by user")


if __name__ == '__main__':
    main()
