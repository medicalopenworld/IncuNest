# Firmware Folder Reorganization — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reorganize Display_HMI and motherBoard firmware projects — clear layer structure in `src/`, organized `include/` subdirectories, no clutter.

**Architecture:** Pure file-level reorganization, no logic changes. `src/` gets hal/drivers/tasks/modules/state/ui/system layers. `include/` splits into config/, protocol/, tasks/, ui/. `platformio.ini` updated to reflect new paths. Both projects must compile cleanly after each task.

**Tech Stack:** PlatformIO, ESP32/ESP32-S3, Arduino framework, FreeRTOS, LVGL 8.x

**Spec:** `docs/superpowers/specs/2026-06-08-firmware-folder-reorganization-design.md`

---

### Task 1: Create dev branch

**Files:** none

- [ ] **Create and switch to dev branch**
```bash
git checkout -b dev
```
Expected: `Switched to a new branch 'dev'`

- [ ] **Verify**
```bash
git branch
```
Expected: `* dev` is the current branch.

- [ ] **Commit**
```bash
git commit --allow-empty -m "chore: open dev branch for firmware folder reorganization"
```

---

### Task 2: Update .gitignore — Display_HMI

**Files:**
- Modify: `Display_HMI/.gitignore`

- [ ] **Check which files are currently tracked but shouldn't be**
```bash
git ls-files Display_HMI/include/Credentials.h Display_HMI/compile_commands.json
git ls-files Display_HMI/managed_components/
```
Note which files appear — those need `git rm --cached`.

- [ ] **Untrack sensitive/artifact files**
```bash
git rm --cached Display_HMI/include/Credentials.h 2>$null
git rm --cached Display_HMI/compile_commands.json 2>$null
git rm -r --cached Display_HMI/managed_components/ 2>$null
```
(Errors for files not tracked are harmless.)

- [ ] **Append to Display_HMI/.gitignore**

Open `Display_HMI/.gitignore` and add at the end:
```gitignore
# Build artifacts
compile_commands.json
managed_components/

# Logs and debug
*.log
serial_*.txt

# Credentials (keep locally, never commit)
include/Credentials.h

# IDE
.vscode/

# Python cache
__pycache__/
*.pyc
```

- [ ] **Commit**
```bash
git add Display_HMI/.gitignore
git commit -m "chore(hmi): update .gitignore — exclude credentials, artifacts, logs"
```

---

### Task 3: Update .gitignore — motherBoard

**Files:**
- Modify: `motherBoard/.gitignore`

- [ ] **Check what's tracked that shouldn't be**
```bash
git ls-files motherBoard/include/Credentials.h motherBoard/compile_commands.json
```

- [ ] **Untrack if present**
```bash
git rm --cached motherBoard/include/Credentials.h 2>$null
git rm --cached motherBoard/compile_commands.json 2>$null
```

- [ ] **Append to motherBoard/.gitignore**

Open `motherBoard/.gitignore` and add at the end:
```gitignore
# Build artifacts
compile_commands.json

# Logs and debug
*.log
serial_*.txt

# Credentials (keep locally, never commit)
include/Credentials.h

# IDE
.vscode/

# Python cache
__pycache__/
*.pyc
```

- [ ] **Commit**
```bash
git add motherBoard/.gitignore
git commit -m "chore(mb): update .gitignore — exclude credentials, artifacts, logs"
```

---

### Task 4: Delete debris files — Display_HMI

**Files to delete:**
- `Display_HMI/SquareLineProject/`
- `Display_HMI/flash_Display_HMI/` and `flash_Display_HMI.zip`
- `Display_HMI/crash_session_hmi.log`
- `Display_HMI/filelist.txt`

- [ ] **Remove tracked files via git**
```bash
git rm -r "Display_HMI/SquareLineProject/"
git rm "Display_HMI/flash_Display_HMI.zip" 2>$null
git rm "Display_HMI/crash_session_hmi.log" 2>$null
git rm "Display_HMI/filelist.txt" 2>$null
```

- [ ] **Remove untracked files/folders**
```bash
Remove-Item -Recurse -Force "Display_HMI/flash_Display_HMI" -ErrorAction SilentlyContinue
```

- [ ] **Commit**
```bash
git add -A Display_HMI/
git commit -m "chore(hmi): remove SquareLine project, build artifacts, and debug logs"
```

---

### Task 5: Delete debris and legacy files — motherBoard

**Files to delete:**
- `motherBoard/build_log.txt`
- `motherBoard/serial_20260531_000831.txt`, `serial_20260601_002629.txt`, `serial_20260601_002827.txt`
- `motherBoard/src/drawGraphicInterphace.cpp`
- `motherBoard/src/userInterface.cpp`
- `motherBoard/src/UI_mainMenu.cpp`
- `motherBoard/src/UI_actuatorsProgress.cpp`
- `motherBoard/src/UI_calibration.cpp`
- `motherBoard/src/UI_settings.cpp`
- `motherBoard/src/sensors.cpp`

- [ ] **Remove log/artifact files**
```bash
git rm motherBoard/build_log.txt 2>$null
git rm "motherBoard/serial_20260531_000831.txt" 2>$null
git rm "motherBoard/serial_20260601_002629.txt" 2>$null
git rm "motherBoard/serial_20260601_002827.txt" 2>$null
```

- [ ] **Remove legacy src files**
```bash
git rm motherBoard/src/drawGraphicInterphace.cpp
git rm motherBoard/src/userInterface.cpp
git rm motherBoard/src/UI_mainMenu.cpp
git rm motherBoard/src/UI_actuatorsProgress.cpp
git rm motherBoard/src/UI_calibration.cpp
git rm motherBoard/src/UI_settings.cpp
git rm motherBoard/src/sensors.cpp
```

- [ ] **Verify motherBoard still compiles**

Run from `motherBoard/`:
```bash
pio run -e IncuNest_V17
```
Expected: clean build. If an undefined reference mentions a deleted file, find which `.cpp` still `#include`s it and remove that include line.

- [ ] **Commit**
```bash
git add -A motherBoard/
git commit -m "chore(mb): delete legacy UI code and debug log files"
```

---

### Task 6: Move partition CSVs to partitions/

**Files:**
- Create: `Display_HMI/partitions/`
- Create: `motherBoard/partitions/`
- Modify: `Display_HMI/platformio.ini` line 23
- Modify: `motherBoard/platformio.ini` line 5

- [ ] **Create partitions/ and move CSVs — Display_HMI**
```bash
New-Item -ItemType Directory -Path Display_HMI/partitions
git mv "Display_HMI/IncuNest_display_v0.csv" "Display_HMI/partitions/display_v0.csv"
git mv "Display_HMI/IncuNest_display_v1_audio.csv" "Display_HMI/partitions/display_v1_audio.csv"
```

- [ ] **Update Display_HMI/platformio.ini**

Change line 23 from:
```ini
board_build.partitions = IncuNest_display_v1_audio.csv
```
to:
```ini
board_build.partitions = partitions/display_v1_audio.csv
```

- [ ] **Create partitions/ and move CSVs — motherBoard**
```bash
New-Item -ItemType Directory -Path motherBoard/partitions
git mv "motherBoard/ESP32_OTA_partition_4MB.csv" "motherBoard/partitions/ESP32_4MB.csv"
git mv "motherBoard/ESP32_OTA_partition_16MB.csv" "motherBoard/partitions/ESP32_16MB.csv"
git mv "motherBoard/ESP32S3_OTA_partition_8MB.csv" "motherBoard/partitions/ESP32S3_8MB.csv"
```

- [ ] **Update motherBoard/platformio.ini**

Change line 5 from:
```ini
board_build.partitions = ESP32S3_OTA_partition_8MB.csv
```
to:
```ini
board_build.partitions = partitions/ESP32S3_8MB.csv
```

- [ ] **Verify both compile**
```bash
cd Display_HMI; pio run; cd ../motherBoard; pio run -e IncuNest_V17
```
Expected: both succeed. If partition error, the path in `platformio.ini` doesn't match the renamed file — compare them.

- [ ] **Commit**
```bash
git add Display_HMI/ motherBoard/
git commit -m "chore: move partition CSVs to partitions/ and rename for clarity"
```

---

### Task 7: Move utility scripts to tools/

**Files:**
- Create: `Display_HMI/tools/`
- Create: `motherBoard/tools/`

- [ ] **Move Display_HMI scripts**
```bash
New-Item -ItemType Directory -Path Display_HMI/tools
git mv Display_HMI/copy_firmware.py Display_HMI/tools/copy_firmware.py
git mv Display_HMI/flash_display.bat Display_HMI/tools/flash_display.bat
git mv Display_HMI/prepare_firmware.bat Display_HMI/tools/prepare_firmware.bat
```

- [ ] **Move motherBoard scripts**
```bash
New-Item -ItemType Directory -Path motherBoard/tools
git mv motherBoard/copy_firmware.py motherBoard/tools/copy_firmware.py
git mv motherBoard/monitor_display.py motherBoard/tools/monitor_display.py
git mv motherBoard/monitor_motherboard.py motherBoard/tools/monitor_motherboard.py
```
> `motherBoard/pre_native.py` stays at root — PlatformIO invokes it via `extra_scripts = pre:pre_native.py` and requires it next to `platformio.ini`.

- [ ] **Commit**
```bash
git add Display_HMI/tools/ motherBoard/tools/
git add -u Display_HMI/ motherBoard/
git commit -m "chore: move utility scripts to tools/ in both firmware projects"
```

---

### Task 8: Reorganize Display_HMI src/

**Files:**
- Create: `Display_HMI/src/drivers/`
- Create: `Display_HMI/src/tasks/`
- Create: `Display_HMI/src/ui/`
- Create: `Display_HMI/src/ui/assets/`
- Move: 25 source files
- Modify: `Display_HMI/platformio.ini` (build_src_filter)

- [ ] **Create new layer folders**
```bash
New-Item -ItemType Directory -Path Display_HMI/src/drivers
New-Item -ItemType Directory -Path Display_HMI/src/tasks
New-Item -ItemType Directory -Path Display_HMI/src/ui
New-Item -ItemType Directory -Path Display_HMI/src/ui/assets
```

- [ ] **Move files to drivers/**
```bash
git mv Display_HMI/src/buzzer.cpp Display_HMI/src/drivers/buzzer.cpp
git mv Display_HMI/src/EEPROM.cpp Display_HMI/src/drivers/EEPROM.cpp
```

- [ ] **Move files to tasks/**
```bash
git mv Display_HMI/src/CommTask.cpp Display_HMI/src/tasks/CommTask.cpp
git mv Display_HMI/src/UITask.cpp Display_HMI/src/tasks/UITask.cpp
git mv Display_HMI/src/AudioManager.cpp Display_HMI/src/tasks/AudioManager.cpp
git mv Display_HMI/src/Wifi_OTA.cpp Display_HMI/src/tasks/Wifi_OTA.cpp
```

- [ ] **Move files to ui/**
```bash
git mv Display_HMI/src/ElementsCreation.cpp Display_HMI/src/ui/ElementsCreation.cpp
git mv Display_HMI/src/ui_helpers.c Display_HMI/src/ui/ui_helpers.c
```

- [ ] **Move LVGL image assets to ui/assets/**
```bash
git mv Display_HMI/src/ui_img_auto_air_png.c     Display_HMI/src/ui/assets/
git mv Display_HMI/src/ui_img_bebe_icon_png.c     Display_HMI/src/ui/assets/
git mv Display_HMI/src/ui_img_candado_png.c       Display_HMI/src/ui/assets/
git mv Display_HMI/src/ui_img_chart_png.c         Display_HMI/src/ui/assets/
git mv Display_HMI/src/ui_img_check_png.c         Display_HMI/src/ui/assets/
git mv Display_HMI/src/ui_img_flag_togo_png.c     Display_HMI/src/ui/assets/
git mv Display_HMI/src/ui_img_flecha_png.c        Display_HMI/src/ui/assets/
git mv Display_HMI/src/ui_img_gota_png.c          Display_HMI/src/ui/assets/
git mv Display_HMI/src/ui_img_incunest2_png.c     Display_HMI/src/ui/assets/
git mv Display_HMI/src/ui_img_mute_icon_png.c     Display_HMI/src/ui/assets/
git mv Display_HMI/src/ui_img_pulse_png.c         Display_HMI/src/ui/assets/
git mv Display_HMI/src/ui_img_sjd_png.c           Display_HMI/src/ui/assets/
git mv Display_HMI/src/ui_img_triangulo_abajo_png.c  Display_HMI/src/ui/assets/
git mv Display_HMI/src/ui_img_triangulo_arriba_png.c Display_HMI/src/ui/assets/
git mv Display_HMI/src/ui_img_windvector_png.c    Display_HMI/src/ui/assets/
git mv Display_HMI/src/ui_img_1007688293.c        Display_HMI/src/ui/assets/
git mv Display_HMI/src/ui_img_1084506651.c        Display_HMI/src/ui/assets/
git mv Display_HMI/src/ui_img_1370137984.c        Display_HMI/src/ui/assets/
git mv Display_HMI/src/ui_img_1508956403.c        Display_HMI/src/ui/assets/
git mv Display_HMI/src/ui_img_296721678.c         Display_HMI/src/ui/assets/
git mv Display_HMI/src/ui_img_302897630.c         Display_HMI/src/ui/assets/
```

- [ ] **Update build_src_filter in Display_HMI/platformio.ini**

AudioManager.cpp is excluded from build (it is superseded by modules/audio/). After moving it to tasks/, the filter path must be updated.

Change the `[env]` section line:
```ini
build_src_filter = +<*> -<AudioManager.cpp>
```
to:
```ini
build_src_filter = +<*> -<tasks/AudioManager.cpp>
```

- [ ] **Verify compile**
```bash
cd Display_HMI; pio run
```
Expected: clean build. PlatformIO automatically finds all `.cpp`/`.c` files recursively in `src/`, so no further changes needed for source files. If errors mention missing headers, proceed to Task 9 and compile again after.

- [ ] **Commit**
```bash
git add Display_HMI/src/ Display_HMI/platformio.ini
git commit -m "refactor(hmi): reorganize src/ into drivers/tasks/ui/assets layers"
```

---

### Task 9: Reorganize Display_HMI include/ and update platformio.ini

**Files:**
- Create: `Display_HMI/include/config/`
- Create: `Display_HMI/include/protocol/`
- Create: `Display_HMI/include/tasks/`
- Create: `Display_HMI/include/ui/`
- Move: 15 header files
- Modify: `Display_HMI/platformio.ini` (build_flags in both envs)

**Strategy:** After moving headers to subdirectories, add each subdirectory to `build_flags` with `-I`. This means no `#include` statements in any source file need to change — the compiler will find `lv_conf.h`, `display_config.h`, etc. by their filename regardless of which subdirectory they live in.

- [ ] **Create subdirectories**
```bash
New-Item -ItemType Directory -Path Display_HMI/include/config
New-Item -ItemType Directory -Path Display_HMI/include/protocol
New-Item -ItemType Directory -Path Display_HMI/include/tasks
New-Item -ItemType Directory -Path Display_HMI/include/ui
```

- [ ] **Move config headers**
```bash
git mv Display_HMI/include/display_config.h   Display_HMI/include/config/display_config.h
git mv Display_HMI/include/EEPROM_defines.h   Display_HMI/include/config/EEPROM_defines.h
git mv Display_HMI/include/lv_conf.h          Display_HMI/include/config/lv_conf.h
```

- [ ] **Move protocol headers**
```bash
git mv Display_HMI/include/display_comms.h      Display_HMI/include/protocol/display_comms.h
git mv Display_HMI/include/Credentials_public.h Display_HMI/include/protocol/Credentials_public.h
```

- [ ] **Move task headers**
```bash
git mv Display_HMI/include/CommTask.h    Display_HMI/include/tasks/CommTask.h
git mv Display_HMI/include/UITask.h     Display_HMI/include/tasks/UITask.h
git mv Display_HMI/include/AudioManager.h Display_HMI/include/tasks/AudioManager.h
git mv Display_HMI/include/Wifi_OTA.h   Display_HMI/include/tasks/Wifi_OTA.h
git mv Display_HMI/include/buzzer.h     Display_HMI/include/tasks/buzzer.h
```

- [ ] **Move UI headers**
```bash
git mv Display_HMI/include/ui.h          Display_HMI/include/ui/ui.h
git mv Display_HMI/include/ui_events.h   Display_HMI/include/ui/ui_events.h
git mv Display_HMI/include/ui_helpers.h  Display_HMI/include/ui/ui_helpers.h
git mv Display_HMI/include/hmi_ding.h    Display_HMI/include/ui/hmi_ding.h
```

- [ ] **Update build_flags in Display_HMI/platformio.ini**

Both `[env:main]` and `[env:crash_test_hmi]` have a `build_flags` block. In each, replace:
```ini
    -I./include
```
with:
```ini
    -I./include
    -I./include/config
    -I./include/protocol
    -I./include/tasks
    -I./include/ui
```

After edits, `[env:main]` build_flags should look like:
```ini
[env:main]
build_flags =
    -D LV_LVGL_H_INCLUDE_SIMPLE
    -I./include
    -I./include/config
    -I./include/protocol
    -I./include/tasks
    -I./include/ui
    -D BOARD_HAS_PSRAM
    -D CORE_DEBUG_LEVEL=3
    ;-D ARDUINO_USB_CDC_ON_BOOT=1
    ;-D CRASH_TEST_HMI=1
    ;-D CRASH_TEST_HMI_DELAY_S=30
```

And `[env:crash_test_hmi]`:
```ini
[env:crash_test_hmi]
build_flags =
    -D LV_LVGL_H_INCLUDE_SIMPLE
    -I./include
    -I./include/config
    -I./include/protocol
    -I./include/tasks
    -I./include/ui
    -D BOARD_HAS_PSRAM
    -D CORE_DEBUG_LEVEL=3
    -D CRASH_TEST_HMI=1
    -D CRASH_TEST_HMI_DELAY_S=30
```

- [ ] **Verify compile**
```bash
cd Display_HMI; pio run
```
Expected: clean build for both envs. If a header is not found, check its new location is listed in build_flags `-I` paths. `lv_conf.h` must be findable — it is now under `include/config/`, which is listed.

- [ ] **Commit**
```bash
git add Display_HMI/include/ Display_HMI/platformio.ini
git commit -m "refactor(hmi): organize include/ into config/protocol/tasks/ui subdirs"
```

---

### Task 10: Reorganize motherBoard src/

**Files:**
- Create: `motherBoard/src/tasks/`
- Create: `motherBoard/src/system/`
- Move: 18 source files + 3 driver headers
- Modify: `motherBoard/platformio.ini` (build_src_filter for native env)

- [ ] **Create new layer folders**

`drivers/`, `hal/`, `modules/`, `state/` already exist. Only create the missing ones:
```bash
New-Item -ItemType Directory -Path motherBoard/src/tasks
New-Item -ItemType Directory -Path motherBoard/src/system
```

- [ ] **Move FreeRTOS task files to tasks/**
```bash
git mv motherBoard/src/CommTask.cpp      motherBoard/src/tasks/CommTask.cpp
git mv motherBoard/src/Wifi_OTA.cpp      motherBoard/src/tasks/Wifi_OTA.cpp
git mv motherBoard/src/DriveUpload.cpp   motherBoard/src/tasks/DriveUpload.cpp
git mv motherBoard/src/GPRS.cpp          motherBoard/src/tasks/GPRS.cpp
```

- [ ] **Move driver files to drivers/**

Source files:
```bash
git mv motherBoard/src/BQ25730.cpp            motherBoard/src/drivers/BQ25730.cpp
git mv motherBoard/src/SPO2.cpp               motherBoard/src/drivers/SPO2.cpp
git mv motherBoard/src/IncuNest_humidifier.cpp motherBoard/src/drivers/IncuNest_humidifier.cpp
```

Colocate their headers (moving from include/ to src/drivers/):
```bash
git mv motherBoard/include/BQ25730.h            motherBoard/src/drivers/BQ25730.h
git mv motherBoard/include/SPO2.h               motherBoard/src/drivers/SPO2.h
git mv motherBoard/include/IncuNest_humidifier.h motherBoard/src/drivers/IncuNest_humidifier.h
```

- [ ] **Move system infrastructure files to system/**
```bash
git mv motherBoard/src/initHardware.cpp         motherBoard/src/system/initHardware.cpp
git mv motherBoard/src/ESP32_config.cpp         motherBoard/src/system/ESP32_config.cpp
git mv motherBoard/src/EEPROM.cpp               motherBoard/src/system/EEPROM.cpp
git mv motherBoard/src/CrashReporter.cpp        motherBoard/src/system/CrashReporter.cpp
git mv motherBoard/src/ISR.cpp                  motherBoard/src/system/ISR.cpp
git mv motherBoard/src/math.cpp                 motherBoard/src/system/math.cpp
git mv motherBoard/src/security.cpp             motherBoard/src/system/security.cpp
git mv motherBoard/src/updateData.cpp           motherBoard/src/system/updateData.cpp
git mv motherBoard/src/calibrateSensors.cpp     motherBoard/src/system/calibrateSensors.cpp
git mv motherBoard/src/usb_host_vcp.cpp         motherBoard/src/system/usb_host_vcp.cpp
git mv motherBoard/src/usb_host_ch34x_vcp.cpp   motherBoard/src/system/usb_host_ch34x_vcp.cpp
git mv motherBoard/src/cdc_acm_host.c           motherBoard/src/system/cdc_acm_host.c
git mv motherBoard/src/PID.cpp                  motherBoard/src/system/PID.cpp
git mv motherBoard/src/Buzzer.cpp               motherBoard/src/system/Buzzer.cpp
```

- [ ] **Update native env build_src_filter in motherBoard/platformio.ini**

The `[env:native]` section references source paths that do not change (modules/ is unchanged). Verify the current filter still points to valid paths:
```ini
build_src_filter =
    +<modules/control/alarm_machine.cpp>
    +<modules/control/pid_wrapper.cpp>
```
These paths are unchanged — no edit needed. Just confirm the lines are still present.

- [ ] **Verify compile**
```bash
cd motherBoard; pio run -e IncuNest_V17
```
Expected: clean build. PlatformIO scans `src/` recursively so all moved files are found automatically. If a header is not found (e.g., `BQ25730.h`), it is now in `src/drivers/` — that path fix is done in Task 11.

- [ ] **Commit**
```bash
git add motherBoard/src/ motherBoard/include/
git commit -m "refactor(mb): reorganize src/ into drivers/tasks/system layers"
```

---

### Task 11: Reorganize motherBoard include/ and update platformio.ini

**Files:**
- Create: `motherBoard/include/config/`
- Create: `motherBoard/include/protocol/`
- Create: `motherBoard/include/tasks/`
- Move: 11 header files
- Modify: `motherBoard/platformio.ini` (build_flags in V17 and V16 envs)

**Strategy:** Same as Task 9 — add subdirs to build_flags so no `#include` statements need changing.

- [ ] **Create subdirectories**
```bash
New-Item -ItemType Directory -Path motherBoard/include/config
New-Item -ItemType Directory -Path motherBoard/include/protocol
New-Item -ItemType Directory -Path motherBoard/include/tasks
```
> `include/usb/` already exists — no change needed there.

- [ ] **Move config headers**
```bash
git mv motherBoard/include/board.h           motherBoard/include/config/board.h
git mv motherBoard/include/task_config.h     motherBoard/include/config/task_config.h
git mv motherBoard/include/preferences_keys.h motherBoard/include/config/preferences_keys.h
git mv motherBoard/include/telemetry_keys.h  motherBoard/include/config/telemetry_keys.h
git mv motherBoard/include/ui_constants.h    motherBoard/include/config/ui_constants.h
git mv motherBoard/include/ESP32_config.h    motherBoard/include/config/ESP32_config.h
```

- [ ] **Move protocol headers**
```bash
git mv motherBoard/include/CommTask.h        motherBoard/include/protocol/CommTask.h
git mv motherBoard/include/Credentials_public.h motherBoard/include/protocol/Credentials_public.h
```

- [ ] **Move task headers**
```bash
git mv motherBoard/include/Wifi_OTA.h       motherBoard/include/tasks/Wifi_OTA.h
git mv motherBoard/include/DriveUpload.h    motherBoard/include/tasks/DriveUpload.h
git mv motherBoard/include/GPRS.h           motherBoard/include/tasks/GPRS.h
git mv motherBoard/include/CrashReporter.h  motherBoard/include/tasks/CrashReporter.h
git mv motherBoard/include/PID.h            motherBoard/include/tasks/PID.h
```

- [ ] **Update build_flags in motherBoard/platformio.ini**

Both `[env:IncuNest_V17]` and `[env:IncuNest_V16]` have build_flags. In each, replace:
```ini
    -I./include
```
with:
```ini
    -I./include
    -I./include/config
    -I./include/protocol
    -I./include/tasks
    -Isrc/drivers
```

After edits, `[env:IncuNest_V17]` build_flags should look like:
```ini
[env:IncuNest_V17]
extends = common
build_flags =
    -DHW_NUM=17
    -DCORE_DEBUG_LEVEL=3
    -g3 -O0
    -DUSER_SETUP_LOADED=1
    -DTFT_INVERSION_ON=1
    -DILI9341_2_DRIVER=1
    -DTFT_MISO=19 -DTFT_MOSI=23 -DTFT_SCLK=18
    -DTFT_CS=15 -DTFT_DC=0 -DTFT_RST=-1 -DTOUCH_CS=-1
    -DLOAD_GLCD=1 -DSPI_FREQUENCY=27000000
    -DBQ25730_TEST
    -D LV_LVGL_H_INCLUDE_SIMPLE
    -I./include
    -I./include/config
    -I./include/protocol
    -I./include/tasks
    -Isrc/drivers
    -Wno-attributes
```

And `[env:IncuNest_V16]` identically (same additions, without `-DBQ25730_TEST`).

- [ ] **Verify both motherBoard envs compile**
```bash
cd motherBoard; pio run -e IncuNest_V17; pio run -e IncuNest_V16
```
Expected: both clean. If a header is still not found, check that its new directory is listed in build_flags.

- [ ] **Commit**
```bash
git add motherBoard/include/ motherBoard/platformio.ini
git commit -m "refactor(mb): organize include/ into config/protocol/tasks subdirs"
```

---

### Task 12: Final verification

- [ ] **Full clean build — Display_HMI**
```bash
cd Display_HMI; pio run -e main; pio run -e crash_test_hmi
```
Both must succeed.

- [ ] **Full clean build — motherBoard**
```bash
cd motherBoard; pio run -e IncuNest_V17; pio run -e IncuNest_V16
```
Both must succeed.

- [ ] **Verify Credentials.h is not tracked**
```bash
git ls-files Display_HMI/include/Credentials.h motherBoard/include/Credentials.h
```
Expected: no output (not tracked).

- [ ] **Check git status is clean**
```bash
git status
```
Expected: only untracked files (none that should be committed). No modified tracked files.

- [ ] **Final commit**
```bash
git commit --allow-empty -m "chore: firmware folder reorganization complete — both projects compile on dev"
```

---

## Summary of structural changes

```
Display_HMI/
  partitions/          ← partition CSVs (renamed)
  tools/               ← utility scripts
  src/
    drivers/           ← buzzer, EEPROM
    tasks/             ← CommTask, UITask, AudioManager, Wifi_OTA
    ui/                ← ElementsCreation, ui_helpers
      assets/          ← all ui_img_*.c (LVGL image data)
    hal/ modules/ state/  ← unchanged
  include/
    config/            ← display_config, EEPROM_defines, lv_conf
    protocol/          ← display_comms, Credentials_public
    tasks/             ← CommTask, UITask, AudioManager, Wifi_OTA, buzzer
    ui/                ← ui.h, ui_events, ui_helpers, hmi_ding

motherBoard/
  partitions/          ← partition CSVs (renamed)
  tools/               ← utility scripts (pre_native.py stays at root)
  src/
    drivers/           ← drv_*, BQ25730, SPO2, IncuNest_humidifier + their .h
    tasks/             ← CommTask, Wifi_OTA, DriveUpload, GPRS
    system/            ← initHardware, ESP32_config, EEPROM, ISR, math, security...
    hal/ modules/ state/  ← unchanged
  include/
    config/            ← board, task_config, preferences_keys, telemetry_keys...
    protocol/          ← CommTask, Credentials_public
    tasks/             ← Wifi_OTA, DriveUpload, GPRS, CrashReporter, PID
    usb/               ← unchanged
```
