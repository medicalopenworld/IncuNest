#include "ui/BabyExitDialog.h"

#include <cstdio>
#include <cstring>

#include "CommTask.h"
#include "UITask.h"
#include "main.h"
#include "ui.h"
#include "ui/BabyWizard.h"

extern ui_lang_t g_lang;

namespace {

enum class ExitStep : uint8_t { Closed, AskReason, AskOutcome };

ExitStep s_step = ExitStep::Closed;
bool s_wasActive = false;   // control state on the previous tick
uint32_t s_seq = 0;

lv_obj_t *s_overlay = nullptr;
lv_obj_t *s_card = nullptr;
lv_obj_t *s_content = nullptr;

const char *TXT(const char *es, const char *en, const char *fr) {
  return (g_lang == LANG_ES) ? es : (g_lang == LANG_FR) ? fr : en;
}

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

void onDismiss(lv_event_t *) { closeDialog(); }

void onWithMother(lv_event_t *) {
  if (s_seq != 0) Communication_SendProfileKangaroo(s_seq);
  closeDialog();
}

void onDischargeChosen(lv_event_t *) { showAskOutcome(); }

void onOutcomePicked(lv_event_t *e) {
  uint8_t outcome = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  if (s_seq != 0) {
    Communication_SendProfileDischarge(s_seq, outcome);
    // The profile is archived on the motherBoard now, so this HMI must stop
    // offering it as "the baby currently in the incubator".
    BabyWizard_ClearActiveProfile();
  }
  closeDialog();
}

lv_obj_t *makeColumn() {
  clearContent();
  lv_obj_t *col = lv_obj_create(s_content);
  lv_obj_remove_style_all(col);
  lv_obj_set_size(col, LV_PCT(100), LV_PCT(100));
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(col, 10, 0);
  lv_obj_set_style_pad_all(col, 4, 0);
  lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
  return col;
}

void showAskReason() {
  lv_obj_t *col = makeColumn();

  lv_obj_t *title = lv_label_create(col);
  char buf[96];
  const char *name = BabyWizard_GetActiveName();
  if (name && name[0]) {
    snprintf(buf, sizeof(buf),
             TXT("Incubadora en reposo\n%s ha salido:",
                 "Incubator idle\n%s has come out:",
                 "Incubateur au repos\n%s est sorti:"),
             name);
  } else {
    snprintf(buf, sizeof(buf), "%s",
             TXT("Incubadora en reposo\nEl bebe ha salido:",
                 "Incubator idle\nThe baby has come out:",
                 "Incubateur au repos\nLe bebe est sorti:"));
  }
  lv_label_set_text(title, buf);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0x0B2E4F), 0);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(title, LV_PCT(100));

  makeBtn(col, TXT("CON LA MADRE", "WITH THE MOTHER", "AVEC LA MERE"),
          onWithMother, lv_color_hex(0x0075EE));
  makeBtn(col, TXT("ALTA", "DISCHARGE", "SORTIE"), onDischargeChosen,
          lv_color_hex(0xE08800));
  makeBtn(col, TXT("AHORA NO", "NOT NOW", "PAS MAINTENANT"), onDismiss,
          lv_color_hex(0x888888));

  s_step = ExitStep::AskReason;
}

void showAskOutcome() {
  lv_obj_t *col = makeColumn();

  lv_obj_t *title = lv_label_create(col);
  lv_label_set_text(title, TXT("Motivo del alta", "Reason for discharge",
                               "Motif de la sortie"));
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0x0B2E4F), 0);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(title, LV_PCT(100));

  // Same 0-3 mapping as PROTOCOL.md / BabyOutcome on the motherBoard.
  makeBtn(col, TXT("SOBREVIVIO", "SURVIVED", "A SURVECU"), onOutcomePicked,
          lv_color_hex(0x00AA00), (void *)(uintptr_t)1);
  makeBtn(col, TXT("NO SOBREVIVIO", "DID NOT SURVIVE", "N'A PAS SURVECU"),
          onOutcomePicked, lv_color_hex(0xAA3333), (void *)(uintptr_t)2);
  makeBtn(col, TXT("TRASLADADO", "TRANSFERRED", "TRANSFERE"), onOutcomePicked,
          lv_color_hex(0x0075EE), (void *)(uintptr_t)3);
  makeBtn(col, TXT("DESCONOCIDO", "UNKNOWN", "INCONNU"), onOutcomePicked,
          lv_color_hex(0x888888), (void *)(uintptr_t)0);

  s_step = ExitStep::AskOutcome;
}

}  // namespace

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
