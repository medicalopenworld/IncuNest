# IncuNest Firmware Flasher — Design Spec

**Date:** 2026-05-20
**Status:** Approved

---

## Overview

A standalone Windows GUI tool (`IncuNest_Flasher.exe`) that allows non-technical users to flash firmware to either the IncuNest motherBoard or Display HMI via USB, without installing any IDE or development tools.

**Phase 1 (this spec):** firmware binaries are distributed alongside the `.exe` in a zip package.
**Phase 2 (future):** the tool downloads the latest binaries automatically from GitHub Releases.

---

## Target boards

| Board | Module | Flash | PSRAM |
|---|---|---|---|
| motherBoard | ESP32-S3-WROOM-1-N8 | 8 MB | None |
| Display HMI | ESP32-S3-WROOM-1-R16N8 | 16 MB | 8 MB OPI |

Both connect via USB. The ESP32-S3 modules enter flash mode automatically via DTR/RTS — no manual button press required.

---

## Architecture

Single Python application packaged as a standalone `.exe` with PyInstaller. No installation required on the end-user machine.

**Python dependencies:** `esptool`, `pyserial`. Both are bundled by PyInstaller.

**Firmware binaries** are distributed in a `firmware/` folder alongside the `.exe` inside a zip package. They are NOT embedded in the `.exe` — this allows firmware updates without rebuilding the tool.

### Project structure (inside repo)

```
tools/flasher/
├── main.py              ← Tkinter app entry point
├── flasher.py           ← esptool wrapper (flashing logic, runs in thread)
├── detector.py          ← board detection by flash size
├── firmware/
│   ├── motherboard/
│   │   ├── bootloader.bin
│   │   ├── partitions.bin
│   │   └── firmware.bin
│   └── display_hmi/
│       ├── bootloader.bin
│       ├── partitions.bin
│       ├── ota_data_initial.bin
│       └── firmware.bin
├── flasher.spec         ← PyInstaller build spec
└── build.bat            ← builds dist/IncuNest_Flasher.exe
```

---

## GUI

Single fixed window (~480×400 px). Top-to-bottom flow:

1. **Port selector** — dropdown listing available COM ports + refresh button
2. **Detect button** — reads flash ID via esptool, identifies board
3. **Board info** — displays detected board name and flash size
4. **Flash button** — enabled only after successful detection
5. **Progress bar** — real percentage from esptool write callbacks
6. **Log area** — scrollable text with step-by-step messages; success in green, errors in red

Flashing runs in a background thread so the UI remains responsive.

---

## Board detection

`detector.py` connects to the selected COM port, reads the flash ID with esptool, and maps flash size to board:

| Flash size detected | Board identified |
|---|---|
| 8 MB | motherBoard |
| 16 MB | Display HMI |
| Anything else | Error — unrecognized board |

---

## Flash addresses

### motherBoard

| File | Address |
|---|---|
| `bootloader.bin` | `0x0000` |
| `partitions.bin` | `0x8000` |
| `firmware.bin` | `0x10000` |

### Display HMI

| File | Address |
|---|---|
| `bootloader.bin` | `0x0000` |
| `partitions.bin` | `0x8000` |
| `ota_data_initial.bin` | `0xE000` |
| `firmware.bin` | `0x10000` |

Addresses are derived from the partition CSV files in the repo:
- `Display_HMI/IncuNest_display_v1_audio.csv`
- `motherBoard/ESP32S3_OTA_partition_8MB.csv`

---

## Error handling

| Situation | User-facing message |
|---|---|
| No COM ports available | "No se encontró ningún puerto COM. Conecta la placa y pulsa Actualizar." |
| Port busy | "No se puede abrir el puerto. ¿Está abierto en otro programa?" |
| Unrecognized flash size | "Placa no reconocida (Xmb). Solo se admiten motherBoard y Display HMI." |
| Connection timeout | "No se pudo conectar. Comprueba el cable y vuelve a intentarlo." |
| Write error | esptool message + "Flasheo fallido. Vuelve a intentarlo." |
| Success | "¡Flasheo completado con éxito!" (green) |

---

## Distribution (Phase 1)

1. Developer compiles firmware in PlatformIO.
2. Developer copies `.bin` files into `tools/flasher/firmware/motherboard/` and `tools/flasher/firmware/display_hmi/`.
3. Developer runs `build.bat` → PyInstaller produces `dist/IncuNest_Flasher.exe`.
4. Developer zips `IncuNest_Flasher.exe` + `firmware/` folder together.
5. End user unzips anywhere and double-clicks `IncuNest_Flasher.exe`.

---

## Out of scope (Phase 1)

- GitHub Releases download (Phase 2)
- macOS / Linux support
- Flashing two boards simultaneously
- OTA updates over WiFi
- Firmware version display
