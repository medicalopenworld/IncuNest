#include "ui/FactoryTest.h"

#include <Wire.h>
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
// exacto en el que se ejecutan. shared-factory-test-bench retira PANEL y
// TOUCH de esta secuencia (feedback de banco: alargaban la bateria sin
// aportar en la linea de montaje). Sus IDs siguen vivos en factory_test.h,
// esta placa simplemente ya no los usa. kLocalCount YA NO coincide con
// FTEST_HMI_END - FTEST_HMI_BASE (esa cuenta, de la tabla compartida, sigue
// siendo 9).
constexpr FtestId kLocalOrder[] = {
    FTEST_HMI_SYSINFO, FTEST_HMI_I2C,     FTEST_HMI_BUZZER,
    FTEST_HMI_SPEAKER, FTEST_HMI_WIFI,    FTEST_HMI_NVS,
    FTEST_HMI_LINK,
};
constexpr int kLocalCount = sizeof(kLocalOrder) / sizeof(kLocalOrder[0]);
constexpr int kMaxRows = kLocalCount + (int)FTEST_MB_COUNT;

// ---------------------------------------------------------------------------
// Estado de una fila (local o de motherBoard). Direccion estable mientras
// dure la pantalla abierta: los locales viven en s_rows[0..kLocalCount-1] en
// el orden de kLocalOrder y no se mueven; los de motherBoard se anaden a
// partir de s_rows[kLocalCount] segun van llegando sus CTRL,FTEST.
// btn/titleLbl/wordLbl: objetos LVGL de la fila, creados UNA vez
// (createRowButton()) y actualizados in-place (updateRowButton()) — design.md
// D4, hallazgo de code review: reconstruir 36 filas x 3 objetos en cada
// renderAll() era exactamente el churn que D4 queria evitar. nullptr hasta
// que createRowButton() la pinta por primera vez. dirty se pone en los mismos
// puntos que antes ponian s_rowsDirty (una mutacion de estado real) y
// updateRowButton() lo consume: sin el, cada renderAll() reescribiria texto y
// color aunque nada hubiera cambiado.
struct RowData {
  unsigned    id;
  bool        isMb;
  bool        started;
  FtestStatus status;
  char        detail[FTEST_DETAIL_MAX + 1];
  lv_obj_t   *btn;
  lv_obj_t   *titleLbl;
  lv_obj_t   *wordLbl;
  bool        dirty;
};

RowData s_rows[kMaxRows];
int     s_rowCount = 0;
int     s_lastTouchedRow = -1;  // para lv_obj_scroll_to_view()
int     s_prevTouchedRow = -1;  // fila ya centrada: evita re-scrollear igual

// Orden de pintado de la cuadricula (indices en s_rows), recalculado SOLO
// cuando algun estado cambia (feedback de banco: reordenar en cada pasada
// hace que la cuadricula salte bajo el dedo del operario). FALLA -> AVISO ->
// en curso (pendiente/RUNNING/WAIT/CONFIRM) -> PASA; SKIP no entra en el
// orden (fila oculta, tampoco cuenta en el resumen visible).
int  s_order[kMaxRows];
int  s_orderCount = 0;
// Ultimo orden efectivamente aplicado a los objetos LVGL (lv_obj_move_to_index()):
// permite reordenar solo cuando computeOrder() cambia algo de verdad.
int  s_prevOrder[kMaxRows];
int  s_prevOrderCount = 0;
bool s_rowsDirty = true;

enum class Step {
  Closed,
  Gate,
  LocalSeq,
  RemoteAwaitFirst,
  RemoteRunning,
  Summary,
};

enum class LocalPhase { None, Buzzer, Speaker, Wifi };
enum class AskKind { None, Buzzer, Speaker };

// Respuesta si/no pendiente de resolver en FactoryTest_Poll(). Los callbacks
// de LVGL (dispatch de evento) solo escriben aqui: nunca resuelven en el
// mismo callback, para no soltar LVGL_Lock() / destruir objetos (incluido el
// boton que esta despachando su propio click) desde dentro del despacho.
enum class Answer { None, Yes, No };

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
bool     s_mbLinkLost = false;
unsigned s_mbPass = 0, s_mbFail = 0, s_mbSkip = 0, s_mbWarn = 0;
unsigned s_remoteConfirmAnsweredRowId = FTEST_ID_NONE;

// Hand-off de UI (hallazgo 4): todo boton (Si/No local, Si/No confirmacion
// remota, Reintentar, Salir, Si/No de la barrera de entrada, abrir/cerrar el
// panel de detalle) solo marca aqui su intencion. Poll() la consume en la
// siguiente pasada, fuera de cualquier despacho de evento LVGL.
Answer    s_pendingLocalAsk = Answer::None;
Answer    s_pendingRemoteConfirm = Answer::None;
RowData  *s_pendingRemoteConfirmRow = nullptr;
RowData  *s_pendingRetryRow = nullptr;
bool      s_exitRequested = false;
Answer    s_pendingGateAnswer = Answer::None;
RowData  *s_pendingDetailRow = nullptr;  // fila cuyo detalle se pide abrir
bool      s_pendingDetailClose = false;

constexpr uint32_t TONE_MS = 300;
constexpr uint32_t LOCAL_ASK_TIMEOUT_MS = 60000;
constexpr uint32_t WIFI_SCAN_TIMEOUT_MS = 15000;
// Hallazgo 5: sin ningun CTRL,FTEST* durante este tiempo en RemoteRunning se
// asume enlace perdido. Hallazgo 6: tope de inactividad en Summary, mismo
// precedente que HELP_IDLE_TIMEOUT_MS (3 min) pero mas largo porque aqui el
// usuario puede estar leyendo una lista larga de resultados.
constexpr uint32_t REMOTE_SILENCE_TIMEOUT_MS = 120000;
constexpr uint32_t SUMMARY_IDLE_TIMEOUT_MS = 600000;

lv_obj_t *s_overlay = nullptr;
lv_obj_t *s_card = nullptr;
lv_obj_t *s_titleLbl = nullptr;
lv_obj_t *s_exitBtnLabel = nullptr;
lv_obj_t *s_body = nullptr;    // cuadricula de botones, scroll vertical
lv_obj_t *s_action = nullptr;  // instruccion / pregunta / resumen

lv_obj_t *s_detailPanel = nullptr;  // panel de detalle modal, o nullptr
RowData  *s_detailRow = nullptr;    // fila cuyo detalle esta abierto

// ---------------------------------------------------------------------------
// Forward declarations (la maquina de estados es mutuamente recursiva:
// finishLocal() encadena con beginLocalTest() del siguiente test).
void beginLocalTest();
void beginLocalSequence();
void finishLocal(FtestStatus st, const char *detail);
void renderAll();
void resolveAsk(bool yes);
void goSummary();
void startRemote();
void markMbPendingAsLinkLost();
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
// Nombres, descripciones e instrucciones traducidos. ftest_id_key() (shared/)
// es el fallback para un id de motherBoard que esta version del display
// todavia no conoce por nombre (tabla ampliada en el futuro sin romper
// compatibilidad).
const char *localTestName(unsigned id) {
  switch (id) {
    case FTEST_HMI_SYSINFO: return TXT("Info sistema", "System info", "Info systeme");
    case FTEST_HMI_I2C:     return TXT("Bus I2C", "I2C bus", "Bus I2C");
    case FTEST_HMI_BUZZER:  return TXT("Zumbador", "Buzzer", "Buzzer");
    case FTEST_HMI_SPEAKER: return TXT("Altavoz", "Speaker", "Haut-parleur");
    case FTEST_HMI_WIFI:    return TXT("WiFi", "WiFi", "WiFi");
    case FTEST_HMI_NVS:     return TXT("Memoria NVS", "NVS memory", "Memoire NVS");
    case FTEST_HMI_LINK:    return TXT("Enlace placa", "Board link", "Liaison carte");
    default: return ftest_id_key(id);
  }
}

// Descripcion de una linea para el panel de detalle: que comprueba cada test
// local.
const char *localTestDesc(unsigned id) {
  switch (id) {
    case FTEST_HMI_SYSINFO:
      return TXT("Flash, PSRAM y memoria libre del display",
                 "Display flash, PSRAM and free heap",
                 "Flash, PSRAM et memoire libre de l'ecran");
    case FTEST_HMI_I2C:
      return TXT("Bus I2C del display (tactil y retroiluminacion)",
                 "Display I2C bus (touch and backlight)",
                 "Bus I2C de l'ecran (tactile et retroeclairage)");
    case FTEST_HMI_BUZZER:
      return TXT("Zumbador del display", "Display buzzer",
                 "Buzzer de l'ecran");
    case FTEST_HMI_SPEAKER:
      return TXT("Altavoz del display", "Display speaker",
                 "Haut-parleur de l'ecran");
    case FTEST_HMI_WIFI:
      return TXT("Conexion WiFi del display", "Display WiFi connection",
                 "Connexion WiFi de l'ecran");
    case FTEST_HMI_NVS:
      return TXT("Memoria NVS del display", "Display NVS memory",
                 "Memoire NVS de l'ecran");
    case FTEST_HMI_LINK:
      return TXT("Enlace serie con la placa", "Serial link to the board",
                 "Liaison serie avec la carte");
    default: return "?";
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
    case FTEST_MB_SENSORBOARD: return TXT("Sensor cabina", "Cabin sensor", "Capteur cabine");
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

// Descripcion de una linea para el panel de detalle: que comprueba cada test
// de motherBoard.
const char *mbTestDesc(unsigned id) {
  switch (id) {
    case FTEST_MB_SYSINFO:
      return TXT("Version y memoria de la placa", "Board firmware and memory",
                 "Version et memoire de la carte");
    case FTEST_MB_INA3221:
      return TXT("Lee las corrientes del sensor INA3221",
                 "Reads currents from the INA3221 sensor",
                 "Lit les courants du capteur INA3221");
    case FTEST_MB_STANDBY:
      return TXT("Consumo electrico en reposo", "Standby power consumption",
                 "Consommation electrique en veille");
    case FTEST_MB_CHARGER:
      return TXT("Estado del cargador de bateria", "Battery charger status",
                 "Etat du chargeur de batterie");
    case FTEST_MB_POWER_SRC:
      return TXT("Fuente de alimentacion activa (red o bateria)",
                 "Active power source (mains or battery)",
                 "Source d'alimentation active (secteur ou batterie)");
    case FTEST_MB_SKIN_ADC:
      return TXT("Lectura de la sonda de piel", "Skin probe reading",
                 "Lecture de la sonde de peau");
    case FTEST_MB_EXT_SHT4X:
      return TXT("Sensor de temperatura y humedad ambiente",
                 "Ambient temperature and humidity sensor",
                 "Capteur de temperature et humidite ambiante");
    case FTEST_MB_SENSORBOARD:
      return TXT("Sensor de cabina por USB o I2C",
                 "Cabin sensor over USB or I2C",
                 "Capteur de cabine par USB ou I2C");
    case FTEST_MB_SB_STATUS:
      return TXT("Estado general del SensorBoard",
                 "SensorBoard overall status",
                 "Etat general de la carte SensorBoard");
    case FTEST_MB_SB_ENV:
      return TXT("Ambiente de cabina del SensorBoard",
                 "SensorBoard cabin environment",
                 "Environnement de cabine du SensorBoard");
    case FTEST_MB_SB_DOOR:
      return TXT("Sensor de puerta abierta/cerrada",
                 "Open/closed door sensor",
                 "Capteur de porte ouverte/fermee");
    case FTEST_MB_SB_LIGHT:
      return TXT("Sensor de luz ambiente", "Ambient light sensor",
                 "Capteur de lumiere ambiante");
    case FTEST_MB_SB_CAMERA:
      return TXT("Camara del SensorBoard", "SensorBoard camera",
                 "Camera du SensorBoard");
    case FTEST_MB_ACTUATORS:
      return TXT("Actuadores en lazo abierto", "Actuators in open loop",
                 "Actionneurs en boucle ouverte");
    case FTEST_MB_FAN_RPM:
      return TXT("Revoluciones del ventilador", "Fan RPM",
                 "Tours du ventilateur");
    case FTEST_MB_HUMID_USB:
      return TXT("Humidificador por USB", "USB humidifier",
                 "Humidificateur USB");
    case FTEST_MB_BUZZER:
      return TXT("Zumbador de la placa", "Board buzzer",
                 "Buzzer de la carte");
    case FTEST_MB_AFE_SPI:
      return TXT("Enlace SPI con el AFE", "SPI link to the AFE",
                 "Liaison SPI avec l'AFE");
    case FTEST_MB_AFE_PROBE:
      return TXT("Sonda de SpO2 conectada", "SpO2 probe connected",
                 "Sonde SpO2 connectee");
    case FTEST_MB_HMI_LINK:
      return TXT("Enlace serie con el display", "Serial link to the display",
                 "Liaison serie avec l'ecran");
    case FTEST_MB_GSM_AT:
      return TXT("El modem GSM responde a comandos AT",
                 "GSM modem responds to AT commands",
                 "Le modem GSM repond aux commandes AT");
    case FTEST_MB_GSM_SIM:
      return TXT("Tarjeta SIM lista", "SIM card ready", "Carte SIM prete");
    case FTEST_MB_GSM_SIGNAL:
      return TXT("Nivel de senal GSM", "GSM signal level",
                 "Niveau de signal GSM");
    case FTEST_MB_GSM_NET:
      return TXT("Registro en la red movil", "Mobile network registration",
                 "Enregistrement sur le reseau mobile");
    case FTEST_MB_WIFI:
      return TXT("Conexion WiFi de la placa", "Board WiFi connection",
                 "Connexion WiFi de la carte");
    case FTEST_MB_TB_PROVISION:
      return TXT("Aprovisionamiento en ThingsBoard",
                 "ThingsBoard provisioning", "Provisionnement ThingsBoard");
    case FTEST_MB_TIME:
      return TXT("Hora sincronizada por red", "Time synced from network",
                 "Heure synchronisee par le reseau");
    case FTEST_MB_NVS:
      return TXT("Memoria NVS de la placa", "Board NVS memory",
                 "Memoire NVS de la carte");
    case FTEST_MB_LITTLEFS:
      return TXT("Sistema de archivos LittleFS", "LittleFS filesystem",
                 "Systeme de fichiers LittleFS");
    default: return "?";
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

// Barrera de entrada (hallazgo 7): se muestra en Step::Gate antes de arrancar
// ningun test, local o remoto.
const char *gateWarningText() {
  return TXT(
      "ATENCION: el equipo debe estar VACIO, sin paciente. Los actuadores se "
      "encenderan en lazo abierto. Continuar?",
      "WARNING: the unit must be EMPTY, no patient inside. Actuators will be "
      "switched on in open loop. Continue?",
      "ATTENTION : l'appareil doit etre VIDE, sans patient. Les actionneurs "
      "seront allumes en boucle ouverte. Continuer ?");
}

const char *currentAskQuestion() {
  switch (s_askKind) {
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
const char *statusText(FtestStatus st, bool started) {
  if (!started) return TXT("Pendiente", "Pending", "En attente");
  switch (st) {
    case FTEST_RUNNING: return TXT("En curso", "Running", "En cours");
    case FTEST_PASS:    return TXT("PASA", "PASS", "REUSSI");
    case FTEST_FAIL:    return TXT("FALLA", "FAIL", "ECHEC");
    case FTEST_SKIP:    return TXT("OMITIDO", "SKIPPED", "OMIS");
    case FTEST_WAIT:    return TXT("ESPERA", "WAIT", "ATTENTE");
    case FTEST_CONFIRM: return TXT("CONFIRMAR", "CONFIRM", "CONFIRMER");
    case FTEST_WARN:    return TXT("AVISO", "WARN", "AVIS");
    default: return "?";
  }
}

// Grupo de color de la cuadricula (feedback de banco): FALLA, AVISO, en curso
// (pendiente/RUNNING/WAIT/CONFIRM) y PASA. SKIP no tiene grupo: se filtra en
// computeOrder() antes de llegar aqui.
int bucketOf(const RowData &r) {
  if (!r.started) return 2;  // pendiente -> igual que "en curso"
  switch (r.status) {
    case FTEST_FAIL: return 0;
    case FTEST_WARN: return 1;
    case FTEST_PASS: return 3;
    default:         return 2;  // RUNNING, WAIT, CONFIRM
  }
}

void gridColors(const RowData &r, lv_color_t *fill, lv_color_t *border) {
  switch (bucketOf(r)) {
    case 0: *fill = lv_color_hex(0xFFE0E4); *border = lv_color_hex(0xD5283C); break;
    case 1: *fill = lv_color_hex(0xFFF0D6); *border = lv_color_hex(0xC98A00); break;
    case 3: *fill = lv_color_hex(0xDFF3FF); *border = lv_color_hex(0x2196C4); break;
    default: *fill = lv_color_hex(0xFFFFFF); *border = lv_color_hex(0x0B2E4F); break;
  }
}

// ---------------------------------------------------------------------------
// Filas de motherBoard: se anaden bajo demanda segun llegan sus CTRL,FTEST
// (spec: "anadir una fila por cada CTRL,FTEST recibido"). Los locales ya
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
  s_rows[i].btn = nullptr;
  s_rows[i].titleLbl = nullptr;
  s_rows[i].wordLbl = nullptr;
  // Fila nueva: createRowButton() la crea desde renderAll() con su primer
  // evento (hallazgo de code review: "se anaden dinamicamente").
  s_rows[i].dirty = true;
  return i;
}

// Destruye los botones de fila (local + motherBoard) de la sesion en curso.
// Idempotente (cada fila pone su btn a nullptr tras borrarlo): FactoryTest_
// Close() y resetState() (siguiente Open()) pueden llamarla sin doble-borrado.
void destroyRowButtons() {
  for (int i = 0; i < s_rowCount; i++) {
    RowData &r = s_rows[i];
    if (r.btn) {
      lv_obj_del(r.btn);
      r.btn = nullptr;
      r.titleLbl = nullptr;
      r.wordLbl = nullptr;
    }
  }
}

// Cierra el panel de detalle si estaba abierto. Se llama al cerrar el
// overlay y al reabrirlo — mismo punto donde antes se destruian el
// rectangulo de color del panel y el capturador de toques (retirados,
// feedback de banco).
void destroyTransientOverlayObjects() {
  if (s_detailPanel) { lv_obj_del(s_detailPanel); s_detailPanel = nullptr; }
  s_detailRow = nullptr;
  s_pendingDetailRow = nullptr;
  s_pendingDetailClose = false;
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

// Hand-off puro (hallazgo 4): resolveAsk() encadena con finishLocal(), que
// puede llegar a runI2c()/runNvs()/persistResults() (sueltan LVGL_Lock()) y a
// renderAll() -> lv_obj_clean(s_action), que destruiria el boton que esta
// despachando este mismo evento. Poll() es quien llama a resolveAsk().
void onAskYesCb(lv_event_t *) { s_pendingLocalAsk = Answer::Yes; }
void onAskNoCb(lv_event_t *) { s_pendingLocalAsk = Answer::No; }

// Idem: Communication_SendFtestConfirm() + renderAll() se difieren a Poll().
void onRemoteConfirmYes(lv_event_t *e) {
  auto *row = (RowData *)lv_event_get_user_data(e);
  if (!row) return;
  s_pendingRemoteConfirmRow = row;
  s_pendingRemoteConfirm = Answer::Yes;
}
void onRemoteConfirmNo(lv_event_t *e) {
  auto *row = (RowData *)lv_event_get_user_data(e);
  if (!row) return;
  s_pendingRemoteConfirmRow = row;
  s_pendingRemoteConfirm = Answer::No;
}

// Barrera de entrada (hallazgo 7): mismo hand-off.
void onGateYesCb(lv_event_t *) { s_pendingGateAnswer = Answer::Yes; }
void onGateNoCb(lv_event_t *) { s_pendingGateAnswer = Answer::No; }

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

void renderGateAction() {
  renderAskUi(gateWarningText(), onGateYesCb, onGateNoCb, nullptr);
}

void renderLocalAction() {
  if (s_askKind != AskKind::None) {
    renderAskUi(currentAskQuestion(), onAskYesCb, onAskNoCb, nullptr);
    return;
  }
  switch (s_localPhase) {
    case LocalPhase::Wifi:
      renderCenteredText(TXT("Buscando redes WiFi...",
                             "Scanning WiFi networks...",
                             "Recherche de reseaux WiFi..."));
      break;
    default:
      break;  // Buzzer/Speaker (tono) y tests instantaneos: sin contenido en
              // la zona de accion mientras corren.
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

// Cabecera de resumen (spec hmi-factory-test: "N errores / M avisos / K OK",
// sin omitidos). Cuenta local + motherBoard juntos; los tests locales nunca
// producen AVISO (solo el estimulo/conectividad de la motherBoard lo hace).
void renderSummary() {
  int localPass = 0, localFail = 0;
  for (int i = 0; i < kLocalCount; i++) {
    if (s_rows[i].status == FTEST_PASS) localPass++;
    else if (s_rows[i].status == FTEST_FAIL) localFail++;
  }
  const unsigned mbPass = s_mbDone ? s_mbPass : 0u;
  const unsigned mbFail = s_mbDone ? s_mbFail : 0u;
  const unsigned mbWarn = s_mbDone ? s_mbWarn : 0u;
  const int totalFail = localFail + (int)mbFail;
  const int totalWarn = (int)mbWarn;
  const int totalPass = localPass + (int)mbPass;

  char header[80];
  snprintf(header, sizeof(header),
          TXT("%d errores - %d avisos - %d OK",
              "%d errors - %d warnings - %d OK",
              "%d erreurs - %d avis - %d OK"),
          totalFail, totalWarn, totalPass);

  char buf[192];
  if (s_mbLinkLost) {
    snprintf(buf, sizeof(buf), "%s\n%s", header,
            TXT("Placa: enlace perdido durante el test",
                "Board: link lost during test",
                "Carte : liaison perdue pendant le test"));
  } else if (s_mbUnsupported) {
    snprintf(buf, sizeof(buf), "%s\n%s", header,
            TXT("Placa: sin soporte", "Board: no support",
                "Carte : non supportee"));
  } else if (s_mbRejected) {
    snprintf(buf, sizeof(buf), "%s\n%s: %s", header,
            TXT("Placa", "Board", "Carte"),
            rejectReasonText(s_mbRejectReason));
  } else {
    snprintf(buf, sizeof(buf), "%s", header);
  }
  renderCenteredText(buf);
}

// ============================================================================
// Cuadricula de resultados
// ============================================================================
constexpr int kGridBtnW = 232;
constexpr int kGridBtnH = 66;

// computeOrder() recalcula el orden de pintado (FALLA -> AVISO -> en curso ->
// PASA, SKIP oculto) SOLO cuando s_rowsDirty esta marcado por una mutacion de
// estado real (ver beginLocalTest/finishLocal/drainFtestEvents/
// markMbPendingAsLinkLost/retryRemote). Recorre por grupo en vez de ordenar
// para evitar tirar de <algorithm> por un caso tan simple.
void computeOrder() {
  int buckets[4][kMaxRows];
  int counts[4] = {0, 0, 0, 0};
  for (int i = 0; i < s_rowCount; i++) {
    if (s_rows[i].started && s_rows[i].status == FTEST_SKIP) continue;
    const int b = bucketOf(s_rows[i]);
    buckets[b][counts[b]++] = i;
  }
  s_orderCount = 0;
  for (int b = 0; b < 4; b++) {
    for (int k = 0; k < counts[b]; k++) {
      s_order[s_orderCount++] = buckets[b][k];
    }
  }
}

// Hand-off puro (hallazgo 4): abrir el panel de detalle se resuelve en
// Poll(), nunca en el despacho de este click.
void onRowBtnClicked(lv_event_t *e) {
  auto *row = (RowData *)lv_event_get_user_data(e);
  if (!row) return;
  s_pendingDetailRow = row;
}

// Actualiza texto/color/visibilidad de una fila YA creada, solo si cambio su
// estado/detail desde la ultima pasada (r.dirty, puesto en los mismos sitios
// que antes ponian s_rowsDirty). SKIP se oculta con LV_OBJ_FLAG_HIDDEN en vez
// de no crearse (no participa en computeOrder(), pero conserva su objeto).
void updateRowButton(int idx) {
  RowData &r = s_rows[idx];
  if (!r.btn || !r.dirty) return;
  r.dirty = false;

  if (r.started && r.status == FTEST_SKIP) {
    lv_obj_add_flag(r.btn, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(r.btn, LV_OBJ_FLAG_HIDDEN);
  }

  // El nombre solo cambia con el idioma; FactoryTest_ApplyLanguage() fuerza
  // r.dirty = true en todas las filas para refrescarlo.
  lv_label_set_text(r.titleLbl, r.isMb ? mbTestName(r.id) : localTestName(r.id));

  lv_color_t fill, border;
  gridColors(r, &fill, &border);
  lv_obj_set_style_bg_color(r.btn, fill, LV_PART_MAIN);
  lv_obj_set_style_border_color(r.btn, border, LV_PART_MAIN);

  lv_label_set_text(r.wordLbl, statusText(r.status, r.started));
  lv_obj_set_style_text_color(r.wordLbl, border, 0);
}

// Crea los objetos de una fila (boton + 2 labels) UNA vez; renderAll() los
// actualiza in-place despues via updateRowButton() (design.md D4, hallazgo de
// code review: reconstruir la cuadricula entera en cada repintado era
// exactamente el churn que D4 queria evitar).
void createRowButton(int idx) {
  RowData &r = s_rows[idx];
  if (r.btn) return;

  lv_obj_t *btn = lv_btn_create(s_body);
  lv_obj_remove_style_all(btn);
  lv_obj_set_size(btn, kGridBtnW, kGridBtnH);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
  lv_obj_set_style_radius(btn, 6, LV_PART_MAIN);
  lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(btn, onRowBtnClicked, LV_EVENT_CLICKED, &r);

  lv_obj_t *title = lv_label_create(btn);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0x0B2E4F), 0);
  lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(title, kGridBtnW - 12);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

  lv_obj_t *word = lv_label_create(btn);
  lv_obj_set_style_text_font(word, &lv_font_montserrat_12, 0);
  lv_obj_align(word, LV_ALIGN_BOTTOM_MID, 0, -4);

  r.btn = btn;
  r.titleLbl = title;
  r.wordLbl = word;
  r.dirty = true;
  updateRowButton(idx);
}

// true si el orden de pintado (post computeOrder()) difiere del ultimo
// aplicado a los objetos LVGL: gatea lv_obj_move_to_index() para no reordenar
// en cada pasada (hallazgo de code review, design.md D4).
bool orderChanged(const int *a, int aCount, const int *b, int bCount) {
  if (aCount != bCount) return true;
  for (int i = 0; i < aCount; i++) {
    if (a[i] != b[i]) return true;
  }
  return false;
}

// true si alguna fila de motherBoard esta en WAIT (instruccion) o en CONFIRM
// sin responder (pregunta Si/No): en ambos casos hay algo que el operario
// debe atender YA en la zona de accion.
bool remoteNeedsAttention() {
  if (!s_mbSupported) return false;
  for (int i = kLocalCount; i < s_rowCount; i++) {
    if (s_rows[i].status == FTEST_WAIT) return true;
    if (s_rows[i].status == FTEST_CONFIRM &&
        s_remoteConfirmAnsweredRowId != s_rows[i].id) {
      return true;
    }
  }
  return false;
}

// Hallazgo de code review: el panel de detalle no debe abrirse si ya hay una
// pregunta Si/No pendiente de resolver (local o remota) — la taparia.
bool hasPendingYesNoQuestion() {
  return s_askKind != AskKind::None || remoteNeedsAttention();
}

// ============================================================================
// Panel de detalle (modal sobre la cuadricula)
// ============================================================================
void onDetailCloseClicked(lv_event_t *) { s_pendingDetailClose = true; }

void onDetailRetryClicked(lv_event_t *e) {
  auto *row = (RowData *)lv_event_get_user_data(e);
  if (!row) return;
  // Reutiliza el mismo hand-off de Reintentar que los botones de la
  // cuadricula ya no ofrecen: Poll() decide si es local o de motherBoard.
  s_pendingRetryRow = row;
  s_pendingDetailClose = true;
}

void buildDetailPanel(RowData &r) {
  const char *name = r.isMb ? mbTestName(r.id) : localTestName(r.id);
  const char *desc = r.isMb ? mbTestDesc(r.id) : localTestDesc(r.id);

  s_detailPanel = lv_obj_create(s_overlay);
  lv_obj_set_size(s_detailPanel, 560, 320);
  lv_obj_center(s_detailPanel);
  lv_obj_set_style_radius(s_detailPanel, 12, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_detailPanel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_clear_flag(s_detailPanel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_move_foreground(s_detailPanel);
  lv_obj_set_style_pad_all(s_detailPanel, 14, LV_PART_MAIN);
  lv_obj_set_flex_flow(s_detailPanel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_detailPanel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(s_detailPanel, 8, LV_PART_MAIN);

  lv_obj_t *title = lv_label_create(s_detailPanel);
  lv_label_set_text(title, name);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0x0B2E4F), 0);
  lv_obj_set_width(title, 500);
  lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t *descLbl = lv_label_create(s_detailPanel);
  lv_label_set_text(descLbl, desc);
  lv_obj_set_style_text_font(descLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(descLbl, lv_color_hex(0x0B2E4F), 0);
  lv_obj_set_width(descLbl, 500);
  lv_label_set_long_mode(descLbl, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(descLbl, LV_TEXT_ALIGN_CENTER, 0);

  char statusLine[96];
  if (r.detail[0]) {
    snprintf(statusLine, sizeof(statusLine), "%s (%s)",
            statusText(r.status, r.started), r.detail);
  } else {
    snprintf(statusLine, sizeof(statusLine), "%s",
            statusText(r.status, r.started));
  }
  lv_obj_t *statusLbl = lv_label_create(s_detailPanel);
  lv_label_set_text(statusLbl, statusLine);
  lv_obj_set_style_text_font(statusLbl, &lv_font_montserrat_16, 0);
  lv_color_t fill, border;
  gridColors(r, &fill, &border);
  lv_obj_set_style_text_color(statusLbl, border, 0);
  lv_obj_set_width(statusLbl, 500);
  lv_label_set_long_mode(statusLbl, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(statusLbl, LV_TEXT_ALIGN_CENTER, 0);

  // Reintentar SOLO en FALLA/AVISO, y solo con la bateria terminada
  // (Step::Summary): reintentar a mitad de la secuencia local o remota
  // interferiria con s_localIdx / el test en curso de esa fila.
  const bool canRetry = (s_step == Step::Summary) && r.started &&
                        (r.status == FTEST_FAIL || r.status == FTEST_WARN);

  lv_obj_t *btnRow = lv_obj_create(s_detailPanel);
  lv_obj_remove_style_all(btnRow);
  lv_obj_set_size(btnRow, 500, 50);
  lv_obj_clear_flag(btnRow, LV_OBJ_FLAG_SCROLLABLE);

  if (canRetry) {
    lv_obj_t *retryBtn = makeBtn(btnRow, TXT("REINTENTAR", "RETRY", "RESSAYER"),
                                 onDetailRetryClicked, lv_color_hex(0x0075EE),
                                 &r);
    lv_obj_set_size(retryBtn, 160, 44);
    lv_obj_align(retryBtn, LV_ALIGN_LEFT_MID, 20, 0);
  }
  lv_obj_t *closeBtn = makeBtn(btnRow, TXT("CERRAR", "CLOSE", "FERMER"),
                               onDetailCloseClicked, lv_color_hex(0xAA3333));
  lv_obj_set_size(closeBtn, 160, 44);
  if (canRetry) {
    lv_obj_align(closeBtn, LV_ALIGN_RIGHT_MID, -20, 0);
  } else {
    lv_obj_align(closeBtn, LV_ALIGN_CENTER, 0, 0);
  }
}

// Reconstruye el panel de detalle en cada renderAll() (igual que la
// cuadricula): asi su contenido (estado, detail) se mantiene al dia mientras
// esta abierto y una fila remota sigue progresando en segundo plano.
void renderDetail() {
  if (s_detailPanel) { lv_obj_del(s_detailPanel); s_detailPanel = nullptr; }
  if (s_detailRow) buildDetailPanel(*s_detailRow);
}

void renderAll() {
  if (!s_body || !s_action) return;

  if (s_rowsDirty) {
    computeOrder();
    s_rowsDirty = false;
  }

  // Hallazgo de code review (design.md D4): crear/actualizar in-place en vez
  // de lv_obj_clean(s_body) + reconstruir todo. Las filas de motherBoard se
  // crean aqui la primera vez que aparecen (createRowButton() es idempotente
  // si r.btn ya existe).
  for (int i = 0; i < s_rowCount; i++) {
    if (!s_rows[i].btn) {
      createRowButton(i);
    } else {
      updateRowButton(i);
    }
  }
  if (orderChanged(s_order, s_orderCount, s_prevOrder, s_prevOrderCount)) {
    for (int k = 0; k < s_orderCount; k++) {
      lv_obj_move_to_index(s_rows[s_order[k]].btn, k);
    }
    memcpy(s_prevOrder, s_order, sizeof(int) * (size_t)s_orderCount);
    s_prevOrderCount = s_orderCount;
  }
  if (s_lastTouchedRow != s_prevTouchedRow) {
    s_prevTouchedRow = s_lastTouchedRow;
    if (s_lastTouchedRow >= 0 && s_rows[s_lastTouchedRow].btn) {
      lv_obj_scroll_to_view(s_rows[s_lastTouchedRow].btn, LV_ANIM_OFF);
    }
  }

  // Hallazgo de code review: si una fila remota pasa a WAIT/CONFIRM mientras
  // el panel de detalle esta abierto, se cierra antes de pintar la pregunta o
  // instruccion (REJECT ya lo cierra via goSummary() ->
  // destroyTransientOverlayObjects()).
  if (s_detailRow && s_step == Step::RemoteRunning && remoteNeedsAttention()) {
    s_detailRow = nullptr;
  }

  lv_obj_clean(s_action);
  switch (s_step) {
    case Step::Gate:
      renderGateAction();
      break;
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

  renderDetail();
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
  r.dirty = true;
  s_rowsDirty = true;
  s_localPhase = LocalPhase::None;
  s_lastTouchedRow = s_localIdx;

  switch (kLocalOrder[s_localIdx]) {
    case FTEST_HMI_SYSINFO: runSysInfo(); return;
    case FTEST_HMI_I2C:     runI2c();     return;
    case FTEST_HMI_NVS:     runNvs();     return;
    case FTEST_HMI_LINK:    runLink();    return;
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
  r.dirty = true;
  s_rowsDirty = true;
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

// Arranca la secuencia local tras la barrera de entrada (Step::Gate ->
// Step::LocalSeq). Solo se llama desde FactoryTest_Poll(), nunca desde el
// callback del boton SI (hallazgo 4: beginLocalTest() puede llegar a
// runI2c()/runNvs(), que sueltan LVGL_Lock()).
void beginLocalSequence() {
  s_step = Step::LocalSeq;
  beginLocalTest();
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
    r.dirty = true;
    s_rowsDirty = true;
    snprintf(r.detail, sizeof(r.detail), "%s", res.detail);
    s_lastTouchedRow = idx;
    if (s_step == Step::RemoteAwaitFirst) s_step = Step::RemoteRunning;
    if (res.status != FTEST_CONFIRM && s_remoteConfirmAnsweredRowId == res.id) {
      s_remoteConfirmAnsweredRowId = FTEST_ID_NONE;
    }
  }
  // Hallazgo 5: cada CTRL,FTEST recibido reinicia el reloj de silencio de
  // RemoteRunning.
  if (n > 0 && s_step == Step::RemoteRunning) {
    s_deadlineMs = millis() + REMOTE_SILENCE_TIMEOUT_MS;
  }
  return n;
}

// Hallazgo 5: sin ningun CTRL,FTEST* durante REMOTE_SILENCE_TIMEOUT_MS se
// asume enlace perdido. Las filas de motherBoard que aun no llegaron a un
// estado terminal se marcan FALLA "sin respuesta"; las que ya terminaron
// (PASS/FAIL/SKIP/WARN, este ultimo tambien final desde
// shared-factory-test-bench) conservan su resultado real.
void markMbPendingAsLinkLost() {
  for (int i = kLocalCount; i < s_rowCount; i++) {
    RowData &r = s_rows[i];
    if (r.status == FTEST_PASS || r.status == FTEST_FAIL ||
        r.status == FTEST_SKIP || r.status == FTEST_WARN) {
      continue;
    }
    r.started = true;
    r.status = FTEST_FAIL;
    r.dirty = true;
    s_rowsDirty = true;
    snprintf(r.detail, sizeof(r.detail), "%s", "sin respuesta");
  }
  s_mbLinkLost = true;
}

void startRemote() {
  // Hallazgo 2: purga eventos y flags de una sesion anterior. La motherBoard
  // puede seguir emitiendo CTRL,FTEST* hasta 16s tras un aborto (Salir con
  // test remoto en curso), ventana que se solapa con toda la secuencia local
  // (varios segundos) que acaba de correr. Sin este drenaje esos eventos
  // "viejos" se pintarian como si fueran de esta sesion.
  FtestResult tmp;
  while (FactoryTest_TakeEvent(&tmp)) {}
  g_pendingFtestDone = false;
  g_pendingFtestReject = false;

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
  r.dirty = true;
  s_rowsDirty = true;
  r.detail[0] = '\0';
  s_lastTouchedRow = idx;
  // Intento nuevo: si el anterior habia terminado en "enlace perdido"
  // (hallazgo 5), no arrastrar ese rotulo al resumen de este reintento.
  s_mbLinkLost = false;
  Communication_SendFtestRun((uint8_t)id);
  s_step = Step::RemoteRunning;
  // Hallazgo 5: reinicia el reloj de silencio; sin esto heredaria el
  // s_deadlineMs de SUMMARY_IDLE_TIMEOUT_MS (10 min) que dejo Step::Summary.
  s_deadlineMs = millis() + REMOTE_SILENCE_TIMEOUT_MS;
  renderAll();
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
  const unsigned mbWarn = s_mbDone ? s_mbWarn : 0u;
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
  p.putUInt(HMI_KEY_FTEST_MBWARN, mbWarn);
  p.putString(HMI_KEY_FTEST_FWVER, fwVer);
  p.end();
  LVGL_Lock();
}

void goSummary() {
  destroyTransientOverlayObjects();
  s_askKind = AskKind::None;
  s_localPhase = LocalPhase::None;
  s_step = Step::Summary;
  // Hallazgo 6: tope de inactividad en Summary. Cualquier pulsacion de
  // REINTENTAR cambia de step antes de que esto importe.
  s_deadlineMs = millis() + SUMMARY_IDLE_TIMEOUT_MS;
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
    s_mbWarn = g_ftestDoneWarn;
    s_mbDone = true;
    goSummary();
    return;
  }
  // Hallazgo 5: silencio de la motherBoard durante REMOTE_SILENCE_TIMEOUT_MS.
  if ((int32_t)(millis() - s_deadlineMs) >= 0) {
    markMbPendingAsLinkLost();
    goSummary();
    return;
  }
  if (n > 0) renderAll();
}

// ============================================================================
// Ciclo de vida
// ============================================================================
void resetState() {
  // Destruye los botones (local + motherBoard) de la sesion anterior antes de
  // reindexar s_rows: FactoryTest_Close() ya los destruye, pero esta llamada
  // es idempotente (hallazgo de code review: "limpiar los punteros y
  // destruir los objetos una sola vez").
  destroyRowButtons();
  s_rowCount = 0;
  for (int i = 0; i < kLocalCount; i++) {
    RowData &r = s_rows[s_rowCount++];
    r.id = kLocalOrder[i];
    r.isMb = false;
    r.started = false;
    r.status = FTEST_RUNNING;
    r.detail[0] = '\0';
    r.btn = nullptr;
    r.titleLbl = nullptr;
    r.wordLbl = nullptr;
    r.dirty = true;
  }
  s_lastTouchedRow = -1;
  s_prevTouchedRow = -1;
  s_orderCount = 0;
  s_prevOrderCount = 0;
  s_rowsDirty = true;
  s_askKind = AskKind::None;
  s_retryMode = false;
  s_localPhase = LocalPhase::None;
  s_localIdx = 0;
  s_mbSupported = false;
  s_mbUnsupported = false;
  s_mbRejected = false;
  s_mbDone = false;
  s_mbLinkLost = false;
  s_mbPass = s_mbFail = s_mbSkip = s_mbWarn = 0;
  s_remoteConfirmAnsweredRowId = FTEST_ID_NONE;
  // Estado de UI pendiente de una sesion anterior (hallazgo 4): ningun flag
  // de hand-off debe sobrevivir a un cierre y reapertura de la pantalla.
  s_pendingLocalAsk = Answer::None;
  s_pendingRemoteConfirm = Answer::None;
  s_pendingRemoteConfirmRow = nullptr;
  s_pendingRetryRow = nullptr;
  s_pendingGateAnswer = Answer::None;
  destroyTransientOverlayObjects();
}

// Hand-off puro (hallazgo 4): FactoryTest_Close() suelta LVGL_Lock() (para
// buzzerOff()/speakerOff()) y llama a lv_scr_load(); Poll() lo hace fuera del
// despacho de este click.
void onExitClicked(lv_event_t *) { s_exitRequested = true; }

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

  // Cuadricula de resultados: flex fila-con-wrap de 3 columnas, SIN limpiar
  // LV_OBJ_FLAG_SCROLLABLE (lo trae por defecto lv_obj_create) para que
  // quepan los 7 locales + hasta 29 de motherBoard sin rediseñar el layout.
  s_body = lv_obj_create(s_card);
  lv_obj_remove_style_all(s_body);
  lv_obj_set_size(s_body, 720, 250);
  lv_obj_align(s_body, LV_ALIGN_TOP_MID, 0, 48);
  lv_obj_set_flex_flow(s_body, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_row(s_body, 8, 0);
  lv_obj_set_style_pad_column(s_body, 8, 0);

  s_action = lv_obj_create(s_card);
  lv_obj_remove_style_all(s_action);
  lv_obj_set_size(s_action, 720, 110);
  lv_obj_align(s_action, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_clear_flag(s_action, LV_OBJ_FLAG_SCROLLABLE);
}

void FactoryTest_Open(void) {
  if (!s_overlay || s_step != Step::Closed) return;

  // Hallazgo 2: purga estado de una sesion anterior. Si se salio con el test
  // remoto en curso, la motherBoard puede seguir emitiendo CTRL,FTEST* hasta
  // 16s despues del aborto; sin este drenaje esos eventos se pintarian o
  // persistirian en esta sesion nueva.
  FtestResult tmp;
  while (FactoryTest_TakeEvent(&tmp)) {}
  g_pendingFtestDone = false;
  g_pendingFtestReject = false;

  resetState();
  // Hallazgo 7: barrera de entrada antes de tocar nada (local o remoto).
  s_step = Step::Gate;
  lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(s_overlay);
  // Hallazgo 3: este overlay vive en lv_layer_top(), igual que el banner de
  // alarma y el icono AUDIO PAUSED; move_foreground() los deja detras si
  // estaban visibles.
  UI_ReassertAlarmOverlays();
  renderAll();
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
  // Hallazgo de code review: no dejar los botones de fila vivos tras cerrar
  // la pantalla (resetState() los destruiria igualmente en el proximo Open(),
  // pero destroyRowButtons() es idempotente y aqui libera los objetos antes).
  destroyRowButtons();
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

  // Hallazgo 4: Salir tiene prioridad sobre cualquier otro hand-off pendiente.
  if (s_exitRequested) {
    s_exitRequested = false;
    FactoryTest_Close();
    return;
  }

  // Hallazgo 7: barrera de entrada.
  if (s_step == Step::Gate) {
    if (s_pendingGateAnswer != Answer::None) {
      const bool yes = s_pendingGateAnswer == Answer::Yes;
      s_pendingGateAnswer = Answer::None;
      if (yes) {
        beginLocalSequence();
      } else {
        FactoryTest_Close();
      }
    }
    return;
  }

  // Panel de detalle: abrir/cerrar se resuelve aqui, fuera del despacho del
  // click que lo pidio (mismo hand-off que Reintentar, hallazgo 4). Hallazgo
  // de code review: no abrirlo si ya hay una pregunta Si/No pendiente (local
  // o remota) — la taparia.
  if (s_pendingDetailRow) {
    RowData *row = s_pendingDetailRow;
    s_pendingDetailRow = nullptr;
    if (!hasPendingYesNoQuestion()) {
      s_detailRow = row;
      renderAll();
    }
    return;
  }
  if (s_pendingDetailClose) {
    s_pendingDetailClose = false;
    s_detailRow = nullptr;
    renderAll();
    return;
  }

  // Hallazgo 4: REINTENTAR (local o remoto, desde el panel de detalle),
  // resuelto fuera del despacho del click que lo pidio.
  if (s_pendingRetryRow) {
    RowData *row = s_pendingRetryRow;
    s_pendingRetryRow = nullptr;
    if (row->isMb) {
      retryRemote(row->id);
    } else {
      retryLocalTest(row->id);
    }
    return;
  }

  if (s_askKind != AskKind::None) {
    // Hallazgo 4: SI/NO de la pregunta local (zumbador/altavoz).
    if (s_pendingLocalAsk != Answer::None) {
      const bool yes = s_pendingLocalAsk == Answer::Yes;
      s_pendingLocalAsk = Answer::None;
      resolveAsk(yes);
      return;
    }
    if ((int32_t)(millis() - s_deadlineMs) >= 0) {
      resolveAsk(false);  // Las preguntas expiran a los 60s como FALLA.
    }
    return;
  }

  // Hallazgo 4: SI/NO de la confirmacion de un test remoto.
  if (s_pendingRemoteConfirm != Answer::None) {
    const bool yes = s_pendingRemoteConfirm == Answer::Yes;
    RowData *row = s_pendingRemoteConfirmRow;
    s_pendingRemoteConfirm = Answer::None;
    s_pendingRemoteConfirmRow = nullptr;
    if (row) {
      Communication_SendFtestConfirm((uint8_t)row->id, yes);
      s_remoteConfirmAnsweredRowId = row->id;
      renderAll();
    }
    return;
  }

  switch (s_step) {
    case Step::LocalSeq:
      switch (s_localPhase) {
        case LocalPhase::Buzzer:  serviceBuzzer();  break;
        case LocalPhase::Speaker: serviceSpeaker(); break;
        case LocalPhase::Wifi:    serviceWifi();    break;
        default: break;
      }
      break;
    case Step::RemoteAwaitFirst: serviceRemoteAwaitFirst(); break;
    case Step::RemoteRunning:    serviceRemoteRunning();    break;
    case Step::Summary:
      // Hallazgo 6: tope de inactividad, misma ruta que Salir.
      if ((int32_t)(millis() - s_deadlineMs) >= 0) {
        FactoryTest_Close();
      }
      break;
    default: break;
  }
}

void FactoryTest_ApplyLanguage(void) {
  if (s_exitBtnLabel) {
    lv_label_set_text(s_exitBtnLabel, TXT("SALIR", "EXIT", "QUITTER"));
  }
  if (s_step != Step::Closed) {
    // Los botones de fila ya creados solo se refrescan si r.dirty (hallazgo
    // de code review); el nombre traducido de la fila necesita ese empujon
    // explicito aqui, no lo pone ninguna mutacion de estado.
    for (int i = 0; i < s_rowCount; i++) s_rows[i].dirty = true;
    renderAll();
  }
}

bool FactoryTest_AudioBusy(void) {
  return s_step == Step::LocalSeq && s_localPhase == LocalPhase::Buzzer;
}
