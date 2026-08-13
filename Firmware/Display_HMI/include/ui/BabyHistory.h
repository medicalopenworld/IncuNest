#pragma once
// "Babies" history screen (baby-history-viewer capability): active profiles
// with a Discharge action, archived profiles paginated 10/page, and a
// weight-evolution chart per baby. Fully independent of the AIR/SKIN
// activation wizard — works with no control active.
#include <lvgl.h>

// Creates the (hidden) overlay. Call once at UI init, after the main screen.
void BabyHistory_Init(lv_obj_t *parent);

// Opens the screen: requests the active list + first archived page.
void BabyHistory_Open(void);

// Drives timeouts/response handling. Call from the UI task loop (LVGL locked),
// same contract as BabyWizard_Poll().
void BabyHistory_Poll(void);
