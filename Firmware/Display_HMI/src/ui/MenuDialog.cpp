#include "ui/MenuDialog.h"

#include "UITask.h"
#include "main.h"
#include "ui.h"
#include "ui/BabyHistory.h"
#include "ui/HelpDialog.h"
#include "ui/TimeDialog.h"

extern void Settings_cb(lv_event_t *e);

lv_obj_t *ui_MenuTimeBtn = nullptr;
lv_obj_t *ui_MenuBabiesBtn = nullptr;
lv_obj_t *ui_MenuSettingsBtn = nullptr;
lv_obj_t *ui_MenuHelpBtn = nullptr;

namespace {

// Misma tarjeta que HelpDialog/TimeDialog: el operador ya conoce ese cuadro.
constexpr lv_coord_t CARD_W = 780, CARD_H = 460;
// 4 opciones de 178 con 16 de hueco: 4*178 + 3*16 = 760, el ancho del
// contenido (tarjeta menos 2*10 de margen).
constexpr lv_coord_t OPT_W = 178, OPT_H = 318, OPT_GAP = 16;

bool s_open = false;
lv_obj_t *s_overlay = nullptr;
lv_obj_t *s_timeSym = nullptr;  // hora actual como "icono" de FECHA Y HORA
lv_obj_t *s_title = nullptr;
lv_obj_t *s_optTitle[4] = {nullptr, nullptr, nullptr, nullptr};

void closeMenu() {
  if (s_overlay) lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  s_open = false;
}

void onClose(lv_event_t *) { closeMenu(); }

// Cada opcion cierra el menu ANTES de abrir su destino: los dialogos se
// apilan por orden de creacion sobre ui_ScreenMain y el menu no debe quedar
// debajo, abierto y sin verse.
void onTime(lv_event_t *) {
  closeMenu();
  TimeDialog_Open();
}

void onBabies(lv_event_t *) {
  closeMenu();
  BabyHistory_Open();
}

// Ajustes es otra pantalla y entra con un fundido de 500 ms sobre la de
// debajo. Cerrar el menu antes destapaba la pantalla principal durante el
// fundido; se queda abierto y lo cierra MenuDialog_Poll() cuando termina.
void onSettings(lv_event_t *) { Settings_cb(nullptr); }

void onHelp(lv_event_t *) {
  closeMenu();
  HelpDialog_Open();
}

lv_obj_t *makeOption(lv_obj_t *parent, int idx, const char *symbol,
                     const lv_font_t *symFont, lv_event_cb_t cb,
                     lv_obj_t **symOut) {
  // Desde y=56: el boton X de la tarjeta ocupa la esquina superior derecha.
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_size(btn, OPT_W, OPT_H);
  lv_obj_align(btn, LV_ALIGN_TOP_LEFT, idx * (OPT_W + OPT_GAP), 56);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x0075EE), LV_PART_MAIN);
  lv_obj_set_style_radius(btn, 14, LV_PART_MAIN);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *sym = lv_label_create(btn);
  lv_label_set_text(sym, symbol);
  lv_obj_set_style_text_font(sym, symFont, 0);
  lv_obj_set_style_text_color(sym, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(sym, LV_ALIGN_TOP_MID, 0, 60);
  if (symOut) *symOut = sym;

  lv_obj_t *ttl = lv_label_create(btn);
  lv_label_set_long_mode(ttl, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(ttl, OPT_W - 16);
  lv_obj_set_style_text_align(ttl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(ttl, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(ttl, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(ttl, LV_ALIGN_TOP_MID, 0, 170);
  s_optTitle[idx] = ttl;
  return btn;
}

// Textos al abrir y no al crear: el idioma puede haber cambiado desde Ajustes.
void refreshTexts() {
  lv_label_set_text(s_title, TR(STR_MENU_UC));
  lv_label_set_text(s_optTitle[0], TR(STR_MENU_TIME));
  lv_label_set_text(s_optTitle[1], TR(STR_MENU_BABIES));
  lv_label_set_text(s_optTitle[2], TR(STR_SETTINGS));
  lv_label_set_text(s_optTitle[3], TR(STR_HELP_UC));
  // La hora del heading (o "Sin hora") como icono de la primera tarjeta.
  lv_label_set_text(s_timeSym,
                    ui_ClockTime ? lv_label_get_text(ui_ClockTime) : "--:--");
}

}  // namespace

void MenuDialog_Init(lv_obj_t *parent) {
  s_overlay = lv_obj_create(parent);
  lv_obj_remove_style_all(s_overlay);
  lv_obj_set_size(s_overlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  lv_obj_set_pos(s_overlay, 0, 0);
  lv_obj_set_style_bg_color(s_overlay, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_overlay, LV_OPA_70, LV_PART_MAIN);
  lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *card = lv_obj_create(s_overlay);
  lv_obj_set_size(card, CARD_W, CARD_H);
  lv_obj_center(card);
  lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
  lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *content = lv_obj_create(card);
  lv_obj_remove_style_all(content);
  lv_obj_set_size(content, CARD_W - 20, CARD_H - 20);
  lv_obj_center(content);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

  s_title = lv_label_create(content);
  lv_obj_set_style_text_font(s_title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(s_title, lv_color_hex(0x000000), 0);
  lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 6);

  ui_MenuTimeBtn = makeOption(content, 0, "", &lv_font_montserrat_48, onTime,
                              &s_timeSym);
  ui_MenuBabiesBtn = makeOption(content, 1, LV_SYMBOL_LIST,
                                &lv_font_montserrat_48, onBabies, nullptr);
  ui_MenuSettingsBtn = makeOption(content, 2, LV_SYMBOL_SETTINGS,
                                  &lv_font_montserrat_48, onSettings, nullptr);
  ui_MenuHelpBtn = makeOption(content, 3, "?", &lv_font_montserrat_48, onHelp,
                              nullptr);

  // Despues del contenido a proposito: ultimo hijo = se dibuja encima.
  lv_obj_t *close = lv_btn_create(card);
  lv_obj_set_size(close, 46, 46);
  lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_set_style_bg_color(close, lv_color_hex(0xAA3333), LV_PART_MAIN);
  lv_obj_set_style_radius(close, 8, LV_PART_MAIN);
  lv_obj_t *x = lv_label_create(close);
  lv_label_set_text(x, "X");
  lv_obj_center(x);
  lv_obj_add_event_cb(close, onClose, LV_EVENT_CLICKED, nullptr);

  refreshTexts();
}

void MenuDialog_Open(void) {
  if (!s_overlay) return;
  refreshTexts();
  s_open = true;
  lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(s_overlay);
}

bool MenuDialog_IsOpen(void) { return s_open; }

void MenuDialog_Close(void) { closeMenu(); }

void MenuDialog_Poll(void) {
  if (!s_open) return;
  // Sin cesion ante alarmas, a diferencia de la ayuda: es un selector de un
  // toque y lo que abre (ajustes, bebes) tiene que seguir a mano durante un
  // corte de red, que es una alarma que puede durar horas.
  // Al cambiar de pantalla, solo cuando el fundido ha terminado: mientras
  // dura, LVGL sigue pintando la pantalla anterior debajo (scr_prev).
  const bool leftMain =
      lv_scr_act() != ui_ScreenMain && lv_disp_get_scr_prev(NULL) == NULL;
  if (leftMain || lv_disp_get_inactive_time(NULL) > MENU_IDLE_TIMEOUT_MS) {
    closeMenu();
  }
}
