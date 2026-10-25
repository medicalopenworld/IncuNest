#include "ui/MaintenanceDialog.h"

#include <cstdio>
#include <cstring>

#include "CommTask.h"
#include "Credentials_public.h"  // SUPPORT_TUTORIAL_URL
#include "UITask.h"
#include "main.h"
#include "esp_timer.h"
#include "modules/maintenance/maintenance.h"
#include "modules/service/service_state.h"  // vista de servicio por horas
#include "state/training_mode.h"  // Training_IsActive()
#include "ui.h"
// Los otros dialogos modales: el aviso espera su turno y no se pinta encima.
#include "ui/AlarmCenter.h"
#include "ui/BabyExitDialog.h"
#include "ui/BabyWizard.h"
#include "ui/FactoryTest.h"
#include "ui/HelpDialog.h"
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

bool s_open = false;
// Armado por el desbloqueo de pantalla. Se consume al abrir el pop-up.
bool s_armed = false;

// Vista que pinta el dialogo: la de limpieza (la de siempre) o la de servicio
// por horas de uso de los actuadores. Comparten overlay, tarjeta y cesiones.
enum class Mode : uint8_t { MAINTENANCE, SERVICE };
Mode s_mode = Mode::MAINTENANCE;
// Actuador cuyo HECHO esta pulsado a la espera de CONFIRMAR; -1 = ninguno.
int s_svcConfirmId = -1;

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
  // Devolver el overlay a ui_ScreenMain: ahi es donde lo espera el camino
  // normal (_Poll() solo lo abre en la pantalla principal).
  if (s_overlay && s_homeParent &&
      lv_obj_get_parent(s_overlay) != s_homeParent) {
    lv_obj_set_parent(s_overlay, s_homeParent);
  }
  s_open = false;
  s_mode = Mode::MAINTENANCE;
  s_svcConfirmId = -1;
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

// ---------------------------------------------------------------------------
//  Vista de SERVICIO: horas de uso de cada actuador desde su ultimo servicio
//  (shared-service-hours). No es una alarma: sin AlarmId ni zumbador. Los datos
//  los manda la placa (CTRL,SERVICE) y se guardan en modules/service.
// ---------------------------------------------------------------------------
constexpr lv_coord_t SVC_ROW_Y0 = 50, SVC_ROW_DY = 78;
constexpr lv_coord_t SVC_TEXT_W = 440;
constexpr lv_coord_t SVC_BTN_X = 20 + SVC_TEXT_W + 10;

struct SvcText {
  ui_str_id_t name;
};
// En el orden de ServiceActuator.
constexpr SvcText SVC_NAMES[SVC_COUNT] = {
    {STR_SVC_HEATER},
    {STR_SVC_FAN},
    {STR_SVC_HUMIDIFIER},
    {STR_SVC_PHOTO},
};

// Reloj monotono en segundos para el "mas tarde": no depende de tener hora.
uint32_t svcNowSec() { return (uint32_t)(esp_timer_get_time() / 1000000LL); }

void onSvcDone(lv_event_t *e) {
  if (Training_IsActive()) {
    UI_ShowToast(TR(STR_NOT_IN_TRAINING), 2500);
    return;
  }
  // Primer paso: solo arma la confirmacion. Registrar un servicio no se puede
  // deshacer desde aqui y no hay PIN, asi que un toque suelto no basta.
  s_svcConfirmId = (int)(uintptr_t)lv_event_get_user_data(e);
  buildContent();
}

void onSvcCancel(lv_event_t *) {
  s_svcConfirmId = -1;
  buildContent();
}

void onSvcConfirm(lv_event_t *e) {
  const int id = (int)(uintptr_t)lv_event_get_user_data(e);
  s_svcConfirmId = -1;
  if (Training_IsActive()) {
    UI_ShowToast(TR(STR_NOT_IN_TRAINING), 2500);
    buildContent();
    return;
  }
  Communication_SendServiceDone((uint8_t)id);
  // La placa contesta con un CTRL,SERVICE que ya trae las horas a 0; hasta
  // entonces se sigue viendo el valor anterior, que es lo honesto.
  buildContent();
  UI_ShowToast(TR(STR_SVC_LOGGED), 2500);
}

void onSvcSwitchView(lv_event_t *) {
  s_svcConfirmId = -1;
  if (s_mode == Mode::SERVICE) {
    s_mode = Mode::MAINTENANCE;
  } else {
    s_mode = Mode::SERVICE;
    Communication_SendServiceReq();  // datos frescos al entrar
  }
  buildContent();
}

// Mismo contrato que onDismiss(): MAS TARDE si hay algo vencido sin callar,
// CERRAR si no.
void onSvcDismiss(lv_event_t *) {
  const uint32_t now = svcNowSec();
  if (ServiceState_ShouldWarn(now)) ServiceState_Snooze(now);
  closeDialog();
}

void buildSvcRow(uint8_t id) {
  const lv_coord_t y = SVC_ROW_Y0 + SVC_ROW_DY * (lv_coord_t)id;
  uint32_t hours = 0, thr = 0;
  if (!ServiceState_Get(id, &hours, &thr)) return;
  const bool due = ServiceState_IsDue(id);

  char head[64];
  snprintf(head, sizeof(head), "%s - %s", TR(SVC_NAMES[id].name),
           due ? TR(STR_MAINT_DUE_NOW) : TR(STR_MAINT_UP_TO_DATE));
  makeWrapLabel(head, 20, y, SVC_TEXT_W, &lv_font_montserrat_18,
                lv_color_hex(due ? COLOR_DUE : COLOR_TEXT));

  char sub[96];
  if (thr != 0) {
    snprintf(sub, sizeof(sub), TR(STR_SVC_HOURS_FMT), (unsigned long)hours,
             (unsigned long)thr);
  } else {
    snprintf(sub, sizeof(sub), TR(STR_SVC_HOURS_NOLIMIT_FMT),
             (unsigned long)hours);
  }
  makeWrapLabel(sub, 20, y + 26, SVC_TEXT_W, &lv_font_montserrat_14,
                lv_color_hex(COLOR_SUB));

  void *ud = (void *)(uintptr_t)id;
  if (s_svcConfirmId == (int)id) {
    lv_obj_t *ok = makeBtn(s_content, TR(STR_SVC_CONFIRM_UC), onSvcConfirm,
                           lv_color_hex(0x2E7D32), ud);
    lv_obj_set_size(ok, 140, BTN_H);
    lv_obj_align(ok, LV_ALIGN_TOP_LEFT, SVC_BTN_X, y + 2);
    lv_obj_t *no = makeBtn(s_content, TR(STR_CANCEL_UC), onSvcCancel,
                           lv_color_hex(0x9E9E9E));
    lv_obj_set_size(no, 120, BTN_H);
    lv_obj_align(no, LV_ALIGN_TOP_LEFT, SVC_BTN_X + 150, y + 2);
  } else {
    lv_obj_t *btn = makeBtn(s_content, TR(STR_SVC_DONE_UC), onSvcDone,
                            lv_color_hex(due ? 0x2E7D32 : 0x9E9E9E), ud);
    lv_obj_set_size(btn, BTN_W, BTN_H);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, SVC_BTN_X, y + 2);
  }
}

void buildServiceContent() {
  lv_obj_t *title = lv_label_create(s_content);
  lv_label_set_text(title, TR(STR_SVC_TITLE));
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

  if (!ServiceState_HasData()) {
    makeWrapLabel(TR(STR_SVC_NO_DATA), 20, SVC_ROW_Y0, CARD_W - 60,
                  &lv_font_montserrat_18, lv_color_hex(COLOR_SUB));
  } else {
    for (uint8_t i = 0; i < SVC_COUNT; i++) buildSvcRow(i);
  }

  makeWrapLabel(TR(STR_SVC_HINT), 20, 366, CARD_W - 60, &lv_font_montserrat_14,
                lv_color_hex(COLOR_SUB));

  lv_obj_t *back = makeBtn(s_content, TR(STR_SVC_BACK_UC), onSvcSwitchView,
                           lv_color_hex(0x0075EE));
  lv_obj_set_size(back, 170, 46);
  lv_obj_align(back, LV_ALIGN_BOTTOM_LEFT, 6, -6);

  lv_obj_t *dismiss = makeBtn(
      s_content,
      ServiceState_ShouldWarn(svcNowSec()) ? TR(STR_MAINT_LATER_UC)
                                           : TR(STR_CLOSE_UC),
      onSvcDismiss, lv_color_hex(0x888888));
  lv_obj_set_size(dismiss, 170, 46);
  lv_obj_align(dismiss, LV_ALIGN_BOTTOM_RIGHT, -6, -6);
}

void buildContent() {
  if (!s_content) return;
  lv_obj_clean(s_content);
  if (s_mode == Mode::SERVICE) {
    buildServiceContent();
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

  // Acceso a la vista de servicio, a la izquierda de MAS TARDE/CERRAR.
  lv_obj_t *svc = makeBtn(s_content, TR(STR_SVC_BUTTON_UC), onSvcSwitchView,
                          lv_color_hex(ServiceState_AnyDue() ? 0xB26A00
                                                             : 0x0075EE));
  lv_obj_set_size(svc, 170, 46);
  lv_obj_align(svc, LV_ALIGN_BOTTOM_RIGHT, -186, -6);
}

void openDialog() {
  if (!s_overlay) return;
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
  s_armed = false;
  // Que el tope de inactividad cuente desde ahora y no desde el ultimo toque
  // de antes de abrirse.
  lv_disp_trig_activity(NULL);
  lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(s_overlay);
}

// Abre el dialogo directamente en la vista de servicio (aviso suave).
void openServiceDialog() {
  s_mode = Mode::SERVICE;
  s_svcConfirmId = -1;
  Communication_SendServiceReq();  // por si hay horas mas frescas que el ultimo aviso
  openDialog();
}

// Una vez por vuelta de UI_Task. Pasa a modules/service el ultimo CTRL,SERVICE
// que dejo Comm_Task y, mientras no haya llegado ninguno, lo pide cada 30 s
// (la placa lo manda sola al UI_READY y cada hora, pero si el display arranco
// antes que ella no hay quien se lo recuerde).
void servicePollData() {
  if (g_pendingService) {
    g_pendingService = false;
    const ServiceReport rep = g_serviceReport;
    ServiceState_Apply(&rep);
    if (s_open && s_mode == Mode::SERVICE) buildContent();
    return;
  }
  if (ServiceState_HasData() || Display_IsBoardLinkLost()) return;
  static uint32_t lastReqSec = 0;
  static bool requested = false;
  const uint32_t now = svcNowSec();
  if (!requested || (uint32_t)(now - lastReqSec) >= 30u) {
    requested = true;
    lastReqSec = now;
    Communication_SendServiceReq();
  }
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

void MaintenanceDialog_NoteUnlocked(void) { s_armed = true; }

bool MaintenanceDialog_IsOpen(void) { return s_open; }

void MaintenanceDialog_Open(void) { openDialog(); }

void MaintenanceDialog_Poll(void) {
  Maintenance_Tick();
  servicePollData();

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

  if (!s_armed) return;
  // El aviso espera su turno: no se cuela encima de otro dialogo modal ni
  // durante una alarma, y solo sale en la pantalla principal (el overlay
  // cuelga de ui_ScreenMain, en otra pantalla no se veria).
  if (mustYield()) return;
  if (lv_scr_act() != ui_ScreenMain) return;
  if (HelpDialog_IsOpen() || Training_IsOpen() || AlarmCenter_IsOpen() ||
      BabyWizard_IsOpen() || BabyExitDialog_IsOpen() || TimeDialog_IsOpen() ||
      FactoryTest_IsOpen()) {
    return;
  }
  // En formacion la pantalla es del alumno: un aviso real encima de una
  // leccion la aborta y confunde lo que es de verdad con lo que es practica.
  if (Training_IsActive()) return;

  if (Maintenance_ShouldWarn()) {
    openDialog();
    return;
  }
  // Aviso de servicio por horas: mismas cesiones que la limpieza, y solo cuando
  // no hay limpieza pendiente (esa abre primero; el boton SERVICIO lleva a la
  // otra vista). NO depende del interruptor AVISOS de la limpieza (apagado de
  // fabrica): tiene el suyo, activado por defecto (ServiceState_ShouldWarn).
  if (ServiceState_ShouldWarn(svcNowSec())) {
    openServiceDialog();
  }
}
