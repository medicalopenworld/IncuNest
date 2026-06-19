# Slide to Unlock Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the hold-to-unlock popup with a horizontal slide-to-unlock widget that appears on any screen touch and unlocks when dragged ≥80% across a 600 px track.

**Architecture:** Four files change in sequence: constants/state in `main.h`, declarations in `ElementsCreation.h`, widget construction in `ElementsCreation.cpp`, and all interaction logic in `UITask.cpp`. No new files are created. The existing timeout timer (`unlockTimeoutTimer`) is reused unchanged; all arc-progress variables and callbacks are removed.

**Tech Stack:** LVGL v8 (lv_obj, lv_anim, lv_indev), C++17, PlatformIO / ESP-IDF, ESP32-S3.

## Global Constraints

- LVGL v8 API only — do not use v9 signatures.
- All `lv_obj_t *` globals initialised to `NULL`; all `lv_timer_t *` / `lv_anim_t` state variables initialised to `NULL` / 0.
- Multilanguage: Spanish index 0, English index 1, French index 2 — keep this order everywhere.
- Build environment: `pio run -e main` must pass with zero errors after every task.
- No new source files — all changes are in existing files listed below.

---

## File Map

| File | Change |
|---|---|
| `include/main.h` | Remove arc-progress vars; add slide-state vars and constants |
| `include/ui/ElementsCreation.h` | Replace 5 old lock declarations with 5 new slider declarations |
| `src/ui/ElementsCreation.cpp` | Replace `ui_UnlockCont` widget block with slider widget; update global var list |
| `src/tasks/UITask.cpp` | Remove arc callbacks; add drag callback + snap-back animation; fix universal trigger; update all references to old objects |

---

## Task 1: Update `main.h` — swap arc-progress state for slide state

**Files:**
- Modify: `include/main.h:247-257`

**Interfaces:**
- Produces: `SLIDE_MARGIN`, `SLIDE_THUMB_MAX_X`, `SLIDE_UNLOCK_X`, `SLIDE_TRACK_W`, `SLIDE_TRACK_H`, `SLIDE_THUMB_SIZE` — consumed by Tasks 3 and 4.
- Produces: `slideDragStartX`, `slideThumbStartX` — consumed by Task 4.
- Keeps: `unlockTimeoutTimer`, `UNLOCK_TIMEOUT_MS` — unchanged, used in Task 4.

- [ ] **Step 1: Replace the arc-progress block with slide-state**

  Locate this block in `include/main.h` (lines ~247–257):

  ```cpp
  // -----------------------------
  // Progress arc for lock long-press
  // -----------------------------

  static lv_obj_t *lockProgressArc = NULL;
  static lv_timer_t *lockProgressTimer = NULL;
  static lv_timer_t *unlockTimeoutTimer = NULL;
  static lv_timer_t *lockStopDebounceTimer = NULL;
  static uint32_t lockProgressStart = 0;
  static const uint32_t LOCK_PROGRESS_DURATION_MS = 1500; // 1.5 seconds
  static const uint32_t UNLOCK_TIMEOUT_MS = 5000;         // 5 seconds timeout
  ```

  Replace it with:

  ```cpp
  // -----------------------------
  // Slide-to-unlock state
  // -----------------------------

  static lv_timer_t *unlockTimeoutTimer = NULL;
  static const uint32_t UNLOCK_TIMEOUT_MS = 5000; // 5 seconds timeout

  static lv_coord_t slideDragStartX  = 0;
  static lv_coord_t slideThumbStartX = 2;

  static const lv_coord_t SLIDE_TRACK_W     = 600;
  static const lv_coord_t SLIDE_TRACK_H     = 70;
  static const lv_coord_t SLIDE_THUMB_SIZE  = 66;
  static const lv_coord_t SLIDE_MARGIN      = 2;
  // Max x the thumb can reach (right edge minus one margin)
  static const lv_coord_t SLIDE_THUMB_MAX_X = SLIDE_TRACK_W - SLIDE_THUMB_SIZE - SLIDE_MARGIN; // 532
  // 80 % of travel triggers unlock: SLIDE_MARGIN + (530 * 0.8) = 426
  static const lv_coord_t SLIDE_UNLOCK_X    = SLIDE_MARGIN + (lv_coord_t)((SLIDE_THUMB_MAX_X - SLIDE_MARGIN) * 0.8f);
  ```

- [ ] **Step 2: Build to confirm no errors**

  ```bash
  pio run -e main 2>&1 | grep -E "error:|warning:" | head -30
  ```

  Expected: zero `error:` lines. (Warnings about unused vars `lockProgressArc` etc. will appear until Task 4 removes the usages — that is fine at this stage; the project will not yet link cleanly until Task 4.)

- [ ] **Step 3: Commit**

  ```bash
  git add include/main.h
  git commit -m "refactor(lock): replace arc-progress state with slide-unlock constants"
  ```

---

## Task 2: Update `ElementsCreation.h` — replace old lock declarations

**Files:**
- Modify: `include/ui/ElementsCreation.h` (lines ~404–432 in the `SCREEN: Lock` section)

**Interfaces:**
- Consumes: nothing from Task 1.
- Produces: `ui_SlideUnlockCont`, `ui_SlideTrack`, `ui_SlideLabel`, `ui_SlideThumb`, `ui_SlideFill` — consumed by Tasks 3 and 4.

- [ ] **Step 1: Replace old declarations**

  Find the Lock screen section in `include/ui/ElementsCreation.h`. It currently contains:

  ```cpp
  extern lv_obj_t * ui_UnlockCont;
  extern lv_obj_t * ui_Panel11;
  extern lv_obj_t * ui_Label4;
  extern lv_obj_t * ui_LockButton2;
  ```

  And separately:

  ```cpp
  extern lv_obj_t * ui_Spinner1;
  ```

  Replace `ui_UnlockCont`, `ui_Panel11`, `ui_Label4`, `ui_LockButton2` with:

  ```cpp
  extern lv_obj_t * ui_SlideUnlockCont;
  extern lv_obj_t * ui_SlideTrack;
  extern lv_obj_t * ui_SlideLabel;
  extern lv_obj_t * ui_SlideThumb;
  extern lv_obj_t * ui_SlideFill;
  ```

  Delete the `extern lv_obj_t * ui_Spinner1;` line entirely.

- [ ] **Step 2: Commit**

  ```bash
  git add include/ui/ElementsCreation.h
  git commit -m "refactor(lock): replace old unlock-popup declarations with slide widget"
  ```

---

## Task 3: Rebuild unlock widget in `ElementsCreation.cpp`

**Files:**
- Modify: `src/ui/ElementsCreation.cpp` — global var list (~line 253) and `ui_ScreenLock_screen_init` (~line 3376)

**Interfaces:**
- Consumes: `SLIDE_TRACK_W`, `SLIDE_TRACK_H`, `SLIDE_THUMB_SIZE`, `SLIDE_MARGIN` from `main.h` (included at line 5).
- Consumes: `ui_img_candado_png` (already used in this file).
- Produces: `ui_SlideUnlockCont`, `ui_SlideTrack`, `ui_SlideLabel`, `ui_SlideThumb`, `ui_SlideFill` initialised as `lv_obj_t *` globals.

- [ ] **Step 1: Update global variable definitions**

  In `src/ui/ElementsCreation.cpp`, find the `// Screen Lock` block in the global definitions section (around line 257). It contains:

  ```cpp
  // Screen Lock
  lv_obj_t *ui_ScreenLock = NULL;
  lv_obj_t *ui_LockButton = NULL;
  lv_obj_t *ui_Container1 = NULL;
  lv_obj_t *ui_AirTempLockCont = NULL;
  ...
  lv_obj_t *ui_UnlockCont = NULL;
  lv_obj_t *ui_Panel11 = NULL;
  lv_obj_t *ui_Label4 = NULL;
  lv_obj_t *ui_LockButton2 = NULL;
  lv_obj_t *ui_TargetSkinTempCont = NULL;
  ...
  lv_obj_t *ui_ArrowSkinLock = NULL;
  lv_obj_t *ui_Spinner1 = NULL;
  ```

  Replace those four old lines with the five new ones (keep every other line intact):

  ```cpp
  lv_obj_t *ui_SlideUnlockCont = NULL;
  lv_obj_t *ui_SlideTrack      = NULL;
  lv_obj_t *ui_SlideLabel      = NULL;
  lv_obj_t *ui_SlideThumb      = NULL;
  lv_obj_t *ui_SlideFill       = NULL;
  ```

  Delete `lv_obj_t *ui_Spinner1 = NULL;`.

- [ ] **Step 2: Replace the widget-creation block**

  Inside `ui_ScreenLock_screen_init()`, find the comment `// --- RE-INSERTED UNLOCK CONT AT THE END TO BE ON TOP LAYER ---` (around line 3376) and delete everything from that comment through the closing `// --------------------------------------------------------------------------` comment (line 3436), inclusive. That removes `ui_UnlockCont`, `ui_Panel11`, `ui_Label4`, `ui_LockButton2`, and `ui_Spinner1`.

  Insert in their place:

  ```cpp
  // --- SLIDE TO UNLOCK OVERLAY (created last so it renders on top) ---
  ui_SlideUnlockCont = lv_obj_create(ui_ScreenLock);
  lv_obj_remove_style_all(ui_SlideUnlockCont);
  lv_obj_set_size(ui_SlideUnlockCont, SLIDE_TRACK_W, SLIDE_TRACK_H);
  lv_obj_set_align(ui_SlideUnlockCont, LV_ALIGN_BOTTOM_MID);
  lv_obj_set_y(ui_SlideUnlockCont, -10);
  lv_obj_add_flag(ui_SlideUnlockCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_SlideUnlockCont, LV_OBJ_FLAG_SCROLLABLE);

  ui_SlideTrack = lv_obj_create(ui_SlideUnlockCont);
  lv_obj_set_size(ui_SlideTrack, SLIDE_TRACK_W, SLIDE_TRACK_H);
  lv_obj_set_align(ui_SlideTrack, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_SlideTrack, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_SlideTrack, lv_color_hex(0x000000),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_SlideTrack, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(ui_SlideTrack, lv_color_hex(0xFFFFFF),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(ui_SlideTrack, 2,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(ui_SlideTrack, SLIDE_TRACK_H / 2,
                          LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(ui_SlideTrack, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  // Progress fill (grows as thumb moves right)
  ui_SlideFill = lv_obj_create(ui_SlideTrack);
  lv_obj_remove_style_all(ui_SlideFill);
  lv_obj_set_size(ui_SlideFill, 0, SLIDE_THUMB_SIZE);
  lv_obj_set_align(ui_SlideFill, LV_ALIGN_LEFT_MID);
  lv_obj_set_x(ui_SlideFill, SLIDE_MARGIN);
  lv_obj_clear_flag(ui_SlideFill, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_SlideFill, lv_color_hex(0xFFFFFF),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_SlideFill, 80, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(ui_SlideFill, SLIDE_THUMB_SIZE / 2,
                          LV_PART_MAIN | LV_STATE_DEFAULT);

  // Label (centred in track, visible behind thumb)
  ui_SlideLabel = lv_label_create(ui_SlideTrack);
  lv_obj_set_align(ui_SlideLabel, LV_ALIGN_CENTER);
  lv_label_set_text(ui_SlideLabel, "DESLIZA PARA DESBLOQUEAR");
  lv_obj_set_style_text_color(ui_SlideLabel, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_SlideLabel, &lv_font_montserrat_18,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_align(ui_SlideLabel, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_clear_flag(ui_SlideLabel, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  // Thumb (white circle with padlock icon; non-clickable so touches reach track)
  ui_SlideThumb = lv_obj_create(ui_SlideTrack);
  lv_obj_remove_style_all(ui_SlideThumb);
  lv_obj_set_size(ui_SlideThumb, SLIDE_THUMB_SIZE, SLIDE_THUMB_SIZE);
  lv_obj_set_align(ui_SlideThumb, LV_ALIGN_LEFT_MID);
  lv_obj_set_x(ui_SlideThumb, SLIDE_MARGIN);
  lv_obj_clear_flag(ui_SlideThumb, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_SlideThumb, lv_color_hex(0xFFFFFF),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_SlideThumb, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(ui_SlideThumb, SLIDE_THUMB_SIZE / 2,
                          LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t *slide_icon = lv_img_create(ui_SlideThumb);
  lv_img_set_src(slide_icon, &ui_img_candado_png);
  lv_obj_set_align(slide_icon, LV_ALIGN_CENTER);
  lv_obj_add_flag(slide_icon, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_clear_flag(slide_icon, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_img_recolor(slide_icon, lv_color_hex(0x000000),
                               LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_img_recolor_opa(slide_icon, 255,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
  // --------------------------------------------------------------------------
  ```

- [ ] **Step 3: Remove the `ui_LockButton2` ext-click line**

  Near the end of `ui_ScreenLock_screen_init()`, find and delete this line:

  ```cpp
  lv_obj_set_ext_click_area(ui_LockButton2, TOUCH_EXT_SMALL);
  ```

- [ ] **Step 4: Build to confirm no errors**

  ```bash
  pio run -e main 2>&1 | grep -E "^.*error:" | head -20
  ```

  Expected: zero `error:` lines. (Linker will still see undefined references to the removed `UITask.cpp` functions — that resolves in Task 4.)

- [ ] **Step 5: Commit**

  ```bash
  git add src/ui/ElementsCreation.cpp
  git commit -m "feat(lock): build slide-to-unlock widget, remove hold-to-unlock popup"
  ```

---

## Task 4: Replace lock logic in `UITask.cpp`

**Files:**
- Modify: `src/tasks/UITask.cpp` — multiple locations described precisely below.

**Interfaces:**
- Consumes: `ui_SlideUnlockCont`, `ui_SlideTrack`, `ui_SlideLabel`, `ui_SlideThumb`, `ui_SlideFill` from Tasks 2–3.
- Consumes: `SLIDE_MARGIN`, `SLIDE_THUMB_MAX_X`, `SLIDE_UNLOCK_X` from Task 1.
- Consumes: `slideDragStartX`, `slideThumbStartX`, `unlockTimeoutTimer`, `UNLOCK_TIMEOUT_MS` from Task 1.

### Step 1 — Remove deleted functions and their forward declarations

- [ ] Delete these five functions entirely (find by name, remove from opening `static void` / `void` to closing `}`):

  1. `lock_progress_timer_cb` (~line 2772)
  2. `start_lock_progress` (~line 2796)
  3. `stop_lock_progress` (~line 2810)
  4. `lock_stop_debounce_cb` (~line 2865)
  5. `UnlockCont_event_cb` (~line 2875)
  6. `add_unlock_press_cb_recursive` (~line 2899)

### Step 2 — Add the snap-back animation callback and drag callback

- [ ] Immediately **before** `show_targets_for_mode` (around line 2709), insert:

  ```cpp
  // Animation callback: moves thumb and shrinks fill simultaneously.
  // `var` is ui_SlideThumb; `v` is the animated x position.
  static void slide_snap_back_anim_cb(void *var, int32_t v) {
    lv_obj_t *thumb = (lv_obj_t *)var;
    lv_obj_set_x(thumb, (lv_coord_t)v);
    if (ui_SlideFill)
      lv_obj_set_width(ui_SlideFill, LV_MAX(0, (lv_coord_t)v - SLIDE_MARGIN));
  }

  static void SlideUnlock_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
      // Cancel any running snap-back before starting a new drag
      lv_anim_del(ui_SlideThumb, slide_snap_back_anim_cb);

      lv_indev_t *indev = lv_indev_get_act();
      lv_point_t pt;
      lv_indev_get_point(indev, &pt);
      slideThumbStartX = lv_obj_get_x(ui_SlideThumb);
      slideDragStartX  = pt.x;

      if (unlockTimeoutTimer) lv_timer_pause(unlockTimeoutTimer);

    } else if (code == LV_EVENT_PRESSING) {
      lv_indev_t *indev = lv_indev_get_act();
      lv_point_t pt;
      lv_indev_get_point(indev, &pt);

      lv_coord_t new_x = slideThumbStartX + (pt.x - slideDragStartX);
      if (new_x < SLIDE_MARGIN)       new_x = SLIDE_MARGIN;
      if (new_x > SLIDE_THUMB_MAX_X)  new_x = SLIDE_THUMB_MAX_X;

      lv_obj_set_x(ui_SlideThumb, new_x);
      lv_obj_set_width(ui_SlideFill, LV_MAX(0, new_x - SLIDE_MARGIN));

    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
      lv_coord_t current_x = lv_obj_get_x(ui_SlideThumb);

      if (current_x >= SLIDE_UNLOCK_X) {
        // --- UNLOCK ---
        lv_obj_add_flag(ui_SlideUnlockCont, LV_OBJ_FLAG_HIDDEN);
        if (unlockTimeoutTimer) {
          lv_timer_del(unlockTimeoutTimer);
          unlockTimeoutTimer = NULL;
        }
        lv_scr_load(ui_ScreenMain);
        locked = false;
      } else {
        // --- SNAP BACK ---
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, ui_SlideThumb);
        lv_anim_set_exec_cb(&a, slide_snap_back_anim_cb);
        lv_anim_set_values(&a, current_x, SLIDE_MARGIN);
        lv_anim_set_time(&a, 200);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_start(&a);

        if (unlockTimeoutTimer) {
          lv_timer_resume(unlockTimeoutTimer);
          lv_timer_reset(unlockTimeoutTimer);
        }
      }
    }
  }
  ```

### Step 3 — Update `show_targets_for_mode`

- [ ] Replace the `ui_UnlockCont` reference and add thumb/fill reset:

  Find in `show_targets_for_mode`:
  ```cpp
  lv_obj_add_flag(ui_UnlockCont, LV_OBJ_FLAG_HIDDEN);
  ```

  Replace with:
  ```cpp
  // Cancel any running snap-back and reset thumb/fill to initial state
  lv_anim_del(ui_SlideThumb, slide_snap_back_anim_cb);
  lv_obj_add_flag(ui_SlideUnlockCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_SlideThumb) lv_obj_set_x(ui_SlideThumb, SLIDE_MARGIN);
  if (ui_SlideFill)  lv_obj_set_width(ui_SlideFill, 0);
  ```

### Step 4 — Update `show_unlock_only` → `show_slide_unlock`

- [ ] Rename the function and update the object reference:

  Find:
  ```cpp
  static void show_unlock_only(void) {
    lv_obj_clear_flag(ui_UnlockCont, LV_OBJ_FLAG_HIDDEN);
  ```

  Replace with:
  ```cpp
  static void show_slide_unlock(void) {
    lv_obj_clear_flag(ui_SlideUnlockCont, LV_OBJ_FLAG_HIDDEN);
  ```

  The rest of the function body (timer start/reset) stays unchanged.

### Step 5 — Update `enter_lock_screen` — remove `stop_lock_progress()` calls

- [ ] Find `enter_lock_screen` (~line 2821):

  ```cpp
  static void enter_lock_screen(void) {
    if (lv_scr_act() == ui_ScreenLock) {
      stop_lock_progress();
      locked = true;
      show_targets_for_mode();
      return;
    }
    stop_lock_progress();
    locked = true;
    lv_scr_load(ui_ScreenLock);
    show_targets_for_mode();
    lv_disp_trig_activity(NULL);
  }
  ```

  Replace with:
  ```cpp
  static void enter_lock_screen(void) {
    if (lv_scr_act() == ui_ScreenLock) {
      locked = true;
      show_targets_for_mode();
      return;
    }
    locked = true;
    lv_scr_load(ui_ScreenLock);
    show_targets_for_mode();
    lv_disp_trig_activity(NULL);
  }
  ```

### Step 6 — Update `inactivity_timer_cb` — remove `stop_lock_progress()` call

- [ ] Find the body of `inactivity_timer_cb` (~line 2911). It contains:

  ```cpp
  if (lv_scr_act() != ui_ScreenLock) {
    stop_lock_progress();
    locked = true;
    lv_scr_load(ui_ScreenLock);
    show_targets_for_mode();
  }
  ```

  Replace with:
  ```cpp
  if (lv_scr_act() != ui_ScreenLock) {
    locked = true;
    lv_scr_load(ui_ScreenLock);
    show_targets_for_mode();
  }
  ```

### Step 7 — Update `LockScreenAnyTouch_cb` — fix universal trigger

- [ ] Find `LockScreenAnyTouch_cb` (~line 2843) and replace the whole function:

  ```cpp
  void LockScreenAnyTouch_cb(lv_event_t *e) {
    if (lv_scr_act() != ui_ScreenLock)
      return;
    lv_obj_t *origin = lv_event_get_target(e);
    // Ignore events that originate within the slider itself
    if (origin == ui_SlideTrack || origin == ui_SlideUnlockCont)
      return;

    if (lv_obj_has_flag(ui_SlideUnlockCont, LV_OBJ_FLAG_HIDDEN)) {
      show_slide_unlock();
    } else if (unlockTimeoutTimer) {
      lv_timer_reset(unlockTimeoutTimer);
    }
  }
  ```

### Step 8 — Update `apply_language` — new unlock text

- [ ] Find in `apply_language` (~line 647):

  ```cpp
  const char *TXT_UNLOCK[] = {"PRESIONA 2 SEG\nPARA DESBLOQUEAR",
                              "PRESS 2 SEC \nTO UNLOCK",
                              "APPUYEZ 2 SEG\nPOUR DEVERROUILLER"};
  ```

  Replace with:
  ```cpp
  const char *TXT_UNLOCK[] = {"DESLIZA PARA DESBLOQUEAR",
                              "SLIDE TO UNLOCK",
                              "GLISSEZ POUR DEVERROUILLER"};
  ```

  Then find:
  ```cpp
  lv_label_set_text(ui_Label4, TXT_UNLOCK[lang]);
  ```

  Replace with:
  ```cpp
  if (ui_SlideLabel) lv_label_set_text(ui_SlideLabel, TXT_UNLOCK[lang]);
  ```

### Step 9 — Update `ui_ScreenMain_init` block — register slider event, remove recursive callback

- [ ] Find the init block (~line 3635) that contains:

  ```cpp
  lv_obj_add_flag(ui_UnlockCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Spinner1, LV_OBJ_FLAG_HIDDEN);

  add_unlock_press_cb_recursive(ui_UnlockCont);
  ```

  Replace with:

  ```cpp
  lv_obj_add_flag(ui_SlideUnlockCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(ui_SlideTrack, SlideUnlock_event_cb, LV_EVENT_ALL, NULL);
  ```

### Step 10 — Build clean

- [ ] Build and confirm zero errors:

  ```bash
  pio run -e main 2>&1 | grep -E "^.*error:" | head -20
  ```

  Expected: no `error:` lines.

- [ ] **Commit**

  ```bash
  git add src/tasks/UITask.cpp
  git commit -m "feat(lock): implement slide-to-unlock with snap-back animation, fix universal touch trigger"
  ```

---

## Task 5: Manual verification on hardware

- [ ] Flash to device: `pio run -e main -t upload`
- [ ] **Scenario 1 — Basic slide**: On lock screen, tap anywhere → slider appears at bottom. Drag right past 80 % → loads main screen.
- [ ] **Scenario 2 — Snap-back**: Tap → slider appears. Drag to ~50 % and release → thumb animates back to left. Slider stays visible.
- [ ] **Scenario 3 — Timeout**: Tap → slider appears. Do nothing for 5 s → slider hides automatically.
- [ ] **Scenario 4 — PPG widget**: When pulse oximeter probe is attached, tap directly on the PPG waveform chart → slider appears (previously broken).
- [ ] **Scenario 5 — HR / PI widget**: Tap on the HR or PI value containers → slider appears.
- [ ] **Scenario 6 — Lock from main**: Press the padlock icon on main screen → goes to lock screen. Tap → slider appears.
- [ ] **Scenario 7 — Inactivity lock**: Leave main screen idle for the inactivity timeout → auto-locks. Tap on lock screen → slider appears.
- [ ] **Scenario 8 — Language**: Change language in settings, lock screen. Tap → label reads in the selected language.
