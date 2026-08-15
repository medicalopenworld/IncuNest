#include "ui/AlarmCenter.h"

#include <cstdio>
#include <cstring>
#include <ctime>

#include "CommTask.h"
#include "UITask.h"
#include "alarm_ids.h"
#include "main.h"
#include "ui.h"

extern ui_lang_t g_lang;
extern Alarm     alarmList[MAX_ALARMS];
extern bool      alarmActive;
extern bool      alarmsMuted;
// Definido en tasks/UITask.cpp. Es el unico camino de silencio del equipo.
extern void MuteAlarm_cb(lv_event_t *e);

namespace {

enum class Step {
  Closed,
  Loading,   // esperando CTRL,ALM_HISTORY
  Showing,
  Detail,        // pop-up abierto con la descripcion ya puesta
  LoadingDetail, // pop-up abierto esperando CTRL,ALM_DESC
};

constexpr uint32_t RESP_TIMEOUT_MS = 2000;

Step     s_step = Step::Closed;
uint32_t s_deadlineMs = 0;
int      s_retries = 0;
uint8_t  s_detailId = 0;
uint32_t s_activeSig = 0;  // conjunto de alarmas activas
uint32_t s_viewSig = 0;    // todo lo que se pinta
// Instante hasta el que se espera la confirmacion de un SILENCIAR/REANUDAR. Si
// vence sin que cambie nada, se repinta igualmente para que el boton no se
// quede en "..." de por vida cuando la orden se pierde.
uint32_t s_silenceWaitUntilMs = 0;

AlarmHistoryMsg s_hist = {0, {}};

lv_obj_t *s_overlay = nullptr;
lv_obj_t *s_card = nullptr;
lv_obj_t *s_content = nullptr;
lv_obj_t *s_dlg = nullptr;      // pop-up de descripcion (hijo del overlay)
lv_obj_t *s_dlgBody = nullptr;  // etiqueta de la descripcion dentro del pop-up

const char *TXT(const char *es, const char *en, const char *fr) {
  return (g_lang == LANG_ES) ? es : (g_lang == LANG_FR) ? fr : en;
}

// Marca de prioridad de 6.3.2.2.2. No es politica: es formatear el numero que
// manda la motherBoard en CTRL,ALM. La placa sigue siendo la dueña de decidir
// que prioridad tiene cada alarma; aqui solo se pinta.
const char *prioMark(uint8_t prio) {
  switch (prio) {
    case ALARM_PRIORITY_HIGH:   return "!!!";
    case ALARM_PRIORITY_MEDIUM: return "!!";
    default:                    return "!";
  }
}

const char *prioName(uint8_t prio) {
  switch (prio) {
    case ALARM_PRIORITY_HIGH:   return TXT("ALTA", "HIGH", "HAUTE");
    case ALARM_PRIORITY_MEDIUM: return TXT("MEDIA", "MEDIUM", "MOYENNE");
    default:                    return TXT("BAJA", "LOW", "BASSE");
  }
}

// Colores de Tabla 2 (rojo/amarillo/cian), atenuados al tono pastel que usan
// las tarjetas del historial de bebes para que el texto oscuro siga legible.
// Aqui no parpadean: el parpadeo de la senal de 1 m lo hace el banner, y una
// lista entera parpadeando es justo lo que la norma llama fatiga de alarma.
lv_color_t cardFill(uint8_t prio) {
  switch (prio) {
    case ALARM_PRIORITY_HIGH:   return lv_color_hex(0xFFE0E4);
    case ALARM_PRIORITY_MEDIUM: return lv_color_hex(0xFFF0D6);
    default:                    return lv_color_hex(0xDFF3FF);
  }
}

lv_color_t cardBorder(uint8_t prio) {
  switch (prio) {
    case ALARM_PRIORITY_HIGH:   return lv_color_hex(0xD5283C);
    case ALARM_PRIORITY_MEDIUM: return lv_color_hex(0xC98A00);
    default:                    return lv_color_hex(0x2196C4);
  }
}

// epoch (UTC) -> "YYYY-MM-DD HH:MM", o "--" si la placa no tenia hora.
void fmtStamp(uint32_t epoch, char *out, size_t len) {
  if (epoch == 0) {
    snprintf(out, len, "--");
    return;
  }
  time_t t = (time_t)epoch;
  struct tm tmv;
  gmtime_r(&t, &tmv);
  snprintf(out, len, "%04d-%02d-%02d %02d:%02d", tmv.tm_year + 1900,
           tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour, tmv.tm_min);
}

void clearContent() {
  if (s_content) lv_obj_clean(s_content);
  // El pop-up cuelga de s_overlay, NO de s_content, asi que lv_obj_clean() no
  // lo toca: hay que borrarlo a mano. Limitarse a poner el puntero a nullptr
  // lo dejaria huerfano y vivo, y reaparecia flotando la siguiente vez que se
  // abriera el centro de alarmas.
  if (s_dlg) {
    lv_obj_del(s_dlg);
    s_dlg = nullptr;
  }
  s_dlgBody = nullptr;
}

lv_obj_t *makeTitle(const char *text) {
  lv_obj_t *lbl = lv_label_create(s_content);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0x0B2E4F), 0);
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

// Icono de AUDIO PAUSED: campana con X discontinua.
//
// Es el simbolo IEC 60417-5576 al que remite la Tabla 5 de 60601-1-8, con la
// variante de trazo discontinuo que el propio texto describe para los estados
// TEMPORIZADOS ("in the event of ALARM PAUSED or AUDIO PAUSED, the X becomes a
// dashed-X where the dashed-X means limited duration"). La X continua queda
// reservada a AUDIO OFF, que este equipo no ofrece.
//
// Se dibuja en vez de incrustarse como imagen porque la fuente ya trae la
// campana y las dos aspas son dos lv_line con estilo discontinuo. PENDIENTE:
// contrastar el trazado contra la lamina original de la Tabla C.1 antes de la
// evaluacion formal — la forma es la correcta, las proporciones exactas no
// estan verificadas contra el documento.
lv_obj_t *makeAudioPausedIcon(lv_obj_t *parent, lv_color_t color) {
  static lv_point_t kDiag1[] = {{3, 3}, {25, 25}};
  static lv_point_t kDiag2[] = {{25, 3}, {3, 25}};

  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, 28, 28);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *bell = lv_label_create(box);
  lv_label_set_text(bell, LV_SYMBOL_BELL);
  lv_obj_set_style_text_color(bell, color, 0);
  lv_obj_set_style_text_font(bell, &lv_font_montserrat_20, 0);
  lv_obj_center(bell);

  for (int i = 0; i < 2; i++) {
    lv_obj_t *ln = lv_line_create(box);
    lv_line_set_points(ln, i == 0 ? kDiag1 : kDiag2, 2);
    lv_obj_set_style_line_width(ln, 3, 0);
    lv_obj_set_style_line_color(ln, color, 0);
    lv_obj_set_style_line_dash_width(ln, 4, 0);
    lv_obj_set_style_line_dash_gap(ln, 3, 0);
    lv_obj_set_style_line_rounded(ln, true, 0);
  }
  return box;
}

// El color de texto explicito es imprescindible: lv_btn pinta su etiqueta en
// blanco por defecto y sobre el relleno claro de la tarjeta quedaria invisible
// (mismo motivo que en BabyHistory.cpp).
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
void openDetail(uint8_t id, const char *title, const char *desc);

// Huellas para detectar "ha cambiado algo": no se guardan ni se transmiten.
//
// Solo el CONJUNTO de alarmas activas. Cuando esto cambia, el registro que
// guarda la placa ya no es el que tenemos en s_hist y hay que volver a
// pedirlo: una alarma que se resuelve con el centro abierto tiene que bajar
// sola de "Activas" a "Registro".
uint32_t activeIdSignature() {
  uint32_t sig = 1u;
  for (int i = 0; i < MAX_ALARMS; i++) {
    if (alarmList[i].state) {
      sig = sig * 31u + (uint32_t)alarmList[i].id + 1u;
    }
  }
  return sig;
}

// Todo lo que se ve. Ademas del conjunto activo incluye el bitmask de
// silenciadas, que es lo que hace que al pulsar SILENCIAR/REANUDAR la fila se
// repinte en cuanto la placa confirma, y que la caducidad de los 10 min
// devuelva el boton a SILENCIAR sin salir y volver a entrar.
uint32_t viewSignature() {
  return activeIdSignature() ^ ctrl_state_msg.silencedBitmask ^
         (alarmsMuted ? 0x8000u : 0u);
}

void onRowSilenceTap(lv_event_t *e);

void closeScreen() {
  if (s_overlay) lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  clearContent();
  s_step = Step::Closed;
}

void onCloseClicked(lv_event_t *) { closeScreen(); }

void onMuteClicked(lv_event_t *) {
  // Se delega en MuteAlarm_cb() en vez de repetir aqui la secuencia
  // (alarmsMuted + hmi_msg.muteAlarm + shouldSendData + AlarmSound_Update).
  // Duplicarla seria una segunda fuente de verdad del silencio esperando a
  // desincronizarse, y silenciar es el unico control con el que el operador
  // puede callar una alarma (IEC 60601-1-8 6.10): tiene que haber un solo
  // camino y que ese este probado.
  MuteAlarm_cb(nullptr);
  UI_ShowToast(TXT("Alarma silenciada", "Alarm silenced", "Alarme silencee"),
               2500);
  // La lista NO se reconstruye aqui a proposito: hacerlo destruiria, desde
  // dentro de su propio callback, el boton que acaba de recibir el toque.
  // alarmsMuted entra en la firma, asi que AlarmCenter_Poll() la reconstruye
  // en la siguiente pasada, ya fuera del despacho del evento.
}

void showLoading() {
  clearContent();
  makeTitle(TXT("Cargando...", "Loading...", "Chargement..."));
  lv_obj_t *close =
      makeBtn(s_content, "X", onCloseClicked, lv_color_hex(0xAA3333));
  lv_obj_set_size(close, 44, 44);
  lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 0, 0);
}

void requestHistory() {
  Communication_SendAlarmHistoryReq();
  s_step = Step::Loading;
  s_deadlineMs = millis() + RESP_TIMEOUT_MS;
}

// ---- Payloads de las tarjetas (estaticos: LVGL exige que el user_data
// sobreviva a la pantalla) ----
struct Row {
  uint8_t id;
  uint8_t priority;
  bool    active;    // true = alarma en curso, descripcion ya disponible aqui
  bool    silenced;  // AUDIO PAUSED, tal como lo reporta la placa
  char    title[ALARM_TITLE_MAX_CHARS + 1];
  char    desc[ALARM_DESC_MAX_CHARS + 1];
};
Row s_activeRows[MAX_ALARMS];
Row s_histRows[10];

void onRowSilenceTap(lv_event_t *e) {
  auto *r = (Row *)lv_event_get_user_data(e);
  if (!r) return;
  // Se manda el comando y se deja que la placa conteste. No se pinta el estado
  // NUEVO adivinado: la duena del AUDIO PAUSED es la maquina de alarmas, que
  // ademas lo caduca sola a los 10 min, y una version local se desincroniza en
  // cuanto caduque.
  //
  // Lo que si se hace es acusar recibo al instante. El estado real tarda hasta
  // un CTRL,STATE (1 s) en llegar, y sin esto el boton se quedaba idéntico y
  // parecia que la pulsacion se habia perdido. Se desactiva y se marca en
  // espera; no afirma haber silenciado, solo que la orden salio.
  Communication_SendAlarmSilence(r->id, !r->silenced);
  s_silenceWaitUntilMs = millis() + RESP_TIMEOUT_MS;

  lv_obj_t *btn = lv_event_get_target(e);
  if (btn) {
    lv_obj_add_state(btn, LV_STATE_DISABLED);
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    if (lbl) {
      lv_label_set_text(lbl, "...");
    }
  }
  // El repintado de verdad llega por la firma en AlarmCenter_Poll(): ese rodeo
  // es lo que evita destruir este boton desde dentro de su propio callback.
}

void onRowTap(lv_event_t *e) {
  auto *r = (Row *)lv_event_get_user_data(e);
  if (!r) return;
  if (r->active) {
    // La alarma esta sonando ahora: CTRL,ALM ya trajo su descripcion, asi que
    // el pop-up abre instantaneo y sin depender de que la placa responda.
    openDetail(r->id, r->title, r->desc);
  } else {
    // Entrada del registro: la descripcion no viaja en CTRL,ALM_HISTORY (no
    // cabe), se pide a la placa.
    s_detailId = r->id;
    openDetail(r->id, r->title,
               TXT("Cargando...", "Loading...", "Chargement..."));
    Communication_SendAlarmDescReq(r->id);
    s_step = Step::LoadingDetail;
    s_deadlineMs = millis() + RESP_TIMEOUT_MS;
    s_retries = 0;
  }
}

void showList() {
  clearContent();
  makeTitle(TXT("Alarmas", "Alarms", "Alarmes"));

  lv_obj_t *close =
      makeBtn(s_content, "X", onCloseClicked, lv_color_hex(0xAA3333));
  lv_obj_set_size(close, 44, 44);
  lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 0, 0);

  // AQUI NO HAY BOTON GLOBAL DE SILENCIO, y es deliberado.
  //
  // Hubo uno en la cabecera ademas del de cada fila. Sobraba por dos motivos.
  // El de fondo: silenciaba todas las condiciones activas de golpe sin decir
  // cuales, que es justo lo que 6.8.1 obliga a que el operador pueda
  // determinar. El practico: creaba un segundo camino hacia el mismo estado
  // —uno por hmi_msg.muteAlarm y otro por HMI,ALM_SILENCE— y dos caminos hacia
  // el unico control que calla una alarma es exactamente la clase de duplicado
  // que este proyecto ya ha pagado antes.
  //
  // El silencio se pide fila a fila, mas abajo.

  lv_obj_t *body = lv_obj_create(s_content);
  lv_obj_remove_style_all(body);
  lv_obj_set_size(body, 620, 340);
  lv_obj_align(body, LV_ALIGN_TOP_MID, 0, 44);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(body, 8, 0);

  // --- Activas ---
  lv_obj_t *secA = lv_label_create(body);
  lv_label_set_text(secA, TXT("Activas", "Active", "Actives"));
  lv_obj_set_style_text_font(secA, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(secA, lv_color_hex(0x0B2E4F), 0);

  int nActive = 0;
  for (int i = 0; i < MAX_ALARMS && nActive < MAX_ALARMS; i++) {
    if (!alarmList[i].state) continue;

    Row &r = s_activeRows[nActive++];
    r.id = (uint8_t)alarmList[i].id;
    r.priority = alarmList[i].priority;
    r.active = true;
    r.silenced = (ctrl_state_msg.silencedBitmask & (1u << r.id)) != 0;
    snprintf(r.title, sizeof(r.title), "%s", alarmList[i].type);
    snprintf(r.desc, sizeof(r.desc), "%s", alarmList[i].description);

    lv_obj_t *card = lv_btn_create(body);
    lv_obj_set_size(card, 600, 76);
    lv_obj_set_style_bg_color(card, cardFill(r.priority), LV_PART_MAIN);
    lv_obj_set_style_border_color(card, cardBorder(r.priority), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xBBD9F7),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(card, onRowTap, LV_EVENT_CLICKED, &r);

    char buf[160];
    snprintf(buf, sizeof(buf), "%s %s\n%s  -  %s", prioMark(r.priority),
             r.title, prioName(r.priority),
             r.silenced ? TXT("AUDIO EN PAUSA", "AUDIO PAUSED", "AUDIO EN PAUSE")
                        : TXT("en curso", "ongoing", "en cours"));
    // 600 de tarjeta - 190 de boton - 34 del icono: deja sitio a ambos.
    makeCardLabel(card, buf, 366);

    // 6.8.1 pide que el operador pueda determinar QUE condiciones estan
    // inactivadas, y 201.12.3.104 que la alarma silenciada mantenga
    // indicacion visual. El icono va por fila, no en un rincon global, que es
    // la unica forma de distinguir cual de varias esta callada.
    if (r.silenced) {
      lv_obj_t *icon = makeAudioPausedIcon(card, cardBorder(r.priority));
      lv_obj_align(icon, LV_ALIGN_RIGHT_MID, -198, 0);
    }

    // 6.8.4: "means to terminate any ALARM SIGNAL inactivation state". El
    // mismo control silencia y cancela el silencio, condicion a condicion.
    lv_obj_t *btn = makeBtn(
        card,
        r.silenced ? TXT("REANUDAR", "RESUME", "REPRENDRE")
                   : TXT("SILENCIAR", "SILENCE", "SILENCE"),
        onRowSilenceTap,
        r.silenced ? lv_color_hex(0x2E7D32) : lv_color_hex(0xE08800), &r);
    lv_obj_set_size(btn, 190, 52);
    lv_obj_align(btn, LV_ALIGN_RIGHT_MID, -4, 0);
  }

  if (nActive == 0) {
    lv_obj_t *none = lv_label_create(body);
    lv_label_set_text(none, TXT("Sin alarmas activas", "No active alarms",
                                "Aucune alarme active"));
    lv_obj_set_style_text_color(none, lv_color_hex(0x0B2E4F), 0);
  }

  // --- Registro (IEC 60601-1-8 6.12.2) ---
  lv_obj_t *secH = lv_label_create(body);
  char hdr[64];
  snprintf(hdr, sizeof(hdr), "%s (%d)",
           TXT("Registro", "Log", "Journal"), s_hist.count);
  lv_label_set_text(secH, hdr);
  lv_obj_set_style_text_font(secH, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(secH, lv_color_hex(0x0B2E4F), 0);

  if (s_hist.count == 0) {
    lv_obj_t *none = lv_label_create(body);
    lv_label_set_text(none, TXT("Todavia no ha saltado ninguna alarma",
                                "No alarms recorded yet",
                                "Aucune alarme enregistree"));
    lv_obj_set_style_text_color(none, lv_color_hex(0x0B2E4F), 0);
  }

  for (int i = 0; i < s_hist.count && i < 10; i++) {
    const AlarmHistoryItem &it = s_hist.items[i];
    Row &r = s_histRows[i];
    r.id = it.id;
    r.priority = it.priority;
    r.active = false;
    r.silenced = false;  // una entrada del registro ya no señaliza nada
    snprintf(r.title, sizeof(r.title), "%s", it.title);
    r.desc[0] = '\0';

    lv_obj_t *card = lv_btn_create(body);
    lv_obj_set_size(card, 600, 76);
    // Resuelta -> verde; sin resolver -> el tono plano del historial de bebes.
    // El verde no es decoracion: es la respuesta a la unica pregunta que se le
    // hace al registro de un vistazo, "esto sigue pasando o ya paso". El
    // borde mantiene el color de PRIORIDAD, que es lo que la senal de alarma
    // codifica por norma y no debe reescribirse aqui.
    lv_obj_set_style_bg_color(
        card, lv_color_hex(it.resolved ? 0xDFF5E1 : 0xEDF3F9), LV_PART_MAIN);
    lv_obj_set_style_border_color(
        card, it.resolved ? lv_color_hex(0x2E7D32) : cardBorder(r.priority),
        LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xBBD9F7),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(card, onRowTap, LV_EVENT_CLICKED, &r);

    char raised[24];
    fmtStamp(it.raisedEpoch, raised, sizeof(raised));
    char buf[192];
    if (!it.resolved) {
      snprintf(buf, sizeof(buf), "%s %s\n%s  -  %s", prioMark(r.priority),
               r.title, raised, TXT("sin resolver", "unresolved", "non resolue"));
    } else {
      // Resuelta con hora conocida: inicio -> fin. Resuelta pero sin reloj
      // sincronizado: se dice que se resolvio y no se inventa una hora.
      char cleared[24];
      fmtStamp(it.clearedEpoch, cleared, sizeof(cleared));
      snprintf(buf, sizeof(buf), "%s %s\n%s  ->  %s", prioMark(r.priority),
               r.title, raised,
               it.clearedEpoch ? cleared
                               : TXT("resuelta", "resolved", "resolue"));
    }
    makeCardLabel(card, buf, 580);
  }

  s_activeSig = activeIdSignature();
  s_viewSig = viewSignature();
  s_step = Step::Showing;
}

// ---------------- Pop-up de descripcion ----------------

void onDetailClose(lv_event_t *) {
  if (s_dlg) {
    lv_obj_del(s_dlg);
    s_dlg = nullptr;
    s_dlgBody = nullptr;
  }
  s_step = Step::Showing;
}

void openDetail(uint8_t id, const char *title, const char *desc) {
  (void)id;
  if (s_dlg) {
    lv_obj_del(s_dlg);
    s_dlg = nullptr;
    s_dlgBody = nullptr;
  }

  s_dlg = lv_obj_create(s_overlay);
  lv_obj_set_size(s_dlg, 520, 320);
  lv_obj_center(s_dlg);
  lv_obj_set_style_radius(s_dlg, 12, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_dlg, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_clear_flag(s_dlg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_move_foreground(s_dlg);
  // Flex en columna en vez de offsets a mano: es lo que evita que el boton de
  // cerrar acabe solapando el texto cuando la descripcion ocupa varias lineas
  // (mismo fallo ya corregido en el dialogo de alta de BabyHistory.cpp).
  lv_obj_set_style_pad_all(s_dlg, 14, LV_PART_MAIN);
  lv_obj_set_flex_flow(s_dlg, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_dlg, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(s_dlg, 10, LV_PART_MAIN);

  lv_obj_t *t = lv_label_create(s_dlg);
  lv_label_set_text(t, title);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(t, lv_color_hex(0x0B2E4F), 0);
  lv_obj_set_width(t, 480);
  lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);

  s_dlgBody = lv_label_create(s_dlg);
  lv_label_set_text(s_dlgBody, desc);
  lv_obj_set_style_text_font(s_dlgBody, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(s_dlgBody, lv_color_hex(0x0B2E4F), 0);
  lv_obj_set_width(s_dlgBody, 480);
  lv_obj_set_height(s_dlgBody, 170);
  lv_label_set_long_mode(s_dlgBody, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(s_dlgBody, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t *ok = makeBtn(s_dlg, TXT("CERRAR", "CLOSE", "FERMER"),
                         onDetailClose, lv_color_hex(0x0075EE));
  lv_obj_set_size(ok, 160, 44);

  s_step = Step::Detail;
}

}  // namespace

void AlarmCenter_Init(void) {
  // lv_layer_top() y no una pantalla: asi el centro de alarmas se abre igual
  // desde la principal, desde el bloqueo o desde cualquier menu, sin depender
  // de que cada pantalla lo tenga instanciado.
  s_overlay = lv_obj_create(lv_layer_top());
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

void AlarmCenter_Open(void) {
  if (!s_overlay || s_step != Step::Closed) return;
  s_hist.count = 0;
  s_retries = 0;

  showLoading();
  requestHistory();

  lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(s_overlay);
}

void AlarmCenter_Close(void) { closeScreen(); }

bool AlarmCenter_IsOpen(void) { return s_step != Step::Closed; }

void AlarmCenter_Poll(void) {
  if (s_step == Step::Closed) return;

  switch (s_step) {
    case Step::Loading:
      if (g_pendingAlarmHistory) {
        g_pendingAlarmHistory = false;
        s_hist = g_alarmHistory;
        s_retries = 0;
        showList();
      } else if (millis() > s_deadlineMs) {
        if (s_retries == 0) {
          s_retries++;
          requestHistory();
        } else {
          // Sin registro no se bloquea la pantalla: las alarmas activas son lo
          // urgente y esas ya estan en alarmList, sin depender de la placa.
          UI_ShowToast(TXT("No se pudo leer el registro",
                           "Could not read the log",
                           "Impossible de lire le journal"),
                       3000);
          s_hist.count = 0;
          s_retries = 0;
          showList();
        }
      }
      break;

    case Step::LoadingDetail:
      if (g_pendingAlarmDesc) {
        g_pendingAlarmDesc = false;
        if (g_alarmDesc.id == s_detailId && s_dlgBody) {
          lv_label_set_text(s_dlgBody, g_alarmDesc.desc);
          s_step = Step::Detail;
        }
        // Si el id no cuadra es una respuesta a una peticion anterior: se
        // ignora y se sigue esperando hasta el timeout, en vez de pintar la
        // descripcion de otra alarma.
      } else if (millis() > s_deadlineMs) {
        if (s_retries == 0) {
          s_retries++;
          Communication_SendAlarmDescReq(s_detailId);
          s_deadlineMs = millis() + RESP_TIMEOUT_MS;
        } else if (s_dlgBody) {
          lv_label_set_text(s_dlgBody,
                            TXT("Descripcion no disponible",
                                "Description unavailable",
                                "Description indisponible"));
          s_step = Step::Detail;
        }
      }
      break;

    case Step::Showing: {
      // Registro recien llegado (pedido al abrir, o tras resolverse algo).
      if (g_pendingAlarmHistory) {
        g_pendingAlarmHistory = false;
        s_hist = g_alarmHistory;
        showList();
        break;
      }

      // La lista se construye una sola vez, asi que sin esto una alarma que
      // salta o se resuelve con el centro abierto no se movería hasta cerrar
      // y volver a entrar. Se compara una firma en vez de reconstruir en cada
      // pasada, que destruiria y recrearia las tarjetas 30 veces por segundo
      // y se comeria el toque del operador a media pulsacion.
      // La orden de silencio se perdio o la placa no la atendio: se repinta
      // para devolver el boton a su estado real en vez de dejarlo en "...".
      if (s_silenceWaitUntilMs &&
          (int32_t)(millis() - s_silenceWaitUntilMs) >= 0) {
        s_silenceWaitUntilMs = 0;
        showList();
        break;
      }

      const uint32_t sig = viewSignature();
      if (sig != s_viewSig) {
        s_viewSig = sig;
        s_silenceWaitUntilMs = 0;  // la placa ha contestado
        // Si lo que cambio es el CONJUNTO de alarmas activas, el registro que
        // tenemos se ha quedado viejo: la que acaba de resolverse ya lleva
        // hora de fin en la placa, y una nueva ni siquiera esta en s_hist.
        // Se vuelve a pedir y la respuesta entra por la rama de arriba; la
        // lista se repinta ya con lo que hay para no dejar la pantalla
        // congelada mientras llega.
        const uint32_t aSig = activeIdSignature();
        if (aSig != s_activeSig) {
          s_activeSig = aSig;
          Communication_SendAlarmHistoryReq();
        }
        showList();
      }
      break;
    }

    default:
      break;  // Detail es puramente reactivo al toque
  }
}
