# Firmware Folder Reorganization — Design Spec

**Date:** 2026-06-08  
**Branch:** dev  
**Scope:** Display_HMI and motherBoard firmware projects

---

## 1. Goals

- Make both firmware projects navigable for contributors unfamiliar with the codebase.
- Organize `src/` by architectural layer so each folder has a single, obvious purpose.
- Remove legacy code, debug artifacts, and SquareLine design tool references.
- Keep `Credentials.h` out of version control.

## 2. Branch

All changes are made on the `dev` branch.

---

## 3. Root-Level Cleanup (both projects)

### Files to delete permanently

**Display_HMI/**
- `SquareLineProject/` — UI design tool project, not part of firmware
- `flash_Display_HMI/` and `flash_Display_HMI.zip` — build artifacts
- `crash_session_hmi.log` — session log
- `filelist.txt` — temporary file

**motherBoard/**
- `build_log.txt` — build artifact
- `serial_20260531_000831.txt`, `serial_20260601_002629.txt`, `serial_20260601_002827.txt` — serial port logs

### `.gitignore` additions (both projects)

```gitignore
# Build artifacts
compile_commands.json
.pio/
managed_components/

# Logs and debug
*.log
serial_*.txt

# Credentials
include/Credentials.h

# IDE
.vscode/

# Python cache
__pycache__/
*.pyc
```

### Partition tables → `partitions/` subfolder

**Display_HMI/partitions/**
- `display_v0.csv` (was `IncuNest_display_v0.csv`)
- `display_v1_audio.csv` (was `IncuNest_display_v1_audio.csv`)

**motherBoard/partitions/**
- `ESP32_4MB.csv` (was `ESP32_OTA_partition_4MB.csv`)
- `ESP32_16MB.csv` (was `ESP32_OTA_partition_16MB.csv`)
- `ESP32S3_8MB.csv` (was `ESP32S3_OTA_partition_8MB.csv`)

`platformio.ini` `board_build.partitions` references must be updated to new paths.

### Utility scripts → `tools/` subfolder

**Display_HMI/tools/**
- `copy_firmware.py`
- `flash_display.bat`
- `prepare_firmware.bat`

**motherBoard/tools/**
- `copy_firmware.py`
- `monitor_display.py`
- `monitor_motherboard.py`

> `pre_native.py` (motherBoard) stays at root — it is invoked directly by PlatformIO via `extra_scripts` and must remain alongside `platformio.ini`.

---

## 4. Display_HMI — Internal Reorganization

### Legacy code to remove

None identified in Display_HMI (legacy already removed in prior refactors).

### `src/` target structure

```
src/
  main.cpp

  hal/
    hal_hmi.cpp / .h              # low-level display + touch driver

  drivers/
    buzzer.cpp / .h               # was src/buzzer.cpp + include/buzzer.h
    EEPROM.cpp                    # was src/EEPROM.cpp

  tasks/
    CommTask.cpp                  # was src/CommTask.cpp
    UITask.cpp                    # was src/UITask.cpp
    AudioManager.cpp              # was src/AudioManager.cpp

  modules/
    audio/
      hmi_audio_module.cpp / .h
    comm/
      hmi_comm_module.cpp / .h

  state/
    hmi_state.cpp / .h

  ui/
    ElementsCreation.cpp          # was src/ElementsCreation.cpp
    ui_helpers.c                  # was src/ui_helpers.c
    assets/                       # LVGL image data arrays (generated, not edited by hand)
      ui_img_auto_air_png.c
      ui_img_bebe_icon_png.c
      ui_img_candado_png.c
      ui_img_chart_png.c
      ui_img_check_png.c
      ui_img_flag_togo_png.c
      ui_img_flecha_png.c
      ui_img_gota_png.c
      ui_img_incunest2_png.c
      ui_img_mute_icon_png.c
      ui_img_pulse_png.c
      ui_img_sjd_png.c
      ui_img_triangulo_abajo_png.c
      ui_img_triangulo_arriba_png.c
      ui_img_windvector_png.c
      ui_img_1007688293.c
      ui_img_1084506651.c
      ui_img_1370137984.c
      ui_img_1508956403.c
      ui_img_296721678.c
      ui_img_302897630.c
```

### `include/` target structure

```
include/
  config/
    display_config.h              # pins, resolution, hardware constants
    EEPROM_defines.h              # NVS keys
    lv_conf.h                     # LVGL config (must remain in include path)

  protocol/
    display_comms.h               # HMI ↔ motherboard protocol
    Credentials_public.h          # public endpoints, no secrets

  tasks/
    CommTask.h
    UITask.h
    AudioManager.h
    Wifi_OTA.h

  ui/
    ui.h
    ui_events.h
    ui_helpers.h
    hmi_ding.h                    # audio asset as C array

  main.h
  touch.h
  ArduinoHttpClient.h             # stays here for now (used at build time)
  # Credentials.h → .gitignore only, never committed
```

### `platformio.ini` changes required

- Add `src/ui/assets` to `build_src_filter` if not picked up automatically.
- Add `-Iinclude/config -Iinclude/protocol -Iinclude/tasks -Iinclude/ui` to `build_flags` so all headers remain reachable. **Important:** `lv_conf.h` must be on the include path — `include/config/` must be explicitly listed.
- Update `board_build.partitions` to `partitions/display_v1_audio.csv` (or whichever is active).

---

## 5. motherBoard — Internal Reorganization

### Legacy code to delete

| File | Reason |
|---|---|
| `src/drawGraphicInterphace.cpp` | Legacy UI from when MB had its own screen |
| `src/userInterface.cpp` | Same |
| `src/UI_mainMenu.cpp` | Same |
| `src/UI_calibration.cpp` | Same |
| `src/UI_actuatorsProgress.cpp` | Same |
| `src/UI_settings.cpp` | Same |
| `src/sensors.cpp` | Replaced by `modules/sensors/sensors_module` |

Any corresponding headers in `include/` for the above are also removed.

### `src/` target structure

```
src/
  main.cpp

  hal/
    hal.h
    hal_hw16.cpp
    hal_hw17.cpp

  drivers/
    drv_ina3221.cpp / .h
    drv_shtc3.cpp / .h
    drv_sts3x.cpp / .h
    BQ25730.cpp / .h              # battery charger driver
    SPO2.cpp / .h                 # pulse oximeter driver
    IncuNest_humidifier.cpp / .h  # humidifier driver

  tasks/
    CommTask.cpp                  # FreeRTOS task: HMI communication
    Wifi_OTA.cpp                  # FreeRTOS task: OTA updates
    DriveUpload.cpp               # FreeRTOS task: Google Drive upload
    GPRS.cpp                      # FreeRTOS task: GPRS connectivity

  modules/
    comm/
      comm_module.cpp / .h
    control/
      control_module.cpp / .h
      alarm_machine.cpp / .h
      pid_wrapper.cpp / .h
    sensors/
      sensors_module.cpp / .h

  state/
    state.cpp / .h

  system/                         # system infrastructure (init, config, security)
    initHardware.cpp
    ESP32_config.cpp
    EEPROM.cpp
    CrashReporter.cpp
    ISR.cpp
    math.cpp
    security.cpp
    updateData.cpp
    calibrateSensors.cpp
    usb_host_vcp.cpp
    usb_host_ch34x_vcp.cpp
    cdc_acm_host.c
    PID.cpp                       # base PID implementation
    Buzzer.cpp
```

### `include/` target structure

```
include/
  config/
    board.h                       # pin definitions, hardware config
    task_config.h                 # FreeRTOS stack sizes, priorities
    preferences_keys.h            # NVS keys
    telemetry_keys.h              # ThingsBoard metric keys
    ui_constants.h                # residual UI constants
    ESP32_config.h

  protocol/
    CommTask.h                    # HMI communication interface
    Credentials_public.h

  usb/
    cdc_acm_host.h
    usb_types_cdc.h
    vcp.hpp
    vcp_ch34x.hpp

  main.h
  # Credentials.h → .gitignore only, never committed
```

### `platformio.ini` changes required

- Update `board_build.partitions` to `partitions/ESP32_16MB.csv` (or whichever is active).
- Update `build_flags` `-I` paths for new `include/` subdirectories.
- Check `build_src_filter` picks up `src/system/`.

---

## 6. What Is NOT in Scope

- Renaming functions, variables, or changing logic.
- Moving `Wifi_OTA.cpp` to a different abstraction.
- Reorganizing `flasher_tool/` or `docs/`.
- Changing communication protocol or module interfaces.

---

## 7. Definition of Done

- [ ] `dev` branch created.
- [ ] All legacy files deleted.
- [ ] All log/artifact files deleted and added to `.gitignore`.
- [ ] `SquareLineProject/` removed.
- [ ] Partition CSVs moved to `partitions/` and references updated.
- [ ] Utility scripts moved to `tools/`.
- [ ] `src/` and `include/` restructured in both projects.
- [ ] Both projects compile without errors after reorganization.
- [ ] `Credentials.h` is listed in `.gitignore` and not tracked by git.
