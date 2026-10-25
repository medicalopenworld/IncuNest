#include "ui/MaintenanceDialog.h"

#include <cstdio>
#include <cstring>

#include "CommTask.h"
#include "Credentials_public.h"  // SUPPORT_TUTORIAL_URL
#include "UITask.h"
#include "main.h"
#include "modules/maintenance/maintenance.h"
#include "modules/maintenance/weigh_tracker.h"
#include "state/training_mode.h"  // Training_IsActive()
#include "ui.h"
// Los otros dialogos modales: el aviso espera su turno y no se pinta encima.
#include "ui/AlarmCenter.h"
#include "ui/BabyExitDialog.h"
#include "ui/BabyHistory.h"
#include "ui/BabyWizard.h"
#include "ui/FactoryTest.h"
#include "ui/HelpDialog.h"
#include "ui/InputKeypad.h"
#include "ui/TimeDialog.h"
#include "ui/training/training.h"

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

// Que cuenta el overlay. Es el mismo overlay, tarjeta y logica de ceder paso
// para los dos avisos; solo cambia el contenido.
enum class Mode : uint8_t {
  Maintenance,  // limpieza y mantenimiento (los tres niveles y el QR)
  WeighPrompt,  // "lleva N h sin pesar": PESAR AHORA / MAS TARDE
  WeighEntry,   // teclado numerico del peso del bebe activo
};

bool s_open = false;
Mode s_mode = Mode::Maintenance;
// Armados por el desbloqueo de pantalla, uno por aviso, para que si caen a la
// vez salgan uno detras de otro sin que cerrar el primero vuelva a armar nada.
// El de mantenimiento se consume al abrirse; el de pesada, en cuanto se
// evalua (ver MaintenanceDialog_Poll): el aviso es "al desbloquear", no a
// media tarde porque el plazo haya vencido con la pantalla ya en uso.
bool s_armedMaint = false;
bool s_armedWeigh = false;

// Textarea de la entrada de peso. Se invalida al limpiar el contenido.
lv_obj_t *s_weighTa = nullptr;
// Tras enviar una pesada la placa contesta con el rango NTE
// (g_pendingProfileRange). Nadie lo espera aqui, asi que se descarta hasta este
// instante (millis) para no dejarlo huerfano. 0 = nada que descartar.
uint32_t s_discardRangeUntilMs = 0;
constexpr uint32_t RANGE_DISCARD_MS = 3000;
// Mismo rango que el paso de peso del asistente (BabyWizard.cpp).
constexpr uint32_t WEIGH_MIN_G = 400, WEIGH_MAX_G = 5000;

lv_obj_t *s_overlay = nullptr;
lv_obj_t *s_content = nullptr;
// Sitio de reposo del overlay (ui_ScreenMain, el que se pasa a _Init). El
// aviso se muda a la pantalla activa al abrirse y vuelve aqui al cerrarse.
lv_obj_t *s_homeParent = nullptr;

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
  s_weighTa = nullptr;
  // Devolver el overlay a ui_ScreenMain: ahi es donde lo espera el camino
  // normal (_Poll() solo lo abre en la pantalla principal).
  if (s_overlay && s_homeParent &&
      lv_obj_get_parent(s_overlay) != s_homeParent) {
    lv_obj_set_parent(s_overlay, s_homeParent);
  }
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
  // En formacion no se registra nada: escribiria NVS del equipo de verdad,
  // justo lo que la franja "no recibe ordenes" niega (ADR-0002).
  if (Training_IsActive()) {
    UI_ShowToast(TR(STR_NOT_IN_TRAINING), 2500);
    return;
  }
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

// ---------------------------------------------------------------- pesada

void onWeighBack(lv_event_t *) {
  s_mode = Mode::WeighPrompt;
  buildContent();
}

void onWeighNow(lv_event_t *) {
  // El aviso no se abre en formacion; si la leccion arranca con el aviso ya
  // abierto, no se envia nada real desde aqui.
  if (Training_IsActive()) {
    UI_ShowToast(TR(STR_NOT_IN_TRAINING), 2500);
    return;
  }
  s_mode = Mode::WeighEntry;
  buildContent();
}

// MAS TARDE calla 24 h. Sin hora no se puede fechar el silencio y el aviso
// volvera en el siguiente desbloqueo (que, sin hora, no avisa de todos modos).
void onWeighLater(lv_event_t *) {
  WeighTracker_Snooze();
  closeDialog();
}

void onWeighRegister(lv_event_t *) {
  if (Training_IsActive()) {
    UI_ShowToast(TR(STR_NOT_IN_TRAINING), 2500);
    return;
  }
  uint32_t grams = 0;
  if (!s_weighTa ||
      !InputKeypad_ReadNumber(s_weighTa, WEIGH_MIN_G, WEIGH_MAX_G, &grams)) {
    char msg[64];
    snprintf(msg, sizeof(msg), TR(STR_VALUE_BETWEEN_FMT), (unsigned)WEIGH_MIN_G,
             (unsigned)WEIGH_MAX_G);
    UI_ShowToast(msg, 2500);
    return;
  }
  const uint32_t seq = BabyWizard_GetActiveSeq();
  if (seq == 0) {
    // El bebe ya no esta al mando (alta mientras el teclado estaba abierto).
    closeDialog();
    return;
  }
  // Descartar antes de enviar y, pasado un plazo, al abandonar la espera: el
  // rango que contesta la placa no lo usa nadie aqui y, huerfano, lo
  // adoptaria el siguiente peticionario (ver embedded-display-hmi.md).
  g_pendingProfileRange = false;
  Communication_SendProfileWeight(seq, (uint16_t)grams);  // sella la pesada
  s_discardRangeUntilMs = (millis() + RANGE_DISCARD_MS) | 1u;
  closeDialog();
  UI_ShowToast(TR(STR_WEIGH_LOGGED), 2500);
}

void buildWeighPrompt() {
  lv_obj_t *title = lv_label_create(s_content);
  lv_label_set_text(title, TR(STR_WEIGH_TITLE));
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

  const char *name = BabyWizard_GetActiveName();
  if (name == nullptr || name[0] == '\0') name = TR(STR_WEIGH_BABY);
  char msg[96];
  snprintf(msg, sizeof(msg), TR(STR_WEIGH_DUE_FMT), name,
           (unsigned)WeighTracker_ElapsedHours());
  lv_obj_t *lbl = makeWrapLabel(msg, 20, 110, CARD_W - 60,
                                &lv_font_montserrat_20,
                                lv_color_hex(COLOR_DUE));
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
  makeWrapLabel(TR(STR_WEIGH_HINT), 20, 190, CARD_W - 60,
                &lv_font_montserrat_14, lv_color_hex(COLOR_SUB));

  lv_obj_t *now = makeBtn(s_content, TR(STR_WEIGH_NOW_UC), onWeighNow,
                          lv_color_hex(0x2E7D32));
  lv_obj_set_size(now, 260, 56);
  lv_obj_align(now, LV_ALIGN_BOTTOM_LEFT, 6, -6);

  lv_obj_t *later = makeBtn(s_content, TR(STR_MAINT_LATER_UC), onWeighLater,
                            lv_color_hex(0x888888));
  lv_obj_set_size(later, 200, 56);
  lv_obj_align(later, LV_ALIGN_BOTTOM_RIGHT, -6, -6);
}

// Misma disposicion que el paso de peso del asistente (BabyWizard
// buildInputStep): textarea de dos a cinco digitos, pista de rango, ATRAS y
// REGISTRAR, teclado numerico debajo. Sin SALTAR ni SIN PESO: aqui el unico
// motivo para estar es registrar un peso.
void buildWeighEntry() {
  lv_obj_t *title = lv_label_create(s_content);
  lv_label_set_text(title, TR(STR_WEIGH_ENTRY_TITLE));
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

  lv_obj_t *ta = lv_textarea_create(s_content);
  lv_obj_set_size(ta, 220, 52);
  lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 40);
  lv_textarea_set_one_line(ta, true);
  lv_obj_set_style_text_font(ta, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_align(ta, LV_TEXT_ALIGN_CENTER, 0);
  lv_textarea_set_accepted_chars(ta, "0123456789");
  lv_textarea_set_max_length(ta, 5);
  lv_textarea_set_placeholder_text(ta, "...");
  // Vacio a proposito, como en el asistente: un valor prefijado invita a
  // confirmar sin pesar y dejaria el dato de ayer pasando por el de hoy.
  s_weighTa = ta;

  char hint[32];
  snprintf(hint, sizeof(hint), "%u - %u g", (unsigned)WEIGH_MIN_G,
           (unsigned)WEIGH_MAX_G);
  lv_obj_t *hintLbl = lv_label_create(s_content);
  lv_label_set_text(hintLbl, hint);
  lv_obj_set_style_text_color(hintLbl, lv_color_hex(COLOR_SUB), 0);
  lv_obj_align(hintLbl, LV_ALIGN_TOP_MID, 0, 98);

  lv_obj_t *back = makeBtn(s_content, TR(STR_BACK_UC), onWeighBack,
                           lv_color_hex(0x888888));
  lv_obj_set_size(back, 150, 46);
  lv_obj_align(back, LV_ALIGN_TOP_LEFT, 6, 126);

  lv_obj_t *reg = makeBtn(s_content, TR(STR_REGISTER_UC), onWeighRegister,
                          lv_color_hex(0x00AA00));
  lv_obj_set_size(reg, 190, 46);
  lv_obj_align(reg, LV_ALIGN_TOP_RIGHT, -6, 126);

  lv_obj_t *kb = InputKeypad_Create(s_content, ta, true);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, -4);
}

void buildContent() {
  if (!s_content) return;
  lv_obj_clean(s_content);
  s_weighTa = nullptr;

  if (s_mode == Mode::WeighPrompt) {
    buildWeighPrompt();
    return;
  }
  if (s_mode == Mode::WeighEntry) {
    buildWeighEntry();
    return;
  }

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

void openDialog(Mode mode) {
  if (!s_overlay) return;
  s_mode = mode;
  // El overlay cuelga de ui_ScreenMain, pero VER RECORDATORIO se pulsa desde
  // Ajustes: sin esto el aviso se pintaba en una pantalla que no estaba a la
  // vista y solo aparecia al volver atras. Se lleva a la pantalla activa y
  // closeDialog() lo devuelve a su sitio.
  lv_obj_t *scr = lv_scr_act();
  if (scr && lv_obj_get_parent(s_overlay) != scr) {
    lv_obj_set_parent(s_overlay, scr);
  }
  buildContent();
  s_open = true;
  // Solo se consume el armado del aviso que se abre: el otro sigue esperando
  // su turno.
  if (mode == Mode::Maintenance) {
    s_armedMaint = false;
  } else {
    s_armedWeigh = false;
  }
  // Que el tope de inactividad cuente desde ahora y no desde el ultimo toque
  // de antes de abrirse.
  lv_disp_trig_activity(NULL);
  lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(s_overlay);
}

}  // namespace

void MaintenanceDialog_Init(lv_obj_t *parent) {
  s_homeParent = parent;
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

void MaintenanceDialog_NoteUnlocked(void) {
  s_armedMaint = true;
  s_armedWeigh = true;
}

bool MaintenanceDialog_IsOpen(void) { return s_open; }

void MaintenanceDialog_Open(void) { openDialog(Mode::Maintenance); }

void MaintenanceDialog_Poll(void) {
  Maintenance_Tick();
  WeighTracker_Tick();

  // Respuesta de rango de la ultima pesada de este dialogo: se descarta en
  // cuanto llega, o al vencer el plazo. Si el asistente se abre entre medias,
  // el rango es suyo y no se toca.
  if (s_discardRangeUntilMs != 0) {
    if (BabyWizard_IsOpen()) {
      s_discardRangeUntilMs = 0;
    } else if (g_pendingProfileRange) {
      g_pendingProfileRange = false;
      s_discardRangeUntilMs = 0;
    } else if ((int32_t)(millis() - s_discardRangeUntilMs) >= 0) {
      s_discardRangeUntilMs = 0;
    }
  }

  if (s_open) {
    // Una alarma o el enlace perdido se llevan la pantalla por delante. El
    // tope cierra un aviso olvidado para devolverle el control al
    // auto-bloqueo: se cierra SIN contestar, asi que volvera a salir en el
    // siguiente desbloqueo si sigue habiendo algo vencido.
    // Tambien se cierra si la pantalla cambia debajo (el aviso puede haberse
    // abierto sobre Ajustes): un overlay huerfano en una pantalla que ya no
    // se ve dejaria s_open a true y bloquearia el auto-bloqueo.
    if (mustYield() ||
        lv_disp_get_inactive_time(NULL) > MNT_IDLE_TIMEOUT_MS ||
        (s_overlay && lv_obj_get_parent(s_overlay) != lv_scr_act())) {
      closeDialog();
    }
    return;
  }

  if (!s_armedMaint && !s_armedWeigh) return;
  // El aviso espera su turno: no se cuela encima de otro dialogo modal ni
  // durante una alarma, y solo sale en la pantalla principal (el overlay
  // cuelga de ui_ScreenMain, en otra pantalla no se veria).
  if (mustYield()) return;
  if (lv_scr_act() != ui_ScreenMain) return;
  if (HelpDialog_IsOpen() || Training_IsOpen() || AlarmCenter_IsOpen() ||
      BabyWizard_IsOpen() || BabyExitDialog_IsOpen() || TimeDialog_IsOpen() ||
      FactoryTest_IsOpen() || BabyHistory_IsOpen()) {
    return;
  }
  // En formacion la pantalla es del alumno: un aviso real encima de una
  // leccion la aborta y confunde lo que es de verdad con lo que es practica.
  if (Training_IsActive()) return;

  // Primero la limpieza. Si el aviso de pesada tambien esta armado, sale
  // detras (en la siguiente pasada, cuando este se cierre) porque su armado
  // sigue en pie.
  if (s_armedMaint && Maintenance_ShouldWarn()) {
    openDialog(Mode::Maintenance);
    return;
  }
  if (s_armedWeigh) {
    // Se evalua UNA vez por desbloqueo y se consume, toque o no: ver la nota
    // de s_armedWeigh.
    s_armedWeigh = false;
    if (WeighTracker_ShouldWarn()) openDialog(Mode::WeighPrompt);
  }
}
