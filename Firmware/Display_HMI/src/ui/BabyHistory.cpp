#include "ui/BabyHistory.h"

#include <cstdio>
#include <cstring>
#include <ctime>

#include "CommTask.h"
#include "UITask.h"
#include "main.h"
#include "state/training_mode.h"
#include "ui.h"
#include "ui/BabyWizard.h"  // BabyWizard_HasLiveSession(), GetActiveSeq()
#include "ui/InputKeypad.h"

namespace {

enum class HistStep {
  Closed,
  LoadingActive,    // waiting CTRL,PROFILE_LIST
  LoadingArchived,  // waiting CTRL,PROFILE_HISTORY
  Showing,
  DischargeDialog,      // outcome selection open (pure UI)
  WaitingDischargeAck,  // waiting CTRL,PROFILE_ACK
  LoadingChart,         // waiting CTRL,WEIGHT_HISTORY
  ShowingChart,
  // Registro de un bebe nuevo (BEBE NUEVO). Tres pantallas de solo UI y un
  // unico punto de envio, REGISTRAR, en la ultima: hasta entonces la X no
  // deja rastro ni en la placa ni en ThingsBoard.
  NewName,
  NewGest,
  NewWeight,
  WaitingNewAck,    // waiting CTRL,PROFILE_ACK to HMI,PROFILE_NEW
  WaitingNewRange,  // waiting CTRL,PROFILE_RANGE to HMI,PROFILE_WEIGHT (ignored)
  // Tras registrar con un bebe bajo terapia: PROFILE_SELECT de ese bebe para
  // que la placa vuelva a tenerlo como "bebe del asistente" (ver
  // afterRegistered()).
  WaitingReselectAck,
};

constexpr uint32_t RESP_TIMEOUT_MS = 2000;
// Como ACK_TIMEOUT_MS del asistente. Sin reintento del PROFILE_NEW: si la
// primera trama llego y se perdio el ACK, reenviarla crearia dos perfiles.
constexpr uint32_t NEW_TIMEOUT_MS = 3000;
constexpr uint32_t PAGE_SIZE = 10;
constexpr size_t BABY_NAME_LEN_LOCAL = 24;  // matches motherBoard BABY_NAME_LEN

// Lista y grafica en la tarjeta de siempre; las pantallas de teclado del
// registro usan la tarjeta grande del asistente para que las teclas midan lo
// mismo en los dos sitios.
constexpr lv_coord_t CARD_W_SMALL = 660, CARD_H_SMALL = 430;
constexpr lv_coord_t CARD_W_BIG = 780, CARD_H_BIG = 460;

HistStep s_step = HistStep::Closed;
uint32_t s_deadlineMs = 0;
int s_retries = 0;
uint32_t s_page = 0;
BabyProfileListMsg s_active = {0, {}};
BabyHistoryMsg s_archived = {0, 0, 0, {}};
bool s_activeLoaded = false;
// Verdadero solo si s_active la contesto la placa (o la simulacion de
// formacion); falso cuando la carga vencio y la lista quedo vacia por
// defecto. La guarda de tres activos solo vale con una lista real.
bool s_activeFromBoard = false;

uint32_t s_dischargeSeq = 0;
uint8_t s_dischargeOutcome = 0;

uint32_t s_chartSeq = 0;
char s_chartName[24] = "";

// Registro en curso. Ningun valor clinico se inventa por defecto: 0 = no
// introducido todavia.
char s_newName[BABY_NAME_LEN_LOCAL] = "";
uint8_t s_newGest = 0;
uint16_t s_newWeight = 0;
uint32_t s_newSeq = 0;
uint32_t s_lastRegisteredSeq = 0;

lv_obj_t *s_overlay = nullptr;
lv_obj_t *s_card = nullptr;
lv_obj_t *s_content = nullptr;
lv_obj_t *s_dlg = nullptr;  // discharge dialog (child of overlay)
lv_obj_t *s_inputTa = nullptr;  // textarea de la pantalla de registro activa

const char *TXT(const char *es, const char *en, const char *fr) {
  return (g_lang == LANG_ES) ? es : (g_lang == LANG_FR) ? fr : en;
}

const char *outcomeText(uint8_t oc) {
  switch (oc) {
    case 1: return TR(STR_OUTCOME_SURVIVED);
    case 2: return TR(STR_OUTCOME_DIED);
    case 3: return TR(STR_OUTCOME_TRANSFER);
    default: return TR(STR_OUTCOME_UNKNOWN);
  }
}

// Only meaningful for outcome==2 (Deceased) — see PROTOCOL.md BabyCause.
const char *causeText(uint8_t cause) {
  switch (cause) {
    case 1: return TR(STR_CAUSE_PREMATURITY);
    case 2:
      return TR(STR_CAUSE_ASPHYXIA);
    case 3:
      return TR(STR_CAUSE_SEPSIS);
    case 4:
      return TR(STR_CAUSE_MALFORMATION);
    case 5: return TR(STR_CAUSE_HYPOTHERMIA);
    case 6: return TR(STR_CAUSE_OTHER);
    default: return TR(STR_CAUSE_UNKNOWN_F);
  }
}

// epoch (UTC) -> "YYYY-MM-DD" en HORA LOCAL, o "--" cuando epoch == 0.
// Mismo criterio que fmtStamp en AlarmCenter: todas las pantallas fechan en la
// misma referencia. Lo almacenado sigue en UTC.
void fmtDate(uint32_t epoch, char *out, size_t len) {
  if (epoch == 0) {
    snprintf(out, len, "--");
    return;
  }
  time_t t = (time_t)(HMI_HasLocalTime() ? HMI_ToLocal(epoch) : epoch);
  struct tm tmv;
  gmtime_r(&t, &tmv);
  snprintf(out, len, "%04d-%02d-%02d", tmv.tm_year + 1900, tmv.tm_mon + 1,
           tmv.tm_mday);
}

void clearContent() {
  if (s_content) lv_obj_clean(s_content);
  s_dlg = nullptr;
  s_inputTa = nullptr;
}

// Mismo patron que BabyWizard::setCardSize: la tarjeta crece para las
// pantallas de teclado y vuelve a su tamano para la lista y la grafica.
void setCardSize(bool big) {
  lv_coord_t w = big ? CARD_W_BIG : CARD_W_SMALL;
  lv_coord_t h = big ? CARD_H_BIG : CARD_H_SMALL;
  if (s_card) {
    lv_obj_set_size(s_card, w, h);
    lv_obj_center(s_card);
  }
  if (s_content) {
    lv_obj_set_size(s_content, w - 20, h - 20);
    lv_obj_center(s_content);
  }
}

lv_obj_t *makeTitle(const char *text) {
  lv_obj_t *lbl = lv_label_create(s_content);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
  lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 6);
  return lbl;
}

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

// Same card look as the wizard's baby list (BabyWizard.cpp): tinted fill with
// a blue border. `accent` marks the tappable-with-actions active rows; the
// archived rows use a flatter tone but keep identical text contrast.
void styleCard(lv_obj_t *card, bool accent) {
  lv_obj_set_style_bg_color(card, lv_color_hex(accent ? 0xE3F0FF : 0xEDF3F9),
                            LV_PART_MAIN);
  lv_obj_set_style_border_color(
      card, lv_color_hex(accent ? 0x0075EE : 0xB8C7D4), LV_PART_MAIN);
  lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
  lv_obj_set_style_radius(card, 8, LV_PART_MAIN);
  lv_obj_set_style_bg_color(card, lv_color_hex(0xBBD9F7),
                            LV_PART_MAIN | LV_STATE_PRESSED);
}

// The explicit text colour is the whole point: lv_btn paints its label white
// by default, so a light card fill without this renders white-on-white.
// Width + LONG_DOT keeps a long baby name from running under the ALTA button.
lv_obj_t *makeCardLabel(lv_obj_t *card, const char *text, lv_coord_t width) {
  lv_obj_t *lbl = lv_label_create(card);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0x0B2E4F), 0);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
  lv_obj_set_width(lbl, width);
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
  lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 6, 0);
  return lbl;
}

void showLoading();
void showList();
void showChart();
void openDischargeDialog(uint32_t seq);
void openCauseDialog();
void requestActive();
void requestArchived(uint32_t page);
void closeScreen();
bool weightHistoryEmpty();
void onNewBabyClicked(lv_event_t *e);

// Las esperas de esta pantalla comparten g_pendingProfileAck / Range con el
// asistente. Al abandonar una espera (timeout, alarma critica, cierre) se
// descarta lo que haya llegado: si quedara puesto, el asistente lo adoptaria
// como el ACK de SU seleccion y trabajaria sobre otro bebe (rango NTE, peso y
// minutos al seq equivocado). Lo que llegue mas tarde todavia lo descarta el
// propio emisor justo antes de enviar (submitNew, afterRegistered,
// BabyWizard::selectExisting).
bool inReplyWait() {
  return s_step == HistStep::WaitingDischargeAck ||
         s_step == HistStep::WaitingNewAck ||
         s_step == HistStep::WaitingNewRange ||
         s_step == HistStep::WaitingReselectAck;
}

void discardPendingReplies() {
  g_pendingProfileAck = false;
  g_pendingProfileRange = false;
}

void closeScreen() {
  if (inReplyWait()) discardPendingReplies();
  if (s_overlay) lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  clearContent();
  setCardSize(false);
  s_step = HistStep::Closed;
}

void onCloseClicked(lv_event_t *) { closeScreen(); }

void showLoading() {
  clearContent();
  setCardSize(false);
  makeTitle(TR(STR_LOADING));
  lv_obj_t *close =
      makeBtn(s_content, "X", onCloseClicked, lv_color_hex(0xAA3333));
  lv_obj_set_size(close, 44, 44);
  lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 0, 0);
}

void requestActive() {
  Communication_SendProfileListReq();
  s_step = HistStep::LoadingActive;
  s_deadlineMs = millis() + RESP_TIMEOUT_MS;
}

void requestArchived(uint32_t page) {
  s_page = page;
  Communication_SendProfileHistoryReq(page);
  s_step = HistStep::LoadingArchived;
  s_deadlineMs = millis() + RESP_TIMEOUT_MS;
}

void requestChart(uint32_t seq, const char *name) {
  s_chartSeq = seq;
  snprintf(s_chartName, sizeof(s_chartName), "%s", name);
  Communication_SendWeightHistoryReq(seq);
  s_step = HistStep::LoadingChart;
  s_deadlineMs = millis() + RESP_TIMEOUT_MS;
  s_retries = 0;
  showLoading();
}

// ---- Card tap payloads (static: LVGL user_data must outlive the screen) ----
struct ActiveRow {
  uint32_t seq;
  char name[24];
};
ActiveRow s_activeRows[3];
BabyHistoryItem s_archRows[10];

void onActiveCardTap(lv_event_t *e) {
  auto *r = (ActiveRow *)lv_event_get_user_data(e);
  requestChart(r->seq, r->name);
}
void onArchCardTap(lv_event_t *e) {
  auto *r = (BabyHistoryItem *)lv_event_get_user_data(e);
  requestChart(r->seq, r->name);
}
void onDischargeTap(lv_event_t *e) {
  // En formacion el historial es el REAL y de solo lectura: el alta de un bebe
  // real no se manda (CommTask la traga) y este boton no debe fingir que si.
  if (Training_IsActive()) {
    UI_ShowToast(TR(STR_NOT_IN_TRAINING), 2500);
    return;
  }
  auto *r = (ActiveRow *)lv_event_get_user_data(e);
  // Stop the tap from also opening the chart (button sits inside the card).
  openDischargeDialog(r->seq);
}
void onPrevPage(lv_event_t *) {
  if (s_page > 0) {
    showLoading();
    requestArchived(s_page - 1);
  }
}
void onNextPage(lv_event_t *) {
  if ((s_page + 1) * PAGE_SIZE < s_archived.totalCount) {
    showLoading();
    requestArchived(s_page + 1);
  }
}

// Minutes -> compact "3h 20m" / "45m". Hours are what a clinician reads
// for therapy exposure; raw minutes get unwieldy after a few days.
static void fmtMinutes(uint32_t minutes, char *out, size_t len) {
  if (minutes < 60u) {
    snprintf(out, len, "%um", (unsigned)minutes);
  } else if (minutes % 60u == 0u) {
    snprintf(out, len, "%uh", (unsigned)(minutes / 60u));
  } else {
    snprintf(out, len, "%uh %um", (unsigned)(minutes / 60u),
             (unsigned)(minutes % 60u));
  }
}

void showList() {
  clearContent();
  setCardSize(false);
  makeTitle(TR(STR_BABIES));

  lv_obj_t *close =
      makeBtn(s_content, "X", onCloseClicked, lv_color_hex(0xAA3333));
  lv_obj_set_size(close, 44, 44);
  lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 0, 0);

  // Scrollable body: active cards first, then archived entries + paging.
  lv_obj_t *body = lv_obj_create(s_content);
  lv_obj_remove_style_all(body);
  lv_obj_set_size(body, 620, 340);
  lv_obj_align(body, LV_ALIGN_TOP_MID, 0, 44);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(body, 8, 0);

  // --- Active section ---
  lv_obj_t *secA = lv_label_create(body);
  lv_label_set_text(secA, TR(STR_ACTIVE_M));
  lv_obj_set_style_text_font(secA, &lv_font_montserrat_16, 0);

  if (s_active.count == 0) {
    lv_obj_t *none = lv_label_create(body);
    lv_label_set_text(none, TR(STR_NO_ACTIVE_BABIES));
  }
  for (int i = 0; i < s_active.count; i++) {
    const BabyProfileListItem &it = s_active.items[i];
    s_activeRows[i].seq = it.seq;
    snprintf(s_activeRows[i].name, sizeof(s_activeRows[i].name), "%s",
             it.name);

    lv_obj_t *card = lv_btn_create(body);
    // Three lines: the therapy counters no longer fit on the kangaroo line
    // once humidity joined them, and the ALTA button caps the label at 420 px.
    lv_obj_set_size(card, 600, 100);
    styleCard(card, true);
    lv_obj_add_event_cb(card, onActiveCardTap, LV_EVENT_CLICKED,
                        &s_activeRows[i]);

    char buf[192];
    char wtxt[16];
    if (it.weightGrams > 0) {
      snprintf(wtxt, sizeof(wtxt), "%u g", (unsigned)it.weightGrams);
    } else {
      snprintf(wtxt, sizeof(wtxt), "--");
    }
    char photoTxt[16], thermoTxt[16], humTxt[16];
    fmtMinutes(it.phototherapyMinutes, photoTxt, sizeof(photoTxt));
    fmtMinutes(it.thermoMinutes, thermoTxt, sizeof(thermoTxt));
    fmtMinutes(it.humidityMinutes, humTxt, sizeof(humTxt));
    snprintf(buf, sizeof(buf),
             TR(STR_BABY_ROW_ACTIVE_FMT), it.name, (unsigned)it.gestWeeks, wtxt,
             (unsigned)it.kangarooCount, photoTxt, thermoTxt, humTxt);
    // 600 card - 150 button - margins: leave the ALTA button clear.
    makeCardLabel(card, buf, 420);

    lv_obj_t *dis = makeBtn(card, TR(STR_DISCHARGE_UC),
                            onDischargeTap, lv_color_hex(0xE08800),
                            &s_activeRows[i]);
    lv_obj_set_size(dis, 150, 44);
    lv_obj_align(dis, LV_ALIGN_RIGHT_MID, -4, 0);
  }

  // Registro de un ingreso sin encender ninguna terapia. Mismo azul que el
  // BEBE NUEVO del asistente: es la misma accion vista desde la otra puerta.
  lv_obj_t *newBtn = makeBtn(body, TR(STR_NEW_BABY_UC), onNewBabyClicked,
                             lv_color_hex(0x0075EE));
  lv_obj_set_size(newBtn, 300, 50);

  // --- Archived section ---
  lv_obj_t *secH = lv_label_create(body);
  char hdr[64];
  snprintf(hdr, sizeof(hdr), "%s (%u)",
           TR(STR_HISTORY), (unsigned)s_archived.totalCount);
  lv_label_set_text(secH, hdr);
  lv_obj_set_style_text_font(secH, &lv_font_montserrat_16, 0);

  if (s_archived.count == 0) {
    // An empty archive is the normal state until someone is discharged —
    // say so, instead of a bare "no records" that reads like a failure.
    lv_obj_t *none = lv_label_create(body);
    lv_label_set_text(none, TR(STR_NO_BABIES_DISCHARGED));
  }
  for (int i = 0; i < s_archived.count; i++) {
    s_archRows[i] = s_archived.items[i];
    const BabyHistoryItem &it = s_archRows[i];

    lv_obj_t *card = lv_btn_create(body);
    lv_obj_set_size(card, 600, 100);
    styleCard(card, false);
    lv_obj_add_event_cb(card, onArchCardTap, LV_EVENT_CLICKED, &s_archRows[i]);

    char date[16];
    fmtDate(it.dischargeEpoch, date, sizeof(date));
    char outcomeBuf[48];
    if (it.outcome == 2) {
      // Cause only adds information for the Deceased outcome; every other
      // outcome shows outcomeText() alone, unchanged from before this field.
      snprintf(outcomeBuf, sizeof(outcomeBuf), "%s (%s)",
               outcomeText(it.outcome), causeText(it.cause));
    } else {
      snprintf(outcomeBuf, sizeof(outcomeBuf), "%s", outcomeText(it.outcome));
    }
    char buf[224];
    char photoTxt[16], thermoTxt[16], humTxt[16];
    fmtMinutes(it.phototherapyMinutes, photoTxt, sizeof(photoTxt));
    fmtMinutes(it.thermoMinutes, thermoTxt, sizeof(thermoTxt));
    fmtMinutes(it.humidityMinutes, humTxt, sizeof(humTxt));
    snprintf(buf, sizeof(buf),
             TR(STR_BABY_ROW_ARCHIVED_FMT),
             it.name, (unsigned)it.gestWeeks, outcomeBuf, date,
             (unsigned)it.kangarooCount, photoTxt, thermoTxt, humTxt);
    makeCardLabel(card, buf, 580);
  }

  // --- Pagination ---
  bool hasPrev = s_page > 0;
  bool hasNext = (s_page + 1) * PAGE_SIZE < s_archived.totalCount;
  if (hasPrev || hasNext) {
    lv_obj_t *nav = lv_obj_create(body);
    lv_obj_remove_style_all(nav);
    lv_obj_set_size(nav, 600, 52);

    lv_obj_t *prev = makeBtn(nav, "<", onPrevPage,
                             hasPrev ? lv_color_hex(0x0075EE)
                                     : lv_color_hex(0xAAAAAA));
    lv_obj_set_size(prev, 90, 44);
    lv_obj_align(prev, LV_ALIGN_LEFT_MID, 40, 0);
    if (!hasPrev) lv_obj_clear_flag(prev, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *pg = lv_label_create(nav);
    char pbuf[24];
    uint32_t totalPages =
        (s_archived.totalCount + PAGE_SIZE - 1) / PAGE_SIZE;
    snprintf(pbuf, sizeof(pbuf), "%u / %u", (unsigned)(s_page + 1),
             (unsigned)(totalPages ? totalPages : 1));
    lv_label_set_text(pg, pbuf);
    lv_obj_align(pg, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *next = makeBtn(nav, ">", onNextPage,
                             hasNext ? lv_color_hex(0x0075EE)
                                     : lv_color_hex(0xAAAAAA));
    lv_obj_set_size(next, 90, 44);
    lv_obj_align(next, LV_ALIGN_RIGHT_MID, -40, 0);
    if (!hasNext) lv_obj_clear_flag(next, LV_OBJ_FLAG_CLICKABLE);
  }

  s_step = HistStep::Showing;
}

// ---------------- New baby (BEBE NUEVO) ----------------
// Tres pantallas: nombre -> semanas -> peso. Solo REGISTRAR (o SIN PESO) en la
// ultima envia algo; la X y ATRAS antes de eso no dejan rastro. El asistente
// crea el perfil a mitad porque necesita el seq para pedir el rango con el
// peso; aqui no hay rango que pedir, asi que el unico punto de envio va al
// final, donde el operador espera que este.

void showNewNameScreen();
void showNewGestScreen();
void showNewWeightScreen();

void onNewCancel(lv_event_t *) {
  // Nada enviado todavia: volver a la lista tal como estaba.
  showList();
}

void showRangeError(uint32_t lo, uint32_t hi) {
  char msg[64];
  snprintf(msg, sizeof(msg), TR(STR_VALUE_BETWEEN_FMT), (unsigned)lo,
           (unsigned)hi);
  UI_ShowToast(msg, 2500);
}

// Una pantalla del registro: titulo, X, un textarea, pista, hasta tres
// botones (izquierda / centro / derecha; `midTxt` nulo = sin boton central) y
// el teclado compartido con el asistente debajo. Sin SALTAR: aqui no hay
// terapia que arrancar en manual.
lv_obj_t *buildNewStep(const char *title, const char *hint, bool digits,
                       const char *leftTxt, lv_event_cb_t onLeft,
                       const char *midTxt, lv_event_cb_t onMid,
                       const char *rightTxt, lv_event_cb_t onRight) {
  clearContent();
  setCardSize(true);
  makeTitle(title);

  lv_obj_t *close =
      makeBtn(s_content, "X", onNewCancel, lv_color_hex(0xAA3333));
  lv_obj_set_size(close, 44, 44);
  lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 0, 0);

  lv_obj_t *ta = lv_textarea_create(s_content);
  lv_obj_set_size(ta, digits ? 220 : 520, 52);
  lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 40);
  lv_textarea_set_one_line(ta, true);
  lv_obj_set_style_text_font(ta, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_align(ta, LV_TEXT_ALIGN_CENTER, 0);
  if (digits) {
    lv_textarea_set_accepted_chars(ta, "0123456789");
    lv_textarea_set_max_length(ta, 5);
  } else {
    lv_textarea_set_max_length(ta, BABY_NAME_LEN_LOCAL - 1);
  }
  lv_textarea_set_placeholder_text(ta, "...");

  lv_obj_t *hintLbl = lv_label_create(s_content);
  lv_label_set_text(hintLbl, hint);
  lv_obj_set_style_text_color(hintLbl, lv_color_hex(0x666666), 0);
  lv_obj_align(hintLbl, LV_ALIGN_TOP_MID, 0, 98);

  lv_obj_t *left = makeBtn(s_content, leftTxt, onLeft, lv_color_hex(0x888888));
  lv_obj_set_size(left, 150, 46);
  lv_obj_align(left, LV_ALIGN_TOP_LEFT, 6, 126);

  if (midTxt) {
    // Ambar, como el SIN PESO del asistente: es la salida "sin dato", no el
    // camino verde.
    lv_obj_t *mid = makeBtn(s_content, midTxt, onMid, lv_color_hex(0xE08800));
    lv_obj_set_size(mid, 160, 46);
    lv_obj_align(mid, LV_ALIGN_TOP_MID, 0, 126);
  }

  lv_obj_t *right =
      makeBtn(s_content, rightTxt, onRight, lv_color_hex(0x00AA00));
  lv_obj_set_size(right, 190, 46);
  lv_obj_align(right, LV_ALIGN_TOP_RIGHT, -6, 126);

  lv_obj_t *kb = InputKeypad_Create(s_content, ta, digits);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, -4);
  s_inputTa = ta;
  return ta;
}

// Paso 1: nombre.
void onNewNameContinue(lv_event_t *) {
  // Sin espacios sobrantes y nunca vacio (tampoco solo espacios): el nombre
  // queda en NVS y en ThingsBoard sin forma de editarlo despues.
  if (!InputKeypad_ReadName(s_inputTa, s_newName, sizeof(s_newName))) {
    UI_ShowToast(TR(STR_ENTER_A_NAME), 2500);
    return;
  }
  showNewGestScreen();
  s_step = HistStep::NewGest;
}

void showNewNameScreen() {
  lv_obj_t *ta = buildNewStep(TR(STR_BABY_NAME), TR(STR_LETTERS_ONLY), false,
                              TR(STR_BACK_UC), onNewCancel, nullptr, nullptr,
                              TR(STR_CONTINUE_UC), onNewNameContinue);
  lv_obj_add_event_cb(ta, InputKeypad_StripCommasCb, LV_EVENT_VALUE_CHANGED,
                      nullptr);
  // Lo ya tecleado en este registro vuelve al volver atras; nunca un nombre
  // de otro registro (s_newName se vacia al pulsar BEBE NUEVO).
  if (s_newName[0] != '\0') lv_textarea_set_text(ta, s_newName);
}

// Paso 2: semanas de gestacion.
void onNewGestBack(lv_event_t *) {
  showNewNameScreen();
  s_step = HistStep::NewName;
}

void onNewGestContinue(lv_event_t *) {
  uint32_t v = 0;
  if (!InputKeypad_ReadNumber(s_inputTa, 20, 40, &v)) {
    showRangeError(20, 40);
    return;
  }
  s_newGest = (uint8_t)v;
  showNewWeightScreen();
  s_step = HistStep::NewWeight;
}

void showNewGestScreen() {
  lv_obj_t *ta = buildNewStep(TR(STR_GEST_AGE_WEEKS), "20 - 40", true,
                              TR(STR_BACK_UC), onNewGestBack, nullptr, nullptr,
                              TR(STR_CONTINUE_UC), onNewGestContinue);
  // Solo lo tecleado en este mismo registro (al volver desde el peso). Nunca
  // un valor de otro bebe: el asistente deja el campo vacio por eso.
  if (s_newGest != 0) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", (unsigned)s_newGest);
    lv_textarea_set_text(ta, buf);
  }
}

// Paso 3: peso al ingreso (opcional) y REGISTRAR.
void submitNew() {
  if (Display_IsBoardLinkLost()) {
    // Nada sale al vacio. El operador se queda en la pantalla de peso con lo
    // tecleado y puede volver a pulsar cuando el enlace regrese.
    UI_ShowToast(TR(STR_NO_BOARD_RESPONSE), 3000);
    return;
  }
  // Un ACK huerfano (un alta que vencio y contesto tarde) no debe leerse como
  // el seq del bebe nuevo: se descarta lo pendiente justo antes de pedir.
  g_pendingProfileAck = false;
  Communication_SendProfileNew(s_newName, s_newGest);
  s_step = HistStep::WaitingNewAck;
  s_deadlineMs = millis() + NEW_TIMEOUT_MS;
  showLoading();
}

void onNewWeightBack(lv_event_t *) {
  showNewGestScreen();
  s_step = HistStep::NewGest;
}

void onNewNoWeight(lv_event_t *) {
  // El perfil queda persistido en PROFILE_NEW; SKIP no cambia nada en la
  // placa, asi que no se envia PROFILE_WEIGHT.
  s_newWeight = 0;
  submitNew();
}

void onNewRegister(lv_event_t *) {
  uint32_t v = 0;
  if (!InputKeypad_ReadNumber(s_inputTa, 400, 5000, &v)) {
    showRangeError(400, 5000);
    return;
  }
  s_newWeight = (uint16_t)v;
  submitNew();
}

void showNewWeightScreen() {
  buildNewStep(TR(STR_ADMISSION_WEIGHT_G), "400 - 5000 g", true,
               TR(STR_BACK_UC), onNewWeightBack, TR(STR_NO_WEIGHT_UC),
               onNewNoWeight, TR(STR_REGISTER_UC), onNewRegister);
  // Vacio a proposito, tambien al volver desde REGISTRAR con error de rango:
  // el peso se teclea, no se confirma por reflejo.
}

void onNewBabyClicked(lv_event_t *) {
  if (s_step != HistStep::Showing) return;
  // La guarda de tres activos solo vale con una lista venida de la placa: si
  // la carga vencio, s_active esta vacia por defecto y un PROFILE_NEW haria
  // que la placa desalojase por FIFO a un paciente activo sin que nadie lo
  // viera. Sin enlace tampoco se abre: REGISTRAR mandaria al vacio.
  if (!s_activeFromBoard || Display_IsBoardLinkLost()) {
    UI_ShowToast(TR(STR_NO_BOARD_RESPONSE), 3000);
    return;
  }
  if (s_active.count >= 3) {
    // La placa desalojaria por FIFO a un activo (y lo archivaria con
    // resultado Desconocido). En la pantalla cuyo objeto es el registro eso
    // seria un fallo de registro clinico: el alta con resultado esta a un
    // toque, que la haga el operador.
    UI_ShowToast(TR(STR_THREE_ACTIVE_DISCHARGE_FIRST), 3000);
    return;
  }
  s_newName[0] = '\0';
  s_newGest = 0;
  s_newWeight = 0;
  s_newSeq = 0;
  showNewNameScreen();
  s_step = HistStep::NewName;
}

// Tras el registro (bien, rechazado o sin respuesta) la lista se recarga: lo
// que muestre la placa es la verdad, no lo que la HMI cree haber mandado.
void finishNewAndReload() {
  s_retries = 0;
  showLoading();
  requestActive();
}

// Cierre de un registro que la placa acepto. Si ahora mismo hay un bebe bajo
// terapia, se reenvia su PROFILE_SELECT antes de dar el registro por hecho:
// la motherBoard guarda como "bebe del asistente" el ultimo seq creado o
// seleccionado (s_wizardSeq en su CommTask.cpp) y lo sella como activo en el
// siguiente arranque de terapia. Sin esto, el registro dejaria ahi al recien
// llegado, y un SALTAR en el asistente tras un canguro o una limpieza
// acreditaria la terapia al bebe equivocado y quitaria al de la incubadora
// su proteccion frente al desalojo FIFO.
void afterRegistered() {
  if (BabyWizard_HasLiveSession()) {
    g_pendingProfileAck = false;
    Communication_SendProfileSelect(BabyWizard_GetActiveSeq());
    s_step = HistStep::WaitingReselectAck;
    s_deadlineMs = millis() + NEW_TIMEOUT_MS;
    return;
  }
  UI_ShowToast(TR(STR_BABY_REGISTERED), 2500);
  finishNewAndReload();
}

// ---------------- Discharge dialog ----------------

// Deceased (2) needs a cause before anything is sent; every other outcome
// goes straight to the ACK wait like before this field existed.
void onOutcomePick(lv_event_t *e) {
  s_dischargeOutcome = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  if (s_dischargeOutcome == 2) {
    openCauseDialog();
    return;
  }
  g_pendingProfileAck = false;  // ver discardPendingReplies()
  Communication_SendProfileDischarge(s_dischargeSeq, s_dischargeOutcome, 0);
  if (s_dlg) {
    lv_obj_del(s_dlg);
    s_dlg = nullptr;
  }
  s_step = HistStep::WaitingDischargeAck;
  s_deadlineMs = millis() + RESP_TIMEOUT_MS;
  showLoading();
}

void onCausePick(lv_event_t *e) {
  uint8_t cause = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  g_pendingProfileAck = false;  // ver discardPendingReplies()
  Communication_SendProfileDischarge(s_dischargeSeq, s_dischargeOutcome, cause);
  if (s_dlg) {
    lv_obj_del(s_dlg);
    s_dlg = nullptr;
  }
  s_step = HistStep::WaitingDischargeAck;
  s_deadlineMs = millis() + RESP_TIMEOUT_MS;
  showLoading();
}

void onDialogCancel(lv_event_t *) {
  // Dismiss without confirming: nothing is sent, profile stays active.
  if (s_dlg) {
    lv_obj_del(s_dlg);
    s_dlg = nullptr;
  }
  s_step = HistStep::Showing;
}

void openDischargeDialog(uint32_t seq) {
  if (s_step != HistStep::Showing) return;
  s_dischargeSeq = seq;
  s_step = HistStep::DischargeDialog;

  s_dlg = lv_obj_create(s_overlay);
  lv_obj_set_size(s_dlg, 480, 360);
  lv_obj_center(s_dlg);
  lv_obj_set_style_radius(s_dlg, 12, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_dlg, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_clear_flag(s_dlg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_move_foreground(s_dlg);
  // Flex column instead of hand-computed offsets. The previous layout mixed
  // TOP_MID offsets with a BOTTOM_MID cancel button, and those are measured
  // against the *content* box (inside the theme's padding), not the 480x360
  // outer box — which is how the last outcome button and CANCEL ended up
  // overlapping. Flex makes the overlap structurally impossible.
  lv_obj_set_style_pad_all(s_dlg, 12, LV_PART_MAIN);
  lv_obj_set_flex_flow(s_dlg, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_dlg, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(s_dlg, 8, LV_PART_MAIN);

  lv_obj_t *title = lv_label_create(s_dlg);
  lv_label_set_text(title, TR(STR_DISCHARGE_OUTCOME));
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

  static const uint8_t OUTCOMES[4] = {1, 2, 3, 0};
  for (int i = 0; i < 4; i++) {
    lv_obj_t *btn = makeBtn(s_dlg, outcomeText(OUTCOMES[i]), onOutcomePick,
                            lv_color_hex(0x0075EE),
                            (void *)(uintptr_t)OUTCOMES[i]);
    lv_obj_set_size(btn, 300, 46);
  }

  lv_obj_t *cancel = makeBtn(s_dlg, TR(STR_CANCEL_UC),
                             onDialogCancel, lv_color_hex(0x888888));
  lv_obj_set_size(cancel, 160, 44);
}

// Replaces the outcome dialog with the cause picker, still inside s_overlay.
// CANCELAR here (via onDialogCancel) aborts the whole discharge, same as
// from the outcome dialog — there is no way back to re-pick the outcome,
// matching BabyExitDialog's "X" semantics: nothing is recorded.
void openCauseDialog() {
  if (s_dlg) {
    lv_obj_del(s_dlg);
    s_dlg = nullptr;
  }
  s_dlg = lv_obj_create(s_overlay);
  lv_obj_set_size(s_dlg, 480, 400);
  lv_obj_center(s_dlg);
  lv_obj_set_style_radius(s_dlg, 12, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_dlg, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_move_foreground(s_dlg);
  lv_obj_set_style_pad_all(s_dlg, 12, LV_PART_MAIN);
  lv_obj_set_flex_flow(s_dlg, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_dlg, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(s_dlg, 6, LV_PART_MAIN);
  // 6 causes + title + cancel don't fit at the outcome dialog's button
  // height without overflowing 400px, so this one scrolls instead.
  lv_obj_add_flag(s_dlg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(s_dlg, LV_DIR_VER);

  lv_obj_t *title = lv_label_create(s_dlg);
  lv_label_set_text(title, TR(STR_CAUSE_OF_DEATH));
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

  // Same 1-6 mapping as PROTOCOL.md / BabyCause on the motherBoard, and as
  // BabyExitDialog's cause screen (the auto-triggered exit flow). Los textos
  // ya no se duplican: ambas pantallas apuntan a los mismos ids del catalogo,
  // asi que solo hay que mantener en sincronia el orden y los codigos.
  struct CauseOption { ui_str_id_t text; uint8_t code; };
  static const CauseOption CAUSES[] = {
      {STR_CAUSE_PREMATURITY_UC, 1},
      {STR_CAUSE_ASPHYXIA_UC, 2},
      {STR_CAUSE_SEPSIS_UC, 3},
      {STR_CAUSE_MALFORMATION_UC, 4},
      {STR_CAUSE_HYPOTHERMIA_UC, 5},
      {STR_CAUSE_OTHER_UC, 6},
  };
  for (const CauseOption &c : CAUSES) {
    lv_obj_t *btn = makeBtn(s_dlg, TR(c.text), onCausePick,
                            lv_color_hex(0x0075EE), (void *)(uintptr_t)c.code);
    lv_obj_set_size(btn, 300, 40);
  }

  lv_obj_t *cancel = makeBtn(s_dlg, TR(STR_CANCEL_UC),
                             onDialogCancel, lv_color_hex(0x888888));
  lv_obj_set_size(cancel, 160, 44);
}

// ---------------- Chart ----------------

void onChartBack(lv_event_t *) {
  showLoading();
  s_activeLoaded = false;
  requestActive();
}

void showChart() {
  clearContent();
  setCardSize(false);

  char title[64];
  snprintf(title, sizeof(title), "%s - %s", s_chartName,
           TR(STR_WEIGHT_EVOLUTION));
  makeTitle(title);

  lv_obj_t *back = makeBtn(s_content, TR(STR_BACK_UC),
                           onChartBack, lv_color_hex(0x888888));
  lv_obj_set_size(back, 120, 44);
  lv_obj_align(back, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t *close =
      makeBtn(s_content, "X", onCloseClicked, lv_color_hex(0xAA3333));
  lv_obj_set_size(close, 44, 44);
  lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 0, 0);

  if (weightHistoryEmpty()) {
    lv_obj_t *none = lv_label_create(s_content);
    lv_label_set_text(none, TR(STR_NO_WEIGHT_DATA));
    lv_obj_align(none, LV_ALIGN_CENTER, 0, 0);
    s_step = HistStep::ShowingChart;
    return;
  }

  // Y range: min/max with a 100 g margin, rounded to 100 g.
  uint16_t wMin = 65535, wMax = 0;
  uint16_t dMax = 0;
  for (int i = 0; i < g_weightHistory.count; i++) {
    if (g_weightHistory.weightGrams[i] < wMin)
      wMin = g_weightHistory.weightGrams[i];
    if (g_weightHistory.weightGrams[i] > wMax)
      wMax = g_weightHistory.weightGrams[i];
    if (g_weightHistory.dayOffset[i] > dMax)
      dMax = g_weightHistory.dayOffset[i];
  }
  int yLo = ((wMin > 100 ? wMin - 100 : 0) / 100) * 100;
  int yHi = ((wMax + 199) / 100) * 100;

  // Eje X: dia de vida cuando abarca mas de un dia; si no, numero de muestra.
  //
  // dayOffset tiene resolucion de DIAS, asi que todos los pesos tomados en la
  // misma jornada llegan con el mismo 0 — y tambien llegan todos a 0 cuando la
  // placa no tenia hora al registrarlos. En ambos casos un eje de "dia de vida"
  // no dice nada: lo unico cierto es el orden en que se tomaron.
  const bool byDay = (dMax > 0);
  int xMax = byDay ? (int)dMax : (g_weightHistory.count - 1);
  // Un solo punto: un rango degenerado 0..0 no es dibujable.
  if (xMax < 1) xMax = 1;

  // Las marcas mayores se reparten como min + (max-min)*i/(n-1) en ENTEROS, asi
  // que pedir mas marcas que valores distintos hay en el rango las hace repetir
  // el mismo numero. Ese era el "eje lleno de ceros": 5 marcas sobre un rango
  // 0..1 dan 0,0,0,0,1. Nunca mas marcas que enteros caben.
  int xTicks = xMax + 1;
  if (xTicks > 5) xTicks = 5;
  if (xTicks < 2) xTicks = 2;

  lv_obj_t *chart = lv_chart_create(s_content);
  lv_obj_set_size(chart, 560, 300);
  // Anchored to the top so the axis labels can't run off the card bottom.
  lv_obj_align(chart, LV_ALIGN_TOP_MID, 10, 56);
  // lv_chart draws its tick labels inside its own padding box, and the
  // default theme reserves nowhere near the draw_size set below — without
  // this the day-of-life numbers were clipped off the bottom edge.
  lv_obj_set_style_pad_bottom(chart, 34, LV_PART_MAIN);
  lv_obj_set_style_pad_left(chart, 55, LV_PART_MAIN);
  lv_obj_set_style_pad_top(chart, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_right(chart, 10, LV_PART_MAIN);
  // SCATTER: real day offsets on X (points are NOT evenly spaced in time).
  lv_chart_set_type(chart, LV_CHART_TYPE_SCATTER);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_X, 0, xMax);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, yLo, yHi);
  lv_chart_set_point_count(chart, g_weightHistory.count);
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_X, 6, 3, xTicks, 2, true,
                         30);
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 6, 3, 5, 2, true,
                         50);

  lv_chart_series_t *ser =
      lv_chart_add_series(chart, lv_color_hex(0x0075EE),
                          LV_CHART_AXIS_PRIMARY_Y);
  for (int i = 0; i < g_weightHistory.count; i++) {
    lv_chart_set_next_value2(chart, ser,
                             byDay ? g_weightHistory.dayOffset[i] : i,
                             g_weightHistory.weightGrams[i]);
  }

  // Below the chart's own bottom edge (56 + 300 = 356), clear of the tick
  // labels now living in the chart's bottom padding. Centrada bajo el eje X,
  // no pegada a la derecha: es el titulo del eje entero, no una nota al margen.
  lv_obj_t *xlbl = lv_label_create(s_content);
  // La etiqueta sigue a lo que se esta pintando de verdad: llamar "dia de vida"
  // a un eje que cuenta muestras seria mentir sobre un dato clinico.
  lv_label_set_text(xlbl,
                    byDay ? TR(STR_DAY_OF_LIFE)
                          : TR(STR_MEASUREMENT));
  lv_obj_set_style_text_font(xlbl, &lv_font_montserrat_14, 0);
  lv_obj_align(xlbl, LV_ALIGN_BOTTOM_MID, 0, -44);

  s_step = HistStep::ShowingChart;
}

bool weightHistoryEmpty() {
  return g_weightHistory.count == 0 || g_weightHistory.seq != s_chartSeq;
}

}  // namespace

void BabyHistory_Init(lv_obj_t *parent) {
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
  lv_obj_set_size(s_card, 660, 430);
  lv_obj_center(s_card);
  lv_obj_set_style_radius(s_card, 12, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_card, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_clear_flag(s_card, LV_OBJ_FLAG_SCROLLABLE);

  s_content = lv_obj_create(s_card);
  lv_obj_remove_style_all(s_content);
  lv_obj_set_size(s_content, 640, 410);
  lv_obj_center(s_content);
  lv_obj_clear_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);
}

bool BabyHistory_IsOpen(void) { return s_step != HistStep::Closed; }

void BabyHistory_Close(void) {
  if (s_step != HistStep::Closed) closeScreen();
}

BabyHistoryStep BabyHistory_GetStep(void) {
  switch (s_step) {
    case HistStep::Closed: return BH_CLOSED;
    case HistStep::LoadingActive:
    case HistStep::LoadingArchived:
    case HistStep::Showing:
    case HistStep::DischargeDialog:
    case HistStep::WaitingDischargeAck: return BH_LIST;
    case HistStep::NewName: return BH_NEW_NAME;
    case HistStep::NewGest: return BH_NEW_GEST;
    case HistStep::NewWeight: return BH_NEW_WEIGHT;
    case HistStep::WaitingNewAck:
    case HistStep::WaitingNewRange:
    case HistStep::WaitingReselectAck: return BH_NEW_WAITING;
    case HistStep::LoadingChart:
    case HistStep::ShowingChart: return BH_CHART;
  }
  return BH_CLOSED;
}

uint32_t BabyHistory_LastRegisteredSeq(void) { return s_lastRegisteredSeq; }

void BabyHistory_Open(void) {
  if (s_step != HistStep::Closed) return;
  s_activeLoaded = false;
  s_active.count = 0;
  s_archived = {0, 0, 0, {}};
  s_retries = 0;
  s_lastRegisteredSeq = 0;
  s_activeFromBoard = false;

  showLoading();
  requestActive();

  if (s_overlay) {
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_overlay);
  }
}

void BabyHistory_Poll(void) {
  if (s_step == HistStep::Closed) return;

  // A critical alarm takes the screen (same rule as the wizard).
  if (UI_IsCriticalAlarmActive()) {
    closeScreen();
    return;
  }

  switch (s_step) {
    case HistStep::LoadingActive:
      if (g_pendingProfileList) {
        g_pendingProfileList = false;
        s_active = g_profileList;
        s_activeLoaded = true;
        s_activeFromBoard = true;
        s_retries = 0;
        requestArchived(0);
      } else if (millis() > s_deadlineMs) {
        if (s_retries == 0) {
          s_retries++;
          requestActive();
        } else {
          UI_ShowToast(TR(STR_NO_BOARD_RESPONSE), 3000);
          s_active.count = 0;
          s_activeLoaded = true;
          s_activeFromBoard = false;  // lista vacia por defecto, no real
          s_retries = 0;
          requestArchived(0);
        }
      }
      break;

    case HistStep::LoadingArchived:
      if (g_pendingBabyHistory) {
        g_pendingBabyHistory = false;
        s_archived = g_babyHistory;
        showList();
      } else if (millis() > s_deadlineMs) {
        if (s_retries == 0) {
          s_retries++;
          requestArchived(s_page);
        } else {
          UI_ShowToast(TR(STR_COULD_NOT_FETCH_HISTORY), 3000);
          s_archived = {s_page, 0, 0, {}};
          s_retries = 0;
          showList();
        }
      }
      break;

    case HistStep::WaitingDischargeAck:
      if (g_pendingProfileAck) {
        g_pendingProfileAck = false;
        if (g_profileAck == 0) {
          UI_ShowToast(TR(STR_DISCHARGE_REFUSED), 3000);
        } else {
          UI_ShowToast(TR(STR_BABY_DISCHARGED), 2500);
        }
        // Refresh both sections (discharged baby moved active -> archived).
        s_retries = 0;
        showLoading();
        requestActive();
      } else if (millis() > s_deadlineMs) {
        discardPendingReplies();
        UI_ShowToast(TR(STR_NO_BOARD_RESPONSE), 3000);
        s_retries = 0;
        showLoading();
        requestActive();
      }
      break;

    case HistStep::WaitingNewAck:
      if (g_pendingProfileAck) {
        g_pendingProfileAck = false;
        if (g_profileAck == 0) {
          // Sin slot elegible (los tres ocupados y el candidato a desalojo
          // esta controlando ahora). La guarda de la HMI lo evita casi
          // siempre; si aun asi pasa, se dice y se vuelve a la lista.
          UI_ShowToast(TR(STR_REGISTER_REFUSED), 3000);
          finishNewAndReload();
        } else {
          s_newSeq = g_profileAck;
          s_lastRegisteredSeq = s_newSeq;
          if (s_newWeight > 0) {
            // El rango que contesta la placa no se usa aqui (no hay terapia
            // que proponer), pero se espera para consumirlo: una respuesta
            // huerfana la descartaria el asistente como obsoleta en su
            // siguiente uso, y mejor no dejarla ahi.
            g_pendingProfileRange = false;
            Communication_SendProfileWeight(s_newSeq, s_newWeight);
            s_step = HistStep::WaitingNewRange;
            s_deadlineMs = millis() + NEW_TIMEOUT_MS;
          } else {
            afterRegistered();
          }
        }
      } else if (millis() > s_deadlineMs) {
        // Sin reintento: el PROFILE_NEW pudo llegar. La lista dira si existe.
        discardPendingReplies();
        UI_ShowToast(TR(STR_NO_BOARD_RESPONSE), 3000);
        finishNewAndReload();
      }
      break;

    case HistStep::WaitingNewRange:
      if (g_pendingProfileRange) {
        g_pendingProfileRange = false;
        afterRegistered();
      } else if (millis() > s_deadlineMs) {
        // El perfil ya existe; el peso puede o no haberse anotado. La lista
        // recargada lo muestra.
        discardPendingReplies();
        UI_ShowToast(TR(STR_NO_BOARD_RESPONSE), 3000);
        finishNewAndReload();
      }
      break;

    case HistStep::WaitingReselectAck:
      // El registro ya esta hecho; esto solo devuelve a la placa el bebe en
      // terapia como "bebe del asistente". Con o sin respuesta, se termina.
      if (g_pendingProfileAck) {
        g_pendingProfileAck = false;
        UI_ShowToast(TR(STR_BABY_REGISTERED), 2500);
        finishNewAndReload();
      } else if (millis() > s_deadlineMs) {
        discardPendingReplies();
        UI_ShowToast(TR(STR_BABY_REGISTERED), 2500);
        finishNewAndReload();
      }
      break;

    case HistStep::LoadingChart:
      if (g_pendingWeightHistory) {
        g_pendingWeightHistory = false;
        showChart();
      } else if (millis() > s_deadlineMs) {
        if (s_retries == 0) {
          s_retries++;
          Communication_SendWeightHistoryReq(s_chartSeq);
          s_deadlineMs = millis() + RESP_TIMEOUT_MS;
        } else {
          // No data / no response: render the empty chart state, not a crash.
          g_weightHistory.count = 0;
          g_weightHistory.seq = s_chartSeq;
          showChart();
        }
      }
      break;

    default:
      break;  // Showing/DischargeDialog/ShowingChart/New* are event-driven
  }
}
