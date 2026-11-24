#include "ui/SexPicker.h"

#include "ui/i18n.h"

namespace {

SexPickerCb s_onPick = nullptr;

void onBtn(lv_event_t *e) {
  const uint8_t sex = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  if (s_onPick) s_onPick(sex);
}

lv_obj_t *addBtn(lv_obj_t *row, const char *text, uint8_t sex, lv_color_t bg) {
  lv_obj_t *btn = lv_btn_create(row);
  lv_obj_set_size(btn, 190, 110);
  lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN);
  lv_obj_set_style_radius(btn, 10, LV_PART_MAIN);
  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lbl, 176);
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(lbl);
  lv_obj_add_event_cb(btn, onBtn, LV_EVENT_CLICKED, (void *)(uintptr_t)sex);
  return btn;
}

} // namespace

lv_obj_t *SexPicker_Create(lv_obj_t *parent, SexPickerCb onPick) {
  s_onPick = onPick;
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, 600, 110);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  // Mismo azul para nino y nina (un dato clinico, no una decoracion); gris
  // para "no consta", que tambien es una respuesta y no un SALTAR.
  addBtn(row, TR(STR_SEX_MALE_UC), 1, lv_color_hex(0x2F6FB0));
  addBtn(row, TR(STR_SEX_FEMALE_UC), 2, lv_color_hex(0x2F6FB0));
  addBtn(row, TR(STR_SEX_UNKNOWN_UC), 0, lv_color_hex(0x888888));
  return row;
}
