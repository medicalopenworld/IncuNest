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
// Columna de los tres niveles, a la derecha del QR.
constexpr lv_coord_t COL_X = 20 + QR_SIZE + 40;
constexpr lv_coord_t COL_W = (CARD_W - 20) - COL_X - 6;
// Dentro de cada fila: el texto no llega hasta el borde, que ahi va el boton.
constexpr lv_coord_t BTN_W = 110, BTN_H = 36;
constexpr lv_coord_t ROW_TEXT_W = COL_W - BTN_W - 10;
constexpr lv_coord_t ROW_Y0 = 52, ROW_DY = 90;

// Ambar de "esto te toca a ti" (el mismo del recuadro del tutorial guiado) y
// azul del texto normal de los dialogos.
constexpr uint32_t COLOR_DUE = 0xB26A00;
constexpr uint32_t COLOR_TEXT = 0x0B2E4F;
constexpr uint32_t COLOR_SUB = 0x666666;

bool s_open = false;
// Armado por el desbloqueo de pantalla. Se consume al abrir el pop-up.
bool s_armed = false;
uint32_t s_openedTick = 0;

lv_obj_t *s_overlay = nullptr;
lv_obj_t *s_content = nullptr;

lv_obj_t *makeBtn(lv_obj_t *parent, const char *text, lv_event_cb_t cb,
                  lv_color_t bg, void *userData = nullptr) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN);
  lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_center(lbl);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, userData);
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
  // Que el auto-bloqueo vuelva a contar desde cero y no desde el ultimo toque
  // de antes de abrirse: mientras el aviso estuvo abierto estuvo exento.
  lv_disp_trig_activity(NULL);
}

void buildContent();

// Registrar un nivel no cierra el aviso: el operador puede haber hecho dos
// (la diaria y la semanal caen juntas cada 7 dias), asi que se repinta con las
// fechas nuevas y el boton de abajo pasa de MAS TARDE a CERRAR cuando ya no
// queda nada por hacer.
void onLevelDone(lv_event_t *e) {
  const mnt_level_t lvl =
      (mnt_level_t)(uintptr_t)lv_event_get_user_data(e);
  Maintenance_MarkDone(lvl);
  buildContent();
  UI_ShowToast(TR(STR_MAINT_LOGGED), 2500);
}

// Un solo boton para las dos situaciones: si algo esta vencido es MAS TARDE
// (calla 24 h), y si no —abierto a mano desde Ajustes, o todo ya registrado—
// es CERRAR, que no tiene nada que aplazar.
void onDismiss(lv_event_t *) {
  if (Maintenance_ShouldWarn()) Maintenance_Snooze();
  closeDialog();
}

struct LevelText {
  ui_str_id_t name;
  ui_str_id_t when;
};
constexpr LevelText LEVELS[MNT_LEVEL_COUNT] = {
    {STR_MAINT_DAILY, STR_MAINT_DAILY_WHEN},
    {STR_MAINT_WEEKLY, STR_MAINT_WEEKLY_WHEN},
    {STR_MAINT_TERMINAL, STR_MAINT_TERMINAL_WHEN},
};

void buildRow(mnt_level_t lvl) {
  const lv_coord_t y = ROW_Y0 + ROW_DY * (lv_coord_t)lvl;
  const bool due = Maintenance_IsDue(lvl);

  // "DIARIA - TOCA AHORA" / "DIARIA - al dia". El separador es ASCII a
  // proposito: las Montserrat cargadas no tienen el punto medio, y el
  // static_assert del catalogo no vigila lo que se compone en runtime.
  char head[64];
  snprintf(head, sizeof(head), "%s - %s", TR(LEVELS[lvl].name),
           due ? TR(STR_MAINT_DUE_NOW) : TR(STR_MAINT_UP_TO_DATE));
  makeWrapLabel(head, COL_X, y, ROW_TEXT_W, &lv_font_montserrat_18,
                lv_color_hex(due ? COLOR_DUE : COLOR_TEXT));

  char last[64];
  Maintenance_FormatLastLine(lvl, last, sizeof(last));
  char sub[220];
  snprintf(sub, sizeof(sub), "%s  %s", TR(LEVELS[lvl].when), last);
  makeWrapLabel(sub, COL_X, y + 24, ROW_TEXT_W, &lv_font_montserrat_14,
                lv_color_hex(COLOR_SUB));

  lv_obj_t *btn =
      makeBtn(s_content, TR(STR_MAINT_DONE_UC), onLevelDone,
              lv_color_hex(due ? 0x2E7D32 : 0x9E9E9E),
              (void *)(uintptr_t)lvl);
  lv_obj_set_size(btn, BTN_W, BTN_H);
  lv_obj_align(btn, LV_ALIGN_TOP_LEFT, COL_X + ROW_TEXT_W + 10, y + 2);
}

void buildContent() {
  if (!s_content) return;
  lv_obj_clean(s_content);

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

  makeWrapLabel(TR(STR_MAINT_QR_HINT), 20, 384, QR_SIZE + 20,
                &lv_font_montserrat_14, lv_color_hex(COLOR_SUB));

  for (int i = 0; i < MNT_LEVEL_COUNT; i++) {
    buildRow((mnt_level_t)i);
  }

  lv_obj_t *dismiss = makeBtn(
      s_content,
      Maintenance_ShouldWarn() ? TR(STR_MAINT_LATER_UC) : TR(STR_CLOSE_UC),
      onDismiss, lv_color_hex(0x888888));
  lv_obj_set_size(dismiss, 170, 46);
  lv_obj_align(dismiss, LV_ALIGN_BOTTOM_RIGHT, -6, -6);
}

void openDialog() {
  if (!s_overlay) return;
  buildContent();
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

void MaintenanceDialog_Open(void) { openDialog(); }

void MaintenanceDialog_Poll(void) {
  Maintenance_Tick();

  if (s_open) {
    // Una alarma o el enlace perdido se llevan la pantalla por delante. El
    // tope cierra un aviso olvidado para devolverle el control al
    // auto-bloqueo: se cierra SIN contestar, asi que volvera a salir en el
    // siguiente desbloqueo si sigue habiendo algo vencido.
    if (mustYield() || lv_tick_elaps(s_openedTick) > MNT_IDLE_TIMEOUT_MS) {
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

  if (!Maintenance_ShouldWarn()) return;
  openDialog();
}
