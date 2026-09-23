#include "ui/HelpDialog.h"

#include <cstdio>
#include <cstring>

#include "CommTask.h"
#include "Credentials_public.h"
#include "UITask.h"
#include "main.h"
#include "modules/support/support_report.h"
#include "ui.h"
#include "ui/training/training.h"

// --- Shared state owned by UITask.cpp (same pattern TimeDialog.cpp uses) ---
namespace {

enum View { VIEW_MENU, VIEW_VIDEO, VIEW_CONTACT };

// Misma tarjeta que TimeDialog/BabyWizard: el operador ya conoce ese cuadro.
constexpr lv_coord_t CARD_W = 780, CARD_H = 460;
// El QR de la URL es corto (version ~4) y con 300 px sobra. El del mailto:
// llega a la version ~20 (97 modulos) con el informe: con 340 px lv_qrcode
// escala a 3 px/modulo enteros, el minimo que lee con soltura la camara de
// un movil a 15-20 cm.
constexpr lv_coord_t QR_SIZE = 300;
constexpr lv_coord_t QR_SIZE_MAILTO = 340;
// Columna de texto a la derecha del QR del mailto:.
constexpr lv_coord_t CONTACT_COL_X = 20 + QR_SIZE_MAILTO + 20;
constexpr lv_coord_t CONTACT_COL_W = (CARD_W - 20) - CONTACT_COL_X - 6;

bool s_open = false;
View s_view = VIEW_MENU;
// El informe se puede quitar del QR (boton SIN INFORME) si el movil no lee
// uno tan denso; con solo destinatario y asunto queda en la version ~5.
bool s_qrWithReport = true;
char s_mailto[SUPPORT_MAILTO_MAX];

lv_obj_t *s_overlay = nullptr;
lv_obj_t *s_card = nullptr;
lv_obj_t *s_content = nullptr;
// Hijo de la tarjeta y no del contenido, para que sobreviva al lv_obj_clean()
// de cada vista: es la unica salida del dialogo sin tocar nada.
lv_obj_t *s_closeBtn = nullptr;
lv_obj_t *s_qr = nullptr;
lv_obj_t *s_qrNoteLbl = nullptr;
lv_obj_t *s_qrToggleLbl = nullptr;

// `lblOut` devuelve el label para los botones cuyo texto cambia en runtime
// (mismo patron que HelpTour.cpp), en vez de buscarlo luego por indice de hijo.
lv_obj_t *makeBtn(lv_obj_t *parent, const char *text, lv_event_cb_t cb,
                  lv_color_t bg, lv_obj_t **lblOut = nullptr) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN);
  lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_center(lbl);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
  if (lblOut) *lblOut = lbl;
  return btn;
}

lv_obj_t *makeTitle(const char *text) {
  lv_obj_t *title = lv_label_create(s_content);
  lv_label_set_text(title, text);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);
  return title;
}

lv_obj_t *makeWrapLabel(const char *text, lv_coord_t x, lv_coord_t y,
                        lv_coord_t w, const lv_font_t *font,
                        lv_color_t color) {
  lv_obj_t *lbl = lv_label_create(s_content);
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lbl, w);
  lv_obj_set_style_text_font(lbl, font, 0);
  lv_obj_set_style_text_color(lbl, color, 0);
  lv_label_set_text(lbl, text);
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, x, y);
  return lbl;
}

// QR con zona de silencio: borde blanco para que la camara lo aisle del fondo.
lv_obj_t *makeQr(lv_coord_t size, lv_coord_t x, lv_coord_t y) {
  lv_obj_t *qr = lv_qrcode_create(s_content, size, lv_color_hex(0x000000),
                                  lv_color_hex(0xFFFFFF));
  lv_obj_align(qr, LV_ALIGN_TOP_LEFT, x, y);
  lv_obj_set_style_border_color(qr, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_width(qr, 8, 0);
  return qr;
}

void forgetContent() {
  s_qr = nullptr;
  s_qrNoteLbl = nullptr;
  s_qrToggleLbl = nullptr;
}

void closeDialog() {
  if (s_overlay) lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  if (s_content) lv_obj_clean(s_content);
  forgetContent();
  s_open = false;
  // El temporizador de auto-bloqueo estuvo en pausa mientras la ayuda estaba
  // abierta: que vuelva a contar desde cero, no desde el ultimo toque.
  lv_disp_trig_activity(NULL);
}

// Mismo criterio que TelemetryHistory::mustYield(): este dialogo es una vista
// sin informacion de alarma propia que compense tapar la pantalla, asi que
// cede ante CUALQUIER alarma activa (no solo las de UI_IsCriticalAlarmActive,
// que es una lista fija de ids y deja fuera varias de prioridad ALTA) y ante
// la perdida del enlace con la placa.
bool mustYield() { return UI_IsAnyAlarmActive() || Display_IsBoardLinkLost(); }

void showView(View v);
void refreshQr();

// ---- Callbacks --------------------------------------------------------------

void onClose(lv_event_t *) { closeDialog(); }
void onBackToMenu(lv_event_t *) { showView(VIEW_MENU); }
void onVideo(lv_event_t *) { showView(VIEW_VIDEO); }

void onContact(lv_event_t *) {
  s_qrWithReport = true;
  showView(VIEW_CONTACT);
}

void onTour(lv_event_t *) {
  closeDialog();
  Training_OpenSelector();
}

void onToggleReport(lv_event_t *) {
  s_qrWithReport = !s_qrWithReport;
  refreshQr();
}

// ---- Vistas -----------------------------------------------------------------

lv_obj_t *makeOption(lv_coord_t x, const char *symbol, const char *title,
                     const char *subtitle, lv_event_cb_t cb, lv_color_t bg) {
  // Desde y=56: el boton X de la tarjeta ocupa la esquina superior derecha
  // (0..46) y pisaba la tercera tarjeta.
  lv_obj_t *btn = lv_btn_create(s_content);
  lv_obj_set_size(btn, 236, 318);
  lv_obj_align(btn, LV_ALIGN_TOP_LEFT, x, 56);
  lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN);
  lv_obj_set_style_radius(btn, 14, LV_PART_MAIN);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *sym = lv_label_create(btn);
  lv_label_set_text(sym, symbol);
  lv_obj_set_style_text_font(sym, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(sym, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(sym, LV_ALIGN_TOP_MID, 0, 28);

  lv_obj_t *ttl = lv_label_create(btn);
  lv_label_set_long_mode(ttl, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(ttl, 210);
  lv_obj_set_style_text_align(ttl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(ttl, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(ttl, lv_color_hex(0xFFFFFF), 0);
  lv_label_set_text(ttl, title);
  lv_obj_align(ttl, LV_ALIGN_TOP_MID, 0, 112);

  lv_obj_t *sub = lv_label_create(btn);
  lv_label_set_long_mode(sub, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(sub, 210);
  lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(sub, lv_color_hex(0xF0F0F0), 0);
  lv_label_set_text(sub, subtitle);
  lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 186);
  return btn;
}

void buildMenu() {
  makeTitle(TR(STR_HELP_UC));

  // 3 tarjetas de 236 con 16 de hueco: 6 + 236 + 16 + 236 + 16 + 236 = 746,
  // dentro de los 760 del contenido.
  makeOption(6, LV_SYMBOL_LIST, TR(STR_HELP_TOUR_TITLE),
             TR(STR_HELP_TOUR_SUB), onTour, lv_color_hex(0x0075EE));
  makeOption(258, LV_SYMBOL_VIDEO, TR(STR_HELP_VIDEO_TITLE),
             TR(STR_HELP_VIDEO_SUB), onVideo, lv_color_hex(0x7B1FA2));
  makeOption(510, LV_SYMBOL_ENVELOPE, TR(STR_HELP_CONTACT_TITLE),
             TR(STR_HELP_CONTACT_SUB), onContact, lv_color_hex(0x00897B));
}

void buildVideo() {
  makeTitle(TR(STR_HELP_VIDEO_TITLE));

  const char *url = SUPPORT_TUTORIAL_URL;
  lv_obj_t *qr = makeQr(QR_SIZE, 20, 56);
  lv_qrcode_update(qr, url, strlen(url));

  makeWrapLabel(TR(STR_HELP_VIDEO_HINT), 360, 90, 380, &lv_font_montserrat_18,
                lv_color_hex(0x0B2E4F));

  makeWrapLabel(TR(STR_HELP_OR_TYPE_URL), 360, 236, 380,
                &lv_font_montserrat_14, lv_color_hex(0x666666));
  makeWrapLabel(url, 360, 258, 380, &lv_font_montserrat_16,
                lv_color_hex(0x0075EE));

  lv_obj_t *back = makeBtn(s_content, TR(STR_BACK_UC), onBackToMenu,
                           lv_color_hex(0x888888));
  lv_obj_set_size(back, 150, 46);
  lv_obj_align(back, LV_ALIGN_BOTTOM_RIGHT, -6, -6);
}

// Genera el mailto: y lo vuelca al QR. Si con informe no cabe
// (lv_qrcode_update devuelve LV_RES_INV), cae a solo destinatario + asunto y
// lo dice en pantalla.
void refreshQr() {
  if (!s_qr) return;
  bool withRep = s_qrWithReport;
  lv_res_t res = LV_RES_INV;
  for (int attempt = 0; attempt < 2 && res != LV_RES_OK; attempt++) {
    const size_t n =
        support_report_build_mailto(s_mailto, sizeof(s_mailto), withRep);
    if (n > 0) res = lv_qrcode_update(s_qr, s_mailto, (uint32_t)n);
    if (res != LV_RES_OK) {
      if (!withRep) break;
      withRep = false;
    }
  }
  s_qrWithReport = withRep;

  if (s_qrNoteLbl) {
    lv_label_set_text(s_qrNoteLbl, withRep ? TR(STR_HELP_QR_NOTE_FULL)
                                           : TR(STR_HELP_QR_NOTE_SHORT));
  }
  if (s_qrToggleLbl) {
    lv_label_set_text(s_qrToggleLbl, withRep ? TR(STR_HELP_NO_REPORT)
                                             : TR(STR_HELP_WITH_REPORT));
  }
}

void buildContact() {
  makeTitle(TR(STR_HELP_CONTACT_HEAD));

  // QR a la izquierda (y 50..390); todo lo demas, botones incluidos, en la
  // columna de la derecha para no pisarlo.
  s_qr = makeQr(QR_SIZE_MAILTO, 20, 50);

  makeWrapLabel(TR(STR_HELP_CONTACT_HINT), CONTACT_COL_X, 56, CONTACT_COL_W,
                &lv_font_montserrat_18, lv_color_hex(0x0B2E4F));

  char subject[SUPPORT_SUBJECT_MAX];
  support_report_subject(subject, sizeof(subject));
  char info[200];
  snprintf(info, sizeof(info), "%s %s\n%s %s", TR(STR_HELP_TO), SUPPORT_EMAIL,
           TR(STR_HELP_SUBJECT), subject);
  makeWrapLabel(info, CONTACT_COL_X, 170, CONTACT_COL_W, &lv_font_montserrat_14,
                lv_color_hex(0x666666));

  s_qrNoteLbl = makeWrapLabel("", CONTACT_COL_X, 236, CONTACT_COL_W,
                              &lv_font_montserrat_14, lv_color_hex(0x0B2E4F));

  // El texto del boton lo fija refreshQr() segun el estado del informe.
  lv_obj_t *toggle = makeBtn(s_content, "", onToggleReport,
                             lv_color_hex(0x888888), &s_qrToggleLbl);
  lv_obj_set_size(toggle, 200, 46);
  lv_obj_align(toggle, LV_ALIGN_BOTTOM_RIGHT, -166, -6);

  lv_obj_t *back = makeBtn(s_content, TR(STR_BACK_UC), onBackToMenu,
                           lv_color_hex(0x888888));
  lv_obj_set_size(back, 150, 46);
  lv_obj_align(back, LV_ALIGN_BOTTOM_RIGHT, -6, -6);

  refreshQr();
}

void showView(View v) {
  if (!s_content) return;
  lv_obj_clean(s_content);
  forgetContent();
  s_view = v;
  switch (v) {
    case VIEW_MENU: buildMenu(); break;
    case VIEW_VIDEO: buildVideo(); break;
    case VIEW_CONTACT: buildContact(); break;
  }
}

}  // namespace

void HelpDialog_Init(lv_obj_t *parent) {
  s_overlay = lv_obj_create(parent);
  lv_obj_remove_style_all(s_overlay);
  lv_obj_set_size(s_overlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  lv_obj_set_pos(s_overlay, 0, 0);
  lv_obj_set_style_bg_color(s_overlay, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_overlay, LV_OPA_70, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_overlay, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(s_overlay, 0, LV_PART_MAIN);
  lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);

  s_card = lv_obj_create(s_overlay);
  lv_obj_set_size(s_card, CARD_W, CARD_H);
  lv_obj_center(s_card);
  lv_obj_set_style_radius(s_card, 12, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_card, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_clear_flag(s_card, LV_OBJ_FLAG_SCROLLABLE);

  s_content = lv_obj_create(s_card);
  lv_obj_remove_style_all(s_content);
  lv_obj_set_size(s_content, CARD_W - 20, CARD_H - 20);
  lv_obj_center(s_content);
  lv_obj_clear_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);

  // Despues de s_content a proposito: ultimo hijo = se dibuja encima.
  s_closeBtn = makeBtn(s_card, "X", onClose, lv_color_hex(0xAA3333));
  lv_obj_set_size(s_closeBtn, 46, 46);
  lv_obj_align(s_closeBtn, LV_ALIGN_TOP_RIGHT, 0, 0);
}

void HelpDialog_Open(void) {
  if (!s_overlay) return;
  s_qrWithReport = true;
  showView(VIEW_MENU);
  s_open = true;
  lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(s_overlay);
}

bool HelpDialog_IsOpen(void) { return s_open; }

void HelpDialog_Poll(void) {
  if (!s_open) return;
  // Una alarma o el enlace perdido se llevan la pantalla por delante; y una
  // ayuda olvidada se cierra sola para devolverle el control al auto-bloqueo.
  if (mustYield() || lv_disp_get_inactive_time(NULL) > HELP_IDLE_TIMEOUT_MS) {
    closeDialog();
  }
}
