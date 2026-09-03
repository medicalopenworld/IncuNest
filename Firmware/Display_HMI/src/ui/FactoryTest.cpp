#include "ui/FactoryTest.h"

#include <Wire.h>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "CommTask.h"
#include "UITask.h"
#include "buzzer.h"
#include "factory_test.h"
#include "main.h"
#include "ui.h"

// Puesto por el callback del boton del splash (ElementsCreation.cpp). Leido
// por intro_timer_cb() (UITask.cpp).
bool g_factoryTestRequested = false;

namespace {

const char *TXT(const char *es, const char *en, const char *fr) {
  return (g_lang == LANG_ES) ? es : (g_lang == LANG_FR) ? fr : en;
}

// ---------------------------------------------------------------------------
// Tabla de tests locales (design.md D9 / spec hmi-factory-test), en el orden
// exacto en el que se ejecutan. kLocalCount coincide con FTEST_HMI_END -
// FTEST_HMI_BASE (comprobado por el static_assert de kHmiKeys en
// shared/src/factory_test.cpp).
constexpr FtestId kLocalOrder[] = {
    FTEST_HMI_SYSINFO, FTEST_HMI_I2C,    FTEST_HMI_PANEL,  FTEST_HMI_TOUCH,
    FTEST_HMI_BUZZER,  FTEST_HMI_SPEAKER, FTEST_HMI_WIFI,   FTEST_HMI_NVS,
    FTEST_HMI_LINK,
};
constexpr int kLocalCount = sizeof(kLocalOrder) / sizeof(kLocalOrder[0]);
constexpr int kMaxRows = kLocalCount + (int)FTEST_MB_COUNT;

// ---------------------------------------------------------------------------
// Estado de una fila de la lista (local o de motherBoard). Direccion estable
// mientras dure la pantalla abierta: los locales viven en s_rows[0..8] en el
// orden de kLocalOrder y no se mueven; los de motherBoard se anaden a partir
// de s_rows[kLocalCount] segun van llegando sus CTRL,FTEST.
struct RowData {
  unsigned    id;
  bool        isMb;
  bool        started;
  FtestStatus status;
  char        detail[FTEST_DETAIL_MAX + 1];
};

RowData s_rows[kMaxRows];
int     s_rowCount = 0;
int     s_lastTouchedRow = -1;  // para lv_obj_scroll_to_view()

enum class Step {
  Closed,
  LocalSeq,
  RemoteAwaitFirst,
  RemoteRunning,
  Summary,
};

enum class LocalPhase { None, Panel, Touch, Buzzer, Speaker, Wifi };
enum class AskKind { None, Panel, Buzzer, Speaker };

Step       s_step = Step::Closed;
LocalPhase s_localPhase = LocalPhase::None;
AskKind    s_askKind = AskKind::None;
int        s_localIdx = 0;
bool       s_retryMode = false;
uint32_t   s_deadlineMs = 0;

bool     s_mbSupported = false;
bool     s_mbUnsupported = false;
bool     s_mbRejected = false;
FtestReject s_mbRejectReason = FTEST_REJECT_BUSY;
bool     s_mbDone = false;
unsigned s_mbPass = 0, s_mbFail = 0, s_mbSkip = 0;
unsigned s_remoteConfirmAnsweredRowId = FTEST_ID_NONE;

constexpr uint32_t TONE_MS = 300;
constexpr uint32_t LOCAL_ASK_TIMEOUT_MS = 60000;
constexpr uint32_t WIFI_SCAN_TIMEOUT_MS = 15000;
constexpr uint32_t PANEL_STEP_MS = 800;
constexpr int       TOUCH_MARGIN = 60;
constexpr int       TOUCH_HIT_PX = 40;
constexpr uint32_t TOUCH_TARGET_TIMEOUT_MS = 20000;

lv_obj_t *s_overlay = nullptr;
lv_obj_t *s_card = nullptr;
lv_obj_t *s_titleLbl = nullptr;
lv_obj_t *s_exitBtnLabel = nullptr;
lv_obj_t *s_body = nullptr;    // lista de filas, scrollable (flex columna)
lv_obj_t *s_action = nullptr;  // instruccion / pregunta / resumen

lv_obj_t *s_panelRect = nullptr;
int       s_panelIdx = 0;

lv_obj_t   *s_touchCatcher = nullptr;
lv_obj_t   *s_touchMarker = nullptr;
int         s_touchIdx = 0;
int         s_touchMaxErrPx = 0;
bool        s_touchHit = false;
lv_point_t  s_touchHitPoint = {0, 0};

const lv_point_t kTouchTargets[5] = {
    {TOUCH_MARGIN, TOUCH_MARGIN},
    {DISPLAY_WIDTH - TOUCH_MARGIN, TOUCH_MARGIN},
    {DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2},
    {TOUCH_MARGIN, DISPLAY_HEIGHT - TOUCH_MARGIN},
    {DISPLAY_WIDTH - TOUCH_MARGIN, DISPLAY_HEIGHT - TOUCH_MARGIN},
};

// R, G, B, blanco, negro — el orden que pide la spec.
const lv_color_t kPanelColors[5] = {
    lv_color_hex(0xFF0000), lv_color_hex(0x00FF00), lv_color_hex(0x0000FF),
    lv_color_hex(0xFFFFFF), lv_color_hex(0x000000),
};

// ---------------------------------------------------------------------------
// Forward declarations (la maquina de estados es mutuamente recursiva:
// finishLocal() encadena con beginLocalTest() del siguiente test).
void beginLocalTest();
void finishLocal(FtestStatus st, const char *detail);
void renderAll();
void resolveAsk(bool yes);
void goSummary();
void startRemote();
void onRetryClicked(lv_event_t *e);
void onExitClicked(lv_event_t *e);

// ---------------------------------------------------------------------------
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

// Zumbador del display: STC8H1K28 @ I2C_ADDR_BACKLIGHT (buzzer.h ya expone
// buzzerOn()/buzzerOff() con el mismo comando). El altavoz es el MISMO chip,
// comando distinto (I2C_CMD_SPEAKER_*): no hay driver propio, se manda aqui
// igual que lo hace AudioManager.cpp.
void speakerOn() {
  Wire.beginTransmission((uint8_t)I2C_ADDR_BACKLIGHT);
  Wire.write(I2C_CMD_SPEAKER_ON);
  Wire.endTransmission();
}
void speakerOff() {
  Wire.beginTransmission((uint8_t)I2C_ADDR_BACKLIGHT);
  Wire.write(I2C_CMD_SPEAKER_OFF);
  Wire.endTransmission();
}

// ---------------------------------------------------------------------------
// Nombres e instrucciones traducidos. ftest_id_key() (shared/) es el
// fallback para un id de motherBoard que esta version del display todavia no
// conoce por nombre (tabla ampliada en el futuro sin romper compatibilidad).
const char *localTestName(unsigned id) {
  switch (id) {
    case FTEST_HMI_SYSINFO: return TXT("Info sistema", "System info", "Info systeme");
    case FTEST_HMI_I2C:     return TXT("Bus I2C", "I2C bus", "Bus I2C");
    case FTEST_HMI_PANEL:   return TXT("Panel", "Panel", "Panneau");
    case FTEST_HMI_TOUCH:   return TXT("Tactil", "Touch", "Tactile");
    case FTEST_HMI_BUZZER:  return TXT("Zumbador", "Buzzer", "Buzzer");
    case FTEST_HMI_SPEAKER: return TXT("Altavoz", "Speaker", "Haut-parleur");
    case FTEST_HMI_WIFI:    return TXT("WiFi", "WiFi", "WiFi");
    case FTEST_HMI_NVS:     return TXT("Memoria NVS", "NVS memory", "Memoire NVS");
    case FTEST_HMI_LINK:    return TXT("Enlace placa", "Board link", "Liaison carte");
    default: return ftest_id_key(id);
  }
}

const char *mbTestName(unsigned id) {
  switch (id) {
    case FTEST_MB_SYSINFO:     return TXT("Info sistema", "System info", "Info systeme");
    case FTEST_MB_INA3221:     return TXT("Sensor corriente", "Current sensor", "Capteur courant");
    case FTEST_MB_STANDBY:     return TXT("Consumo reposo", "Standby current", "Consommation veille");
    case FTEST_MB_CHARGER:     return TXT("Cargador", "Charger", "Chargeur");
    case FTEST_MB_POWER_SRC:   return TXT("Fuente alimentacion", "Power source", "Source alimentation");
    case FTEST_MB_SKIN_ADC:    return TXT("Sonda piel", "Skin probe", "Sonde peau");
    case FTEST_MB_EXT_SHT4X:   return TXT("Sensor ambiente", "Ambient sensor", "Capteur ambiant");
    case FTEST_MB_SENSOR_SRC:  return TXT("Origen sensores", "Sensor source", "Source capteurs");
    case FTEST_MB_SB_LINK:     return TXT("Enlace SensorBoard", "SensorBoard link", "Liaison SensorBoard");
    case FTEST_MB_SB_STATUS:   return TXT("Estado SensorBoard", "SensorBoard status", "Etat SensorBoard");
    case FTEST_MB_SB_ENV:      return TXT("Ambiente SensorBoard", "SensorBoard env", "Environnement SensorBoard");
    case FTEST_MB_SB_DOOR:     return TXT("Puerta", "Door", "Porte");
    case FTEST_MB_SB_LIGHT:    return TXT("Sensor luz", "Light sensor", "Capteur lumiere");
    case FTEST_MB_SB_CAMERA:   return TXT("Camara", "Camera", "Camera");
    case FTEST_MB_ACTUATORS:   return TXT("Actuadores", "Actuators", "Actionneurs");
    case FTEST_MB_FAN_RPM:     return TXT("RPM ventilador", "Fan RPM", "RPM ventilateur");
    case FTEST_MB_HUMID_USB:   return TXT("Humidificador USB", "Humidifier USB", "Humidificateur USB");
    case FTEST_MB_BUZZER:      return TXT("Zumbador placa", "Board buzzer", "Buzzer carte");
    case FTEST_MB_AFE_SPI:     return TXT("AFE SPI", "AFE SPI", "AFE SPI");
    case FTEST_MB_AFE_PROBE:   return TXT("Sonda SpO2", "SpO2 probe", "Sonde SpO2");
    case FTEST_MB_HMI_LINK:    return TXT("Enlace HMI", "HMI link", "Liaison HMI");
    case FTEST_MB_GSM_AT:      return TXT("Modem GSM", "GSM modem", "Modem GSM");
    case FTEST_MB_GSM_SIM:     return TXT("SIM", "SIM", "SIM");
    case FTEST_MB_GSM_SIGNAL:  return TXT("Senal GSM", "GSM signal", "Signal GSM");
    case FTEST_MB_GSM_NET:     return TXT("Red GSM", "GSM network", "Reseau GSM");
    case FTEST_MB_WIFI:        return TXT("WiFi placa", "Board WiFi", "WiFi carte");
    case FTEST_MB_TB_PROVISION: return TXT("ThingsBoard", "ThingsBoard", "ThingsBoard");
    case FTEST_MB_TIME:        return TXT("Hora", "Time", "Heure");
    case FTEST_MB_NVS:         return TXT("NVS placa", "Board NVS", "NVS carte");
    case FTEST_MB_LITTLEFS:    return TXT("LittleFS", "LittleFS", "LittleFS");
    default: return ftest_id_key(id);
  }
}

const char *mbWaitInstruction(unsigned id) {
  switch (id) {
    case FTEST_MB_SB_DOOR:
      return TXT("Abre y cierra la puerta", "Open and close the door",
                 "Ouvrez et fermez la porte");
    case FTEST_MB_SB_LIGHT:
      return TXT("Tapa el sensor de luz", "Cover the light sensor",
                 "Couvrez le capteur de lumiere");
    default:
      return TXT("Sigue las instrucciones", "Follow the instructions",
                 "Suivez les instructions");
  }
}

const char *mbConfirmQuestion(unsigned id) {
  switch (id) {
    case FTEST_MB_BUZZER:
      return TXT("Suena el zumbador de la placa?",
                 "Does the board buzzer sound?",
                 "Le buzzer de la carte sonne-t-il ?");
    default:
      return TXT("Confirma el resultado", "Confirm the result",
                 "Confirmez le resultat");
  }
}

const char *rejectReasonText(FtestReject r) {
  switch (r) {
    case FTEST_REJECT_BUSY:
      return TXT("Ya hay un test en curso", "A test is already running",
                 "Un test est deja en cours");
    case FTEST_REJECT_CONTROL_ACTIVE:
      return TXT("Control activo: apaga el control antes del test",
                 "Control active: turn off control before testing",
                 "Controle actif : eteignez le controle avant le test");
    case FTEST_REJECT_UNKNOWN_ID:
      return TXT("Test desconocido", "Unknown test", "Test inconnu");
    default:
      return "?";
  }
}

const char *currentAskQuestion() {
  switch (s_askKind) {
    case AskKind::Panel:
      return TXT("Se han visto los 5 colores correctamente?",
                 "Did you see all 5 colors correctly?",
                 "Avez-vous vu les 5 couleurs correctement ?");
    case AskKind::Buzzer:
      return TXT("Se ha oido el zumbador del display?",
                 "Did you hear the display buzzer?",
                 "Avez-vous entendu le buzzer de l'ecran ?");
    case AskKind::Speaker:
      return TXT("Se ha oido el altavoz del display?",
                 "Did you hear the display speaker?",
                 "Avez-vous entendu le haut-parleur de l'ecran ?");
    default:
      return "";
  }
}

// ---------------------------------------------------------------------------
// Paleta: mismos colores de prioridad que AlarmCenter (PASA=6.3.2.2.2 cian,
// FALLA=rojo, SKIP/espera=amarillo), sin decoracion nueva.
const char *statusText(FtestStatus st, bool started) {
  if (!started) return TXT("Pendiente", "Pending", "En attente");
  switch (st) {
    case FTEST_RUNNING: return TXT("En curso", "Running", "En cours");
    case FTEST_PASS:    return TXT("PASA", "PASS", "REUSSI");
    case FTEST_FAIL:    return TXT("FALLA", "FAIL", "ECHEC");
    case FTEST_SKIP:    return TXT("OMITIDO", "SKIPPED", "OMIS");
    case FTEST_WAIT:    return TXT("ESPERA", "WAIT", "ATTENTE");
    case FTEST_CONFIRM: return TXT("CONFIRMAR", "CONFIRM", "CONFIRMER");
    default: return "?";
  }
}
lv_color_t statusFill(FtestStatus st, bool started) {
  if (!started) return lv_color_hex(0xEDF3F9);
  switch (st) {
    case FTEST_PASS: return lv_color_hex(0xDFF3FF);
    case FTEST_FAIL: return lv_color_hex(0xFFE0E4);
    case FTEST_SKIP:
    case FTEST_WAIT:
    case FTEST_CONFIRM: return lv_color_hex(0xFFF0D6);
    default: return lv_color_hex(0xEDF3F9);
  }
}
lv_color_t statusBorder(FtestStatus st, bool started) {
  if (!started) return lv_color_hex(0xB9C6D6);
  switch (st) {
    case FTEST_PASS: return lv_color_hex(0x2196C4);
    case FTEST_FAIL: return lv_color_hex(0xD5283C);
    case FTEST_SKIP:
    case FTEST_WAIT:
    case FTEST_CONFIRM: return lv_color_hex(0xC98A00);
    default: return lv_color_hex(0x7F93A8);
  }
}

// ---------------------------------------------------------------------------
// Filas de motherBoard: se anaden bajo demanda segun llegan sus CTRL,FTEST
// (spec: "anadir una fila por cada CTRL,FTEST recibido"). Los 9 locales ya
// estan en s_rows[0..kLocalCount-1] desde que se abre la pantalla.
int mbRowIndex(unsigned id) {
  for (int i = kLocalCount; i < s_rowCount; i++) {
    if (s_rows[i].id == id) return i;
  }
  if (s_rowCount >= kMaxRows) {
    // No deberia pasar: kMaxRows = locales + FTEST_MB_COUNT y
    // ftest_parse_result() ya descarta ids fuera de la tabla de motherBoard.
    return kLocalCount;
  }
  const int i = s_rowCount++;
  s_rows[i].id = id;
  s_rows[i].isMb = true;
  s_rows[i].started = false;
  s_rows[i].status = FTEST_RUNNING;
  s_rows[i].detail[0] = '\0';
  return i;
}

void destroyTransientOverlayObjects() {
  if (s_panelRect) { lv_obj_del(s_panelRect); s_panelRect = nullptr; }
  if (s_touchMarker) { lv_obj_del(s_touchMarker); s_touchMarker = nullptr; }
  if (s_touchCatcher) { lv_obj_del(s_touchCatcher); s_touchCatcher = nullptr; }
}

// ============================================================================
// Render
// ============================================================================
void renderCenteredText(const char *text) {
  lv_obj_t *lbl = lv_label_create(s_action);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0x0B2E4F), 0);
  lv_obj_set_width(lbl, 700);
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(lbl);
}

void onAskYesCb(lv_event_t *) { resolveAsk(true); }
void onAskNoCb(lv_event_t *) { resolveAsk(false); }

void onRemoteConfirmYes(lv_event_t *e) {
  auto *row = (RowData *)lv_event_get_user_data(e);
  if (!row) return;
  Communication_SendFtestConfirm((uint8_t)row->id, true);
  s_remoteConfirmAnsweredRowId = row->id;
  renderAll();
}
void onRemoteConfirmNo(lv_event_t *e) {
  auto *row = (RowData *)lv_event_get_user_data(e);
  if (!row) return;
  Communication_SendFtestConfirm((uint8_t)row->id, false);
  s_remoteConfirmAnsweredRowId = row->id;
  renderAll();
}

void renderAskUi(const char *question, lv_event_cb_t yesCb, lv_event_cb_t noCb,
                 void *userData) {
  lv_obj_t *lbl = lv_label_create(s_action);
  lv_label_set_text(lbl, question);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0x0B2E4F), 0);
  lv_obj_set_width(lbl, 700);
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_t *yes = makeBtn(s_action, TXT("SI", "YES", "OUI"), yesCb,
                          lv_color_hex(0x2E7D32), userData);
  lv_obj_set_size(yes, 140, 44);
  lv_obj_align(yes, LV_ALIGN_BOTTOM_MID, -80, 0);

  lv_obj_t *no = makeBtn(s_action, TXT("NO", "NO", "NON"), noCb,
                         lv_color_hex(0xAA3333), userData);
  lv_obj_set_size(no, 140, 44);
  lv_obj_align(no, LV_ALIGN_BOTTOM_MID, 80, 0);
}

void renderLocalAction() {
  if (s_askKind != AskKind::None) {
    renderAskUi(currentAskQuestion(), onAskYesCb, onAskNoCb, nullptr);
    return;
  }
  switch (s_localPhase) {
    case LocalPhase::Touch: {
      char buf[64];
      snprintf(buf, sizeof(buf),
              TXT("Toca el objetivo (%d/5)", "Touch the target (%d/5)",
                  "Touchez la cible (%d/5)"),
              s_touchIdx + 1);
      renderCenteredText(buf);
      break;
    }
    case LocalPhase::Wifi:
      renderCenteredText(TXT("Buscando redes WiFi...",
                             "Scanning WiFi networks...",
                             "Recherche de reseaux WiFi..."));
      break;
    default:
      break;  // Panel (el rectangulo de color tapa la pantalla) y tests
              // instantaneos: sin contenido en la zona de accion.
  }
}

void renderRemoteAction() {
  if (!s_mbSupported) return;
  for (int i = kLocalCount; i < s_rowCount; i++) {
    if (s_rows[i].status == FTEST_WAIT) {
      renderCenteredText(mbWaitInstruction(s_rows[i].id));
      return;
    }
    if (s_rows[i].status == FTEST_CONFIRM) {
      if (s_remoteConfirmAnsweredRowId == s_rows[i].id) {
        renderCenteredText(TXT("Esperando a la motherBoard...",
                               "Waiting for the motherBoard...",
                               "En attente de la carte..."));
      } else {
        renderAskUi(mbConfirmQuestion(s_rows[i].id), onRemoteConfirmYes,
                    onRemoteConfirmNo, &s_rows[i]);
      }
      return;
    }
  }
}

void renderSummary() {
  int localPass = 0, localFail = 0, localSkip = 0;
  for (int i = 0; i < kLocalCount; i++) {
    if (s_rows[i].status == FTEST_PASS) localPass++;
    else if (s_rows[i].status == FTEST_FAIL) localFail++;
    else if (s_rows[i].status == FTEST_SKIP) localSkip++;
  }
  char buf[192];
  if (s_mbUnsupported) {
    snprintf(buf, sizeof(buf),
            TXT("Pantalla: %d PASA / %d FALLA / %d OMIT.\nPlaca: sin soporte",
                "Display: %d PASS / %d FAIL / %d SKIP\nBoard: no support",
                "Ecran : %d REUSSI / %d ECHEC / %d OMIS\nCarte : non supportee"),
            localPass, localFail, localSkip);
  } else if (s_mbRejected) {
    snprintf(buf, sizeof(buf),
            TXT("Pantalla: %d PASA / %d FALLA / %d OMIT.\nPlaca: %s",
                "Display: %d PASS / %d FAIL / %d SKIP\nBoard: %s",
                "Ecran : %d REUSSI / %d ECHEC / %d OMIS\nCarte : %s"),
            localPass, localFail, localSkip, rejectReasonText(s_mbRejectReason));
  } else if (s_mbDone) {
    snprintf(
        buf, sizeof(buf),
        TXT("Pantalla: %d PASA / %d FALLA / %d OMIT.\nPlaca: %u PASA / %u FALLA / %u OMIT.",
            "Display: %d PASS / %d FAIL / %d SKIP\nBoard: %u PASS / %u FAIL / %u SKIP",
            "Ecran : %d REUSSI / %d ECHEC / %d OMIS\nCarte : %u REUSSI / %u ECHEC / %u OMIS"),
        localPass, localFail, localSkip, s_mbPass, s_mbFail, s_mbSkip);
  } else {
    snprintf(buf, sizeof(buf),
            TXT("Pantalla: %d PASA / %d FALLA / %d OMIT.",
                "Display: %d PASS / %d FAIL / %d SKIP",
                "Ecran : %d REUSSI / %d ECHEC / %d OMIS"),
            localPass, localFail, localSkip);
  }
  renderCenteredText(buf);
}

void buildRowCard(int idx) {
  RowData &r = s_rows[idx];
  const char *name = r.isMb ? mbTestName(r.id) : localTestName(r.id);

  lv_obj_t *card = lv_obj_create(s_body);
  lv_obj_remove_style_all(card);
  lv_obj_set_size(card, lv_pct(100), 40);
  lv_obj_set_style_bg_color(card, statusFill(r.status, r.started), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(card, statusBorder(r.status, r.started),
                                LV_PART_MAIN);
  lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
  lv_obj_set_style_radius(card, 6, LV_PART_MAIN);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  const bool showRetry =
      (s_step == Step::Summary) && r.started && (r.status == FTEST_FAIL);

  char line[80];
  if (r.detail[0]) {
    snprintf(line, sizeof(line), "%s - %s (%s)", name,
            statusText(r.status, r.started), r.detail);
  } else {
    snprintf(line, sizeof(line), "%s - %s", name,
            statusText(r.status, r.started));
  }
  lv_obj_t *lbl = lv_label_create(card);
  lv_label_set_text(lbl, line);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0x0B2E4F), 0);
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
  lv_obj_set_width(lbl, showRetry ? 500 : 690);
  lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);

  if (showRetry) {
    lv_obj_t *btn = makeBtn(card, TXT("REINTENTAR", "RETRY", "RESSAYER"),
                            onRetryClicked, lv_color_hex(0x0075EE), &r);
    lv_obj_set_size(btn, 130, 32);
    lv_obj_align(btn, LV_ALIGN_RIGHT_MID, -4, 0);
  }

  if (idx == s_lastTouchedRow) {
    lv_obj_scroll_to_view(card, LV_ANIM_OFF);
  }
}

void renderAll() {
  if (!s_body || !s_action) return;

  lv_obj_clean(s_body);
  for (int i = 0; i < s_rowCount; i++) buildRowCard(i);

  lv_obj_clean(s_action);
  switch (s_step) {
    case Step::LocalSeq:
      renderLocalAction();
      break;
    case Step::RemoteAwaitFirst:
      renderCenteredText(TXT("Esperando a la motherBoard...",
                             "Waiting for the motherBoard...",
                             "En attente de la carte..."));
      break;
    case Step::RemoteRunning:
      renderRemoteAction();
      break;
    case Step::Summary:
      renderSummary();
      break;
    default:
      break;
  }

  if (s_titleLbl) {
    lv_label_set_text(s_titleLbl, s_step == Step::Summary
                                       ? TXT("RESUMEN", "SUMMARY", "RESUME")
                                       : TXT("TEST DE FABRICA",
                                             "FACTORY TEST", "TEST USINE"));
  }
}

// ============================================================================
// Tests locales instantaneos
// ============================================================================
void runSysInfo() {
  const uint32_t flash = ESP.getFlashChipSize();
  const uint32_t psram = ESP.getPsramSize();
  const uint32_t heap = ESP.getFreeHeap();
  char detail[FTEST_DETAIL_MAX + 1];
  snprintf(detail, sizeof(detail), "%uMB/%uMB/%ukB",
          (unsigned)(flash / 1000000u), (unsigned)(psram / 1000000u),
          (unsigned)(heap / 1024u));
  // PSRAM "aprox 8MB": el propio SDK reserva parte del banco OPI, asi que el
  // valor reportado suele quedar algo por debajo de 8*1024*1024 exactos.
  const bool ok = flash == 16u * 1024u * 1024u &&
                  psram >= 7u * 1024u * 1024u && heap >= 60u * 1024u;
  finishLocal(ok ? FTEST_PASS : FTEST_FAIL, detail);
}

void runI2c() {
  // Fuera de LVGL_Lock(): son transacciones I2C (convencion de la placa).
  LVGL_Unlock();
  Wire.beginTransmission(DISPLAY_I2C_ADDR_TOUCH);
  const uint8_t touchErr = Wire.endTransmission();
  Wire.beginTransmission((uint8_t)I2C_ADDR_BACKLIGHT);
  const uint8_t blErr = Wire.endTransmission();
  // PCA9557 @ 0x18: no poblado en esta revision de hardware (ver
  // embedded-display-hmi.md). Informativo, nunca hace fallar el test.
  Wire.beginTransmission((uint8_t)0x18);
  const uint8_t pcaErr = Wire.endTransmission();
  LVGL_Lock();

  char detail[FTEST_DETAIL_MAX + 1];
  snprintf(detail, sizeof(detail), "0x18:%s", pcaErr == 0 ? "si" : "no");
  const bool ok = (touchErr == 0) && (blErr == 0);
  finishLocal(ok ? FTEST_PASS : FTEST_FAIL, detail);
}

void runNvs() {
  // Fuera de LVGL_Lock(): NVS puede tardar hasta ~30 ms en un ciclo de
  // wear-leveling (mismo motivo que el resto de escrituras de Preferences).
  LVGL_Unlock();
  Preferences p;
  p.begin(HMI_NS_FTEST, false);
  const uint32_t probeValue = (uint32_t)millis();
  p.putUInt(HMI_KEY_FTEST_PROBE, probeValue);
  const uint32_t readBack = p.getUInt(HMI_KEY_FTEST_PROBE, 0xFFFFFFFFu);
  p.end();
  LVGL_Lock();
  finishLocal(readBack == probeValue ? FTEST_PASS : FTEST_FAIL, "");
}

void runLink() {
  char detail[FTEST_DETAIL_MAX + 1];
  snprintf(detail, sizeof(detail), "%s", ctrl_state_msg.fwVer);
  const bool ok = Display_BoardEverSeen() && !Display_IsBoardLinkLost();
  finishLocal(ok ? FTEST_PASS : FTEST_FAIL, detail);
}

// ============================================================================
// PANEL
// ============================================================================
void showPanelColor() {
  if (s_panelRect) { lv_obj_del(s_panelRect); s_panelRect = nullptr; }
  s_panelRect = lv_obj_create(s_overlay);
  lv_obj_remove_style_all(s_panelRect);
  lv_obj_set_size(s_panelRect, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  lv_obj_set_pos(s_panelRect, 0, 0);
  lv_obj_clear_flag(s_panelRect, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(s_panelRect, kPanelColors[s_panelIdx], LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_panelRect, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_move_foreground(s_panelRect);
  s_deadlineMs = millis() + PANEL_STEP_MS;
}

void beginPanel() {
  s_panelIdx = 0;
  showPanelColor();
}

void servicePanel() {
  if ((int32_t)(millis() - s_deadlineMs) < 0) return;
  s_panelIdx++;
  if (s_panelIdx < 5) {
    showPanelColor();
    return;
  }
  if (s_panelRect) { lv_obj_del(s_panelRect); s_panelRect = nullptr; }
  s_localPhase = LocalPhase::None;
  s_askKind = AskKind::Panel;
  s_deadlineMs = millis() + LOCAL_ASK_TIMEOUT_MS;
  renderAll();
}

// ============================================================================
// TOUCH
// ============================================================================
void onTouchCatcherEvent(lv_event_t *) {
  // Hand-off puro: nada de logica aqui. serviceTouch() (llamado solo desde
  // FactoryTest_Poll(), nunca desde el despacho de este evento) es quien
  // decide y quien puede borrar con seguridad s_touchCatcher — borrar el
  // MISMO objeto que esta despachando su propio evento, a media pulsacion,
  // es el patron inseguro que este hand-off evita.
  lv_indev_t *indev = lv_indev_get_act();
  if (!indev) return;
  lv_indev_get_point(indev, &s_touchHitPoint);
  s_touchHit = true;
}

void showTouchTarget() {
  if (s_touchMarker) { lv_obj_del(s_touchMarker); s_touchMarker = nullptr; }
  s_touchMarker = lv_obj_create(s_overlay);
  lv_obj_remove_style_all(s_touchMarker);
  lv_obj_set_size(s_touchMarker, 30, 30);
  lv_obj_set_style_radius(s_touchMarker, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(s_touchMarker, lv_color_hex(0x0075EE), 0);
  lv_obj_set_style_bg_opa(s_touchMarker, LV_OPA_COVER, 0);
  lv_obj_set_pos(s_touchMarker, kTouchTargets[s_touchIdx].x - 15,
                kTouchTargets[s_touchIdx].y - 15);
  lv_obj_move_foreground(s_touchMarker);
  s_deadlineMs = millis() + TOUCH_TARGET_TIMEOUT_MS;
}

void beginTouch() {
  s_touchIdx = 0;
  s_touchMaxErrPx = 0;
  s_touchHit = false;
  if (!s_touchCatcher) {
    s_touchCatcher = lv_obj_create(s_overlay);
    lv_obj_remove_style_all(s_touchCatcher);
    lv_obj_set_size(s_touchCatcher, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_pos(s_touchCatcher, 0, 0);
    lv_obj_add_flag(s_touchCatcher, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_touchCatcher, onTouchCatcherEvent, LV_EVENT_PRESSED,
                        nullptr);
  }
  lv_obj_move_foreground(s_touchCatcher);
  showTouchTarget();
}

void finishTouch(FtestStatus st, const char *detail) {
  if (s_touchCatcher) { lv_obj_del(s_touchCatcher); s_touchCatcher = nullptr; }
  if (s_touchMarker) { lv_obj_del(s_touchMarker); s_touchMarker = nullptr; }
  finishLocal(st, detail);
}

void serviceTouch() {
  if (s_touchHit) {
    s_touchHit = false;
    const int dx = s_touchHitPoint.x - kTouchTargets[s_touchIdx].x;
    const int dy = s_touchHitPoint.y - kTouchTargets[s_touchIdx].y;
    const int dist = (int)sqrtf((float)(dx * dx + dy * dy));
    if (dist > s_touchMaxErrPx) s_touchMaxErrPx = dist;
    if (dist <= TOUCH_HIT_PX) {
      s_touchIdx++;
      if (s_touchIdx >= 5) {
        char detail[FTEST_DETAIL_MAX + 1];
        snprintf(detail, sizeof(detail), "%dpx", s_touchMaxErrPx);
        finishTouch(FTEST_PASS, detail);
        return;
      }
      showTouchTarget();
      return;
    }
  }
  if ((int32_t)(millis() - s_deadlineMs) >= 0) {
    finishTouch(FTEST_FAIL, "timeout");
  }
}

// ============================================================================
// BUZZER / SPEAKER
// ============================================================================
void beginBuzzer() {
  buzzerOn();
  s_deadlineMs = millis() + TONE_MS;
}
void serviceBuzzer() {
  if ((int32_t)(millis() - s_deadlineMs) < 0) return;
  buzzerOff();
  s_localPhase = LocalPhase::None;
  s_askKind = AskKind::Buzzer;
  s_deadlineMs = millis() + LOCAL_ASK_TIMEOUT_MS;
  renderAll();
}

void beginSpeaker() {
  speakerOn();
  s_deadlineMs = millis() + TONE_MS;
}
void serviceSpeaker() {
  if ((int32_t)(millis() - s_deadlineMs) < 0) return;
  speakerOff();
  s_localPhase = LocalPhase::None;
  s_askKind = AskKind::Speaker;
  s_deadlineMs = millis() + LOCAL_ASK_TIMEOUT_MS;
  renderAll();
}

// ============================================================================
// WIFI
// ============================================================================
void beginWifi() {
  if (WIFIIsConnected()) {
    char detail[FTEST_DETAIL_MAX + 1];
    snprintf(detail, sizeof(detail), "%s %ddBm", WiFi.macAddress().c_str(),
            (int)WiFi.RSSI());
    finishLocal(FTEST_PASS, detail);
    return;
  }
  WiFi.scanNetworks(true);
  s_deadlineMs = millis() + WIFI_SCAN_TIMEOUT_MS;
}

void serviceWifi() {
  const int16_t n = WiFi.scanComplete();
  if (n >= 0) {
    char detail[FTEST_DETAIL_MAX + 1];
    snprintf(detail, sizeof(detail), "%s %dred", WiFi.macAddress().c_str(),
            (int)n);
    WiFi.scanDelete();
    finishLocal(n >= 1 ? FTEST_PASS : FTEST_FAIL, detail);
    return;
  }
  if (n == WIFI_SCAN_FAILED || (int32_t)(millis() - s_deadlineMs) >= 0) {
    WiFi.scanDelete();
    char detail[FTEST_DETAIL_MAX + 1];
    snprintf(detail, sizeof(detail), "%s timeout", WiFi.macAddress().c_str());
    finishLocal(FTEST_FAIL, detail);
  }
}

// ============================================================================
// Secuencia local
// ============================================================================
void beginLocalTest() {
  RowData &r = s_rows[s_localIdx];
  r.started = true;
  r.status = FTEST_RUNNING;
  r.detail[0] = '\0';
  s_localPhase = LocalPhase::None;
  s_lastTouchedRow = s_localIdx;

  switch (kLocalOrder[s_localIdx]) {
    case FTEST_HMI_SYSINFO: runSysInfo(); return;
    case FTEST_HMI_I2C:     runI2c();     return;
    case FTEST_HMI_NVS:     runNvs();     return;
    case FTEST_HMI_LINK:    runLink();    return;
    case FTEST_HMI_PANEL:
      beginPanel();
      s_localPhase = LocalPhase::Panel;
      break;
    case FTEST_HMI_TOUCH:
      beginTouch();
      s_localPhase = LocalPhase::Touch;
      break;
    case FTEST_HMI_BUZZER:
      beginBuzzer();
      s_localPhase = LocalPhase::Buzzer;
      break;
    case FTEST_HMI_SPEAKER:
      beginSpeaker();
      s_localPhase = LocalPhase::Speaker;
      break;
    case FTEST_HMI_WIFI:
      beginWifi();
      s_localPhase = LocalPhase::Wifi;
      break;
    default:
      finishLocal(FTEST_SKIP, "?");
      return;
  }
  renderAll();
}

void finishLocal(FtestStatus st, const char *detail) {
  RowData &r = s_rows[s_localIdx];
  r.status = st;
  snprintf(r.detail, sizeof(r.detail), "%s", detail ? detail : "");
  s_localPhase = LocalPhase::None;

  if (s_retryMode) {
    s_retryMode = false;
    goSummary();
    return;
  }
  s_localIdx++;
  if (s_localIdx >= kLocalCount) {
    startRemote();
    return;
  }
  beginLocalTest();
}

void resolveAsk(bool yes) {
  s_askKind = AskKind::None;
  finishLocal(yes ? FTEST_PASS : FTEST_FAIL, "");
}

void retryLocalTest(unsigned id) {
  for (int i = 0; i < kLocalCount; i++) {
    if ((unsigned)kLocalOrder[i] == id) {
      s_localIdx = i;
      s_retryMode = true;
      s_step = Step::LocalSeq;
      beginLocalTest();
      return;
    }
  }
}

// ============================================================================
// Remoto (motherBoard)
// ============================================================================
int drainFtestEvents() {
  int n = 0;
  FtestResult res;
  while (FactoryTest_TakeEvent(&res)) {
    n++;
    s_mbSupported = true;
    const int idx = mbRowIndex(res.id);
    RowData &r = s_rows[idx];
    r.started = true;
    r.status = res.status;
    snprintf(r.detail, sizeof(r.detail), "%s", res.detail);
    s_lastTouchedRow = idx;
    if (s_step == Step::RemoteAwaitFirst) s_step = Step::RemoteRunning;
    if (res.status != FTEST_CONFIRM && s_remoteConfirmAnsweredRowId == res.id) {
      s_remoteConfirmAnsweredRowId = FTEST_ID_NONE;
    }
  }
  return n;
}

void startRemote() {
  Communication_SendFtestStart();
  s_step = Step::RemoteAwaitFirst;
  s_deadlineMs = millis() + FTEST_MB_RESPONSE_TIMEOUT_MS;
  renderAll();
}

void retryRemote(unsigned id) {
  const int idx = mbRowIndex(id);
  RowData &r = s_rows[idx];
  r.started = true;
  r.status = FTEST_RUNNING;
  r.detail[0] = '\0';
  s_lastTouchedRow = idx;
  Communication_SendFtestRun((uint8_t)id);
  s_step = Step::RemoteRunning;
  renderAll();
}

void onRetryClicked(lv_event_t *e) {
  auto *row = (RowData *)lv_event_get_user_data(e);
  if (!row) return;
  if (row->isMb) {
    retryRemote(row->id);
  } else {
    retryLocalTest(row->id);
  }
}

void persistResults() {
  uint32_t passMask = 0, failMask = 0;
  for (int i = 0; i < kLocalCount; i++) {
    const unsigned bit = s_rows[i].id - (unsigned)FTEST_HMI_BASE;
    if (s_rows[i].status == FTEST_PASS) passMask |= (1u << bit);
    else if (s_rows[i].status == FTEST_FAIL) failMask |= (1u << bit);
  }
  const uint32_t epoch = HMI_GetEpochNow();
  const unsigned mbPass = s_mbDone ? s_mbPass : 0u;
  const unsigned mbFail = s_mbDone ? s_mbFail : 0u;
  const unsigned mbSkip = s_mbDone ? s_mbSkip : 0u;
  char fwVer[sizeof(ctrl_state_msg.fwVer)];
  snprintf(fwVer, sizeof(fwVer), "%s", ctrl_state_msg.fwVer);

  // Fuera de LVGL_Lock(): escritura de Preferences (mismo motivo que el resto
  // de escrituras periodicas de NVS de UITask.cpp).
  LVGL_Unlock();
  Preferences p;
  p.begin(HMI_NS_FTEST, false);
  p.putUInt(HMI_KEY_FTEST_EPOCH, epoch);
  p.putUInt(HMI_KEY_FTEST_PASSMASK, passMask);
  p.putUInt(HMI_KEY_FTEST_FAILMASK, failMask);
  p.putUInt(HMI_KEY_FTEST_MBPASS, mbPass);
  p.putUInt(HMI_KEY_FTEST_MBFAIL, mbFail);
  p.putUInt(HMI_KEY_FTEST_MBSKIP, mbSkip);
  p.putString(HMI_KEY_FTEST_FWVER, fwVer);
  p.end();
  LVGL_Lock();
}

void goSummary() {
  destroyTransientOverlayObjects();
  s_askKind = AskKind::None;
  s_localPhase = LocalPhase::None;
  s_step = Step::Summary;
  persistResults();
  renderAll();
}

void serviceRemoteAwaitFirst() {
  const int n = drainFtestEvents();
  if (n > 0) renderAll();
  if (s_step != Step::RemoteAwaitFirst) return;  // ya paso a RemoteRunning

  if (g_pendingFtestReject) {
    g_pendingFtestReject = false;
    s_mbRejected = true;
    s_mbRejectReason = (FtestReject)g_ftestRejectReason;
    goSummary();
    return;
  }
  if ((int32_t)(millis() - s_deadlineMs) >= 0) {
    s_mbUnsupported = true;
    goSummary();
  }
}

void serviceRemoteRunning() {
  const int n = drainFtestEvents();

  if (g_pendingFtestReject) {
    g_pendingFtestReject = false;
    s_mbRejected = true;
    s_mbRejectReason = (FtestReject)g_ftestRejectReason;
    goSummary();
    return;
  }
  if (g_pendingFtestDone) {
    g_pendingFtestDone = false;
    s_mbPass = g_ftestDonePass;
    s_mbFail = g_ftestDoneFail;
    s_mbSkip = g_ftestDoneSkip;
    s_mbDone = true;
    goSummary();
    return;
  }
  if (n > 0) renderAll();
}

// ============================================================================
// Ciclo de vida
// ============================================================================
void resetState() {
  s_rowCount = 0;
  for (int i = 0; i < kLocalCount; i++) {
    RowData &r = s_rows[s_rowCount++];
    r.id = kLocalOrder[i];
    r.isMb = false;
    r.started = false;
    r.status = FTEST_RUNNING;
    r.detail[0] = '\0';
  }
  s_lastTouchedRow = -1;
  s_askKind = AskKind::None;
  s_retryMode = false;
  s_localPhase = LocalPhase::None;
  s_localIdx = 0;
  s_mbSupported = false;
  s_mbUnsupported = false;
  s_mbRejected = false;
  s_mbDone = false;
  s_mbPass = s_mbFail = s_mbSkip = 0;
  s_remoteConfirmAnsweredRowId = FTEST_ID_NONE;
  destroyTransientOverlayObjects();
}

void onExitClicked(lv_event_t *) { FactoryTest_Close(); }

}  // namespace

void FactoryTest_Init(void) {
  // lv_layer_top(): mismo criterio que AlarmCenter — accesible sin depender
  // de que ninguna pantalla concreta lo tenga instanciado. Solo se abre desde
  // el splash, pero vivir en la capa superior evita tener que reparentarlo.
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
  lv_obj_set_size(s_card, 760, 440);
  lv_obj_center(s_card);
  lv_obj_set_style_radius(s_card, 12, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_card, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_clear_flag(s_card, LV_OBJ_FLAG_SCROLLABLE);

  s_titleLbl = lv_label_create(s_card);
  lv_label_set_text(s_titleLbl, "TEST DE FABRICA");
  lv_obj_set_style_text_font(s_titleLbl, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(s_titleLbl, lv_color_hex(0x0B2E4F), 0);
  lv_obj_align(s_titleLbl, LV_ALIGN_TOP_MID, 0, 8);

  lv_obj_t *exitBtn = makeBtn(s_card, TXT("SALIR", "EXIT", "QUITTER"),
                              onExitClicked, lv_color_hex(0xAA3333));
  lv_obj_set_size(exitBtn, 110, 40);
  lv_obj_align(exitBtn, LV_ALIGN_TOP_RIGHT, -8, 6);
  s_exitBtnLabel = lv_obj_get_child(exitBtn, 0);

  // Lista de filas: contenedor flex-columna SIN limpiar LV_OBJ_FLAG_SCROLLABLE
  // (lo trae por defecto lv_obj_create) para que quepan los 9 locales + hasta
  // 30 de motherBoard sin rediseñar el layout.
  s_body = lv_obj_create(s_card);
  lv_obj_remove_style_all(s_body);
  lv_obj_set_size(s_body, 720, 250);
  lv_obj_align(s_body, LV_ALIGN_TOP_MID, 0, 48);
  lv_obj_set_flex_flow(s_body, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(s_body, 6, 0);

  s_action = lv_obj_create(s_card);
  lv_obj_remove_style_all(s_action);
  lv_obj_set_size(s_action, 720, 110);
  lv_obj_align(s_action, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_clear_flag(s_action, LV_OBJ_FLAG_SCROLLABLE);
}

void FactoryTest_Open(void) {
  if (!s_overlay || s_step != Step::Closed) return;
  resetState();
  s_step = Step::LocalSeq;
  lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(s_overlay);
  beginLocalTest();
}

void FactoryTest_Close(void) {
  if (s_step == Step::Closed) {
    g_factoryTestRequested = false;
    return;
  }
  const bool remoteBusy =
      (s_step == Step::RemoteAwaitFirst || s_step == Step::RemoteRunning);
  if (remoteBusy) {
    Communication_SendFtestAbort();
  }
  destroyTransientOverlayObjects();
  LVGL_Unlock();
  buzzerOff();
  speakerOff();
  LVGL_Lock();
  if (s_overlay) lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  s_step = Step::Closed;
  g_factoryTestRequested = false;
  lv_scr_load(ui_ScreenMain);
}

bool FactoryTest_IsOpen(void) { return s_step != Step::Closed; }

void FactoryTest_Poll(void) {
  if (s_step == Step::Closed) return;

  if (s_askKind != AskKind::None) {
    if ((int32_t)(millis() - s_deadlineMs) >= 0) {
      resolveAsk(false);  // Las preguntas expiran a los 60s como FALLA.
    }
    return;
  }

  switch (s_step) {
    case Step::LocalSeq:
      switch (s_localPhase) {
        case LocalPhase::Panel:   servicePanel();   break;
        case LocalPhase::Touch:   serviceTouch();   break;
        case LocalPhase::Buzzer:  serviceBuzzer();  break;
        case LocalPhase::Speaker: serviceSpeaker(); break;
        case LocalPhase::Wifi:    serviceWifi();    break;
        default: break;
      }
      break;
    case Step::RemoteAwaitFirst: serviceRemoteAwaitFirst(); break;
    case Step::RemoteRunning:    serviceRemoteRunning();    break;
    default: break;
  }
}

void FactoryTest_ApplyLanguage(void) {
  if (s_exitBtnLabel) {
    lv_label_set_text(s_exitBtnLabel, TXT("SALIR", "EXIT", "QUITTER"));
  }
  if (s_step != Step::Closed) renderAll();
}

bool FactoryTest_AudioBusy(void) {
  return s_step == Step::LocalSeq && s_localPhase == LocalPhase::Buzzer;
}
