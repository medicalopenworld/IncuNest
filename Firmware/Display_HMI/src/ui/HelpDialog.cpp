#include "ui/HelpDialog.h"

#include <Arduino.h>

#include <cstdio>
#include <cstring>

#include "CommTask.h"
#include "Credentials_public.h"
#include "UITask.h"
#include "Wifi_OTA.h"
#include "main.h"
#include "modules/support/support_report.h"
#include "ui.h"
#include "ui/HelpTour.h"

// --- Shared state owned by UITask.cpp (same pattern TimeDialog.cpp uses) ---
extern ui_lang_t g_lang;

namespace {

enum View { VIEW_MENU, VIEW_VIDEO, VIEW_CONTACT, VIEW_RESULT };

// Estado del envio desde el equipo que se pinta en la vista de resultado.
enum SendState {
  SEND_NONE,     // el operador eligio directamente el QR del movil
  SEND_OFFLINE,  // sin servidor: no se encolo nada
  SEND_SENDING,  // encolado, esperando a la tarea WiFi/OTA
  SEND_OK,
  SEND_FAIL,
};

// Misma tarjeta que TimeDialog/BabyWizard: el operador ya conoce ese cuadro.
constexpr lv_coord_t CARD_W = 780, CARD_H = 460;
// El QR de la URL es corto (version ~4) y con 300 px sobra. El del mailto:
// puede llegar a la version 24 (113 modulos) antes de degradar: con 340 px
// lv_qrcode escala a 3 px/modulo enteros (con 300 se quedaria en 2), que es
// el minimo que lee con soltura la camara de un movil a 15-20 cm.
constexpr lv_coord_t QR_SIZE = 300;
constexpr lv_coord_t QR_SIZE_MAILTO = 340;
// Columna de texto a la derecha del QR del mailto:.
constexpr lv_coord_t RESULT_COL_X = 20 + QR_SIZE_MAILTO + 20;
constexpr lv_coord_t RESULT_COL_W = (CARD_W - 20) - RESULT_COL_X - 6;

bool s_open = false;
View s_view = VIEW_MENU;
SendState s_send = SEND_NONE;
uint32_t s_sendT0 = 0;
// Contenido del QR mailto:. El informe lo puede quitar el operador (boton
// SIN INFORME) si su movil no lee un QR tan denso; el mensaje solo se quita
// automaticamente cuando ni asi cabe.
bool s_qrWithReport = true;
bool s_qrWithMessage = true;
char s_msg[SUPPORT_MESSAGE_MAX + 1];
char s_mailto[SUPPORT_MAILTO_MAX];

lv_obj_t *s_overlay = nullptr;
lv_obj_t *s_card = nullptr;
lv_obj_t *s_content = nullptr;
// Hijo de la tarjeta y no del contenido, para que sobreviva al lv_obj_clean()
// de cada vista: es la unica salida del dialogo sin tocar nada.
lv_obj_t *s_closeBtn = nullptr;
lv_obj_t *s_msgTa = nullptr;
lv_obj_t *s_statusLbl = nullptr;
lv_obj_t *s_qr = nullptr;
lv_obj_t *s_qrNoteLbl = nullptr;
lv_obj_t *s_qrToggleLbl = nullptr;

// lv_btnmatrix y no lv_keyboard, por el mismo motivo que en BabyWizard.cpp:
// en LVGL 8.3 el keymap de lv_keyboard vive en un global de fichero, asi que
// lv_keyboard_set_map() se lo cambiaria tambien al teclado permanente de
// credenciales WiFi (ui_Keyboard1). Letras y cifras en un solo mapa para no
// necesitar boton de cambio de modo.
const char *KB_MAP[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
                        "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
                        "A", "S", "D", "F", "G", "H", "J", "K", "L", "-", "\n",
                        "Z", "X", "C", "V", "B", "N", "M", ".",
                        LV_SYMBOL_BACKSPACE, "\n",
                        " ", ""};
// 40 botones: 10 + 10 + 10 + 9 + 1. Debe cuadrar con KB_MAP. La tecla de
// borrar ocupa dos anchos para que la fila cuadre con las de arriba.
const lv_btnmatrix_ctrl_t KB_CTRL[40] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 2,
    1};

const char *TXT(const char *es, const char *en, const char *fr) {
  return (g_lang == LANG_ES) ? es : (g_lang == LANG_FR) ? fr : en;
}

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

void forgetContent() {
  s_msgTa = nullptr;
  s_statusLbl = nullptr;
  s_qr = nullptr;
  s_qrNoteLbl = nullptr;
  s_qrToggleLbl = nullptr;
}

void closeDialog() {
  if (s_overlay) lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  if (s_content) lv_obj_clean(s_content);
  forgetContent();
  s_open = false;
  // Cerrar es abandonar la conversacion: una peticion aun no publicada no
  // debe salir mas tarde sin que nadie vea el resultado.
  SupportRequest_Reset();
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

// ---- Callbacks --------------------------------------------------------------

void onClose(lv_event_t *) { closeDialog(); }
void onBackToMenu(lv_event_t *) { showView(VIEW_MENU); }
void onVideo(lv_event_t *) { showView(VIEW_VIDEO); }
void onContact(lv_event_t *) { showView(VIEW_CONTACT); }

void onTour(lv_event_t *) {
  closeDialog();
  HelpTour_Start();
}

void onKeyPress(lv_event_t *e) {
  lv_obj_t *bm = lv_event_get_target(e);
  lv_obj_t *ta = (lv_obj_t *)lv_event_get_user_data(e);
  if (!bm || !ta) return;
  const char *txt =
      lv_btnmatrix_get_btn_text(bm, lv_btnmatrix_get_selected_btn(bm));
  if (!txt) return;
  if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
    lv_textarea_del_char(ta);
  } else {
    lv_textarea_add_text(ta, txt);
  }
}

void captureMessage() {
  if (!s_msgTa) return;
  const char *t = lv_textarea_get_text(s_msgTa);
  strncpy(s_msg, t ? t : "", SUPPORT_MESSAGE_MAX);
  s_msg[SUPPORT_MESSAGE_MAX] = '\0';
}

void onSend(lv_event_t *) {
  captureMessage();
  s_qrWithReport = true;
  s_qrWithMessage = true;
  if (!WIFIIsConnectedToServer()) {
    // Sin servidor no se encola nada: una peticion que saliera horas despues
    // ya no tendria relacion con lo que el operador quiso contar.
    s_send = SEND_OFFLINE;
  } else {
    SupportRequest_Submit(s_msg);
    s_send = SEND_SENDING;
    s_sendT0 = millis();
  }
  showView(VIEW_RESULT);
}

void onQrMobile(lv_event_t *) {
  captureMessage();
  s_qrWithReport = true;
  s_qrWithMessage = true;
  s_send = SEND_NONE;
  showView(VIEW_RESULT);
}

void refreshQr();

void onToggleReport(lv_event_t *) {
  s_qrWithReport = !s_qrWithReport;
  s_qrWithMessage = true;
  refreshQr();
}

// ---- Vistas -----------------------------------------------------------------

lv_obj_t *makeOption(lv_coord_t x, const char *symbol, const char *title,
                     const char *subtitle, lv_event_cb_t cb, lv_color_t bg) {
  lv_obj_t *btn = lv_btn_create(s_content);
  lv_obj_set_size(btn, 236, 330);
  lv_obj_align(btn, LV_ALIGN_TOP_LEFT, x, 44);
  lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN);
  lv_obj_set_style_radius(btn, 14, LV_PART_MAIN);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *sym = lv_label_create(btn);
  lv_label_set_text(sym, symbol);
  lv_obj_set_style_text_font(sym, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(sym, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(sym, LV_ALIGN_TOP_MID, 0, 34);

  lv_obj_t *ttl = lv_label_create(btn);
  lv_label_set_long_mode(ttl, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(ttl, 210);
  lv_obj_set_style_text_align(ttl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(ttl, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(ttl, lv_color_hex(0xFFFFFF), 0);
  lv_label_set_text(ttl, title);
  lv_obj_align(ttl, LV_ALIGN_TOP_MID, 0, 122);

  lv_obj_t *sub = lv_label_create(btn);
  lv_label_set_long_mode(sub, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(sub, 210);
  lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(sub, lv_color_hex(0xF0F0F0), 0);
  lv_label_set_text(sub, subtitle);
  lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 196);
  return btn;
}

void buildMenu() {
  makeTitle(TXT("AYUDA", "HELP", "AIDE"));

  // 3 tarjetas de 236 con 16 de hueco: 6 + 236 + 16 + 236 + 16 + 236 = 746,
  // dentro de los 760 del contenido.
  makeOption(6, LV_SYMBOL_LIST,
             TXT("TUTORIAL GUIADO", "GUIDED TUTORIAL", "TUTORIEL GUIDE"),
             TXT("Recorre la pantalla paso a paso: que hace cada boton y "
                 "como usar cada funcion.",
                 "Walk through the screen step by step: what each button "
                 "does and how to use each function.",
                 "Parcourez l'ecran pas a pas : le role de chaque bouton et "
                 "comment utiliser chaque fonction."),
             onTour, lv_color_hex(0x0075EE));
  makeOption(258, LV_SYMBOL_VIDEO,
             TXT("VIDEO TUTORIAL", "VIDEO TUTORIAL", "TUTORIEL VIDEO"),
             TXT("Escanea un codigo QR con tu movil para ver el video en "
                 "nuestra web.",
                 "Scan a QR code with your phone to watch the video on our "
                 "website.",
                 "Scannez un code QR avec votre telephone pour voir la video "
                 "sur notre site."),
             onVideo, lv_color_hex(0x7B1FA2));
  makeOption(510, LV_SYMBOL_ENVELOPE,
             TXT("CONTACTAR SOPORTE", "CONTACT SUPPORT", "CONTACTER LE SUPPORT"),
             TXT("Escribe a soporte tecnico. El numero de serie y el estado "
                 "del equipo se adjuntan solos.",
                 "Write to technical support. Serial number and device "
                 "status are attached automatically.",
                 "Ecrivez au support technique. Le numero de serie et l'etat "
                 "de l'appareil sont joints automatiquement."),
             onContact, lv_color_hex(0x00897B));
}

void buildVideo() {
  makeTitle(TXT("VIDEO TUTORIAL", "VIDEO TUTORIAL", "TUTORIEL VIDEO"));

  const char *url = SUPPORT_TUTORIAL_URL;
  lv_obj_t *qr = lv_qrcode_create(s_content, QR_SIZE, lv_color_hex(0x000000),
                                  lv_color_hex(0xFFFFFF));
  lv_qrcode_update(qr, url, strlen(url));
  lv_obj_align(qr, LV_ALIGN_TOP_LEFT, 20, 56);
  // Zona de silencio del QR: borde blanco para que la camara lo aisle del
  // fondo de la tarjeta.
  lv_obj_set_style_border_color(qr, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_width(qr, 8, 0);

  makeWrapLabel(TXT("Escanea el codigo con la camara de tu movil para abrir "
                    "el video tutorial en la web de Medical Open World.",
                    "Scan the code with your phone camera to open the video "
                    "tutorial on the Medical Open World website.",
                    "Scannez le code avec l'appareil photo de votre telephone "
                    "pour ouvrir le tutoriel video sur le site de Medical "
                    "Open World."),
                360, 90, 380, &lv_font_montserrat_18, lv_color_hex(0x0B2E4F));

  makeWrapLabel(TXT("O escribe esta direccion:", "Or type this address:",
                    "Ou saisissez cette adresse :"),
                360, 236, 380, &lv_font_montserrat_14, lv_color_hex(0x666666));
  makeWrapLabel(url, 360, 258, 380, &lv_font_montserrat_16,
                lv_color_hex(0x0075EE));

  lv_obj_t *back = makeBtn(s_content, TXT("VOLVER", "BACK", "RETOUR"),
                           onBackToMenu, lv_color_hex(0x888888));
  lv_obj_set_size(back, 150, 46);
  lv_obj_align(back, LV_ALIGN_BOTTOM_RIGHT, -6, -6);
}

void buildContact() {
  makeTitle(TXT("CONTACTAR CON SOPORTE", "CONTACT SUPPORT",
                "CONTACTER LE SUPPORT"));

  char subject[SUPPORT_SUBJECT_MAX];
  support_report_subject(subject, sizeof(subject));
  char hdr[160];
  snprintf(hdr, sizeof(hdr), "%s %s   |   %s %s",
           TXT("Para:", "To:", "A :"), SUPPORT_EMAIL,
           TXT("Asunto:", "Subject:", "Objet :"), subject);
  lv_obj_t *hdrLbl = makeWrapLabel(hdr, 6, 32, 748, &lv_font_montserrat_14,
                                   lv_color_hex(0x666666));
  lv_label_set_long_mode(hdrLbl, LV_LABEL_LONG_DOT);

  s_msgTa = lv_textarea_create(s_content);
  lv_obj_set_size(s_msgTa, 748, 48);
  lv_obj_align(s_msgTa, LV_ALIGN_TOP_LEFT, 6, 54);
  lv_textarea_set_one_line(s_msgTa, true);
  lv_textarea_set_max_length(s_msgTa, SUPPORT_MESSAGE_MAX);
  lv_obj_set_style_text_font(s_msgTa, &lv_font_montserrat_20, 0);
  // El aviso de privacidad va en el propio campo: este texto sale del equipo
  // por MQTT en claro y queda a la vista en el QR, asi que nunca debe llevar
  // datos del paciente (el informe tecnico ya va aparte y no los contiene).
  lv_textarea_set_placeholder_text(
      s_msgTa, TXT("Describe el problema (opcional). Sin datos del paciente.",
                   "Describe the problem (optional). No patient data.",
                   "Decrivez le probleme (facultatif). Sans donnees patient."));
  if (s_msg[0]) lv_textarea_set_text(s_msgTa, s_msg);

  lv_obj_t *back = makeBtn(s_content, TXT("VOLVER", "BACK", "RETOUR"),
                           onBackToMenu, lv_color_hex(0x888888));
  lv_obj_set_size(back, 150, 46);
  lv_obj_align(back, LV_ALIGN_TOP_LEFT, 6, 110);

  lv_obj_t *qr = makeBtn(s_content, TXT("QR MOVIL", "PHONE QR", "QR MOBILE"),
                         onQrMobile, lv_color_hex(0x7B1FA2));
  lv_obj_set_size(qr, 200, 46);
  lv_obj_align(qr, LV_ALIGN_TOP_MID, 0, 110);

  lv_obj_t *send = makeBtn(s_content, TXT("ENVIAR", "SEND", "ENVOYER"),
                           onSend, lv_color_hex(0x00AA00));
  lv_obj_set_size(send, 200, 46);
  lv_obj_align(send, LV_ALIGN_TOP_RIGHT, -6, 110);

  lv_obj_t *kb = lv_btnmatrix_create(s_content);
  lv_btnmatrix_set_map(kb, KB_MAP);
  lv_btnmatrix_set_ctrl_map(kb, KB_CTRL);
  lv_obj_set_size(kb, 750, 250);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, -4);
  lv_obj_set_style_text_font(kb, &lv_font_montserrat_20, LV_PART_ITEMS);
  lv_obj_add_event_cb(kb, onKeyPress, LV_EVENT_VALUE_CHANGED, s_msgTa);
}

const char *sendStatusText(lv_color_t *color) {
  switch (s_send) {
    case SEND_OFFLINE:
      *color = lv_color_hex(0xE08800);
      return TXT("Sin conexion con el servidor. Envia el correo desde tu "
                 "movil con el QR.",
                 "No connection to the server. Send the email from your phone "
                 "with the QR.",
                 "Pas de connexion au serveur. Envoyez l'e-mail depuis votre "
                 "telephone avec le QR.");
    case SEND_SENDING:
      *color = lv_color_hex(0x0B2E4F);
      return TXT("Enviando desde el equipo...", "Sending from the device...",
                 "Envoi depuis l'appareil...");
    case SEND_OK:
      *color = lv_color_hex(0x00AA00);
      return TXT("Peticion registrada en el servidor. Puedes ademas enviar "
                 "el correo desde tu movil.",
                 "Request logged on the server. You can also send the email "
                 "from your phone.",
                 "Demande enregistree sur le serveur. Vous pouvez aussi "
                 "envoyer l'e-mail depuis votre telephone.");
    case SEND_FAIL:
      *color = lv_color_hex(0xAA3333);
      return TXT("No se pudo enviar desde el equipo. Usa el QR con tu movil.",
                 "Could not send from the device. Use the QR with your phone.",
                 "Envoi impossible depuis l'appareil. Utilisez le QR avec "
                 "votre telephone.");
    case SEND_NONE:
    default:
      *color = lv_color_hex(0x0B2E4F);
      return TXT("Escanea el QR con tu movil: se abrira un correo con todo "
                 "relleno, solo tienes que enviarlo.",
                 "Scan the QR with your phone: an email opens with everything "
                 "filled in, just send it.",
                 "Scannez le QR avec votre telephone : un e-mail s'ouvre "
                 "deja rempli, il suffit de l'envoyer.");
  }
}

void refreshStatus() {
  if (!s_statusLbl) return;
  lv_color_t color;
  const char *txt = sendStatusText(&color);
  lv_label_set_text(s_statusLbl, txt);
  lv_obj_set_style_text_color(s_statusLbl, color, 0);
}

// Genera el mailto: y lo vuelca al QR, degradando el contenido si no cabe:
// primero cae el mensaje libre, despues el informe. Lo que quede fuera se
// dice en pantalla para que el operador sepa que tendra que escribirlo.
void refreshQr() {
  if (!s_qr) return;
  bool withMsg = s_qrWithMessage && s_msg[0] != '\0';
  bool withRep = s_qrWithReport;
  lv_res_t res = LV_RES_INV;
  for (int attempt = 0; attempt < 3 && res != LV_RES_OK; attempt++) {
    const size_t n = support_report_build_mailto(s_mailto, sizeof(s_mailto),
                                                 s_msg, withMsg, withRep);
    if (n > 0) res = lv_qrcode_update(s_qr, s_mailto, (uint32_t)n);
    if (res != LV_RES_OK) {
      if (withMsg) {
        withMsg = false;
      } else if (withRep) {
        withRep = false;
      } else {
        break;
      }
    }
  }
  s_qrWithMessage = withMsg;
  s_qrWithReport = withRep;

  if (s_qrNoteLbl) {
    const char *note;
    if (withMsg && withRep) {
      note = TXT("El QR incluye tu mensaje y el informe tecnico del equipo.",
                 "The QR includes your message and the device technical "
                 "report.",
                 "Le QR inclut votre message et le rapport technique de "
                 "l'appareil.");
    } else if (withRep) {
      note = s_msg[0] ? TXT("El QR incluye el informe tecnico. Tu mensaje no "
                            "cabia: escribelo en el correo.",
                            "The QR includes the technical report. Your "
                            "message did not fit: type it in the email.",
                            "Le QR inclut le rapport technique. Votre message "
                            "ne tenait pas : saisissez-le dans l'e-mail.")
                      : TXT("El QR incluye el informe tecnico del equipo.",
                            "The QR includes the device technical report.",
                            "Le QR inclut le rapport technique de l'appareil.");
    } else if (withMsg) {
      note = TXT("QR sin informe tecnico: solo destinatario, asunto y tu "
                 "mensaje.",
                 "QR without technical report: recipient, subject and your "
                 "message only.",
                 "QR sans rapport technique : destinataire, objet et votre "
                 "message seulement.");
    } else {
      note = TXT("QR minimo: solo destinatario y asunto.",
                 "Minimal QR: recipient and subject only.",
                 "QR minimal : destinataire et objet seulement.");
    }
    lv_label_set_text(s_qrNoteLbl, note);
  }
  if (s_qrToggleLbl) {
    lv_label_set_text(s_qrToggleLbl,
                      s_qrWithReport
                          ? TXT("SIN INFORME", "NO REPORT", "SANS RAPPORT")
                          : TXT("CON INFORME", "WITH REPORT", "AVEC RAPPORT"));
  }
}

void buildResult() {
  makeTitle(TXT("ENVIAR DESDE EL MOVIL", "SEND FROM YOUR PHONE",
                "ENVOYER DEPUIS LE TELEPHONE"));

  // QR a la izquierda (y 50..390); todo lo demas, botones incluidos, en la
  // columna de la derecha para no pisarlo.
  s_qr = lv_qrcode_create(s_content, QR_SIZE_MAILTO, lv_color_hex(0x000000),
                          lv_color_hex(0xFFFFFF));
  lv_obj_align(s_qr, LV_ALIGN_TOP_LEFT, 20, 50);
  lv_obj_set_style_border_color(s_qr, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_width(s_qr, 8, 0);

  s_statusLbl = makeWrapLabel("", RESULT_COL_X, 56, RESULT_COL_W,
                              &lv_font_montserrat_18, lv_color_hex(0x0B2E4F));
  refreshStatus();

  char subject[SUPPORT_SUBJECT_MAX];
  support_report_subject(subject, sizeof(subject));
  char info[200];
  snprintf(info, sizeof(info), "%s %s\n%s %s", TXT("Para:", "To:", "A :"),
           SUPPORT_EMAIL, TXT("Asunto:", "Subject:", "Objet :"), subject);
  makeWrapLabel(info, RESULT_COL_X, 186, RESULT_COL_W, &lv_font_montserrat_14,
                lv_color_hex(0x666666));

  s_qrNoteLbl = makeWrapLabel("", RESULT_COL_X, 250, RESULT_COL_W,
                              &lv_font_montserrat_14, lv_color_hex(0x0B2E4F));

  // El texto del boton lo fija refreshQr() segun el estado del informe.
  lv_obj_t *toggle = makeBtn(s_content, "", onToggleReport,
                             lv_color_hex(0x888888), &s_qrToggleLbl);
  lv_obj_set_size(toggle, 200, 46);
  lv_obj_align(toggle, LV_ALIGN_BOTTOM_RIGHT, -166, -6);

  lv_obj_t *close = makeBtn(s_content, TXT("CERRAR", "CLOSE", "FERMER"),
                            onClose, lv_color_hex(0x0075EE));
  lv_obj_set_size(close, 150, 46);
  lv_obj_align(close, LV_ALIGN_BOTTOM_RIGHT, -6, -6);

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
    case VIEW_RESULT: buildResult(); break;
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
  // Cualquier resultado de una peticion anterior es de otra conversacion.
  SupportRequest_Reset();
  s_msg[0] = '\0';
  s_send = SEND_NONE;
  s_qrWithReport = true;
  s_qrWithMessage = true;
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
    return;
  }
  if (s_view != VIEW_RESULT || s_send != SEND_SENDING) return;

  const SupportRequestState st = SupportRequest_GetState();
  if (st == SUPPORT_SENT) {
    s_send = SEND_OK;
  } else if (st == SUPPORT_FAILED) {
    s_send = SEND_FAIL;
  } else if (millis() - s_sendT0 > SUPPORT_SEND_TIMEOUT_MS) {
    // Sin respuesta de la tarea WiFi: se descarta la peticion para que no
    // salga mas tarde sin que el operador lo sepa.
    SupportRequest_Reset();
    s_send = SEND_FAIL;
  }
  if (s_send != SEND_SENDING) refreshStatus();
}
