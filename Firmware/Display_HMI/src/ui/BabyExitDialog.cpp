#include "ui/BabyExitDialog.h"

#include <cstdio>
#include <cstring>

#include "CommTask.h"
#include "UITask.h"
#include "main.h"
#include "ui.h"
#include "ui/BabyWizard.h"

namespace {

enum class ExitStep : uint8_t { Closed, AskReason, AskOutcome, AskCause };

ExitStep s_step = ExitStep::Closed;
bool s_wasActive = false;   // control state on the previous tick
uint32_t s_seq = 0;
// Outcome awaiting a cause before it is actually sent. Only Deceased (2)
// routes through AskCause; every other outcome is sent as-is.
uint8_t s_pendingOutcome = 0;

lv_obj_t *s_overlay = nullptr;
lv_obj_t *s_card = nullptr;
lv_obj_t *s_content = nullptr;
// Child of the card, never of s_content, so the step rebuilds don't destroy
// it: the outcome screen has no way back, and without this X the only exits
// from it would be recording an outcome the nurse may not know yet.
lv_obj_t *s_closeBtn = nullptr;

void clearContent() {
  if (s_content) lv_obj_clean(s_content);
}

void closeDialog() {
  if (s_overlay) lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  clearContent();
  s_step = ExitStep::Closed;
}

lv_obj_t *makeBtn(lv_obj_t *parent, const char *text, lv_event_cb_t cb,
                  lv_color_t bg, void *user = nullptr) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN);
  lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
  lv_obj_set_height(btn, 52);
  lv_obj_set_width(btn, LV_PCT(100));
  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  // Explicit: lv_btn's default label colour is white, which vanishes on the
  // light fills used elsewhere in these screens.
  lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
  lv_obj_center(lbl);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user);
  return btn;
}

void showAskReason();
void showAskOutcome();
void showAskCause();

void onDismiss(lv_event_t *) { closeDialog(); }

// Same semantics as "NOT NOW": nothing is recorded, and the dialog will be
// offered again on the next active -> idle edge.
void buildCloseButton() {
  if (!s_card || s_closeBtn) return;
  s_closeBtn = makeBtn(s_card, "X", onDismiss, lv_color_hex(0xAA3333));
  lv_obj_set_size(s_closeBtn, 46, 46);
  lv_obj_align(s_closeBtn, LV_ALIGN_TOP_RIGHT, 0, 0);
}

void onWithMother(lv_event_t *) {
  if (s_seq != 0) Communication_SendProfileKangaroo(s_seq);
  closeDialog();
}

void onDischargeChosen(lv_event_t *) { showAskOutcome(); }

// Deceased (2) needs a cause before anything is sent; every other outcome
// goes straight out. s_pendingOutcome carries it across to onCausePicked.
void onOutcomePicked(lv_event_t *e) {
  uint8_t outcome = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  if (outcome == 2) {
    s_pendingOutcome = outcome;
    showAskCause();
    return;
  }
  if (s_seq != 0) {
    Communication_SendProfileDischarge(s_seq, outcome, 0);
    // The profile is archived on the motherBoard now, so this HMI must stop
    // offering it as "the baby currently in the incubator".
    BabyWizard_ClearActiveProfile();
  }
  closeDialog();
}

void onCausePicked(lv_event_t *e) {
  uint8_t cause = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  if (s_seq != 0) {
    Communication_SendProfileDischarge(s_seq, s_pendingOutcome, cause);
    BabyWizard_ClearActiveProfile();
  }
  closeDialog();
}

// scrollable: AskCause has more rows than fit at the same button height as
// the other steps, so it needs to scroll instead of overflowing the card.
lv_obj_t *makeColumn(bool scrollable = false) {
  clearContent();
  lv_obj_t *col = lv_obj_create(s_content);
  lv_obj_remove_style_all(col);
  lv_obj_set_size(col, LV_PCT(100), LV_PCT(100));
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(col, 10, 0);
  lv_obj_set_style_pad_all(col, 4, 0);
  // s_closeBtn ("X") is a sibling of s_content on s_card, not a flex child
  // here, so flex can't keep clear of it on its own: a short title (e.g. the
  // one-line "Motivo del alta") left just enough room for the first button
  // to start underneath the top-right corner, and painted-on-top there meant
  // the X visibly clipped into "SOBREVIVIO"/"SURVIVED". 60 clears the 46px
  // close button with margin even if this content box ever sits flush with
  // the card's top edge.
  lv_obj_set_style_pad_top(col, 60, 0);
  if (scrollable) {
    lv_obj_add_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(col, LV_DIR_VER);
  } else {
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
  }
  return col;
}

void showAskReason() {
  lv_obj_t *col = makeColumn();

  lv_obj_t *title = lv_label_create(col);
  char buf[96];
  const char *name = BabyWizard_GetActiveName();
  if (name && name[0]) {
    snprintf(buf, sizeof(buf),
             TR(STR_EXIT_IDLE_NAMED_FMT), name);
  } else {
    snprintf(buf, sizeof(buf), "%s",
             TR(STR_EXIT_IDLE_ANON));
  }
  lv_label_set_text(title, buf);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0x0B2E4F), 0);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(title, LV_PCT(100));

  // "AHORA NO"/"NOT NOW" removed: it was a second button with the exact same
  // semantics as the "X" close button (nothing recorded, dialog re-offered on
  // the next active -> idle edge). With only two buttons left, they grow to
  // fill the freed-up space instead of leaving it empty.
  lv_obj_t *withMotherBtn =
      makeBtn(col, TR(STR_WITH_THE_MOTHER),
              onWithMother, lv_color_hex(0x0075EE));
  lv_obj_set_height(withMotherBtn, 90);
  lv_obj_t *dischargeBtn = makeBtn(
      col, TR(STR_DISCHARGE_UC), onDischargeChosen,
      lv_color_hex(0xE08800));
  lv_obj_set_height(dischargeBtn, 90);

  s_step = ExitStep::AskReason;
}

void showAskOutcome() {
  lv_obj_t *col = makeColumn();

  lv_obj_t *title = lv_label_create(col);
  lv_label_set_text(title, TR(STR_REASON_FOR_DISCHARGE));
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0x0B2E4F), 0);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(title, LV_PCT(100));

  // Same 0-3 mapping as PROTOCOL.md / BabyOutcome on the motherBoard.
  makeBtn(col, TR(STR_OUTCOME_SURVIVED_UC), onOutcomePicked,
          lv_color_hex(0x00AA00), (void *)(uintptr_t)1);
  makeBtn(col, TR(STR_OUTCOME_DIED_UC),
          onOutcomePicked, lv_color_hex(0xAA3333), (void *)(uintptr_t)2);
  makeBtn(col, TR(STR_OUTCOME_TRANSFER_UC), onOutcomePicked,
          lv_color_hex(0x0075EE), (void *)(uintptr_t)3);
  makeBtn(col, TR(STR_OUTCOME_UNKNOWN_UC), onOutcomePicked,
          lv_color_hex(0x888888), (void *)(uintptr_t)0);

  s_step = ExitStep::AskOutcome;
}

void showAskCause() {
  lv_obj_t *col = makeColumn(/*scrollable=*/true);

  lv_obj_t *title = lv_label_create(col);
  lv_label_set_text(title, TR(STR_CAUSE_OF_DEATH));
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0x0B2E4F), 0);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(title, LV_PCT(100));

  // Same 0-6 mapping as PROTOCOL.md / BabyCause on the motherBoard. Ordered
  // by frequency of neonatal mortality causes in low-resource settings
  // (WHO/UNICEF), not alphabetically; 0 (unspecified) has no button here.
  struct CauseOption { ui_str_id_t text; uint8_t code; };
  static const CauseOption CAUSES[] = {
      {STR_CAUSE_PREMATURITY_UC, 1},
      {STR_CAUSE_ASPHYXIA_UC, 2},
      {STR_CAUSE_SEPSIS_UC, 3},
      {STR_CAUSE_MALFORMATION_UC, 4},
      {STR_CAUSE_HYPOTHERMIA_UC, 5},
      {STR_CAUSE_OTHER_UC, 6},
  };
  for (const CauseOption &c : CAUSES) {
    lv_obj_t *btn = makeBtn(col, TR(c.text), onCausePicked,
                            lv_color_hex(0x0075EE), (void *)(uintptr_t)c.code);
    lv_obj_set_height(btn, 44);
  }

  s_step = ExitStep::AskCause;
}

}  // namespace

bool BabyExitDialog_IsOpen(void) { return s_step != ExitStep::Closed; }

void BabyExitDialog_Init(lv_obj_t *parent) {
  // Parent to ui_ScreenMain explicitly, never lv_scr_act(): at UI-init time
  // the active screen is still the splash, and an overlay parented there is
  // gone the moment ui_ScreenMain loads.
  s_overlay = lv_obj_create(parent);
  lv_obj_remove_style_all(s_overlay);
  lv_obj_set_size(s_overlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  lv_obj_set_pos(s_overlay, 0, 0);
  lv_obj_set_style_bg_color(s_overlay, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_overlay, LV_OPA_70, LV_PART_MAIN);
  lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);

  s_card = lv_obj_create(s_overlay);
  lv_obj_set_size(s_card, 520, 400);
  lv_obj_center(s_card);
  lv_obj_set_style_radius(s_card, 12, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_card, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_clear_flag(s_card, LV_OBJ_FLAG_SCROLLABLE);

  s_content = lv_obj_create(s_card);
  lv_obj_remove_style_all(s_content);
  lv_obj_set_size(s_content, 480, 360);
  lv_obj_center(s_content);
  lv_obj_clear_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);

  // After s_content on purpose: last child = drawn on top of both screens.
  buildCloseButton();
}

void BabyExitDialog_Tick(bool anyControlActive) {
  // Edge detection only: active -> idle. Staying idle must not re-open the
  // dialog every tick, and idle -> active is not interesting.
  bool wasActive = s_wasActive;
  s_wasActive = anyControlActive;

  if (s_step != ExitStep::Closed) return;
  if (!(wasActive && !anyControlActive)) return;

  // Nothing to attribute the exit to (wizard skipped, or already discharged).
  s_seq = BabyWizard_GetActiveSeq();
  if (s_seq == 0) return;

  // A critical alarm owns the screen; asking paperwork over it would be wrong.
  if (UI_IsCriticalAlarmActive()) return;

  showAskReason();
  if (s_overlay) {
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_overlay);
  }
}
