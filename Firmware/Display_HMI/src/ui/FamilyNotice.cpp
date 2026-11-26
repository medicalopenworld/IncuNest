#include "ui/FamilyNotice.h"

#include <cstring>

#include "CommTask.h"
#include "UITask.h"
#include "modules/family/family_view.h"
#include "ui.h"
#include "ui/ElementsCreation.h"
#include "ui/i18n.h"

namespace {

constexpr lv_coord_t CARD_W = 792;
constexpr lv_coord_t CARD_H = 117;
constexpr lv_coord_t QR_SIZE = 105;
constexpr lv_coord_t QR_MARGIN = 6;                       // margen blanco
constexpr lv_coord_t QR_BOX = QR_SIZE + 2 * QR_MARGIN;    // = CARD_H
constexpr lv_coord_t TEXT_PAD = 16;

lv_obj_t *s_card = nullptr;
lv_obj_t *s_holdLbl = nullptr;
lv_obj_t *s_scanLbl = nullptr;
lv_obj_t *s_qrBox = nullptr;
lv_obj_t *s_qr = nullptr;

// Ultimo mensaje valido (antes de recibir nada: sin aviso y sin URL).
FamilyMsg s_msg = {false, {0}};
// URL que tiene ahora el QR dibujado, y si cupo.
char s_qrUrl[FAMILY_URL_MAX + 1] = {0};
bool s_qrOk = false;

// Lo ultimo pintado: -1 = nada todavia, para forzar la primera pasada.
int s_shownMode = -1;
int s_shownLang = -1;
// Enlace con la placa en la pasada anterior (para ver el flanco).
bool s_prevLinkLost = false;

// Peticiones de estado tras recuperar el enlace. Una placa vieja nunca manda
// CTRL,FAMILY: se reintenta un numero acotado de veces y se para en cuanto
// llega una linea CTRL,FAMILY.
constexpr uint8_t REQ_STATE_TRIES = 3;
constexpr uint32_t REQ_STATE_RETRY_MS = 3000;
uint8_t s_reqLeft = 0;     // peticiones que faltan por mandar
uint32_t s_reqLastMs = 0;  // lv_tick_get() de la ultima mandada

// Una tarjeta que roba el toque dejaria sin abrir el deslizador de desbloqueo:
// ni ella ni ningun hijo deben ser clicables ni desplazables.
void makeInert(lv_obj_t *o) {
  lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *makeLabel(lv_obj_t *parent, const lv_font_t *font) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  makeInert(l);
  return l;
}

bool linkLost() { return Display_IsBoardLinkLost(); }

bool hasNotice() { return s_msg.notice; }

// Vuelca la URL al QR pequeno de la tarjeta solo si cambio. false = sin URL o
// no cupo (la tarjeta cae a modo TEXT). Corre en cada pasada, haya aviso o no,
// para que s_qrOk nunca se quede con el resultado de una URL vieja. Solo elige
// TEXT o TEXT_QR en la pantalla de bloqueo: la ayuda pinta su propio QR.
bool syncQr() {
  if (!s_qr) return false;
  if (s_msg.url[0] == '\0') {
    s_qrUrl[0] = '\0';
    s_qrOk = false;
    return false;
  }
  if (strcmp(s_qrUrl, s_msg.url) != 0) {
    strncpy(s_qrUrl, s_msg.url, sizeof(s_qrUrl) - 1);
    s_qrUrl[sizeof(s_qrUrl) - 1] = '\0';
    s_qrOk = lv_qrcode_update(s_qr, s_qrUrl, (uint32_t)strlen(s_qrUrl)) ==
             LV_RES_OK;
  }
  return s_qrOk;
}

void applyTexts() {
  lv_label_set_text(s_holdLbl, TR(STR_FAMILY_HOLDING));
  lv_label_set_text(s_scanLbl, TR(STR_FAMILY_SCAN));
}

void applyMode(FamilyViewMode mode) {
  if (mode == FAMILY_VIEW_HIDDEN) {
    lv_obj_add_flag(s_card, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  const bool withQr = (mode == FAMILY_VIEW_TEXT_QR);
  if (withQr) {
    lv_obj_clear_flag(s_qrBox, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_scanLbl, LV_OBJ_FLAG_HIDDEN);
    const lv_coord_t w = CARD_W - QR_BOX - 2 * TEXT_PAD;
    lv_obj_set_width(s_holdLbl, w);
    lv_obj_set_style_text_align(s_holdLbl, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(s_holdLbl, LV_ALIGN_TOP_LEFT, TEXT_PAD, 14);
    lv_obj_set_width(s_scanLbl, w);
    lv_obj_align(s_scanLbl, LV_ALIGN_BOTTOM_LEFT, TEXT_PAD, -14);
  } else {
    lv_obj_add_flag(s_qrBox, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_scanLbl, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(s_holdLbl, CARD_W - 2 * TEXT_PAD);
    lv_obj_set_style_text_align(s_holdLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_holdLbl, LV_ALIGN_CENTER, 0, 0);
  }
  lv_obj_clear_flag(s_card, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace

void FamilyNotice_Init(lv_obj_t *lockScreen) {
  s_card = lv_obj_create(lockScreen);
  lv_obj_remove_style_all(s_card);
  lv_obj_set_size(s_card, CARD_W, CARD_H);
  lv_obj_set_align(s_card, LV_ALIGN_BOTTOM_LEFT);
  lv_obj_set_x(s_card, 4);
  lv_obj_set_y(s_card, -1);
  lv_obj_set_style_bg_color(s_card, lv_color_hex(0x054E92), 0);
  lv_obj_set_style_bg_opa(s_card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(s_card, 8, 0);
  lv_obj_set_style_clip_corner(s_card, true, 0);
  lv_obj_add_flag(s_card, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(s_card, UI_FLAG_KEEP_TEXT_COLOR);
  makeInert(s_card);
  // Debajo del deslizador de desbloqueo (lo pisa ~28 px): se coloca en el
  // indice de ui_UnlockCont, que sube uno. El resto de hijos de la pantalla
  // conserva su orden relativo.
  if (ui_UnlockCont && lv_obj_get_parent(ui_UnlockCont) == lockScreen) {
    lv_obj_move_to_index(s_card, (int32_t)lv_obj_get_index(ui_UnlockCont));
  }

  s_holdLbl = makeLabel(s_card, &lv_font_montserrat_26);
  s_scanLbl = makeLabel(s_card, &lv_font_montserrat_18);

  // Margen blanco alrededor del QR: caja blanca de 105 + 2 x 6 px.
  s_qrBox = lv_obj_create(s_card);
  lv_obj_remove_style_all(s_qrBox);
  lv_obj_set_size(s_qrBox, QR_BOX, QR_BOX);
  lv_obj_align(s_qrBox, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_set_style_bg_color(s_qrBox, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(s_qrBox, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(s_qrBox, 8, 0);
  lv_obj_add_flag(s_qrBox, LV_OBJ_FLAG_HIDDEN);
  makeInert(s_qrBox);

  s_qr = lv_qrcode_create(s_qrBox, QR_SIZE, lv_color_hex(0x000000),
                          lv_color_hex(0xFFFFFF));
  lv_obj_align(s_qr, LV_ALIGN_CENTER, 0, 0);
  makeInert(s_qr);

  applyTexts();
  s_shownLang = (int)g_lang;
}

void FamilyNotice_Poll(void) {
  if (!s_card) return;

  // Flanco del enlace. Con el enlace caido se olvida el mensaje: la placa solo
  // reenvia CTRL,FAMILY al cambiar o ante UI_READY/REQ,STATE, asi que un aviso
  // viejo no puede reaparecer solo. Al volver, peticiones de estado (hasta
  // REQ_STATE_TRIES, cada REQ_STATE_RETRY_MS) hacen que la placa reenvie el
  // aviso y la URL. (Nadie mas pide estado al recuperar el enlace:
  // Display_StateSync_Service solo corre mientras !g_stateSynced, y esa
  // bandera no se baja nunca tras una caida.)
  const bool lost = linkLost();
  if (lost) {
    s_msg.notice = false;
    s_msg.url[0] = '\0';
    g_pendingFamily = false;  // un mensaje en vuelo tampoco vale
    s_reqLeft = 0;
  } else if (s_prevLinkLost) {
    s_reqLeft = REQ_STATE_TRIES;
    s_reqLastMs = lv_tick_get() - REQ_STATE_RETRY_MS;  // la primera, ya
  }
  s_prevLinkLost = lost;

  if (g_pendingFamily) {
    g_pendingFamily = false;
    s_msg = g_familyMsg;
    s_reqLeft = 0;  // la placa ya contesto: no hace falta insistir
  }

  if (s_reqLeft > 0 && lv_tick_elaps(s_reqLastMs) >= REQ_STATE_RETRY_MS) {
    Communication_RequestState();
    s_reqLastMs = lv_tick_get();
    s_reqLeft--;
  }

  FamilyViewInputs in = {};
  in.notice = hasNotice();
  // El QR pequeno se sincroniza haya aviso o no (ver syncQr).
  in.hasUrl = syncQr();
  in.onLockScreen = (lv_scr_act() == ui_ScreenLock);
  in.anyAlarm = UI_IsAnyAlarmActive();
  in.linkLost = linkLost();
  in.pulseOxShown = !lv_obj_has_flag(ui_LockPulseOxFrame, LV_OBJ_FLAG_HIDDEN);
  in.unlockSliderShown = !lv_obj_has_flag(ui_UnlockCont, LV_OBJ_FLAG_HIDDEN);

  const int mode = (int)family_view_mode(&in);
  const bool langChanged = s_shownLang != (int)g_lang;
  if (langChanged) {
    applyTexts();
    s_shownLang = (int)g_lang;
  }
  // El modo se reaplica tambien con el idioma nuevo: el ancho del texto no
  // cambia, pero asi el reflujo queda dentro de la misma rama.
  if (mode != s_shownMode || langChanged) {
    applyMode((FamilyViewMode)mode);
    s_shownMode = mode;
  }
}

bool Family_HasUrl(void) {
  // URL guardada y enlace vivo, haya aviso o no: la tarjeta Familia de la
  // ayuda es para que la enfermera ensene el QR a la madre al ingreso. No
  // depende del QR pequeno de la tarjeta de bloqueo (la ayuda pinta el suyo).
  return family_help_entry_visible(s_msg.url[0] != '\0', linkLost());
}

const char *Family_Url(void) { return Family_HasUrl() ? s_msg.url : ""; }
