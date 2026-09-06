#include "ui/FactoryTest.h"

#include <Wire.h>
#include <cstdio>
#include <cstring>

#include "CommTask.h"
#include "UITask.h"
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
// aportar en la linea de montaje); shared-factory-test-bench2 retira BUZZER
// y SPEAKER (el jig actual no puede verificarlos). Sus IDs siguen vivos en
// factory_test.h, esta placa simplemente ya no los usa. kLocalCount YA NO
// coincide con FTEST_HMI_END - FTEST_HMI_BASE (esa cuenta, de la tabla
// compartida, sigue siendo 9).
constexpr FtestId kLocalOrder[] = {
    FTEST_HMI_SYSINFO, FTEST_HMI_I2C, FTEST_HMI_WIFI,
    FTEST_HMI_NVS,      FTEST_HMI_LINK,
};
constexpr int kLocalCount = sizeof(kLocalOrder) / sizeof(kLocalOrder[0]);
constexpr int kMaxRows = kLocalCount + (int)FTEST_MB_COUNT;
// Tests esperados en la bateria completa (barra de progreso, D5): los
// locales activos (kLocalCount) mas la tabla de motherBoard entera. SKIP
// cuenta como terminado, igual que PASA/FALLA/AVISO.
constexpr int kExpectedTotal = kLocalCount + (int)FTEST_MB_COUNT;
// Vigilancia por fila (hallazgo del banco 2026-09-06): si una fila de
// motherBoard lleva mas de esto en RUNNING/WAIT/CONFIRM sin cambiar de
// estado, el display la marca FALLA "timeout". La motherBoard ya tiene su
// propia cota de 90 s por test y deberia emitir FAIL "timeout" antes; esta
// es la red de seguridad del display si la placa se cuelga en ese test.
constexpr uint32_t FTEST_ROW_TIMEOUT_MS = 100000;
// Paginacion de la cuadricula (feedback de banco: el scroll no se maneja
// bien con guantes). 3 columnas x 3 filas por pagina: con la barra de
// progreso y el veredicto abajo del todo, es lo que cabe en la tarjeta sin
// scroll (ver FactoryTest_Init()).
constexpr int kPageCols = 3;
constexpr int kPageRowsPerPage = 3;
constexpr int kPageCapacity = kPageCols * kPageRowsPerPage;

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
  // Ultima vez (millis()) que status cambio de verdad. Solo lo consulta
  // checkRowTimeouts() para las filas de motherBoard en RUNNING/WAIT/CONFIRM
  // (vigilancia por fila, FTEST_ROW_TIMEOUT_MS).
  uint32_t    lastChangeMs;
};

RowData s_rows[kMaxRows];
int     s_rowCount = 0;
int     s_lastTouchedRow = -1;  // fila "en curso" a seguir con la paginacion
int     s_prevTouchedRow = -1;  // ultima fila en curso ya seguida

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

// Paginacion de la cuadricula (feedback de banco 2026-09-06): sin scroll,
// paginas de kPageCapacity botones. s_currentPage indexa sobre s_order (solo
// filas visibles, ya ordenadas). s_pageFollowsTest sigue la fila en curso
// (s_lastTouchedRow) salvo que el operario haya paginado a mano con < / >;
// vuelve a seguir en el siguiente cambio de test (ver renderAll()).
int  s_currentPage = 0;
int  s_prevAppliedPage = -1;
bool s_pageFollowsTest = true;
int  s_pendingPageDelta = 0;  // hand-off de los botones < / > (hallazgo 4)

enum class Step {
  Closed,
  Gate,
  LocalSeq,
  RemoteAwaitFirst,
  RemoteRunning,
  Summary,
};

// Buzzer/Speaker retirados de la secuencia local (shared-factory-test-bench2:
// el jig actual no puede verificarlos): la unica fase con contenido propio en
// la zona de accion es Wifi (escaneo).
enum class LocalPhase { None, Wifi };

// Respuesta si/no pendiente de resolver en FactoryTest_Poll(). Los callbacks
// de LVGL (dispatch de evento) solo escriben aqui: nunca resuelven en el
// mismo callback, para no soltar LVGL_Lock() / destruir objetos (incluido el
// boton que esta despachando su propio click) desde dentro del despacho.
enum class Answer { None, Yes, No };

Step       s_step = Step::Closed;
LocalPhase s_localPhase = LocalPhase::None;
int        s_localIdx = 0;
bool       s_retryMode = false;
uint32_t   s_deadlineMs = 0;

// hmi-factory-test-settings-entry: true si esta apertura vino de la fila
// "Test de hardware" de ui_ScreenSettings, en vez del boton del splash. Solo
// lo consulta FactoryTest_Close() para decidir si vuelve a ui_ScreenMain o se
// queda en Settings (ver comentario de FactoryTest_Open() en el header).
bool     s_openedFromSettings = false;
// Hand-off puro de la fila de Settings (hallazgo 4, mismo criterio que el
// resto de FactoryTest.cpp): FactoryTest_Poll() lo consume incluso con el
// overlay cerrado, porque el resto de Poll() bail-outea en Step::Closed.
bool     s_pendingOpenFromSettings = false;

bool     s_mbSupported = false;
bool     s_mbUnsupported = false;
bool     s_mbRejected = false;
FtestReject s_mbRejectReason = FTEST_REJECT_BUSY;
bool     s_mbDone = false;
bool     s_mbLinkLost = false;
unsigned s_mbPass = 0, s_mbFail = 0, s_mbSkip = 0, s_mbWarn = 0;
unsigned s_remoteConfirmAnsweredRowId = FTEST_ID_NONE;

// Hand-off de UI (hallazgo 4): todo boton (Si/No confirmacion remota,
// Reintentar, Salir, Si/No de la barrera de entrada, abrir/cerrar el panel
// de detalle, paginar) solo marca aqui su intencion. Poll() la consume en la
// siguiente pasada, fuera de cualquier despacho de evento LVGL.
Answer    s_pendingRemoteConfirm = Answer::None;
RowData  *s_pendingRemoteConfirmRow = nullptr;
RowData  *s_pendingRetryRow = nullptr;
bool      s_exitRequested = false;
Answer    s_pendingGateAnswer = Answer::None;
RowData  *s_pendingDetailRow = nullptr;  // fila cuyo detalle se pide abrir
bool      s_pendingDetailClose = false;

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
lv_obj_t *s_body = nullptr;    // cuadricula de botones, paginada (sin scroll)
lv_obj_t *s_action = nullptr;  // instruccion / pregunta / resumen

// Paginacion (D5): fila de navegacion bajo la cuadricula.
lv_obj_t *s_pageNavRow = nullptr;
lv_obj_t *s_pageBackBtn = nullptr;
lv_obj_t *s_pageNextBtn = nullptr;
lv_obj_t *s_pageLabel = nullptr;

// Barra de progreso y veredicto (D5), abajo del todo de la tarjeta.
lv_obj_t *s_bottomRow = nullptr;
lv_obj_t *s_progressBar = nullptr;
lv_obj_t *s_verdictBox = nullptr;
lv_obj_t *s_verdictLabel = nullptr;

lv_obj_t *s_detailPanel = nullptr;  // panel de detalle modal, o nullptr
RowData  *s_detailRow = nullptr;    // fila cuyo detalle esta abierto

// Pop-up modal de la barrera de entrada (banco 2026-09-06, hallazgo del
// operario: dos botones en rojo con "0 errores"). Mismo patron que
// s_detailPanel: se destruye/reconstruye en cada renderAll(), solo existe
// mientras s_step == Step::Gate.
lv_obj_t *s_gatePopup = nullptr;

// ---------------------------------------------------------------------------
// Forward declarations (la maquina de estados es mutuamente recursiva:
// finishLocal() encadena con beginLocalTest() del siguiente test).
void beginLocalTest();
void beginLocalSequence();
void finishLocal(FtestStatus st, const char *detail);
void renderAll();
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

// ---------------------------------------------------------------------------
// Nombres, descripciones e instrucciones traducidos. ftest_id_key() (shared/)
// es el fallback para un id de motherBoard que esta version del display
// todavia no conoce por nombre (tabla ampliada en el futuro sin romper
// compatibilidad).
const char *localTestName(unsigned id) {
  switch (id) {
    case FTEST_HMI_SYSINFO: return TXT("Info sistema", "System info", "Info systeme");
    case FTEST_HMI_I2C:     return TXT("Bus I2C", "I2C bus", "Bus I2C");
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
    case FTEST_MB_ENV_SENSOR:  return TXT("Sensor ambiental", "Ambient sensor", "Capteur ambiant");
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
    case FTEST_MB_ENV_SENSOR:
      return TXT(
          "SensorBoard por USB, sensores I2C2 o SHT4x exterior: cualquiera vale",
          "SensorBoard over USB, I2C2 sensors or external SHT4x: either works",
          "SensorBoard par USB, capteurs I2C2 ou SHT4x exterieur : l'un des "
          "deux suffit");
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

// Barrera de entrada (hallazgo 7): pop-up modal en Step::Gate antes de
// arrancar ningun test, local o remoto (banco 2026-09-06: ya no se pinta en
// la zona de accion — buildGatePopup()).
const char *gateTitleText() {
  return TXT("ATENCION", "WARNING", "ATTENTION");
}

const char *gateBodyText() {
  return TXT(
      "El equipo debe estar VACIO, sin paciente. Los actuadores se "
      "encenderan en lazo abierto. Continuar?",
      "The unit must be EMPTY, no patient inside. Actuators will be "
      "switched on in open loop. Continue?",
      "L'appareil doit etre VIDE, sans patient. Les actionneurs seront "
      "allumes en boucle ouverte. Continuer ?");
}

// Indicador de paginacion (D5): "Pagina i/n".
const char *pageIndicatorFmt() {
  return TXT("Pagina %d/%d", "Page %d/%d", "Page %d/%d");
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

// Codigo de color unico de la pantalla (feedback de banco): blanco = en
// curso/testeando, verde = OK, amarillo = aviso, rojo = error. Lo comparten
// los botones de la cuadricula (gridColors()), el panel de detalle
// (buildDetailPanel()) y el veredicto (renderVerdictAndProgress()).
void colorsForBucket(int bucket, lv_color_t *fill, lv_color_t *border) {
  switch (bucket) {
    case 0: *fill = lv_color_hex(0xFFE0E4); *border = lv_color_hex(0xD5283C); break;  // rojo/error
    case 1: *fill = lv_color_hex(0xFFF0D6); *border = lv_color_hex(0xC98A00); break;  // amarillo/aviso
    case 3: *fill = lv_color_hex(0xDFF3E4); *border = lv_color_hex(0x1B7F3B); break;  // verde/OK
    default: *fill = lv_color_hex(0xFFFFFF); *border = lv_color_hex(0x0B2E4F); break; // blanco/en curso
  }
}

void gridColors(const RowData &r, lv_color_t *fill, lv_color_t *border) {
  colorsForBucket(bucketOf(r), fill, border);
}

// Veredicto de la bateria (D5): HW ERROR si hay al menos un FALLA (local o de
// motherBoard) o si la placa quedo sin soporte, fue rechazada o perdio el
// enlace a mitad de bateria; HW OK en cualquier otro caso. Los AVISOS no
// cuentan. Mismo criterio para el veredicto en pantalla
// (renderVerdictAndProgress()) y su persistencia (persistResults()).
bool computeHwError() {
  for (int i = 0; i < s_rowCount; i++) {
    if (s_rows[i].started && s_rows[i].status == FTEST_FAIL) return true;
  }
  return s_mbUnsupported || s_mbRejected || s_mbLinkLost;
}

// Cuenta PASA/FALLA/AVISO/OMITIDO de las filas [startIdx, s_rowCount) ya
// empezadas (banco 2026-09-06: la cabecera y la persistencia SIEMPRE se
// calculan de aqui, nunca de los contadores sueltos de CTRL,FTEST_DONE — ver
// comentario de renderSummary()).
void countRows(int startIdx, int *pass, int *fail, int *warn, int *skip) {
  *pass = *fail = *warn = *skip = 0;
  for (int i = startIdx; i < s_rowCount; i++) {
    if (!s_rows[i].started) continue;
    switch (s_rows[i].status) {
      case FTEST_PASS: (*pass)++; break;
      case FTEST_FAIL: (*fail)++; break;
      case FTEST_WARN: (*warn)++; break;
      case FTEST_SKIP: (*skip)++; break;
      default: break;
    }
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
  s_rows[i].lastChangeMs = millis();
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
  if (s_gatePopup) { lv_obj_del(s_gatePopup); s_gatePopup = nullptr; }
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

// Hand-off puro (hallazgo 4): Communication_SendFtestConfirm() + renderAll()
// se difieren a Poll().
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

// Pop-up modal de la barrera de entrada (banco 2026-09-06: dejo de pintarse
// en s_action — un texto ahi se confundia con el resto de la pantalla y el
// operario podia perderselo; ahora tapa la cuadricula y la barra, igual que
// el panel de detalle). Se destruye/reconstruye en cada renderAll() (mismo
// patron que renderDetail()): solo existe mientras s_step == Step::Gate.
void buildGatePopup() {
  s_gatePopup = lv_obj_create(s_overlay);
  lv_obj_set_size(s_gatePopup, 560, 260);
  lv_obj_center(s_gatePopup);
  lv_obj_set_style_radius(s_gatePopup, 12, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_gatePopup, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_clear_flag(s_gatePopup, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_move_foreground(s_gatePopup);
  lv_obj_set_style_pad_all(s_gatePopup, 18, LV_PART_MAIN);
  lv_obj_set_flex_flow(s_gatePopup, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_gatePopup, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(s_gatePopup, 16, LV_PART_MAIN);

  lv_obj_t *title = lv_label_create(s_gatePopup);
  lv_label_set_text(title, gateTitleText());
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0xAA3333), 0);

  lv_obj_t *body = lv_label_create(s_gatePopup);
  lv_label_set_text(body, gateBodyText());
  lv_obj_set_style_text_font(body, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(body, lv_color_hex(0x0B2E4F), 0);
  lv_obj_set_width(body, 500);
  lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t *btnRow = lv_obj_create(s_gatePopup);
  lv_obj_remove_style_all(btnRow);
  lv_obj_set_size(btnRow, 500, 60);
  lv_obj_clear_flag(btnRow, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *yes = makeBtn(btnRow, TXT("SI", "YES", "OUI"), onGateYesCb,
                          lv_color_hex(0x2E7D32));
  lv_obj_set_size(yes, 200, 56);
  lv_obj_align(yes, LV_ALIGN_LEFT_MID, 10, 0);

  lv_obj_t *no = makeBtn(btnRow, TXT("NO", "NO", "NON"), onGateNoCb,
                         lv_color_hex(0xAA3333));
  lv_obj_set_size(no, 200, 56);
  lv_obj_align(no, LV_ALIGN_RIGHT_MID, -10, 0);
}

// Llamada en cada renderAll() (mismo patron que renderDetail()): destruye el
// pop-up anterior y lo reconstruye solo si sigue en Step::Gate.
void renderGatePopup() {
  if (s_gatePopup) { lv_obj_del(s_gatePopup); s_gatePopup = nullptr; }
  if (s_step == Step::Gate) buildGatePopup();
}

void renderLocalAction() {
  switch (s_localPhase) {
    case LocalPhase::Wifi:
      renderCenteredText(TXT("Buscando redes WiFi...",
                             "Scanning WiFi networks...",
                             "Recherche de reseaux WiFi..."));
      break;
    default:
      break;  // Tests instantaneos: sin contenido en la zona de accion.
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
// sin omitidos). Cuenta SIEMPRE de las filas (s_rows[], local + motherBoard),
// nunca de s_mbPass/s_mbFail/s_mbWarn sueltos (esos solo alimentan
// countRows() cuando drainFtestEvents() los vuelca en una fila): banco
// 2026-09-06, la cabecera usaba esos contadores y SOLO si s_mbDone, mientras
// el veredicto (computeHwError()) ya miraba las filas — un timeout o un
// enlace perdido a mitad de bateria (que SI marcan filas FALLA) pintaba
// botones en rojo con la cabecera en "0 errores" y el veredicto en HW ERROR,
// incoherentes entre si.
void renderSummary() {
  int totalPass, totalFail, totalWarn, totalSkip;
  countRows(0, &totalPass, &totalFail, &totalWarn, &totalSkip);

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

// Numero de paginas de la cuadricula (D5): al menos 1 aunque s_orderCount
// sea 0 (recien abierta la pantalla, antes de computeOrder()).
int pageCount() {
  if (s_orderCount <= 0) return 1;
  return (s_orderCount + kPageCapacity - 1) / kPageCapacity;
}

// Aplica al conjunto de botones YA creados el hidden/visible que corresponde
// a s_currentPage, sin tocar el orden. Se llama solo cuando el orden o la
// pagina cambiaron de verdad (mismo gateo anti-churn que orderChanged(),
// design.md D4): togglear LV_OBJ_FLAG_HIDDEN en cada pasada invalidaria el
// layout sin necesidad. Las filas en SKIP no estan en s_order: su hidden lo
// pone updateRowButton(), esta funcion no las toca.
void applyPageVisibility() {
  const int pages = pageCount();
  if (s_currentPage >= pages) s_currentPage = pages - 1;
  if (s_currentPage < 0) s_currentPage = 0;
  const int start = s_currentPage * kPageCapacity;
  const int end = start + kPageCapacity;
  for (int k = 0; k < s_orderCount; k++) {
    lv_obj_t *btn = s_rows[s_order[k]].btn;
    if (!btn) continue;
    if (k >= start && k < end) {
      lv_obj_clear_flag(btn, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

// Hand-off de los botones < / > (hallazgo 4): paginar es una mutacion de
// estado de UI, se resuelve en Poll() como el resto.
void onPageBackCb(lv_event_t *) { s_pendingPageDelta = -1; }
void onPageNextCb(lv_event_t *) { s_pendingPageDelta = 1; }

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
// pregunta Si/No remota pendiente de resolver — la taparia.
bool hasPendingYesNoQuestion() {
  return remoteNeedsAttention();
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
  // Mismo codigo de color que la cuadricula (blanco/verde/amarillo/rojo):
  // fondo del chip = fill, texto = border.
  lv_obj_t *statusLbl = lv_label_create(s_detailPanel);
  lv_label_set_text(statusLbl, statusLine);
  lv_obj_set_style_text_font(statusLbl, &lv_font_montserrat_16, 0);
  lv_color_t fill, border;
  gridColors(r, &fill, &border);
  lv_obj_set_style_text_color(statusLbl, border, 0);
  lv_obj_set_style_bg_color(statusLbl, fill, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(statusLbl, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(statusLbl, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_all(statusLbl, 6, LV_PART_MAIN);
  lv_obj_set_width(statusLbl, 500);
  lv_label_set_long_mode(statusLbl, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(statusLbl, LV_TEXT_ALIGN_CENTER, 0);

  // Descartes del anillo FTEST (banco 2026-09-06): visibles solo en el
  // detalle de "Enlace" (el test local que mas de cerca vigila el enlace
  // serie con la MB) y solo si > 0 — g_ftestRingDrops es un contador global
  // de Comm_Task, no de esta fila.
  if (!r.isMb && r.id == (unsigned)FTEST_HMI_LINK && g_ftestRingDrops > 0) {
    char dropLine[64];
    snprintf(dropLine, sizeof(dropLine),
            TXT("Anillo FTEST: %u descartes", "FTEST ring: %u drops",
                "Anneau FTEST : %u pertes"),
            (unsigned)g_ftestRingDrops);
    lv_obj_t *dropLbl = lv_label_create(s_detailPanel);
    lv_label_set_text(dropLbl, dropLine);
    lv_obj_set_style_text_font(dropLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(dropLbl, lv_color_hex(0xC98A00), 0);
    lv_obj_set_width(dropLbl, 500);
    lv_label_set_long_mode(dropLbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(dropLbl, LV_TEXT_ALIGN_CENTER, 0);
  }

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

// Barra de progreso y veredicto (D5), abajo del todo de la tarjeta.
// Terminales/esperados: los esperados son kExpectedTotal (constante); los
// terminales cuentan PASA/FALLA/AVISO/SKIP de TODAS las filas ya empezadas
// (local + motherBoard) — SKIP cuenta como terminado para que la barra
// llegue al 100 % aunque el equipo no lleve ese hardware. El veredicto es
// "EN CURSO..." (blanco) hasta Step::Summary; ahi es HW OK (verde) si ningun
// FALLA, o HW ERROR (rojo) si hay alguno, o si la placa no respondio,
// rechazo el test o se perdio el enlace a mitad — mismo codigo de color que
// la cuadricula (colorsForBucket()).
void renderVerdictAndProgress() {
  if (!s_progressBar || !s_verdictBox || !s_verdictLabel) return;

  int terminal = 0;
  for (int i = 0; i < s_rowCount; i++) {
    const RowData &r = s_rows[i];
    if (!r.started) continue;
    if (r.status == FTEST_PASS || r.status == FTEST_FAIL ||
        r.status == FTEST_SKIP || r.status == FTEST_WARN) {
      terminal++;
    }
  }
  lv_bar_set_value(s_progressBar, terminal, LV_ANIM_OFF);

  int bucket = 2;  // blanco: en curso
  const char *text = TXT("EN CURSO...", "RUNNING...", "EN COURS...");
  if (s_step == Step::Summary) {
    const bool hwError = computeHwError();
    bucket = hwError ? 0 : 3;
    text = hwError ? "HW ERROR" : "HW OK";
  }
  lv_color_t fill, border;
  colorsForBucket(bucket, &fill, &border);
  lv_obj_set_style_bg_color(s_verdictBox, fill, LV_PART_MAIN);
  lv_obj_set_style_text_color(s_verdictLabel, border, 0);
  lv_label_set_text(s_verdictLabel, text);
}

void renderAll() {
  if (!s_body || !s_action) return;

  // Barrera de entrada (banco 2026-09-06): mientras el pop-up de Step::Gate
  // esta abierto, la cuadricula y la barra de progreso/veredicto NO se
  // pintan (nada que testear todavia, el operario aun no ha contestado si
  // el equipo esta vacio). Se ocultan los contenedores enteros en vez de
  // saltarse su logica de abajo, que es idempotente y mas simple de dejar
  // corriendo siempre.
  const bool gateOpen = (s_step == Step::Gate);
  if (gateOpen) {
    lv_obj_add_flag(s_body, LV_OBJ_FLAG_HIDDEN);
    if (s_pageNavRow) lv_obj_add_flag(s_pageNavRow, LV_OBJ_FLAG_HIDDEN);
    if (s_bottomRow) lv_obj_add_flag(s_bottomRow, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(s_body, LV_OBJ_FLAG_HIDDEN);
    if (s_pageNavRow) lv_obj_clear_flag(s_pageNavRow, LV_OBJ_FLAG_HIDDEN);
    if (s_bottomRow) lv_obj_clear_flag(s_bottomRow, LV_OBJ_FLAG_HIDDEN);
  }

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
  const bool orderJustChanged =
      orderChanged(s_order, s_orderCount, s_prevOrder, s_prevOrderCount);
  if (orderJustChanged) {
    for (int k = 0; k < s_orderCount; k++) {
      lv_obj_move_to_index(s_rows[s_order[k]].btn, k);
    }
    memcpy(s_prevOrder, s_order, sizeof(int) * (size_t)s_orderCount);
    s_prevOrderCount = s_orderCount;
  }

  // Paginacion (D5): la pagina sigue a la fila en curso (s_lastTouchedRow)
  // salvo que el operario haya paginado a mano con < / >; un cambio de test
  // reactiva el seguimiento (mismo criterio que antes usaba el scroll).
  if (s_lastTouchedRow != s_prevTouchedRow) {
    s_prevTouchedRow = s_lastTouchedRow;
    s_pageFollowsTest = true;
  }
  if (s_pageFollowsTest && s_lastTouchedRow >= 0) {
    for (int k = 0; k < s_orderCount; k++) {
      if (s_order[k] == s_lastTouchedRow) {
        s_currentPage = k / kPageCapacity;
        break;
      }
    }
  }
  if (orderJustChanged || s_currentPage != s_prevAppliedPage) {
    applyPageVisibility();
    s_prevAppliedPage = s_currentPage;
  }
  if (s_pageLabel) {
    char buf[24];
    snprintf(buf, sizeof(buf), pageIndicatorFmt(), s_currentPage + 1,
            pageCount());
    lv_label_set_text(s_pageLabel, buf);
  }
  if (s_pageBackBtn) {
    if (s_currentPage <= 0) lv_obj_add_state(s_pageBackBtn, LV_STATE_DISABLED);
    else lv_obj_clear_state(s_pageBackBtn, LV_STATE_DISABLED);
  }
  if (s_pageNextBtn) {
    if (s_currentPage >= pageCount() - 1)
      lv_obj_add_state(s_pageNextBtn, LV_STATE_DISABLED);
    else
      lv_obj_clear_state(s_pageNextBtn, LV_STATE_DISABLED);
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
      // Banco 2026-09-06: el aviso ya no se pinta aqui, es un pop-up modal
      // (renderGatePopup(), mas abajo).
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

  renderVerdictAndProgress();
  renderDetail();
  renderGatePopup();
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

// D4 (shared-factory-test-bench2, banco 2026-09-06): un endTransmission()
// vacio no prueba nada frente al STC8H1K28 ni al GT911 — ambos pueden
// NACKear una escritura vacia con hardware sano. El veredicto real usa la
// evidencia de que sus inits del arranque funcionaron (UI_TouchInitOk()/
// UI_BacklightInitOk(), UITask.cpp); el sondeo de direcciones queda solo en
// el detalle, informativo.
void runI2c() {
  // Fuera de LVGL_Lock(): son transacciones I2C (convencion de la placa).
  LVGL_Unlock();
  Wire.beginTransmission(DISPLAY_I2C_ADDR_TOUCH);
  const uint8_t touchErr = Wire.endTransmission();
  Wire.beginTransmission((uint8_t)I2C_ADDR_BACKLIGHT);
  const uint8_t blErr = Wire.endTransmission();
  // PCA9557 @ 0x18: no poblado en esta revision de hardware (ver
  // embedded-display-hmi.md).
  Wire.beginTransmission((uint8_t)0x18);
  const uint8_t pcaErr = Wire.endTransmission();
  LVGL_Lock();

  char detail[FTEST_DETAIL_MAX + 1];
  snprintf(detail, sizeof(detail), "0x14:%s 0x30:%s 0x18:%s",
          touchErr == 0 ? "si" : "no", blErr == 0 ? "si" : "no",
          pcaErr == 0 ? "si" : "no");
  const bool ok = UI_TouchInitOk() && UI_BacklightInitOk();
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
    r.lastChangeMs = millis();
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

// Vigilancia por fila (banco 2026-09-06): si una fila de motherBoard lleva
// mas de FTEST_ROW_TIMEOUT_MS en RUNNING/WAIT/CONFIRM sin cambiar de estado,
// se marca FALLA "timeout". La motherBoard tiene su propia cota de 90 s por
// test y deberia haber emitido FAIL "timeout" antes; esto es solo la red de
// seguridad del display si la placa se cuelga en ese test — no decide el
// resumen por si sola, eso lo sigue haciendo el silencio de 120 s
// (markMbPendingAsLinkLost()) si la placa deja de responder del todo.
int checkRowTimeouts() {
  int n = 0;
  const uint32_t now = millis();
  for (int i = kLocalCount; i < s_rowCount; i++) {
    RowData &r = s_rows[i];
    if (!r.started) continue;
    if (r.status != FTEST_RUNNING && r.status != FTEST_WAIT &&
        r.status != FTEST_CONFIRM) {
      continue;
    }
    if ((int32_t)(now - r.lastChangeMs) < (int32_t)FTEST_ROW_TIMEOUT_MS) continue;
    r.status = FTEST_FAIL;
    r.dirty = true;
    s_rowsDirty = true;
    r.lastChangeMs = now;
    snprintf(r.detail, sizeof(r.detail), "%s", "timeout");
    if (s_remoteConfirmAnsweredRowId == r.id) {
      s_remoteConfirmAnsweredRowId = FTEST_ID_NONE;
    }
    n++;
  }
  return n;
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
  r.lastChangeMs = millis();
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
  // Contadores de motherBoard SIEMPRE de las filas (mismo criterio que
  // renderSummary()), no de s_mbPass/s_mbFail/s_mbSkip/s_mbWarn sueltos:
  // incluye timeout/enlace perdido, que ya marcaron sus filas FALLA aunque
  // CTRL,FTEST_DONE nunca llegara (s_mbDone false) o llegara con otros
  // numeros.
  int mbPass, mbFail, mbWarn, mbSkip;
  countRows(kLocalCount, &mbPass, &mbFail, &mbWarn, &mbSkip);
  char fwVer[sizeof(ctrl_state_msg.fwVer)];
  snprintf(fwVer, sizeof(fwVer), "%s", ctrl_state_msg.fwVer);
  // Veredicto unico de la bateria (D5): 1 = HW OK, 2 = HW ERROR. Mismo
  // criterio que renderVerdictAndProgress() (computeHwError()).
  const uint32_t verdict = computeHwError() ? 2u : 1u;

  // Fuera de LVGL_Lock(): escritura de Preferences (mismo motivo que el resto
  // de escrituras periodicas de NVS de UITask.cpp).
  LVGL_Unlock();
  Preferences p;
  p.begin(HMI_NS_FTEST, false);
  p.putUInt(HMI_KEY_FTEST_EPOCH, epoch);
  p.putUInt(HMI_KEY_FTEST_PASSMASK, passMask);
  p.putUInt(HMI_KEY_FTEST_FAILMASK, failMask);
  p.putUInt(HMI_KEY_FTEST_MBPASS, (uint32_t)mbPass);
  p.putUInt(HMI_KEY_FTEST_MBFAIL, (uint32_t)mbFail);
  p.putUInt(HMI_KEY_FTEST_MBSKIP, (uint32_t)mbSkip);
  p.putUInt(HMI_KEY_FTEST_MBWARN, (uint32_t)mbWarn);
  p.putString(HMI_KEY_FTEST_FWVER, fwVer);
  p.putUInt(HMI_KEY_FTEST_VERDICT, verdict);
  p.end();
  LVGL_Lock();
}

void goSummary() {
  destroyTransientOverlayObjects();
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
  const int nTimeout = checkRowTimeouts();

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
    // Banco 2026-09-06: CTRL,FTEST_DONE solo cierra la bateria (design.md D2)
    // — ya NO alimenta la cabecera ni la persistencia, que salen siempre de
    // las filas (countRows(), renderSummary(), persistResults()). Si sus
    // contadores no cuadran con lo que las filas realmente muestran (p.ej.
    // una fila que este display ya habia marcado FALLA por timeout de fila
    // antes de que llegara FTEST_DONE), se deja constancia en el log sin
    // tocar lo que ve el operario.
    int rowPass, rowFail, rowWarn, rowSkip;
    countRows(kLocalCount, &rowPass, &rowFail, &rowWarn, &rowSkip);
    if (rowPass != (int)s_mbPass || rowFail != (int)s_mbFail ||
        rowWarn != (int)s_mbWarn || rowSkip != (int)s_mbSkip) {
      COMM_LOG("[FTEST] FTEST_DONE (p%u f%u w%u s%u) no cuadra con filas "
               "(p%d f%d w%d s%d)\n",
               (unsigned)s_mbPass, (unsigned)s_mbFail, (unsigned)s_mbWarn,
               (unsigned)s_mbSkip, rowPass, rowFail, rowWarn, rowSkip);
    }
    goSummary();
    return;
  }
  // Hallazgo 5: silencio de la motherBoard durante REMOTE_SILENCE_TIMEOUT_MS.
  if ((int32_t)(millis() - s_deadlineMs) >= 0) {
    markMbPendingAsLinkLost();
    goSummary();
    return;
  }
  if (n > 0 || nTimeout > 0) renderAll();
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
    r.lastChangeMs = millis();
    r.dirty = true;
  }
  s_lastTouchedRow = -1;
  s_prevTouchedRow = -1;
  s_orderCount = 0;
  s_prevOrderCount = 0;
  s_rowsDirty = true;
  s_currentPage = 0;
  s_prevAppliedPage = -1;
  s_pageFollowsTest = true;
  s_pendingPageDelta = 0;
  s_retryMode = false;
  s_localPhase = LocalPhase::None;
  s_localIdx = 0;
  s_mbSupported = false;
  s_mbUnsupported = false;
  s_mbRejected = false;
  s_mbDone = false;
  s_mbLinkLost = false;
  s_mbPass = s_mbFail = s_mbSkip = s_mbWarn = 0;
  // Descartes de una sesion anterior no deben arrastrarse a esta (banco
  // 2026-09-06): es un contador global de Comm_Task, se limpia al abrir.
  g_ftestRingDrops = 0;
  s_remoteConfirmAnsweredRowId = FTEST_ID_NONE;
  // Estado de UI pendiente de una sesion anterior (hallazgo 4): ningun flag
  // de hand-off debe sobrevivir a un cierre y reapertura de la pantalla.
  s_pendingRemoteConfirm = Answer::None;
  s_pendingRemoteConfirmRow = nullptr;
  s_pendingRetryRow = nullptr;
  s_pendingGateAnswer = Answer::None;
  destroyTransientOverlayObjects();
}

// Hand-off puro (hallazgo 4): FactoryTest_Close() llama a lv_scr_load(); Poll()
// lo hace fuera del despacho de este click.
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

  // 760x466 (screen 800x480, ~7 px de margen arriba y abajo): D5 necesita
  // sitio para la navegacion de paginas y la barra de progreso + veredicto
  // abajo del todo, que no cabian en los 440 px originales.
  s_card = lv_obj_create(s_overlay);
  lv_obj_set_size(s_card, 760, 466);
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

  // Cuadricula de resultados: flex fila-con-wrap de 3 columnas, paginada (D5:
  // el scroll no se maneja bien con guantes). kPageRowsPerPage filas de
  // kGridBtnH + su espaciado caben exactas en los 214 px de alto; el resto de
  // filas quedan ocultas por applyPageVisibility() hasta que se pasa pagina.
  s_body = lv_obj_create(s_card);
  lv_obj_remove_style_all(s_body);
  lv_obj_set_size(s_body, 720, 214);
  lv_obj_align(s_body, LV_ALIGN_TOP_MID, 0, 52);
  lv_obj_set_flex_flow(s_body, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_clear_flag(s_body, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_row(s_body, 8, 0);
  lv_obj_set_style_pad_column(s_body, 8, 0);

  // Navegacion de paginas (D5): < / > deshabilitados en los extremos
  // (renderAll()), indicador "Pagina i/n" en medio.
  s_pageNavRow = lv_obj_create(s_card);
  lv_obj_remove_style_all(s_pageNavRow);
  lv_obj_set_size(s_pageNavRow, 300, 34);
  lv_obj_align(s_pageNavRow, LV_ALIGN_TOP_MID, 0, 272);
  lv_obj_clear_flag(s_pageNavRow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(s_pageNavRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(s_pageNavRow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(s_pageNavRow, 12, LV_PART_MAIN);

  s_pageBackBtn = makeBtn(s_pageNavRow, "<", onPageBackCb, lv_color_hex(0x0075EE));
  lv_obj_set_size(s_pageBackBtn, 50, 34);

  s_pageLabel = lv_label_create(s_pageNavRow);
  lv_obj_set_style_text_font(s_pageLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_pageLabel, lv_color_hex(0x0B2E4F), 0);
  lv_label_set_text(s_pageLabel, "Pagina 1/1");

  s_pageNextBtn = makeBtn(s_pageNavRow, ">", onPageNextCb, lv_color_hex(0x0075EE));
  lv_obj_set_size(s_pageNextBtn, 50, 34);

  s_action = lv_obj_create(s_card);
  lv_obj_remove_style_all(s_action);
  lv_obj_set_size(s_action, 720, 96);
  lv_obj_align(s_action, LV_ALIGN_TOP_MID, 0, 312);
  lv_obj_clear_flag(s_action, LV_OBJ_FLAG_SCROLLABLE);

  // Barra de progreso y veredicto (D5), abajo del todo de la tarjeta.
  s_bottomRow = lv_obj_create(s_card);
  lv_obj_remove_style_all(s_bottomRow);
  lv_obj_set_size(s_bottomRow, 720, 46);
  lv_obj_align(s_bottomRow, LV_ALIGN_BOTTOM_MID, 0, -6);
  lv_obj_clear_flag(s_bottomRow, LV_OBJ_FLAG_SCROLLABLE);

  s_progressBar = lv_bar_create(s_bottomRow);
  lv_obj_set_size(s_progressBar, 480, 18);
  lv_obj_align(s_progressBar, LV_ALIGN_LEFT_MID, 0, 0);
  lv_bar_set_range(s_progressBar, 0, kExpectedTotal);
  lv_obj_set_style_bg_color(s_progressBar, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_progressBar, lv_color_hex(0x0075EE), LV_PART_INDICATOR);

  s_verdictBox = lv_obj_create(s_bottomRow);
  lv_obj_remove_style_all(s_verdictBox);
  lv_obj_set_size(s_verdictBox, 216, 46);
  lv_obj_align(s_verdictBox, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_set_style_radius(s_verdictBox, 8, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_verdictBox, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(s_verdictBox, LV_OBJ_FLAG_SCROLLABLE);

  s_verdictLabel = lv_label_create(s_verdictBox);
  lv_obj_set_style_text_font(s_verdictLabel, &lv_font_montserrat_20, 0);
  lv_obj_center(s_verdictLabel);
}

void FactoryTest_Open(bool fromSettings) {
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
  s_openedFromSettings = fromSettings;
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
  // hmi-factory-test-settings-entry: "No" en la barrera de entrada (o Salir
  // sin haberla contestado) todavia no arranco ningun test — si se entro
  // desde la fila de Settings, se queda ahi en vez de navegar a
  // ui_ScreenMain. Cualquier cierre posterior (bateria completa o abortada a
  // mitad) conserva el comportamiento de siempre.
  const bool stayInSettings = s_openedFromSettings && s_step == Step::Gate;
  destroyTransientOverlayObjects();
  // Hallazgo de code review: no dejar los botones de fila vivos tras cerrar
  // la pantalla (resetState() los destruiria igualmente en el proximo Open(),
  // pero destroyRowButtons() es idempotente y aqui libera los objetos antes).
  destroyRowButtons();
  if (s_overlay) lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  s_step = Step::Closed;
  g_factoryTestRequested = false;
  if (!stayInSettings) {
    lv_scr_load(ui_ScreenMain);
  }
}

void FactoryTest_RequestOpenFromSettings(void) {
  s_pendingOpenFromSettings = true;
}

bool FactoryTest_IsOpen(void) { return s_step != Step::Closed; }

void FactoryTest_Poll(void) {
  if (s_step == Step::Closed) {
    // hmi-factory-test-settings-entry: hand-off de la fila de Settings,
    // resuelto aqui incluso con el overlay cerrado (el resto de esta funcion
    // bail-outea antes de llegar a este punto en Step::Closed).
    if (s_pendingOpenFromSettings) {
      s_pendingOpenFromSettings = false;
      FactoryTest_Open(true);
    }
    return;
  }

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

  // Hand-off de los botones < / > (hallazgo 4): paginar es una mutacion de
  // estado de UI, no una lectura pura, se resuelve aqui como el resto.
  if (s_pendingPageDelta != 0) {
    const int delta = s_pendingPageDelta;
    s_pendingPageDelta = 0;
    s_currentPage += delta;
    // D5: el operario pagino a mano, no se le mueve hasta el siguiente
    // cambio de test (renderAll() reactiva s_pageFollowsTest ahi).
    s_pageFollowsTest = false;
    renderAll();
    return;
  }

  // Panel de detalle: abrir/cerrar se resuelve aqui, fuera del despacho del
  // click que lo pidio (mismo hand-off que Reintentar, hallazgo 4). Hallazgo
  // de code review: no abrirlo si ya hay una pregunta Si/No remota
  // pendiente — la taparia.
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
        case LocalPhase::Wifi: serviceWifi(); break;
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

// hmi-factory-test-settings-entry: fila "Test de hardware" de
// ui_ScreenSettings. Consulta directamente el estado real del boton
// (LV_STATE_DISABLED) en vez de una cache propia: asi se autocorrige si
// ui_ScreenSettings se llegara a recrear (_ui_screen_delete()) con los
// objetos en su apariencia por defecto.
void FactoryTest_RefreshSettingsRow(void) {
  if (!ui_HwTestButton || !ui_HwTestLabel || !ui_HwTestArrow ||
      !ui_HwTestSubLabel) {
    return;
  }

  const bool enabled = !UI_AnyControlActive();
  const bool wasEnabled =
      !lv_obj_has_state(ui_HwTestButton, LV_STATE_DISABLED);
  if (enabled == wasEnabled) return;  // sin cambios: no repintar

  if (enabled) {
    lv_obj_clear_state(ui_HwTestButton, LV_STATE_DISABLED);
    lv_obj_add_flag(ui_HwTestButton, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_text_color(ui_HwTestLabel, lv_color_hex(0x0B2E4F),
                                LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_HwTestArrow, lv_color_hex(0x0B2E4F),
                                LV_PART_MAIN);
    lv_obj_add_flag(ui_HwTestSubLabel, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_state(ui_HwTestButton, LV_STATE_DISABLED);
    lv_obj_clear_flag(ui_HwTestButton, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_text_color(ui_HwTestLabel, lv_color_hex(0x9AA0A6),
                                LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_HwTestArrow, lv_color_hex(0x9AA0A6),
                                LV_PART_MAIN);
    lv_obj_clear_flag(ui_HwTestSubLabel, LV_OBJ_FLAG_HIDDEN);
  }
}

