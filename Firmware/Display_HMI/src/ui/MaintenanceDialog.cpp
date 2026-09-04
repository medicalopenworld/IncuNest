#include "ui/MaintenanceDialog.h"

#include <cstdio>
#include <cstring>

#include "CommTask.h"
#include "Credentials_public.h"  // SUPPORT_TUTORIAL_URL
#include "UITask.h"
#include "main.h"
#include "modules/maintenance/maintenance.h"
#include "ui.h"
// Los otros dialogos modales: el aviso espera su turno y no se pinta encima.
#include "ui/AlarmCenter.h"
#include "ui/BabyExitDialog.h"
#include "ui/BabyWizard.h"
#include "ui/HelpDialog.h"
#include "ui/HelpTour.h"
#include "ui/TimeDialog.h"

namespace {

// Misma tarjeta que HelpDialog/TimeDialog: el operador ya conoce ese cuadro.
constexpr lv_coord_t CARD_W = 780, CARD_H = 460;
// El QR de la URL de tutoriales es corto (version ~4); con 300 px sobra, es
// la misma medida que usa la vista "Video tutorial" de la ayuda.
constexpr lv_coord_t QR_SIZE = 300;
// Columna de texto a la derecha del QR.
constexpr lv_coord_t COL_X = 20 + QR_SIZE + 40;
constexpr lv_coord_t COL_W = (CARD_W - 20) - COL_X - 6;

bool s_open = false;
// Armado por el desbloqueo de pantalla. Se consume al abrir el pop-up.
bool s_armed = false;
uint32_t s_openedTick = 0;

lv_obj_t *s_overlay = nullptr;
lv_obj_t *s_content = nullptr;

lv_obj_t *makeBtn(lv_obj_t *parent, const char *text, lv_event_cb_t cb,
                  lv_color_t bg) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN);
  lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_center(lbl);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
  return btn;
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

// Mismo criterio que HelpDialog::mustYield(): este pop-up no lleva
// informacion de alarma propia que compense tapar la pantalla, asi que cede
// ante CUALQUIER alarma activa y ante la perdida del enlace con la placa.
bool mustYield() { return UI_IsAnyAlarmActive() || Display_IsBoardLinkLost(); }

void closeDialog() {
  if (s_overlay) lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  if (s_content) lv_obj_clean(s_content);
  s_open = false;
}

void onDone(lv_event_t *) {
  Maintenance_MarkDone();
  closeDialog();
  UI_ShowToast(TR(STR_MAINT_LOGGED), 2500);
}

void onLater(lv_event_t *) {
  Maintenance_Snooze();
  closeDialog();
}

void buildContent(mnt_reason_t reason) {
  lv_obj_t *title = lv_label_create(s_content);
  lv_label_set_text(title, TR(STR_MAINT_TITLE));
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

  // QR con zona de silencio: borde blanco para que la camara lo aisle del
  // fondo (igual que en HelpDialog).
  const char *url = SUPPORT_TUTORIAL_URL;
  lv_obj_t *qr = lv_qrcode_create(s_content, QR_SIZE, lv_color_hex(0x000000),
                                  lv_color_hex(0xFFFFFF));
  lv_obj_align(qr, LV_ALIGN_TOP_LEFT, 20, 56);
  lv_obj_set_style_border_color(qr, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_width(qr, 8, 0);
  lv_qrcode_update(qr, url, strlen(url));

  char msg[260];
  if (reason == MNT_REASON_NEW_BABY) {
    snprintf(msg, sizeof(msg), "%s", TR(STR_MAINT_NEW_BABY));
  } else {
    const int32_t days = Maintenance_DaysSince();
    snprintf(msg, sizeof(msg), TR(STR_MAINT_DUE_FMT), (long)(days < 0 ? 0 : days));
  }
  makeWrapLabel(msg, COL_X, 56, COL_W, &lv_font_montserrat_18,
                lv_color_hex(0x0B2E4F));

  makeWrapLabel(TR(STR_MAINT_QR_HINT), COL_X, 210, COL_W,
                &lv_font_montserrat_14, lv_color_hex(0x666666));

  char lastLine[96];
  Maintenance_FormatLastLine(lastLine, sizeof(lastLine));
  makeWrapLabel(lastLine, COL_X, 286, COL_W, &lv_font_montserrat_14,
                lv_color_hex(0x666666));

  lv_obj_t *later = makeBtn(s_content, TR(STR_MAINT_LATER_UC), onLater,
                            lv_color_hex(0x888888));
  lv_obj_set_size(later, 170, 46);
  lv_obj_align(later, LV_ALIGN_BOTTOM_RIGHT, -6, -6);

  lv_obj_t *done = makeBtn(s_content, TR(STR_MAINT_DONE_UC), onDone,
                           lv_color_hex(0x2E7D32));
  lv_obj_set_size(done, 260, 46);
  lv_obj_align(done, LV_ALIGN_BOTTOM_RIGHT, -186, -6);
}

void openWith(mnt_reason_t reason) {
  if (!s_overlay) return;
  lv_obj_clean(s_content);
  buildContent(reason);
  s_open = true;
  s_armed = false;
  s_openedTick = lv_tick_get();
  lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(s_overlay);
}

}  // namespace

void MaintenanceDialog_Init(lv_obj_t *parent) {
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

  // La tarjeta no se guarda: a diferencia de HelpDialog, aqui no hay boton X
  // que tenga que sobrevivir al lv_obj_clean() del contenido.
  lv_obj_t *card = lv_obj_create(s_overlay);
  lv_obj_set_size(card, CARD_W, CARD_H);
  lv_obj_center(card);
  lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
  lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  s_content = lv_obj_create(card);
  lv_obj_remove_style_all(s_content);
  lv_obj_set_size(s_content, CARD_W - 20, CARD_H - 20);
  lv_obj_center(s_content);
  lv_obj_clear_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);
}

void MaintenanceDialog_NoteUnlocked(void) { s_armed = true; }

bool MaintenanceDialog_IsOpen(void) { return s_open; }

void MaintenanceDialog_Poll(void) {
  Maintenance_Tick();

  if (s_open) {
    // Una alarma o el enlace perdido se llevan la pantalla por delante. El
    // tope cierra un aviso olvidado para devolverle el control al
    // auto-bloqueo: se cierra SIN contestar, asi que volvera a salir en el
    // siguiente desbloqueo (contestar es lo unico que lo calla).
    if (mustYield() || lv_tick_elaps(s_openedTick) > MNT_IDLE_TIMEOUT_MS ||
        Maintenance_PendingReason() == MNT_REASON_NONE) {
      closeDialog();
    }
    return;
  }

  if (!s_armed) return;
  // El aviso espera su turno: no se cuela encima de otro dialogo modal ni
  // durante una alarma, y solo sale en la pantalla principal (el overlay
  // cuelga de ui_ScreenMain, en otra pantalla no se veria).
  if (mustYield()) return;
  if (lv_scr_act() != ui_ScreenMain) return;
  if (HelpDialog_IsOpen() || HelpTour_IsOpen() || AlarmCenter_IsOpen() ||
      BabyWizard_IsOpen() || BabyExitDialog_IsOpen() || TimeDialog_IsOpen()) {
    return;
  }

  const mnt_reason_t reason = Maintenance_PendingReason();
  if (reason == MNT_REASON_NONE) return;
  openWith(reason);
}
