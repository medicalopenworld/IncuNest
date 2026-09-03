#include "UITask.h"
// #include "AudioManager.h"  // Deshabilitado para migración Arduino 3.x
#include "CommTask.h"
#include "buzzer.h"
#include "ui/AlarmCenter.h"
#include "ui/TelemetryHistory.h"
#include "ui/BabyHistory.h"
#include "ui/BabyExitDialog.h"
#include "ui/TimeDialog.h"
#include "ui/HelpDialog.h"
#include "ui/HelpTour.h"
#include "ui/BabyWizard.h"
#include "display_config.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
// alarm_priority(): la prioridad del banner sale de la misma tabla de shared/
// que usa la motherboard, no de una copia local.
#include "alarm_policy.h"
// Y el RITMO de la senal acustica sale del mismo sitio, por lo mismo: los dos
// zumbadores del equipo tienen que emitir el patron identico.
#include "alarm_audio_pattern.h"
#include "main.h"
#include "ui.h"
#include <PCA9557.h>
#include <SPI.h>
#include <TAMC_GT911.h>

static const char *TAG = "UI";

SemaphoreHandle_t g_lvgl_mutex = NULL;

void LVGL_Mutex_Init(void) {
  if (!g_lvgl_mutex) {
    g_lvgl_mutex = xSemaphoreCreateRecursiveMutex();
  }
}

// --- Skin probe UI (RF-SKIN-008/009/010, UI-SKIN-002/003/004) ---
// Also the generic toast surface for the whole UI — see UI_ShowToast().
static lv_obj_t *ui_SkinProbeToast =
    nullptr; // Toast de bloqueo cuando sonda no válida

// --- Power-off popup UI elements ---
static lv_obj_t *ui_PwrOffOverlay = nullptr;
static lv_obj_t *ui_PwrOffArc = nullptr;
static lv_obj_t *ui_PwrOffSymbol = nullptr;
static bool pwrOffPopupVisible = false;

static lv_obj_t *ui_AudioPlayBtn;
static lv_obj_t *ui_AudioStopBtn;
static lv_obj_t *ui_AudioPlayLabel;
static lv_obj_t *ui_VolumeLabel; // Muestra "Vol: XX"
static lv_obj_t *ui_VolumeUpBtn;
static lv_obj_t *ui_VolumeDownBtn;

// Novedades para Gráficas Históricas (Declaradas en ElementsCreation.cpp)

// Series de datos (Se definen aquí para ser usadas por UITask)


// ==========================================
// Globals
// ==========================================
double airTempValue, skinTempValue;
volatile double airTempValueDetected = 0.00, skinTempValueDetected = 0.00;
int humValue;
volatile int humValueDetected = 0;
int selectedPanel = NO_PANEL_SELECTED;
int lastSelectedPanel = NO_PANEL_SELECTED;
bool skinPanelEnabled = false;
bool switchTemp = false;
bool switchHum = false;
bool tempSwitched = false;
bool humSwitched = false;
bool arrowsActive = false;
bool darkMode = false;
bool humidityEnabled = false;
int g_selectedAlarmId = -1;
volatile bool g_pendingAlarmUpdate = false;

bool alarmActive = false;
bool alarmsMuted = false;
// Hasta este instante, CTRL,STATE no puede revertir alarmsMuted. Cubre el
// viaje de ida y vuelta del comando de silencio (ver CommTask.cpp).
uint32_t g_muteHoldUntilMs = 0;
#define MUTE_HOLD_MS 2000u

bool prevTempAlarm = false;
bool prevHumAlarm = false;
int alarmSlotToIndex[MAX_ALARM_DISPLAY] = {-1, -1, -1, -1};

int chartLastPressed = -1;

bool wifiVisible = false;
bool isConnected = false;
static bool pendingReconnect = false;
static uint32_t disconnectTimestampMs = 0;
char wifi_ssid[64] = "";
char wifi_pass[64] = "";

bool LanguagesVisible = false;
bool locked = true;

static bool spo2ProbeAttached = false;

lv_chart_series_t *airTempSeries = NULL;
lv_chart_series_t *skinTempSeries = NULL;
lv_chart_series_t *humSeries = NULL;
lv_chart_series_t *lockPPGSeries = NULL;

bool g_stateSynced = false;
uint32_t g_lastStateReqMs = 0;
bool g_ui_initialized = false;

static bool eepromDirty = false;
static unsigned long lastVarChangeTime = 0;

ui_lang_t g_lang = LANG_EN;

// Phototherapy Timer
int photoTimerMinutes = PHOTO_TIMER_DEFAULT_MINUTES;
bool photoTimerActive = false;
unsigned long photoTimerStartMs = 0;
static bool g_photo_safety_confirmed = false;
// Same one-shot gate pattern as g_photo_safety_confirmed, for the baby-data
// wizard that now precedes phototherapy activation: the wizard re-triggers
// the switch when it finishes, and this flag lets that second pass through.
static bool g_photo_wizard_done = false;
// Same one-shot gate, for the baby-data wizard that now precedes humidity
// activation. Humidity hours are accounted per baby, so the profile has to be
// known before the humidifier starts.
static bool g_hum_wizard_done = false;
static int lastPhotoMinutesSent = -1;

Alarm alarmList[MAX_ALARMS];

// ==========================================
// esp_lcd RGB Panel (bounce buffers anti-flicker)
// ==========================================
static esp_lcd_panel_handle_t lcd_panel = NULL;

// Bounce buffer: N líneas en SRAM interno desacoplan LCD DMA de PSRAM.
// N=24 (divisor exacto de las 480 líneas del panel, evita wraps parciales)
// amplía el margen de "drenaje" frente a jitter de CPU/bus (p.ej. tráfico
// WiFi) sin comprometer el heap interno — medido con WiFi conectado:
// heap_int_min=87980 B (ver log [DIAG]); el salto de 20→24 líneas cuesta
// ~12.8 KB adicionales, dejando margen holgado para picos de TLS/mbedTLS.
// NO cubre stalls largos (borrado de sector NVS, hasta ~30 ms, ver comentario
// en UITask.cpp:doNVSWrite) — eso se ataca con WiFi.persistent(false)
// (Wifi_OTA.cpp) y CONFIG_SPIRAM_FETCH_INSTRUCTIONS/RODATA (platformio.ini).
// 800 * 24 * 2 = 38.4KB por bounce buffer (x2 ping-pong = ~76.8KB SRAM)
#define BOUNCE_BUF_LINES 24
#define BOUNCE_BUF_SIZE_PX (DISPLAY_WIDTH * BOUNCE_BUF_LINES)

// ==========================================
// LCD Glitch diagnostics
// ==========================================
// Frame interval > este umbral se considera frame lento (glitch potencial)
#define LCD_VSYNC_SLOW_THRESHOLD_US 25000 // 25 ms → <40 fps

static volatile uint32_t g_lcd_bounce_empty_count = 0; // underruns DMA (solo aplica con num_fbs=0; con num_fbs=1 este callback no se invoca — ver g_lcd_bounce_frame_finish_count)
static volatile uint32_t g_lcd_vsync_slow_count = 0;   // frames lentos
static volatile uint32_t g_lcd_vsync_total = 0;        // frames totales
static volatile uint32_t g_lcd_bounce_frame_finish_count = 0; // ciclos de bounce buffer completados
static volatile int64_t g_lcd_last_vsync_us = 0;
static volatile int64_t g_lcd_worst_frame_us = 0; // peor intervalo visto

static bool IRAM_ATTR lcd_on_bounce_empty(esp_lcd_panel_handle_t panel,
                                          void *bounce_buf, int pos_px,
                                          int len_bytes, void *user_ctx) {
  g_lcd_bounce_empty_count++;
  return false;
}

// Con num_fbs=1 el driver rellena el bounce buffer por memcpy (no llama a
// on_bounce_empty). Este callback SÍ se invoca una vez por cada ciclo
// completo de bounce buffer (~1 por frame). Si el DMA se reinicia por un
// underrun (desync de un frame, ver esp_lcd_panel_rgb.c:
// lcd_rgb_panel_try_restart_transmission), ocurre dentro del vblank y no
// altera el intervalo entre vsyncs — por eso este contador es la única señal
// disponible sin parchear el driver vendored.
static bool IRAM_ATTR lcd_on_bounce_frame_finish(esp_lcd_panel_handle_t panel,
                                                 const esp_lcd_rgb_panel_event_data_t *edata,
                                                 void *user_ctx) {
  g_lcd_bounce_frame_finish_count++;
  return false;
}

static bool IRAM_ATTR lcd_on_vsync(esp_lcd_panel_handle_t panel,
                                   const esp_lcd_rgb_panel_event_data_t *edata,
                                   void *user_ctx) {
  int64_t now = esp_timer_get_time();
  if (g_lcd_last_vsync_us > 0) {
    int64_t delta = now - g_lcd_last_vsync_us;
    if (delta > g_lcd_worst_frame_us)
      g_lcd_worst_frame_us = delta;
    if (delta > LCD_VSYNC_SLOW_THRESHOLD_US)
      g_lcd_vsync_slow_count++;
  }
  g_lcd_last_vsync_us = now;
  g_lcd_vsync_total++;
  return false;
}

// Llamar periódicamente desde una tarea para volcar diagnóstico al log
void lcd_diagnostics_log() {
  uint32_t underruns = g_lcd_bounce_empty_count;
  uint32_t slow_frames = g_lcd_vsync_slow_count;
  uint32_t total = g_lcd_vsync_total;
  uint32_t frame_finish = g_lcd_bounce_frame_finish_count;
  int64_t worst_us = g_lcd_worst_frame_us;

  // Reset contadores después de leer
  g_lcd_bounce_empty_count = 0;
  g_lcd_vsync_slow_count = 0;
  g_lcd_vsync_total = 0;
  g_lcd_bounce_frame_finish_count = 0;
  g_lcd_worst_frame_us = 0;

  float fps = (worst_us > 0) ? (1000000.0f / worst_us) : 0.0f;

  // frame_finish debería igualar total (±1-2 por skew de lectura no atómica
  // entre ISR y esta función) en régimen normal. Una desviación mayor indica
  // ciclos de bounce buffer re-arrancados por underrun — el modo de fallo
  // que "underruns"/"slow_frames" NO detectan con num_fbs=1. Tolerancia=2
  // es una calibración inicial: si en campo aparecen falsos positivos
  // frecuentes con mismatch=2 y sin glitch visible reportado, subirla a 3.
  int32_t frame_mismatch = (int32_t)total - (int32_t)frame_finish;
  if (frame_mismatch < 0) frame_mismatch = -frame_mismatch;

  if (underruns > 0 || slow_frames > 0 || frame_mismatch > 2) {
    ESP_LOGW("LCD_DIAG",
             "GLITCH DETECTED — underruns=%lu  slow_frames=%lu/%lu  "
             "frame_finish=%lu (mismatch=%ld)  worst_frame=%.1fms (%.1f fps)",
             underruns, slow_frames, total, frame_finish, (long)frame_mismatch,
             worst_us / 1000.0f, fps);
  } else {
    // Bench-debug: imprime siempre para ver contención sub-umbral (worst_frame
    // por debajo de 25ms) que "GLITCH DETECTED" no reporta. Correlar con
    // glitches vistos a simple vista sin evento de WiFi/flash en el log.
    ESP_LOGI("LCD_DIAG", "ok — frames=%lu  worst_frame=%.2fms (%.1f fps)",
             total, worst_us / 1000.0f, fps);
  }
}

static uint32_t g_currentFreqWrite = DISPLAY_FREQ_WRITE;

uint32_t lcd_get_freq_write() { return g_currentFreqWrite; }

void lcd_set_freq_write(uint32_t freq_hz) {
  g_currentFreqWrite = freq_hz;
  { Preferences p; p.begin(HMI_NS_CFG, false); p.putUInt(HMI_KEY_DISP_FREQ, freq_hz); p.end(); }
  ESP_LOGW("LCD", "freq_write saved: %lu Hz — restarting...", freq_hz);
  delay(200);
  ESP.restart();
}

// Touch GT911 — pines y resolución desde display_config.h
TAMC_GT911 ts =
    TAMC_GT911(DISPLAY_TOUCH_SDA, DISPLAY_TOUCH_SCL, DISPLAY_TOUCH_INT,
               DISPLAY_TOUCH_RST, DISPLAY_WIDTH, DISPLAY_HEIGHT);

static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t disp_draw_buf[DISPLAY_WIDTH * DISPLAY_HEIGHT / COLOR_DIVISOR];
static lv_disp_drv_t disp_drv;
static lv_timer_t *intro_timer = NULL;
static uint32_t intro_start_ms = 0;

// ==========================================
// Definitions
// ==========================================
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area,
                   lv_color_t *color_p) {
  int offsetx1 = area->x1;
  int offsetx2 = area->x2;
  int offsety1 = area->y1;
  int offsety2 = area->y2;
  esp_lcd_panel_draw_bitmap(lcd_panel, offsetx1, offsety1, offsetx2 + 1,
                            offsety2 + 1, color_p);
  lv_disp_flush_ready(disp);
}

static void intro_timer_cb(lv_timer_t *t) {
  uint32_t elapsed = millis() - intro_start_ms;
  bool synced      = g_stateSynced;
  bool min_elapsed = (elapsed >= 5000);
  bool timedout    = (elapsed >= 15000);
  if (!timedout && (!synced || !min_elapsed)) return;
  if (intro_timer) {
    lv_timer_del(intro_timer);
    intro_timer = NULL;
  }
  lv_scr_load(ui_ScreenMain);
  if (!synced)
    ESP_LOGW(TAG, "Intro: no state sync after 10s — proceeding anyway");
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  static uint16_t lastX = 0, lastY = 0;
  static uint8_t releaseCount = 0;
  // GT911 clears its buffer after each I2C read, causing phantom releases when
  // LVGL reads faster than the controller updates. Require 3 consecutive
  // "not touched" frames (~30ms at 10ms/frame) before reporting RELEASED.
  const uint8_t DEBOUNCE_FRAMES = 3;

  ts.read();
  if (ts.isTouched) {
    releaseCount = 0;
    lastX = ts.points[0].x;
    lastY = ts.points[0].y;
    data->state = LV_INDEV_STATE_PR;
    data->point.x = lastX;
    data->point.y = lastY;
  } else {
    if (releaseCount < DEBOUNCE_FRAMES) {
      releaseCount++;
      data->state = LV_INDEV_STATE_PR;
      data->point.x = lastX;
      data->point.y = lastY;
    } else {
      data->state = LV_INDEV_STATE_REL;
    }
  }
}

// Forward declarations
void set_active_panel(lv_obj_t *active, lv_obj_t *inactive);
void temp_chart_show_for_selected_panel(void);
void ui_set_switch_state_silent(lv_obj_t *sw, bool on);
void computeAndSendActuation(void);
void ActivateTempControlUI(bool isAirMode);

// Fully disables Skin mode — same effect as flipping the Skin switch OFF.
// Call this both from the switch-OFF handler and from the probe-disconnect
// path.
void UI_UpdatePowerBars(int tempPwm, int humPwm) {
  if (ui_AirPowerBar)  lv_bar_set_value(ui_AirPowerBar,  tempPwm, LV_ANIM_ON);
  if (ui_SkinPowerBar) lv_bar_set_value(ui_SkinPowerBar, tempPwm, LV_ANIM_ON);
  if (ui_HumPowerBar)  lv_bar_set_value(ui_HumPowerBar,  humPwm,  LV_ANIM_ON);
}

static void skin_mode_force_off() {
  // Uncheck the switch FIRST — line 3219 re-reads skinPanelEnabled from it
  // every cycle, so leaving it checked would immediately undo everything else.
  ui_set_switch_state_silent(ui_Switch4, false);

  skinPanelEnabled = false;
  hmi_msg.skinModeEnabled = false;
  hmi_msg.shouldSendData = true;

  if (ui_SkinPanelCont)
    lv_obj_add_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);
  lastSelectedPanel = AIR_PANEL_SELECTED;
  if (ui_AirTempBarCont)
    lv_obj_clear_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);

  if (selectedPanel == SKIN_PANEL_SELECTED) {
    selectedPanel = AIR_PANEL_SELECTED;
    lastSelectedPanel = selectedPanel;

    set_active_panel(ui_AirPanel, ui_SkinPanel);

    if (ui_AirTempBarCont)
      lv_obj_clear_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_SkinTempBarCont)
      lv_obj_add_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_AirTempLockCont)
      lv_obj_clear_flag(ui_AirTempLockCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_TargetAirTempCont)
      lv_obj_clear_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_SkinTempLockCont)
      lv_obj_add_flag(ui_SkinTempLockCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_TargetSkinTempCont)
      lv_obj_add_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);

    if (tempSwitched) {
      hmi_msg.controlMode = CONTROL_AIR;
      temp_chart_show_for_selected_panel();
    }
  }
}

// Ultimo valor pintado de cada MEDIDA. A nivel de fichero y no dentro de
// update_labels() porque link_lost_blank_update() tiene que invalidarlos: al
// recuperar el enlace, la primera lectura debe volver a pintarse aunque
// coincida con la ultima buena, o los "--" se quedarian puestos.
static double l_airDet = -1.0, l_skinDet = -1.0;
static int l_humDet = -1;

// Borra las MEDIDAS mientras la placa este callada, y las barras con ellas.
//
// Es la parte de seguridad del asunto: una cifra congelada no se ve
// congelada. El operador lee 36,5 °C y entiende que el bebe esta a 36,5 °C
// AHORA, cuando es lo ultimo que llego antes de caerse el enlace. Un "--" no
// se puede malinterpretar.
//
// Las CONSIGNAS no se tocan: son lo que el operador pidio, no una medida, y
// siguen siendo ciertas. Lo que ha dejado de saberse es si se cumplen.
//
// Corre en CADA pasada del bucle de UI, no desde update_labels(): a esa solo
// se la llama cuando llega telemetria, que es exactamente lo que deja de
// pasar cuando el enlace cae.
// Reloj de pared de la cabecera.
//
// La cadena se recalcula en cada pasada pero SOLO se escribe cuando cambia:
// repintar dos labels a 20 Hz para que cambien una vez por minuto es trabajo
// tirado, y el repintado innecesario ya dio problemas de fluidez aqui.
//
// "Sin hora" solo cuando no hay ni epoch: sin eso no hay nada que pintar. Con
// epoch pero sin zona resuelta se pinta igual, sin aplicar offset (el epoch
// tal cual, probablemente UTC) — mismo criterio que ya usan AlarmCenter.cpp y
// BabyHistory.cpp (HMI_HasLocalTime() ? HMI_ToLocal(epoch) : epoch). Decision
// de producto 2026-09-02: una hora aproximada es mas util que ninguna, y el
// operador la corrige a mano si hace falta (tocar el reloj -> Adjust Time).
void clock_update(void) {
  if (!ui_ClockTime || !ui_ClockDate) return;

  static char lastTime[8] = {0};
  static char lastDate[12] = {0};
  char nowTime[8];
  char nowDate[12];

  const uint32_t epochNow = HMI_GetEpochNow();
  if (epochNow == 0) {
    // Orden ES/EN/FR, como ui_lang_t en main.h.
    static const char *const TXT_NO_TIME[] = {"Sin hora", "No time",
                                              "Sans heure"};
    snprintf(nowTime, sizeof(nowTime), "%s", TXT_NO_TIME[g_lang]);
    nowDate[0] = '\0';
  } else {
    const time_t t =
        (time_t)(HMI_HasLocalTime() ? HMI_ToLocal(epochNow) : epochNow);
    struct tm tmv;
    gmtime_r(&t, &tmv);
    snprintf(nowTime, sizeof(nowTime), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
    snprintf(nowDate, sizeof(nowDate), "%02d/%02d/%04d", tmv.tm_mday,
             tmv.tm_mon + 1, tmv.tm_year + 1900);
  }

  if (strcmp(nowTime, lastTime) != 0) {
    strncpy(lastTime, nowTime, sizeof(lastTime) - 1);
    lastTime[sizeof(lastTime) - 1] = '\0';
    lv_label_set_text(ui_ClockTime, nowTime);
    // Replica del reloj en el heading de ui_ScreenLock (ver "mantener
    // IncuNest/reloj/conectividad en la pantalla de bloqueo").
    if (ui_LockHeadingClockTime) lv_label_set_text(ui_LockHeadingClockTime, nowTime);
  }
  if (strcmp(nowDate, lastDate) != 0) {
    strncpy(lastDate, nowDate, sizeof(lastDate) - 1);
    lastDate[sizeof(lastDate) - 1] = '\0';
    lv_label_set_text(ui_ClockDate, nowDate);
    if (ui_LockHeadingClockDate) lv_label_set_text(ui_LockHeadingClockDate, nowDate);
  }
}

// Estado del enlace de la PLACA en la pantalla de WiFi.
//
// El resto de esa pantalla habla del radio del propio display, que solo sirve
// para su OTA. La motherBoard es quien sube telemetria, y su conexion no
// aparecia: se podia leer "conectado" mientras el equipo no mandaba un dato.
// Etiquetado explicitamente para que no se confunda con lo de al lado.
//
// Se refresca solo con la pantalla visible, y solo escribe cuando cambia.
void wifi_board_status_update(void) {
  if (!ui_WifiBoardStatus) return;
  if (!wifiVisible) return;

  // Orden ES/EN/FR, como ui_lang_t en main.h.
  static const char *const TXT_BOARD[] = {"Placa: ", "Board: ", "Carte : "};

  char now[64];
  snprintf(now, sizeof(now), "%s%s", TXT_BOARD[g_lang],
           getConnectivityString(ctrl_state_msg.serverCommStatus, g_lang));

  static char last[64] = {0};
  if (strcmp(now, last) == 0) return;
  strncpy(last, now, sizeof(last) - 1);
  last[sizeof(last) - 1] = '\0';
  lv_label_set_text(ui_WifiBoardStatus, now);

  // Verde solo con servidor: tener WiFi pero no llegar a ThingsBoard es
  // exactamente el caso que esta pantalla tenia que dejar de ocultar.
  const bool onServer =
      (ctrl_state_msg.serverCommStatus == COMM_STATUS_WIFI_SERVER ||
       ctrl_state_msg.serverCommStatus == COMM_STATUS_GPRS_SERVER);
  lv_obj_set_style_text_color(ui_WifiBoardStatus,
                              onServer ? lv_color_hex(0x2E9E4F)
                                       : lv_color_hex(0xCC7A00),
                              LV_PART_MAIN);
}

// Aplica el estado de conectividad a UNA instancia del indicador (icono +
// 4 barras). Factorizado para que ui_ScreenMain y su replica en
// ui_ScreenLock (ver "mantener IncuNest/reloj/conectividad en la pantalla de
// bloqueo") pinten exactamente igual sin duplicar la logica. `lastSignature`
// es el cache de "no cambio nada desde la ultima pasada" de ESA instancia:
// cada llamador pasa su propia estatica, para que la primera pintura de una
// instancia no se salte solo porque la otra ya estaba al dia.
static void apply_connectivity_indicator(lv_obj_t *icon, lv_obj_t *bars[4],
                                          int status, int linkBars,
                                          int *lastSignature) {
  if (!icon) return;

  const int signature = status * 100 + (linkBars + 1);
  if (signature == *lastSignature) return;
  *lastSignature = signature;

  const bool onServer =
      (status == COMM_STATUS_WIFI_SERVER || status == COMM_STATUS_GPRS_SERVER);
  const bool hasTransport = (status != COMM_STATUS_NONE);
  const lv_color_t color = !hasTransport  ? lv_color_hex(0x888888)
                            : onServer    ? lv_color_hex(0x2E9E4F)
                                          : lv_color_hex(0xCC7A00);

  const char *iconTxt;
  switch (status) {
  case COMM_STATUS_WIFI_ONLY:
  case COMM_STATUS_WIFI_SERVER:
    iconTxt = LV_SYMBOL_WIFI;
    break;
  case COMM_STATUS_GPRS_ONLY:
  case COMM_STATUS_GPRS_SERVER:
    iconTxt = "2G";
    break;
  default:
    iconTxt = LV_SYMBOL_CLOSE;
    break;
  }
  lv_label_set_text(icon, iconTxt);
  lv_obj_set_style_text_color(icon, color, LV_PART_MAIN);

  // Sin transporte no hay cobertura que mostrar; con transporte pero sin dato
  // fiable (linkBars<0: placa antigua o lectura de RSSI/CSQ aun no llegada)
  // se pintan las 4 en gris, nunca se inventa un nivel.
  const int fillCount =
      (!hasTransport) ? 0 : (linkBars < 0) ? 0 : (linkBars > 4 ? 4 : linkBars);
  for (int i = 0; i < 4; i++) {
    if (!bars[i]) continue;
    if (!hasTransport) {
      lv_obj_add_flag(bars[i], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_clear_flag(bars[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(
        bars[i], (i < fillCount) ? color : lv_color_hex(0x404040),
        LV_PART_MAIN);
  }
}

// Indicador de conectividad del heading, visible en TODAS las pantallas (a
// diferencia de wifi_board_status_update, que solo vive en Ajustes/WiFi): un
// icono WIFI/2G/sin enlace mas 4 barras de cobertura, para que el operador
// vea de un vistazo si el equipo puede subir telemetria sin tener que entrar
// a Ajustes. Se pinta tanto en ui_ScreenMain como en su replica de
// ui_ScreenLock, cada una con su propio cache de "sin cambios".
//
// Con el enlace HMI-placa caido (Display_IsBoardLinkLost()) no hay dato
// fresco del que fiarse: se pinta como "sin datos", igual criterio que
// link_lost_blank_update() aplica a las medidas.
void connectivity_heading_update(void) {
  const bool linkLost = Display_IsBoardLinkLost();
  const int status = linkLost ? COMM_STATUS_NONE : ctrl_state_msg.serverCommStatus;
  const int linkBars = linkLost ? -1 : ctrl_state_msg.linkBars;

  static int lastSignatureMain = -999;
  apply_connectivity_indicator(ui_ConnIcon, ui_ConnBar, status, linkBars,
                                &lastSignatureMain);

  static int lastSignatureLock = -999;
  apply_connectivity_indicator(ui_LockHeadingConnIcon, ui_LockHeadingConnBar,
                                status, linkBars, &lastSignatureLock);
}

void link_lost_blank_update(void) {
  static bool wasLost = false;
  const bool lost = Display_IsBoardLinkLost();
  if (!lost) {
    wasLost = false;
    return;
  }
  if (wasLost) {
    return;  // ya estan en blanco; no hay que reescribir en cada pasada
  }
  wasLost = true;

  l_airDet = l_skinDet = -1.0;
  l_humDet = -1;

  const char *DEAD = "--";
  lv_label_set_text(ui_TempAirDetected, DEAD);
  lv_label_set_text(ui_TempAirDetectedRight, DEAD);
  lv_label_set_text(ui_Label18, DEAD);
  lv_label_set_text(ui_TempSkinDetected, DEAD);
  lv_label_set_text(ui_TempSkinDetectedRight, DEAD);
  lv_label_set_text(ui_Label14, DEAD);
  lv_label_set_text(ui_HumDetected, DEAD);
  lv_label_set_text(ui_HumDetectedRight, DEAD);
  lv_label_set_text(ui_Label20, DEAD);

  lv_bar_set_value(ui_AirTempBar, 0, LV_ANIM_OFF);
  lv_bar_set_value(ui_SkinTempBar, 0, LV_ANIM_OFF);
  lv_bar_set_value(ui_HumBar, 0, LV_ANIM_OFF);
}

void update_labels() {
  if (!g_ui_initialized)
    return;

  static double l_airDesired = -1.0, l_skinDesired = -1.0;
  static int l_humDesired = -1;
  static int l_photoMins = -1;

  char buffer[BUFFER_SIZE];

  if (abs(airTempValue - l_airDesired) > TEMP_LABEL_UPDATE_THRESHOLD) {
    l_airDesired = airTempValue;
    snprintf(buffer, sizeof(buffer), "%.1f°C", airTempValue);
    lv_label_set_text(ui_TempAirDesired, buffer);
    lv_label_set_text(ui_TargetAirTempNumLabel, buffer);
  }

  if (abs(skinTempValue - l_skinDesired) > TEMP_LABEL_UPDATE_THRESHOLD) {
    l_skinDesired = skinTempValue;
    snprintf(buffer, sizeof(buffer), "%.1f°C", skinTempValue);
    lv_label_set_text(ui_TempSkinDesired, buffer);
    lv_label_set_text(ui_TargetSkinTempNumLabel, buffer);
  }

  if (humValue != l_humDesired) {
    l_humDesired = humValue;
    snprintf(buffer, sizeof(buffer), "%d%%", humValue);
    lv_label_set_text(ui_HumDesired, buffer);
    lv_label_set_text(ui_Label24, buffer);
  }

  // Con el enlace caido NO se pintan medidas, y la guarda va AQUI DENTRO.
  //
  // Ponerla solo en link_lost_blank_update() no bastaba: update_labels()
  // tiene ocho llamantes (las flechas de consigna, el cambio de idioma, el
  // arranque...), asi que en cuanto se tocaba cualquier cosa las cifras
  // viejas volvian a pintarse un segundo despues de haberlas borrado, sin
  // que el enlace se hubiera recuperado. Fallo observado en banco.
  //
  // Con la guarda dentro da igual quien llame: mientras la placa calle, la
  // unica medida que se puede pintar es "no la hay".
  const bool linkLost = Display_IsBoardLinkLost();

  // Cada medida se juzga POR SEPARADO. El enlace puede estar perfectamente
  // vivo y faltar un solo sensor, y hasta ahora solo la caida del enlace tenia
  // derecho a "--": un sensor retirado pintaba el 0 que mandaba la placa, que
  // es un numero plausible y alarmante en vez de la ausencia de dato que es.
  // Mismo criterio para las dos causas — lo que se ha perdido es saberlo.
  const bool airOk =
      !linkLost && !PROTO_TEL_TEMP_IS_UNAVAILABLE(airTempValueDetected);
  const bool skinOk =
      !linkLost && !PROTO_TEL_TEMP_IS_UNAVAILABLE(skinTempValueDetected);
  const bool humOk = !linkLost && humValueDetected != PROTO_TEL_HUM_UNAVAILABLE;

  // Para no reescribir los mismos "--" en cada pasada, igual que hacen los
  // ultimos valores pintados con las medidas buenas.
  static bool airDead = false, skinDead = false, humDead = false;
  const char *NO_DATA = "--";

  if (airOk) {
    airDead = false;
    if (abs(airTempValueDetected - l_airDet) > TEMP_LABEL_UPDATE_THRESHOLD) {
      l_airDet = airTempValueDetected;
      snprintf(buffer, sizeof(buffer), "%.1f°C", airTempValueDetected);
      lv_label_set_text(ui_TempAirDetected, buffer);
      lv_label_set_text(ui_TempAirDetectedRight, buffer);
      lv_label_set_text(ui_Label18, buffer);
    }
  } else if (!airDead) {
    airDead = true;
    l_airDet = -1.0;  // fuerza repintado cuando el sensor vuelva
    lv_label_set_text(ui_TempAirDetected, NO_DATA);
    lv_label_set_text(ui_TempAirDetectedRight, NO_DATA);
    lv_label_set_text(ui_Label18, NO_DATA);
  }

  if (skinOk) {
    skinDead = false;
    if (abs(skinTempValueDetected - l_skinDet) > TEMP_LABEL_UPDATE_THRESHOLD) {
      l_skinDet = skinTempValueDetected;
      snprintf(buffer, sizeof(buffer), "%.1f°C", skinTempValueDetected);
      lv_label_set_text(ui_TempSkinDetected, buffer);
      lv_label_set_text(ui_TempSkinDetectedRight, buffer);
      lv_label_set_text(ui_Label14, buffer);
    }
  } else if (!skinDead) {
    skinDead = true;
    l_skinDet = -1.0;
    lv_label_set_text(ui_TempSkinDetected, NO_DATA);
    lv_label_set_text(ui_TempSkinDetectedRight, NO_DATA);
    lv_label_set_text(ui_Label14, NO_DATA);
  }

  if (humOk) {
    humDead = false;
    if (humValueDetected != l_humDet) {
      l_humDet = humValueDetected;
      snprintf(buffer, sizeof(buffer), "%d%%", humValueDetected);
      lv_label_set_text(ui_HumDetected, buffer);
      lv_label_set_text(ui_HumDetectedRight, buffer);
      lv_label_set_text(ui_Label20, buffer);
    }
  } else if (!humDead) {
    humDead = true;
    l_humDet = -1;
    lv_label_set_text(ui_HumDetected, NO_DATA);
    lv_label_set_text(ui_HumDetectedRight, NO_DATA);
    lv_label_set_text(ui_Label20, NO_DATA);
  }

  int airBar =
      (airTempValueDetected <= TEMP_BAR_DISPLAY_MIN)
          ? 0
          : (airTempValueDetected >= TEMP_BAR_DISPLAY_MAX
                 ? TEMP_BAR_RANGE
                 : (int)round(airTempValueDetected - TEMP_BAR_DISPLAY_MIN));
  int skinBar =
      (skinTempValueDetected <= TEMP_BAR_DISPLAY_MIN)
          ? 0
          : (skinTempValueDetected >= TEMP_BAR_DISPLAY_MAX
                 ? TEMP_BAR_RANGE
                 : (int)round(skinTempValueDetected - TEMP_BAR_DISPLAY_MIN));
  int humBar = constrain(humValueDetected, HUM_BAR_MIN, HUM_BAR_MAX);

  // Las barras salen de las mismas medidas muertas: a cero cuando la medida no
  // esta disponible, o dibujarian un nivel que ya nadie esta midiendo. Se juzga
  // por medida y no por enlace, por lo mismo que las cifras: con la sonda
  // retirada, una barra de piel a media altura seria igual de mentirosa.
  lv_bar_set_value(ui_AirTempBar, airOk ? airBar : 0, LV_ANIM_OFF);
  lv_bar_set_value(ui_SkinTempBar, skinOk ? skinBar : 0, LV_ANIM_OFF);
  lv_bar_set_value(ui_HumBar, humOk ? humBar : 0, LV_ANIM_OFF);

  // Update photo timer label if not active
  if (!photoTimerActive) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d:%02d",
             photoTimerMinutes / SECONDS_PER_MINUTE,
             photoTimerMinutes % SECONDS_PER_MINUTE);
    lv_label_set_text(ui_PhotoTimeValueLabel, buf);
  }

  // Derive skin probe presence from detected temperature and update switch
  // visibility. lastProbePresent starts as false so the first call with
  // skinTempValueDetected=0 (no TEL received yet) does NOT trigger a spurious
  // "probe lost" event that would undo state restoration done by
  // Display_ApplyCtrlState.
  static bool lastProbePresent = false;
  bool probePresent = (skinTempValueDetected > SKIN_PROBE_DETECT_THRESHOLD);
  if (probePresent != lastProbePresent) {
    lastProbePresent = probePresent;
    g_skinProbeState =
        probePresent ? SKIN_PROBE_VALID : SKIN_PROBE_NOT_CONNECTED;
    if (probePresent) {
      if (ui_Switch4)
        lv_obj_clear_flag(ui_Switch4, LV_OBJ_FLAG_HIDDEN);
      const char *text = (g_lang == LANG_ES)   ? "MODO PIEL"
                         : (g_lang == LANG_FR) ? "MODE PEAU"
                                               : "SKIN MODE";
      if (ui_SkinOptionLabel)
        lv_label_set_text(ui_SkinOptionLabel, text);
    } else {
      if (ui_Switch4)
        lv_obj_add_flag(ui_Switch4, LV_OBJ_FLAG_HIDDEN);
      const char *text = (g_lang == LANG_ES)   ? "SIN SONDA DE PIEL"
                         : (g_lang == LANG_FR) ? "SONDE PEAU ABSENTE"
                                               : "NO SKIN PROBE DETECTED";
      if (ui_SkinOptionLabel)
        lv_label_set_text(ui_SkinOptionLabel, text);
      if (skinPanelEnabled) {
        skin_mode_force_off();
      } else if (ui_SkinPanelCont) {
        lv_obj_add_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);
      }
    }
  }
}

const char *getConnectivityString(int status, ui_lang_t lang) {
  switch (status) {
  case COMM_STATUS_NONE:
    if (lang == LANG_ES)
      return "DESCONECTADO";
    if (lang == LANG_FR)
      return "DECONNECTE";
    return "DISCONNECTED";
  case COMM_STATUS_GPRS_ONLY:
    if (lang == LANG_ES)
      return "2G (SIN SERVER)";
    if (lang == LANG_FR)
      return "2G (SANS SERVEUR)";
    return "2G (NO SERVER)";
  case COMM_STATUS_GPRS_SERVER:
    if (lang == LANG_ES)
      return "2G + SERVIDOR";
    if (lang == LANG_FR)
      return "2G + SERVEUR";
    return "2G + SERVER";
  case COMM_STATUS_WIFI_ONLY:
    if (lang == LANG_ES)
      return "WIFI (SIN SERVER)";
    if (lang == LANG_FR)
      return "WIFI (SANS SERVEUR)";
    return "WIFI (NO SERVER)";
  case COMM_STATUS_WIFI_SERVER:
    if (lang == LANG_ES)
      return "WIFI + SERVIDOR";
    if (lang == LANG_FR)
      return "WIFI + SERVEUR";
    return "WIFI + SERVER";
  default:
    return "-";
  }
}

static void photo_safety_apply_language(ui_lang_t lang) {
  if (!ui_PhotoSafetyModal)
    return;
  lv_label_set_text(ui_PhotoSafetyTitleLabel,
      (lang == LANG_ES)   ? "PROTECCION OCULAR OBLIGATORIA"
      : (lang == LANG_FR) ? "PROTECTION OCULAIRE OBLIGATOIRE"
                          : "EYE PROTECTION REQUIRED");
  lv_label_set_text(ui_PhotoSafetyBodyLabel,
      (lang == LANG_ES)
          ? "Coloque proteccion ocular al paciente\nantes de activar la fototerapia."
      : (lang == LANG_FR)
          ? "Protegez les yeux du patient avec des\nlunettes adaptees avant d'activer\nla phototherapie."
          : "Protect the patient's eyes with eye\nshields before activating phototherapy.");
  lv_label_set_text(ui_PhotoSafetyTurnOnLabel,
      (lang == LANG_ES)   ? "ENCENDER"
      : (lang == LANG_FR) ? "ACTIVER"
                          : "TURN ON");
}

static void photo_safety_popup_show(bool show) {
  if (!ui_PhotoSafetyOverlay)
    return;
  if (show) {
    photo_safety_apply_language(g_lang);
    lv_obj_clear_flag(ui_PhotoSafetyOverlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui_PhotoSafetyOverlay);
  } else {
    lv_obj_add_flag(ui_PhotoSafetyOverlay, LV_OBJ_FLAG_HIDDEN);
  }
}

void photo_safety_overlay_cb(lv_event_t *e) {
  if (lv_event_get_target(e) == ui_PhotoSafetyOverlay)
    photo_safety_popup_show(false);
}

void photo_turnon_cb(lv_event_t *) {
  photo_safety_popup_show(false);
  g_photo_safety_confirmed = true;
  ui_set_switch_state_silent(ui_Switch3, true);
  lv_event_send(ui_Switch3, LV_EVENT_VALUE_CHANGED, NULL);
}

// Called by BabyWizard when its phototherapy flow completes. Mirrors
// photo_turnon_cb: raise the one-shot gate and re-trigger the switch, so the
// handler runs again and this time falls through to the ISO 7010 safety
// popup — which stays the last step before the lamp actually turns on.
void ActivatePhototherapyFromWizard() {
  g_photo_wizard_done = true;
  ui_set_switch_state_silent(ui_Switch3, true);
  lv_event_send(ui_Switch3, LV_EVENT_VALUE_CHANGED, NULL);
}

// Same shape for humidity: raise the one-shot gate and re-trigger ui_Switch2
// so its handler runs again and this time falls through to the real
// activation. No safety popup stands between the wizard and the humidifier.
void ActivateHumidityFromWizard() {
  g_hum_wizard_done = true;
  ui_set_switch_state_silent(ui_Switch2, true);
  lv_event_send(ui_Switch2, LV_EVENT_VALUE_CHANGED, NULL);
}

static void update_main_toggle_buttons() {
  if (!ui_TempToggleBtn || !ui_HumToggleBtn || !ui_PhotoToggleBtn)
    return;

  bool t = lv_obj_has_state(ui_Switch1, LV_STATE_CHECKED);
  bool h = lv_obj_has_state(ui_Switch2, LV_STATE_CHECKED);
  bool p = lv_obj_has_state(ui_Switch3, LV_STATE_CHECKED);

  lv_color_t blue = lv_color_hex(0x4EC7FF);
  lv_color_t red  = lv_color_hex(0xFF4040);
  lv_obj_t  *lbl;

  lv_obj_set_style_bg_color(ui_TempToggleBtn, t ? red : blue, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(ui_TempToggleBtn, t ? red : blue, LV_PART_MAIN | LV_STATE_PRESSED);
  lbl = lv_obj_get_child(ui_TempToggleBtn, 0);
  if (lbl) lv_label_set_text(lbl, t ? "TURN OFF" : "TURN ON");

  lv_obj_set_style_bg_color(ui_HumToggleBtn, h ? red : blue, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(ui_HumToggleBtn, h ? red : blue, LV_PART_MAIN | LV_STATE_PRESSED);
  lbl = lv_obj_get_child(ui_HumToggleBtn, 0);
  if (lbl) lv_label_set_text(lbl, h ? "TURN OFF" : "TURN ON");

  lv_obj_set_style_bg_color(ui_PhotoToggleBtn, p ? red : blue, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(ui_PhotoToggleBtn, p ? red : blue, LV_PART_MAIN | LV_STATE_PRESSED);
  lbl = lv_obj_get_child(ui_PhotoToggleBtn, 0);
  if (lbl) lv_label_set_text(lbl, p ? "TURN OFF" : "TURN ON");
}

void UI_ApplyLanguage(ui_lang_t lang) {
  g_lang = lang;
  { Preferences p; p.begin(HMI_NS_CFG, false); p.putUChar(HMI_KEY_LANG, (uint8_t)g_lang); p.end(); }
  eepromDirty = true;
  lastVarChangeTime = millis();

  const char *TXT_CONTROLTEMP[] = {"TEMPERATURA", "TEMPERATURE", "TEMPERATURE"};
  const char *TXT_CONTROLHUM[] = {"HUMEDAD", "HUMIDITY", "HUMIDITY"};
  const char *TXT_PHOTO[] = {"FOTOTERAPIA", "PHOTOTHERAPY", "PHOTOTHERAPIE"};
  const char *TXT_AIR[] = {"AIRE", "AIR", "AIR"};
  const char *TXT_SKIN[] = {"PIEL", "SKIN", "PEAU"};
  const char *TXT_ON[] = {"ON", "ON", "ON"};
  const char *TXT_OFF[] = {"OFF", "OFF", "OFF"};
  const char *TXT_SETTINGS[] = {"AJUSTES", "SETTINGS", "PARAMETRES"};
  const char *TXT_LANG[] = {"IDIOMA", "LANGUAGE", "LANGUE"};
  const char *TXT_WIFI[] = {"WIFI", "WIFI", "WIFI"};
  const char *TXT_CONNECT[] = {"CONECTAR", "CONNECT", "CONNEXION"};
  const char *TXT_SSID[] = {"SSID", "SSID", "SSID"};
  const char *TXT_PASSWORD[] = {"CONTRASENA", "PASSWORD", "MOT DE PASSE"};
  const char *TXT_SKINMODE[] = {"MODO PIEL", "SKIN MODE", "MODE PEAU"};
  const char *TXT_INFO[] = {"INFO", "INFO", "INFO"};
  const char *TXT_MODES[] = {"MODOS", "MODES", "MODES"};
  const char *TXT_MODES_TITLE[] = {"MODOS", "MODES", "MODES"};
  const char *TXT_HMI_VERSION[] = {
      "VERSION PANTALLA:", "DISPLAY VERSION:", "VERSION ECRAN:"};
  const char *TXT_MB_VERSION[] = {
      "VERSION PLACA:", "MOTHERBOARD VERSION:", "VERSION CARTE:"};
  const char *TXT_SN[] = {"S/N:", "S/N:", "S/N:"};
  const char *TXT_HEATER_ERROR_RESTART[] = {
      "Error calentador\nToque para mas informacion",
      "Heater error\nTouch for more information",
      "Erreur chauffage\nToucher pour plus d'infos"};
  const char *TXT_ALARMS[] = {"ALARMAS", "ALARMS", "ALARMES"};
  const char *TXT_VIEWDETAIL[] = {"VER DETALLES", "VIEW DETAILS",
                                  "VOIR DETAILS"};
  const char *TXT_HUMCHART[] = {"GRAFICO HUMEDAD", "HUMIDITY CHART",
                                "GRAPHIQUE HUMIDITE"};
  const char *TXT_AIRTEMPCHART[] = {"GRAFICO TEMPERATURA AIRE",
                                    "AIR TEMPERATURE CHART",
                                    "GRAPHIQUE TEMPERATURE AIR"};
  const char *TXT_SKINTEMPCHART[] = {"GRAFICO TEMPERATURA PIEL",
                                     "SKIN TEMPERATURE CHART",
                                     "GRAPHIQUE TEMPERATURE PEAU"};
  const char *TXT_TABTEMP[] = {"TEMPERATURA", "TEMPERATURE", "TEMPERATURE"};
  const char *TXT_TABHUM[] = {"HUMEDAD", "HUMIDITY", "HUMIDITE"};
  const char *TXT_AIRTEMP[] = {
      "TEMPERATURA AIRE:", "AIR TEMPERATURE:", "AIR TEMPERATURE:"};
  const char *TXT_BABYTEMP[] = {
      "TEMPERATURA BEBE:", "BABY TEMPERATURE:", "TEMPERATURE BEBE:"};
  const char *TXT_HUM[] = {"HUMEDAD:", "HUMIDITY:", "HUMIDITE:"};
  const char *TXT_TARGETTEMP[] = {
      "TEMP. OBJETIVO:", "TARGET TEMP:", "TEMP. OBJECTIF:"};
  const char *TXT_TARGETHUM[] = {
      "HUMEDAD OBJETIVO:", "TARGET HUMIDITY:", "HUMIDITE OBJECTIF:"};
  const char *TXT_STATUS[] = {"ESTADO:", "STATUS:", "ETAT:"};
  const char *TXT_UNLOCK[] = {"PRESIONA 2 SEG\nPARA DESBLOQUEAR",
                              "PRESS 2 SEC \nTO UNLOCK",
                              "APPUYEZ 2 SEG\nPOUR DEVERROUILLER"};
  const char *TXT_INCUNEST[] = {"INCUNEST", "INCUNEST", "INCUNEST"};
  const char *TXT_SET[] = {"AJUSTAR", "SET", "REGLER"};
  const char *TXT_WIFISSID[] = {"WIFISSID", "WIFISSID", "WIFISSID"};
  const char *TXT_WIFICONNECTEDTO[] = {"WIFI CONECTADO A", "WIFI CONNECTED TO",
                                       "WIFI CONNECTE A"};
  const char *TXT_ALARMSDESC[] = {"DESCRIPCION DE ALARMAS",
                                  "ALARMS DESCRIPTION",
                                  "DESCRIPTION DES ALARMES"};
  const char *TXT_OXICHART[] = {"GRAFICO OXIMETRIA", "OXIMETRY CHART",
                                "GRAPHIQUE OXIMETRIE"};
  const char *TXT_PULSEOXIMETRY[] = {"PULSIOXIMETRIA", "PULSE OXIMETRY",
                                     "PULSIOXYMETRIE"};
  const char *TXT_LANG_OPTIONS[] = {"ESPANOL\nINGLES\nFRANCES",
                                    "SPANISH\nENGLISH\nFRENCH",
                                    "ESPAGNOL\nANGLAIS\nFRANCAIS"};
  const char *TXT_CONNECTIVITY[] = {
      "CONECTIVIDAD:", "CONNECTIVITY:", "CONNECTIVITE:"};
  const char *TXT_PHOTOSTART[] = {"EMPEZAR", "START", "DEMARRER"};
  const char *TXT_DARKMODE[] = {"MODO OSCURO", "DARK MODE", "MODE SOMBRE"};
  lv_label_set_text(ui_Label2, TXT_CONTROLTEMP[lang]);
  lv_label_set_text(ui_HumidityLabel, TXT_CONTROLHUM[lang]);
  lv_label_set_text(ui_PhototherapyLabel, TXT_PHOTO[lang]);
  lv_label_set_text(ui_Label30, TXT_AIR[lang]);
  lv_label_set_text(ui_Label31, TXT_SKIN[lang]);
  lv_label_set_text(ui_Label9, TXT_ON[lang]);
  lv_label_set_text(ui_Label15, TXT_OFF[lang]);
  lv_label_set_text(ui_Label13, TXT_ON[lang]);
  lv_label_set_text(ui_Label16, TXT_OFF[lang]);
  lv_label_set_text(ui_Label10, TXT_ON[lang]);
  lv_label_set_text(ui_Label17, TXT_OFF[lang]);
  lv_label_set_text(ui_Label8, TXT_SETTINGS[lang]);
  lv_label_set_text(ui_LanguagesLabel, TXT_LANG[lang]);
  lv_label_set_text(ui_WifiLabel, TXT_WIFI[lang]);
  lv_label_set_text(ui_ConnectLabel, TXT_CONNECT[lang]);
  lv_label_set_text(ui_SSIDLabel, TXT_SSID[lang]);
  lv_label_set_text(ui_PassLabel, TXT_PASSWORD[lang]);
  lv_label_set_text(ui_SkinOptionLabel, TXT_SKINMODE[lang]);
  lv_label_set_text(ui_InfoLabel, TXT_INFO[lang]);
  lv_label_set_text(ui_DarkModeLabel, TXT_DARKMODE[lang]);
  lv_label_set_text(ui_ModesLabel, TXT_MODES[lang]);
  lv_label_set_text(ui_ModesTitleLabel, TXT_MODES_TITLE[lang]);
  {
    const char *TXT_HUMIDITY_MODE[] = {"CONTROL HUMEDAD", "HUMIDITY CONTROL",
                                       "CONTROLE HUMIDITE"};
    lv_label_set_text(ui_HumidityModeLabel, TXT_HUMIDITY_MODE[lang]);
  }
  lv_label_set_text(ui_HMIVerTitle, TXT_HMI_VERSION[lang]);
  lv_label_set_text(ui_MBVerTitle, TXT_MB_VERSION[lang]);
  lv_label_set_text(ui_SNTitle, TXT_SN[lang]);
  if (ui_ConnTitle)
    lv_label_set_text(ui_ConnTitle, TXT_CONNECTIVITY[lang]);

  if (ui_HeaterErrorTempLabel)
    lv_label_set_text(ui_HeaterErrorTempLabel, TXT_HEATER_ERROR_RESTART[lang]);
  if (ui_HeaterErrorHumLabel)
    lv_label_set_text(ui_HeaterErrorHumLabel, TXT_HEATER_ERROR_RESTART[lang]);

  lv_label_set_text(ui_Label6, TXT_SET[lang]);
  lv_label_set_text(ui_Label7, TXT_SET[lang]);
  lv_label_set_text(ui_WifiSSIDLabel, TXT_WIFISSID[lang]);
  lv_label_set_text(ui_WifiConnectedToLabel, TXT_WIFICONNECTEDTO[lang]);
  lv_label_set_text(ui_AlarmDetailLabel, TXT_ALARMSDESC[lang]);
  lv_label_set_text(ui_Label35, TXT_OXICHART[lang]);
  lv_label_set_text(ui_Label39, TXT_PULSEOXIMETRY[lang]);
  lv_dropdown_set_options(ui_LanguagesDropDown, TXT_LANG_OPTIONS[lang]);
  lv_dropdown_set_selected(ui_LanguagesDropDown, lang);

  {
    lv_obj_t *btnm = lv_tabview_get_tab_btns(ui_AlarmsTabview);
    if (btnm && lv_obj_check_type(btnm, &lv_btnmatrix_class)) {
      static const char *map_alarm[3];
      map_alarm[0] = TXT_ALARMS[lang];
      map_alarm[1] = TXT_VIEWDETAIL[lang];
      map_alarm[2] = "";
      lv_btnmatrix_set_map(btnm, map_alarm);
    }
  }

  lv_label_set_text(ui_Label36, TXT_HUMCHART[lang]);
  lv_label_set_text(ui_Label37, TXT_AIRTEMPCHART[lang]);
  lv_label_set_text(ui_Label38, TXT_SKINTEMPCHART[lang]);
  {
    lv_obj_t *btnm = lv_tabview_get_tab_btns(ui_TabView1);
    if (btnm && lv_obj_check_type(btnm, &lv_btnmatrix_class)) {
      static const char *map_chart[3];
      map_chart[0] = TXT_TABTEMP[lang];
      map_chart[1] = TXT_TABHUM[lang];
      map_chart[2] = "";
      lv_btnmatrix_set_map(btnm, map_chart);
    }
  }


  lv_label_set_text(ui_Label11, TXT_AIRTEMP[lang]);
  lv_label_set_text(ui_Label12, TXT_BABYTEMP[lang]);
  lv_label_set_text(ui_Label19, TXT_HUM[lang]);
  lv_label_set_text(ui_TargetAirTempLabel, TXT_TARGETTEMP[lang]);
  lv_label_set_text(ui_TargetSkinTempLabel, TXT_TARGETTEMP[lang]);
  lv_label_set_text(ui_Label23, TXT_TARGETHUM[lang]);
  lv_label_set_text(ui_StatusLabel, TXT_STATUS[lang]);
  lv_label_set_text(ui_Label4, TXT_UNLOCK[lang]);

  // Phototherapy Timer
  if (ui_PhotoLockLabel) {
    const char *TXT_PHOTO_LOCK[] = {
        "FOTOTERAPIA:", "PHOTOTHERAPY:", "PHOTOTHERAPIE:"};
    lv_label_set_text(ui_PhotoLockLabel, TXT_PHOTO_LOCK[lang]);
  }

  // Boton de tendencia de telemetria (pantalla de bloqueo)
  if (ui_ChartLockLabel) {
    const char *TXT_TREND[] = {"TENDENCIA", "TREND", "TENDANCE"};
    lv_label_set_text(ui_ChartLockLabel, TXT_TREND[lang]);
  }

  // Babies history button (baby-history-viewer)
  if (ui_BabiesButtonLabel) {
    const char *TXT_BABIES[] = {"Bebes", "Babies", "Bebes"};
    lv_label_set_text(ui_BabiesButtonLabel, TXT_BABIES[lang]);
  }

  photo_safety_apply_language(lang);
  TelemetryHistory_ApplyLanguage();

  update_labels();
  UI_SyncAll();
}

void LanguagesDropDown_cb(lv_event_t *e) {
  lv_obj_t *dd = lv_event_get_target(e);
  uint16_t sel = lv_dropdown_get_selected(dd);
  if (sel == 0)
    UI_ApplyLanguage(LANG_ES);
  else if (sel == 1)
    UI_ApplyLanguage(LANG_EN);
  else if (sel == 2)
    UI_ApplyLanguage(LANG_FR);

  hmi_msg.language = sel;
  hmi_msg.shouldSendData = true;
}

lv_chart_series_t *configure_temp_chart(lv_obj_t *chart, lv_palette_t pal,
                                        lv_coord_t min, lv_coord_t max) {
  lv_chart_series_t *s = lv_chart_get_series_next(chart, NULL);
  while (s != NULL) {
    lv_chart_series_t *next = lv_chart_get_series_next(chart, s);
    s->x_points = NULL;
    s->x_ext_buf_assigned = 0;
    lv_chart_remove_series(chart, s);
    s = next;
  }
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(chart, 50);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, min, max);
  lv_chart_series_t *series =
      lv_chart_add_series(chart, lv_palette_main(pal), LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_SECONDARY_Y, 0, 0, 0, 0, false,
                         0);
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_X, 0, 0, 0, 0, false, 0);
  for (int i = 0; i < lv_chart_get_point_count(chart); i++) {
    series->y_points[i] = LV_CHART_POINT_NONE;
  }
  lv_chart_refresh(chart);
  return series;
}

void temp_chart_show_for_selected_panel(void) {
  if (!tempSwitched) {
    lv_obj_add_flag(ui_AirTempChartCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_SkinTempChartCont, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  if (selectedPanel != AIR_PANEL_SELECTED &&
      selectedPanel != SKIN_PANEL_SELECTED) {
    selectedPanel = AIR_PANEL_SELECTED;
    lastSelectedPanel = selectedPanel;
    hmi_msg.controlMode = CONTROL_AIR;
  }
  lv_obj_add_flag(ui_AirTempChartCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_SkinTempChartCont, LV_OBJ_FLAG_HIDDEN);
  if (selectedPanel == AIR_PANEL_SELECTED) {
    lv_obj_clear_flag(ui_AirTempChartCont, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(ui_SkinTempChartCont, LV_OBJ_FLAG_HIDDEN);
  }
}

void chart_add_air_temp(float v) {
  if (!airTempSeries)
    return;
  lv_chart_set_next_value(ui_AirTempChart, airTempSeries, (lv_coord_t)v);
}

void chart_add_skin_temp(float v) {
  if (!skinTempSeries)
    return;
  lv_chart_set_next_value(ui_SkinTempChart, skinTempSeries, (lv_coord_t)v);
}


void set_active_panel(lv_obj_t *active, lv_obj_t *inactive) {
  lv_color_t active_col = darkMode ? COLOR_PANEL_GRAY : COLOR_PANEL_WHITE;
  lv_color_t inactive_col = darkMode ? COLOR_PANEL_DARK : COLOR_PANEL_GRAY;

  lv_obj_set_style_bg_color(active, active_col, LV_PART_MAIN);
  lv_obj_set_style_opa(active, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(inactive, inactive_col, LV_PART_MAIN);
  lv_obj_set_style_opa(inactive, LV_OPA_COVER, LV_PART_MAIN);
}

void updateButtonVisibility() {
  if (isConnected) {
    lv_obj_add_flag(ui_WifiConnectButton, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_WifiDisconnectButton, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(ui_WifiConnectButton, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_WifiDisconnectButton, LV_OBJ_FLAG_HIDDEN);
  }
}

void WifiButton_cb(lv_event_t *e) {
  lv_obj_add_flag(ui_LanguagesDropDown, LV_OBJ_FLAG_HIDDEN);
  LanguagesVisible = false;
  lv_obj_add_flag(ui_InfoDetailsCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ModesConfigCont, LV_OBJ_FLAG_HIDDEN);
  isConnected = (WiFi.status() == WL_CONNECTED);

  // Pre-fill TextAreas with saved credentials so the user only needs to
  // correct what's wrong. Falls back to compile-time defaults if EEPROM
  // is empty or contains invalid (non-printable) data.
  {
    String savedSSID, savedPass;
    { Preferences p; p.begin(HMI_NS_WIFI, true);
      savedSSID = p.getString(HMI_KEY_SSID,     "");
      savedPass = p.getString(HMI_KEY_PASSWORD, "");
      p.end(); }

    // Validate: 1–32 printable ASCII chars for SSID, up to 63 for password
    auto isValidCred = [](const String &s, size_t maxLen) -> bool {
      if (s.length() == 0 || s.length() > maxLen)
        return false;
      for (size_t i = 0; i < s.length(); i++) {
        uint8_t c = (uint8_t)s[i];
        if (c < 0x20 || c > 0x7E)
          return false;
      }
      return true;
    };

    if (!isValidCred(savedSSID, 32)) {
      savedSSID = WIFI_SSID;
      savedPass = WIFI_PASSWORD;
      ESP_LOGW("WiFiUI", "EEPROM credentials invalid, showing defaults");
    }

    strncpy(wifi_ssid, savedSSID.c_str(), sizeof(wifi_ssid) - 1);
    wifi_ssid[sizeof(wifi_ssid) - 1] = '\0';
    strncpy(wifi_pass, savedPass.c_str(), sizeof(wifi_pass) - 1);
    wifi_pass[sizeof(wifi_pass) - 1] = '\0';

    if (ui_TextArea1)
      lv_textarea_set_text(ui_TextArea1, wifi_ssid);
    if (ui_TextArea2)
      lv_textarea_set_text(ui_TextArea2, wifi_pass);
  }

  // Config form is always visible when WiFi panel is open
  lv_obj_clear_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
  if (isConnected) {
    lv_obj_clear_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ui_WifiSSIDLabel, WiFi.SSID().c_str());
  }
  updateButtonVisibility();
  wifiVisible = true;
}

void InfoButton_cb(lv_event_t *e) {
  lv_obj_add_flag(ui_LanguagesDropDown, LV_OBJ_FLAG_HIDDEN);
  LanguagesVisible = false;
  lv_obj_add_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ModesConfigCont, LV_OBJ_FLAG_HIDDEN);

  lv_obj_clear_flag(ui_InfoDetailsCont, LV_OBJ_FLAG_HIDDEN);

  // Update values
  //
  // La version lleva pegada la MARCA DE COMPILACION del binario que esta
  // corriendo. FWversion es una constante que solo cambia cuando alguien se
  // acuerda de subirla, asi que no sirve para responder la pregunta que de
  // verdad surge en el banco: "¿lo que hay flasheado es lo que acabo de
  // compilar?". __DATE__/__TIME__ los pone el compilador en cada build y no
  // se pueden olvidar.
  {
    char verBuf[48];
    snprintf(verBuf, sizeof(verBuf), "%s (%s %s)", FWversion, __DATE__,
             __TIME__);
    lv_label_set_text(ui_HMIVerValue, verBuf);
  }
  lv_label_set_text(ui_MBVerValue, ctrl_state_msg.fwVer);
  char snBuf[16];
  snprintf(snBuf, sizeof(snBuf), "%04d", in3.serialNumber);
  lv_label_set_text(ui_SNValue, snBuf);

  if (ui_ConnValue) {
    lv_label_set_text(
        ui_ConnValue,
        getConnectivityString(ctrl_state_msg.serverCommStatus, g_lang));
  }

  wifiVisible = false;
}

void ModesButton_cb(lv_event_t *e) {
  lv_obj_add_flag(ui_LanguagesDropDown, LV_OBJ_FLAG_HIDDEN);
  LanguagesVisible = false;
  lv_obj_add_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_InfoDetailsCont, LV_OBJ_FLAG_HIDDEN);

  lv_obj_clear_flag(ui_ModesConfigCont, LV_OBJ_FLAG_HIDDEN);

  wifiVisible = false;
}

// Disparado al tocar la hora de cabecera (ui_ClockButton, ui_ScreenMain), no
// desde Settings: el reloj vive ahi, asi que no hace falta ocultar ningun
// panel de la pantalla de ajustes (son pantallas distintas, LVGL no las
// muestra a la vez).
//
// Todo el dialogo vive en TimeDialog.cpp: es un pop-up modal con teclado
// numerico y la mascara en blanco, no los 5 spinboxes prefijados que habia
// aqui antes.
void ClockButton_cb(lv_event_t *e) { TimeDialog_Open(); }

// Boton "?" del heading (ui_HelpButton). Todo el menu vive en HelpDialog.cpp
// (spec hmi-help-center): tutorial guiado, QR del video tutorial y contacto
// con soporte.
void HelpButton_cb(lv_event_t *e) { HelpDialog_Open(); }

void LanguageButton_cb(lv_event_t *e) {
  lv_obj_add_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_InfoDetailsCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ModesConfigCont, LV_OBJ_FLAG_HIDDEN);
  wifiVisible = false;
  lv_obj_clear_flag(ui_LanguagesDropDown, LV_OBJ_FLAG_HIDDEN);
  LanguagesVisible = true;
  // Abrir el desplegable no cambia el idioma; eso lo hace
  // LanguagesDropDown_cb(), que si manda estado.
}

void TextArea_Change_cb(lv_event_t *e) {
  lv_obj_t *ta = lv_event_get_target(e);
  const char *txt = lv_textarea_get_text(ta);
  if (ta == ui_TextArea1) {
    strncpy(wifi_ssid, txt, sizeof(wifi_ssid) - 1);
    wifi_ssid[sizeof(wifi_ssid) - 1] = '\0';
  } else if (ta == ui_TextArea2) {
    strncpy(wifi_pass, txt, sizeof(wifi_pass) - 1);
    wifi_pass[sizeof(wifi_pass) - 1] = '\0';
  }
}

void TextArea_focus_cb(lv_event_t *e) {
  lv_obj_t *ta = lv_event_get_target(e);
  lv_keyboard_set_textarea(ui_Keyboard1, ta);
  lv_obj_clear_flag(ui_Keyboard1, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiConnectButton, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiDisconnectButton, LV_OBJ_FLAG_HIDDEN);
}

void Keyboard_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    lv_obj_add_flag(ui_Keyboard1, LV_OBJ_FLAG_HIDDEN);
    const char *txt1 = lv_textarea_get_text(ui_TextArea1);
    const char *txt2 = lv_textarea_get_text(ui_TextArea2);
    strncpy(wifi_ssid, txt1, sizeof(wifi_ssid) - 1);
    wifi_ssid[sizeof(wifi_ssid) - 1] = '\0';
    strncpy(wifi_pass, txt2, sizeof(wifi_pass) - 1);
    wifi_pass[sizeof(wifi_pass) - 1] = '\0';
    updateButtonVisibility();
  }
}

void AirPanel_cb(lv_event_t *e) {
  if (!tempSwitched)
    return;
  selectedPanel = AIR_PANEL_SELECTED;
  lastSelectedPanel = selectedPanel;
  set_active_panel(ui_AirPanel, ui_SkinPanel);

  // Visibility
  lv_obj_clear_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);

  // Lock Screen Sync (if objects exist)
  if (ui_AirTempLockCont)
    lv_obj_clear_flag(ui_AirTempLockCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_TargetAirTempCont)
    lv_obj_clear_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_SkinTempLockCont)
    lv_obj_add_flag(ui_SkinTempLockCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_TargetSkinTempCont)
    lv_obj_add_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);

  hmi_msg.controlMode = CONTROL_AIR;
  hmi_msg.shouldSendData = true;
  temp_chart_show_for_selected_panel();
}

void SkinPanel_cb(lv_event_t *e) {
  if (!tempSwitched)
    return;
  if (!skinPanelEnabled)
    return;
  selectedPanel = SKIN_PANEL_SELECTED;
  lastSelectedPanel = selectedPanel;
  set_active_panel(ui_SkinPanel, ui_AirPanel);

  // Visibility
  lv_obj_clear_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);

  // Lock Screen Sync
  if (ui_SkinTempLockCont)
    lv_obj_clear_flag(ui_SkinTempLockCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_TargetSkinTempCont)
    lv_obj_clear_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_AirTempLockCont)
    lv_obj_add_flag(ui_AirTempLockCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_TargetAirTempCont)
    lv_obj_add_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);

  hmi_msg.controlMode = CONTROL_SKIN;
  hmi_msg.shouldSendData = true;
  temp_chart_show_for_selected_panel();
}

void PhotoTimeMinusBtn_cb(lv_event_t *e) {
  // Aqui no se manda estado: estos botones solo mueven photoTimerMinutes, que
  // es local del display. hmi_msg.photoMinutesRemaining lo fija PhotoStartBtn
  // al arrancar el temporizador, que es cuando la placa necesita saberlo.
  if (photoTimerActive)
    return;

  if (photoTimerMinutes > PHOTO_TIMER_MIN_MINUTES) {
    photoTimerMinutes -= PHOTO_TIMER_STEP_MINUTES;
  }

  char buf[16];
  snprintf(buf, sizeof(buf), "%d:%02d", photoTimerMinutes / SECONDS_PER_MINUTE,
           photoTimerMinutes % SECONDS_PER_MINUTE);
  lv_label_set_text(ui_PhotoTimeValueLabel, buf);
  eepromDirty = true;
  lastVarChangeTime = millis();
}

void PhotoTimePlusBtn_cb(lv_event_t *e) {
  // Mismo motivo que en PhotoTimeMinusBtn_cb: nada de la trama cambia aqui.
  if (photoTimerActive)
    return;

  if (photoTimerMinutes < PHOTO_TIMER_MAX_MINUTES) {
    photoTimerMinutes += PHOTO_TIMER_STEP_MINUTES;
  }

  char buf[16];
  snprintf(buf, sizeof(buf), "%d:%02d", photoTimerMinutes / SECONDS_PER_MINUTE,
           photoTimerMinutes % SECONDS_PER_MINUTE);
  lv_label_set_text(ui_PhotoTimeValueLabel, buf);
  eepromDirty = true;
  lastVarChangeTime = millis();
}

void PhotoStartBtn_cb(lv_event_t *e) {
  if (photoTimerActive)
    return;

  photoTimerActive = true;
  photoTimerStartMs = millis();

  // Update visual state via SyncAll
  UI_SyncAll();

  hmi_msg.photoMinutesRemaining = photoTimerMinutes;
  hmi_msg.shouldSendData = true;
}

void PhotoCancelBtn_cb(lv_event_t *e) {
  photoTimerActive = false;

  // Notify Motherboard to stop timer but keep light ON (Continuous mode)
  hmi_msg.photoMinutesRemaining = 0;
  hmi_msg.shouldSendData = true;

  // Update visual state via SyncAll (this handles colors, labels and "X"
  // visibility)
  UI_SyncAll();
}

void temp_content_set_visible(bool visible) {
  lv_obj_t *widgets[] = {
      ui_Panel1,        ui_AirPanelCont,     ui_ArrowDownTemp,
      ui_ArrowUpTemp,   ui_ImgArrowDownTemp, ui_ImgArrowUpTemp,
      ui_Label6,        ui_TempButton
  };
  for (auto w : widgets) {
    if (!w) continue;
    if (visible) lv_obj_clear_flag(w, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(w, LV_OBJ_FLAG_HIDDEN);
  }
  if (ui_SkinPanelCont) {
    bool showSkin = visible && skinPanelEnabled;
    if (showSkin) lv_obj_clear_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);
    else          lv_obj_add_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);
  }
}

// Generic bottom-toast, reused by the skin-probe block message, the
// wizard's SKIN-without-range guardrail, and BabyWizard's own messages.
void UI_ShowToast(const char *msg, uint32_t ms) {
  if (!ui_SkinProbeToast) return;
  lv_label_set_text(ui_SkinProbeToast, msg);
  lv_obj_clear_flag(ui_SkinProbeToast, LV_OBJ_FLAG_HIDDEN);
  // Al frente de su propia capa: el banner de alarma y el centro de alarmas
  // tambien viven en lv_layer_top() y el orden ahi lo fija quien se mueve
  // ultimo, no la jerarquia.
  lv_obj_move_foreground(ui_SkinProbeToast);
  lv_timer_create(
      [](lv_timer_t *t) {
        if (ui_SkinProbeToast)
          lv_obj_add_flag(ui_SkinProbeToast, LV_OBJ_FLAG_HIDDEN);
        lv_timer_del(t);
      },
      ms, nullptr);
}

static bool isFanHeaterAlarmActive() {
  for (int i = 0; i < MAX_ALARMS; i++) {
    if ((alarmList[i].id == ALARM_FAN_FAILURE || alarmList[i].id == ALARM_HEATER_FAULT) && alarmList[i].state)
      return true;
  }
  return false;
}

// Criterio de interrupción de la INTERFAZ (no el corte de calefactor de la
// motherBoard: son preguntas distintas y coincidir sería casualidad, no
// diseño). Deliberadamente separado de shared/alarm_policy.h's
// alarm_cuts_heater(), que responde "¿hay que cortar el calefactor?", no
// "¿hay que interrumpir el asistente?". Mapeo 1:1 del conjunto viejo
// (TEMPERATURE_ALARM, AIR_THERMAL_CUTOUT_ALARM, SKIN_THERMAL_CUTOUT_ALARM,
// FAN_ISSUE_ALARM) a los IDs nuevos — TEMPERATURE_ALARM era una única
// condición simétrica que cambiaba de sensor según el modo, de ahí las
// cuatro variantes de desviación. Si algún día se quiere unificar con un
// criterio de la motherBoard, hace falta una función propia y bien nombrada
// (no alarm_cuts_heater()), con su propia revisión clínica.
// Usada por BabyWizard para interrumpir un asistente abierto (Section 4).
bool UI_IsCriticalAlarmActive() {
  for (int i = 0; i < MAX_ALARMS; i++) {
    if (!alarmList[i].state) continue;
    if (alarmList[i].id == ALARM_AIR_THERMAL_CUTOUT ||
        alarmList[i].id == ALARM_SKIN_THERMAL_CUTOUT ||
        alarmList[i].id == ALARM_FAN_FAILURE ||
        alarmList[i].id == ALARM_AIR_TEMP_DEVIATION_HIGH ||
        alarmList[i].id == ALARM_AIR_TEMP_DEVIATION_LOW ||
        alarmList[i].id == ALARM_SKIN_TEMP_DEVIATION_HIGH ||
        alarmList[i].id == ALARM_SKIN_TEMP_DEVIATION_LOW) {
      return true;
    }
  }
  return false;
}

// Cualquier alarma activa, no solo las criticas de UI_IsCriticalAlarmActive.
// La usa TelemetryHistory para decidir si debe cederle la pantalla al banner
// de alarma: ese overlay no tiene informacion de alarma propia que compense
// taparlo, a diferencia de AlarmCenter (que si es donde se atiende).
bool UI_IsAnyAlarmActive() {
  for (int i = 0; i < MAX_ALARMS; i++) {
    if (alarmList[i].state) return true;
  }
  return false;
}

// "Is the incubator doing something to a baby right now?" — the single
// definition of a live care session, shared by the baby-exit dialog (which
// only asks paperwork when this goes false) and by BabyWizard's
// already-identified-baby shortcut (only trustworthy while this is true).
// Phototherapy is read from hmi_msg because ui_Switch3 is toggled silently
// during its activation gates, so the switch state lies mid-flow.
bool UI_AnyControlActive() {
  return switchTemp || switchHum ||
         hmi_msg.phototherapyMode != PHOTOTHERAPY_OFF;
}

// Runs everything the temperature switch's ON branch used to do inline,
// now invoked from BabyWizard's "Apply" step once the mandatory baby-data
// wizard completes (temp-control-activation-wizard spec) instead of
// immediately on the switch toggle. isAirMode selects which panel becomes
// active; for isAirMode=false the probe check lives in the ui_Switch4 handler
// this ends up firing, which is the only precondition SKIN has.
void ActivateTempControlUI(bool isAirMode) {
  lv_color_t active_col = darkMode ? COLOR_PANEL_GRAY : COLOR_PANEL_WHITE;

  chartLastPressed = 0;
  lv_tabview_set_act(ui_TabView1, 0, LV_ANIM_ON);

  if (isAirMode) {
    selectedPanel = AIR_PANEL_SELECTED;
    lastSelectedPanel = AIR_PANEL_SELECTED;
    set_active_panel(ui_AirPanel, ui_SkinPanel);
    hmi_msg.controlMode = CONTROL_AIR;
  } else {
    selectedPanel = SKIN_PANEL_SELECTED;
    lastSelectedPanel = SKIN_PANEL_SELECTED;
    set_active_panel(ui_SkinPanel, ui_AirPanel);
    hmi_msg.controlMode = CONTROL_SKIN;
  }

  temp_chart_show_for_selected_panel();

  // Enable temperature arrows (SKIN mode additionally disables them itself
  // via the arrow press/release handlers' SKIN_PANEL_SELECTED check — the
  // fixed 36.5 degC clinical standard has no manual adjustment).
  arrowsActive = true;
  lv_obj_add_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(ui_ArrowDownTemp, active_col, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_ArrowUpTemp, active_col, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_Panel1, active_col, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ui_Panel1, LV_OPA_COVER, LV_PART_MAIN);

  switchTemp = true;
  tempSwitched = true;
  ui_set_switch_state_silent(ui_Switch1, true);
  temp_content_set_visible(true);
  computeAndSendActuation();
}

/* Switch callback for temperature and humidity */
void Switch_cb(lv_event_t *e) {
  lv_color_t active_col = darkMode ? COLOR_PANEL_GRAY : COLOR_PANEL_WHITE;
  lv_color_t inactive_col = darkMode ? COLOR_PANEL_DARK : COLOR_PANEL_GRAY;
  lv_obj_t *obj = lv_event_get_target(e); // switch that triggered the event
  lv_obj_t *panel = NULL;

  bool checked = lv_obj_has_state(obj, LV_STATE_CHECKED);

  if (obj == ui_Switch1) { // TEMPERATURE SWITCH
    switchTemp = checked;
    tempSwitched = checked;
    panel = ui_Panel1;

    if (checked) { // Temperature switch turned ON
      if (isFanHeaterAlarmActive()) {
        ui_set_switch_state_silent(ui_Switch1, false);
        switchTemp = false;
        tempSwitched = false;
        return;
      }
      // Defer actual activation until the mandatory baby-data wizard
      // completes (temp-control-activation-wizard spec): the switch stays
      // visually ON (LVGL already applied LV_STATE_CHECKED before this
      // callback ran), but switchTemp/tempSwitched roll back to false so no
      // actuation is sent and no panel/arrow UI changes happen yet. Cancel
      // reverts the switch to OFF (BabyWizard); Apply calls
      // ActivateTempControlUI() below to do everything this branch used to
      // do inline, then sends the real actuation.
      switchTemp = false;
      tempSwitched = false;
      bool desiredIsAirMode = (lastSelectedPanel != SKIN_PANEL_SELECTED);
      BabyWizard_Open(desiredIsAirMode);
      return;
    } else { // Temperature switch turned OFF
      selectedPanel = NO_PANEL_SELECTED;
      lv_obj_add_flag(ui_AirTempChartCont,
                      LV_OBJ_FLAG_HIDDEN); // hide temp chart
      lv_obj_add_flag(ui_SkinTempChartCont,
                      LV_OBJ_FLAG_HIDDEN); // hide skin temp chart
      arrowsActive = false;

      // If humidity is ON, switch to humidity chart
      if (switchHum) {
        chartLastPressed = 1;
        lv_tabview_set_act(ui_TabView1, 1, LV_ANIM_ON);
        lv_obj_clear_flag(ui_HumChartCont, LV_OBJ_FLAG_HIDDEN);
      } else {
        // no charts active
        lv_obj_add_flag(ui_HumChartCont, LV_OBJ_FLAG_HIDDEN);
        chartLastPressed = -1;
      }

      // Disable temperature arrows
      lv_obj_clear_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_clear_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_bg_color(ui_ArrowDownTemp, inactive_col, LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_ArrowUpTemp, inactive_col, LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_Panel1, inactive_col, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_Panel1, LV_OPA_COVER, LV_PART_MAIN);

      // Gray out both panels
      lv_obj_set_style_bg_color(ui_AirPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_SkinPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
      lv_obj_set_style_opa(ui_AirPanel, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_opa(ui_SkinPanel, LV_OPA_COVER, LV_PART_MAIN);
    }
    temp_content_set_visible(checked);
  } else if (obj == ui_Switch2) { // HUMIDITY SWITCH
    if (checked) {
      // Mandatory baby-data wizard, same gate as phototherapy: humidity is a
      // therapy applied to a specific baby and its hours are accounted per
      // profile, so the baby must be identified before the humidifier starts.
      // Skipped while a care session is live (temperature or phototherapy
      // already running for a baby this HMI identified) — re-asking mid-care
      // is pure friction. The switch goes back to OFF and stays there until
      // ActivateHumidityFromWizard() re-triggers this handler.
      if (!g_hum_wizard_done && !BabyWizard_HasLiveSession()) {
        ui_set_switch_state_silent(ui_Switch2, false);
        BabyWizard_OpenForHumidity();
        return;
      }
      // Gate cleared — consume it only HERE, where the humidifier actually
      // starts, so a re-entrant event can never find it already spent.
      g_hum_wizard_done = false;
    }

    switchHum = checked;
    humSwitched = checked;
    panel = ui_Panel3;

    if (checked) {
      // Show humidity content panel
      lv_obj_clear_flag(ui_HumPanelCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui_Panel3, LV_OBJ_FLAG_HIDDEN);
      // mark last pressed chart and show hum page
      chartLastPressed = 1;
      lv_tabview_set_act(ui_TabView1, 1, LV_ANIM_ON);
      lv_obj_clear_flag(ui_HumChartCont, LV_OBJ_FLAG_HIDDEN); // show hum

      // Show humidity target in lock screen
      lv_obj_clear_flag(ui_HumLockDesiredCont, LV_OBJ_FLAG_HIDDEN);

      // Enable humidity arrows
      lv_obj_add_flag(ui_ImgArrowDownHum, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_flag(ui_ImgArrowUpHum, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_bg_color(ui_ArrowDownHum, active_col, LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_ArrowUpHum, active_col, LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_Panel3, active_col, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_Panel3, LV_OPA_COVER, LV_PART_MAIN);
    } else {
      // Hide humidity content panel
      lv_obj_add_flag(ui_HumPanelCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_Panel3, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_HumChartCont, LV_OBJ_FLAG_HIDDEN); // hide hum chart
      // Humidity OFF
      lv_obj_clear_flag(ui_ImgArrowDownHum, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_clear_flag(ui_ImgArrowUpHum, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_bg_color(ui_ArrowDownHum, inactive_col, LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_ArrowUpHum, inactive_col, LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_Panel3, inactive_col, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_Panel3, LV_OPA_COVER, LV_PART_MAIN);

      lv_obj_set_style_opa(ui_ArrowDownHum, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_opa(ui_ArrowUpHum, LV_OPA_COVER, LV_PART_MAIN);

      // If temperature is ON, switch to temperature chart
      if (switchTemp) {
        chartLastPressed = 0;
        lv_tabview_set_act(ui_TabView1, 0, LV_ANIM_ON);
        lv_obj_clear_flag(ui_AirTempChartCont, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(ui_AirTempChartCont, LV_OBJ_FLAG_HIDDEN);
        chartLastPressed = -1;
      }
      // Hide humidity target when humidity turned off
      lv_obj_add_flag(ui_HumLockDesiredCont, LV_OBJ_FLAG_HIDDEN);
    }
  } else if (obj == ui_Switch3) { // PHOTOTHERAPY SWITCH
    bool checked = lv_obj_has_state(obj, LV_STATE_CHECKED);

    if (checked) {
      // Gate order: baby-data wizard FIRST, ISO 7010 M025 safety popup last.
      // The safety popup warns about eye protection, a hazard that starts the
      // instant the lamp does — so it has to stay the final step immediately
      // before activation. Putting the wizard after it would insert a long
      // data-entry flow between the warning and the hazard.
      // Skip the wizard only while a care session is live — i.e. temperature
      // or humidity is already running for a baby this HMI has identified:
      // asking again for the same baby mid-care is pure friction. Once the
      // incubator has gone fully idle the session is over, so the lamp asks
      // again even though the profile is still remembered
      // (BabyWizard_HasLiveSession()).
      if (!g_photo_wizard_done && !BabyWizard_HasLiveSession()) {
        ui_set_switch_state_silent(ui_Switch3, false);
        BabyWizard_OpenForPhototherapy();
        return;
      }

      // Show safety popup unless already confirmed via it
      if (!g_photo_safety_confirmed) {
        ui_set_switch_state_silent(ui_Switch3, false);
        photo_safety_popup_show(true);
        return;
      }

      // Both gates cleared — consume them only HERE, where the lamp actually
      // turns on. Clearing the wizard flag earlier caused an infinite loop:
      // the safety popup re-triggers this handler, and by then the wizard
      // gate had forgotten it was already satisfied, so it reopened.
      g_photo_wizard_done = false;
      g_photo_safety_confirmed = false;

      // Active state on Motherboard immediately
      hmi_msg.phototherapyMode = PHOTOTHERAPY_ON;
      hmi_msg.shouldSendData = true;

      lv_obj_set_style_bg_color(ui_PhotoTimerPanel, active_col, LV_PART_MAIN);
      lv_obj_add_flag(ui_PhotoTimeMinusBtn, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_flag(ui_PhotoTimePlusBtn, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_flag(ui_PhotoStartBtn, LV_OBJ_FLAG_CLICKABLE);

      // Restore button colors
      lv_obj_set_style_bg_color(ui_PhotoTimeMinusBtn, lv_color_hex(0x0075EE),
                                LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_PhotoTimePlusBtn, lv_color_hex(0x0075EE),
                                LV_PART_MAIN);

      // Reset timer UI if no timer is running
      if (!photoTimerActive) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d:%02d",
                 photoTimerMinutes / SECONDS_PER_MINUTE,
                 photoTimerMinutes % SECONDS_PER_MINUTE);
        lv_label_set_text(ui_PhotoTimeValueLabel, buf);

        // Use localized text
        const char *TXT_PHOTOSTART[] = {"EMPEZAR", "START", "DEMARRER"};
        lv_label_set_text(ui_PhotoStartLabel, TXT_PHOTOSTART[g_lang]);
        lv_obj_set_style_bg_color(ui_PhotoStartBtn, lv_color_hex(0x00AA00),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);

        // Signal CONTINUOUS mode to Motherboard (Minutes = 0)
        hmi_msg.photoMinutesRemaining = 0;
        hmi_msg.shouldSendData = true;
      } else {
        // If already running (e.g. skin/air change while timer was active)
        lv_obj_set_style_bg_color(ui_PhotoStartBtn, lv_color_hex(0x888888),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
      }
      if (ui_PhotoTimerCont)
        lv_obj_clear_flag(ui_PhotoTimerCont, LV_OBJ_FLAG_HIDDEN);
    } else {
      // OFF / Grayed out
      hmi_msg.phototherapyMode = PHOTOTHERAPY_OFF;
      hmi_msg.shouldSendData = true;

      lv_obj_set_style_bg_color(ui_PhotoTimerPanel, inactive_col, LV_PART_MAIN);
      lv_obj_clear_flag(ui_PhotoTimeMinusBtn, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_clear_flag(ui_PhotoTimePlusBtn, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_clear_flag(ui_PhotoStartBtn, LV_OBJ_FLAG_CLICKABLE);

      // Buttons to light gray as requested
      lv_obj_set_style_bg_color(ui_PhotoTimeMinusBtn, COLOR_PANEL_LIGHT_GRAY,
                                LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_PhotoTimePlusBtn, COLOR_PANEL_LIGHT_GRAY,
                                LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_PhotoStartBtn, COLOR_PANEL_LIGHT_GRAY,
                                LV_PART_MAIN);

      // Reset visual values when turned OFF (but keep the stored minutes)
      char buf[16];
      snprintf(buf, sizeof(buf), "%d min", photoTimerMinutes);
      lv_label_set_text(ui_PhotoTimeValueLabel, buf);

      const char *TXT_PHOTOSTART[] = {"EMPEZAR", "START", "DEMARRER"};
      lv_label_set_text(ui_PhotoStartLabel, TXT_PHOTOSTART[g_lang]);

      if (ui_PhotoCancelBtn)
        lv_obj_add_flag(ui_PhotoCancelBtn, LV_OBJ_FLAG_HIDDEN);

      if (ui_PhotoTimerCont)
        lv_obj_add_flag(ui_PhotoTimerCont, LV_OBJ_FLAG_HIDDEN);

      photoTimerActive = false;
    }
  } else if (obj == ui_Switch4) { // SKIN BLOCK SWITCH
    bool checked = lv_obj_has_state(obj, LV_STATE_CHECKED);

    // RF-SKIN-007/009: Block skin mode if probe not valid (ARQ-SKIN-002/003)
    if (checked && g_skinProbeState != SKIN_PROBE_VALID) {
      // Revert the switch visually without triggering callback again
      ui_set_switch_state_silent(ui_Switch4, false);
      // RF-SKIN-010/UI-SKIN-004: Show clear message to user
      const char *msg =
          (g_lang == LANG_ES)
              ? "Modo piel no disponible:\nConecte la sonda de temperatura"
          : (g_lang == LANG_FR)
              ? "Mode peau indisponible:\nConnectez la sonde de temperature"
              : "Skin mode unavailable:\nConnect the skin temperature probe";
      UI_ShowToast(msg, 4000);
      ESP_LOGW(TAG,
               "[SKIN-PROBE] Blocked skin mode activation - probe state=%d",
               g_skinProbeState);
      return;
    }

    // La sonda es el UNICO requisito para entrar en modo piel, y por eso
    // el bloque de arriba es el unico que puede rechazar la activacion.
    // Antes hacia falta ademas un rango NTE utilizable del wizard (o sea,
    // haber dado el peso), lo que dejaba el modo piel inaccesible durante
    // toda una sesion por haber saltado ese paso al encenderla. El punto
    // de consigna de piel son 36.5 degC fijos y no se derivan del rango
    // de aire, asi que el rango nunca fue condicion para controlar por
    // piel: solo lo era para PROPONER una temperatura de aire.

    skinPanelEnabled = checked;
    hmi_msg.skinModeEnabled = checked;
    hmi_msg.shouldSendData = true;

    if (checked) {
      // The skin block is only a panel *preference* until temperature control
      // actually runs: showing the baby-temperature controller with the
      // control switched off advertises a therapy that is not happening.
      // temp_content_set_visible() already owns the single condition
      // (visible && skinPanelEnabled), so delegate instead of clearing the
      // HIDDEN flag by hand.
      temp_content_set_visible(tempSwitched);

      lv_obj_set_style_bg_color(ui_SkinPanelCont, COLOR_PANEL_WHITE,
                                LV_PART_MAIN);
      lv_obj_set_style_opa(ui_SkinPanelCont, LV_OPA_COVER, LV_PART_MAIN);

      // Design Section 4: SKIN setpoint is fixed at the clinical standard,
      // never manually adjustable (arrow presses no-op for SKIN_PANEL_SELECTED).
      if (tempSwitched) {
        skinTempValue = SKIN_FIXED_SETPOINT_C;
        computeAndSendActuation();
      }
    } else {
      // Hide container of skin and restore Air panel
      skin_mode_force_off();
    }
  } else if (obj == ui_SwitchDarkMode) { // DARK MODE SWITCH
    bool checked = lv_obj_has_state(obj, LV_STATE_CHECKED);
    darkMode = checked;
    { Preferences p; p.begin(HMI_NS_CFG, false); p.putUChar(HMI_KEY_DARK_MODE, darkMode ? 1 : 0); p.end(); }
    eepromDirty = true;
    lastVarChangeTime = millis();
    UI_ApplyTheme();
  } else if (obj == ui_SwitchHumidityMode) { // HUMIDITY ENABLE SWITCH
    bool checked = lv_obj_has_state(obj, LV_STATE_CHECKED);
    humidityEnabled = checked;
    { Preferences p; p.begin(HMI_NS_CFG, false); p.putUChar(HMI_KEY_HUM_EN, humidityEnabled ? 1 : 0); p.end(); }
    eepromDirty = true;
    lastVarChangeTime = millis();
    if (humidityEnabled) {
      lv_obj_clear_flag(ui_HumCont, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(ui_HumCont, LV_OBJ_FLAG_HIDDEN);
      // Turn off humidity if it was active
      if (switchHum) {
        lv_obj_clear_state(ui_Switch2, LV_STATE_CHECKED);
        lv_event_send(ui_Switch2, LV_EVENT_VALUE_CHANGED, NULL);
      }
    }
  }

  // If temperature is OFF, disable panels and arrows (por si acaso)
  if (!tempSwitched) {
    arrowsActive = false;

    lv_obj_set_style_bg_color(ui_AirPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_SkinPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
    lv_obj_set_style_opa(ui_AirPanel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_opa(ui_SkinPanel, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_clear_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
  } else {
    // If a panel is active, enable arrows
    if (selectedPanel != NO_PANEL_SELECTED) {
      arrowsActive = true;
      lv_obj_add_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
    } else {
      arrowsActive = false;
      lv_obj_clear_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_clear_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
    }
  }

  // Change background color of associated panel
  if (panel != NULL) {
    if (checked) {
      lv_obj_set_style_bg_color(panel, COLOR_PANEL_WHITE, LV_PART_MAIN);
      lv_obj_set_style_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    } else {
      lv_obj_set_style_bg_color(panel, COLOR_PANEL_GRAY, LV_PART_MAIN);
      lv_obj_set_style_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    }
  }

  // Manage tabview and tab buttons so that when only one switch is ON
  // the UI behaves as if only a single tab exists (no header/button to switch).
  lv_obj_t *tab_btns_cont =
      lv_obj_get_child(ui_TabView1, 0); // header container (may be NULL)
  lv_obj_t *temp_tab_btn = NULL;
  lv_obj_t *hum_tab_btn = NULL;
  if (tab_btns_cont) {
    temp_tab_btn = lv_obj_get_child(tab_btns_cont, 0);
    hum_tab_btn = lv_obj_get_child(tab_btns_cont, 1);
  }

  if (!switchTemp && !switchHum) {
    // No charts: hide entire tabview
    lv_obj_add_flag(ui_TabView1, LV_OBJ_FLAG_HIDDEN);
  } else {
    // Ensure tabview visible
    lv_obj_clear_flag(ui_TabView1, LV_OBJ_FLAG_HIDDEN);

    // Show/hide individual tab buttons (labels)
    if (temp_tab_btn) {
      if (switchTemp)
        lv_obj_clear_flag(temp_tab_btn, LV_OBJ_FLAG_HIDDEN);
      else
        lv_obj_add_flag(temp_tab_btn, LV_OBJ_FLAG_HIDDEN);
    }
    if (hum_tab_btn) {
      if (switchHum)
        lv_obj_clear_flag(hum_tab_btn, LV_OBJ_FLAG_HIDDEN);
      else
        lv_obj_add_flag(hum_tab_btn, LV_OBJ_FLAG_HIDDEN);
    }

    // If exactly one chart is visible, hide the header container
    if ((switchTemp && !switchHum) || (!switchTemp && switchHum)) {
      if (tab_btns_cont)
        lv_obj_add_flag(tab_btns_cont, LV_OBJ_FLAG_HIDDEN);
    } else {
      if (tab_btns_cont)
        lv_obj_clear_flag(tab_btns_cont, LV_OBJ_FLAG_HIDDEN);
    }

    // Select active tab: single visible -> that tab; both visible -> respect
    // chartLastPressed
    if (switchTemp && !switchHum) {
      lv_tabview_set_act(ui_TabView1, 0, LV_ANIM_ON);
    } else if (!switchTemp && switchHum) {
      lv_tabview_set_act(ui_TabView1, 1, LV_ANIM_ON);
    } else if (switchTemp && switchHum) {
      if (chartLastPressed == 1)
        lv_tabview_set_act(ui_TabView1, 1, LV_ANIM_ON);
      else
        lv_tabview_set_act(ui_TabView1, 0, LV_ANIM_ON);
    }
  }

  // --- Actuation mode selection logic ---
  computeAndSendActuation();
}

// Shared tail of Switch_cb's actuation logic — also called by
// ActivateTempControlUI() when the baby-data wizard's "Apply" step
// activates control outside the normal switch-toggle flow.
void computeAndSendActuation() {
  if (switchTemp && switchHum) {
    hmi_msg.actuation = ACTUATION_TEMP_AND_HUMIDITY;
  } else if (switchTemp) {
    hmi_msg.actuation = ACTUATION_TEMPERATURE;
  } else if (switchHum) {
    hmi_msg.actuation = ACTUATION_HUMIDITY;
  } else {
    hmi_msg.actuation = ACTUATION_NONE;
  }

  hmi_msg.desiredAirTemperature = airTempValue;
  hmi_msg.desiredSkinTemperature = skinTempValue;
  hmi_msg.desiredHumidity = humValue;
  hmi_msg.shouldSendData = true;

  update_labels();
  UI_SyncAll();
}

void setup_panel_callbacks() {
  lv_obj_add_event_cb(ui_AirPanel, AirPanel_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_SkinPanel, SkinPanel_cb, LV_EVENT_CLICKED, NULL);
}

void setup_arrow_callbacks() {
  lv_obj_add_event_cb(
      ui_ImgArrowUpTemp,
      [](lv_event_t *e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t *target = lv_event_get_target(e);

        if (code == LV_EVENT_PRESSED) {
          lv_obj_set_style_transform_zoom(target, 280,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
          lv_obj_set_style_transform_zoom(target, 256,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (code == LV_EVENT_CLICKED) {
          if (!tempSwitched)
            return;
          // SKIN setpoint is fixed by the baby-data wizard (Section 4) —
          // no manual adjustment, so this arrow no-ops outside AIR mode.
          if (selectedPanel != AIR_PANEL_SELECTED)
            return;
          airTempValue += TEMP_INCREMENT;
          if (airTempValue > AIR_TEMP_MAX)
            airTempValue = AIR_TEMP_MAX;
          hmi_msg.desiredAirTemperature = airTempValue;
          hmi_msg.shouldSendData = true;
          eepromDirty = true;
          lastVarChangeTime = millis();
          update_labels();
        }
      },
      LV_EVENT_ALL, NULL);

  lv_obj_add_event_cb(
      ui_ImgArrowDownTemp,
      [](lv_event_t *e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t *target = lv_event_get_target(e);

        if (code == LV_EVENT_PRESSED) {
          lv_obj_set_style_transform_zoom(target, 280,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
          lv_obj_set_style_transform_zoom(target, 256,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (code == LV_EVENT_CLICKED) {
          if (!tempSwitched)
            return;
          // SKIN setpoint is fixed by the baby-data wizard (Section 4) —
          // no manual adjustment, so this arrow no-ops outside AIR mode.
          if (selectedPanel != AIR_PANEL_SELECTED)
            return;
          airTempValue -= TEMP_INCREMENT;
          if (airTempValue < AIR_TEMP_MIN)
            airTempValue = AIR_TEMP_MIN;
          hmi_msg.desiredAirTemperature = airTempValue;
          hmi_msg.shouldSendData = true;
          eepromDirty = true;
          lastVarChangeTime = millis();
          update_labels();
        }
      },
      LV_EVENT_ALL, NULL);
}

void setup_arrow_hum_callbacks() {
  lv_obj_add_event_cb(
      ui_ImgArrowUpHum,
      [](lv_event_t *e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t *target = lv_event_get_target(e);

        if (code == LV_EVENT_PRESSED) {
          lv_obj_set_style_transform_zoom(target, 280,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
          lv_obj_set_style_transform_zoom(target, 256,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (code == LV_EVENT_CLICKED) {
          if (!switchHum)
            return;
          humValue += HUM_STEP;
          if (humValue > HUM_MAX)
            humValue = HUM_MAX;
          hmi_msg.desiredHumidity = humValue;
          hmi_msg.shouldSendData = true;
          eepromDirty = true;
          lastVarChangeTime = millis();
          update_labels();
        }
      },
      LV_EVENT_ALL, NULL);

  lv_obj_add_event_cb(
      ui_ImgArrowDownHum,
      [](lv_event_t *e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t *target = lv_event_get_target(e);

        if (code == LV_EVENT_PRESSED) {
          lv_obj_set_style_transform_zoom(target, 280,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
          lv_obj_set_style_transform_zoom(target, 256,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (code == LV_EVENT_CLICKED) {
          if (!switchHum)
            return;
          humValue -= HUM_STEP;
          if (humValue < HUM_MIN)
            humValue = HUM_MIN;
          hmi_msg.desiredHumidity = humValue;
          hmi_msg.shouldSendData = true;
          eepromDirty = true;
          lastVarChangeTime = millis();
          update_labels();
        }
      },
      LV_EVENT_ALL, NULL);
}

void blink_cb(void *obj, int32_t v) {
  lv_obj_set_style_bg_opa((lv_obj_t *)obj, v, LV_PART_MAIN);
}

void start_alarm_blink(lv_obj_t *obj) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_40);
  lv_anim_set_time(&a, ANIM_TIME_MS);
  lv_anim_set_playback_time(&a, ANIM_PLAYBACK_MS);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_exec_cb(&a, blink_cb);
  lv_anim_start(&a);
}

// ---------------------------------------------------------------------------
// Banner de alarma
// ---------------------------------------------------------------------------
// IEC 60601-1-8 6.3.2.2.2 exige al menos una senal visual que identifique la
// condicion concreta Y su prioridad, legible a 1 m. Antes de esto, fuera de la
// pantalla de alarmas solo habia un badge con el numero de alarmas activas,
// que no identifica ni la condicion ni la prioridad.
//
// Vive en lv_layer_top() y no colgado de una pantalla: un overlay parentado a
// una pantalla concreta desaparece al hacer lv_scr_load(), y la senal tiene
// que existir mientras exista la condicion, este el operador donde este.
//
// El color y la frecuencia salen de la Tabla 2 de la misma norma: ALTA rojo
// 1,4-2,8 Hz, MEDIA amarillo 0,4-0,8 Hz, BAJA cian o amarillo CONSTANTE. Que
// la baja no parpadee no es un atajo: la tabla lo exige.
//
// Lo que parpadea es el FONDO, nunca el texto. La nota 2 de 6.3.2.2.2
// desaconseja expresamente el texto que se enciende y se apaga porque cuesta
// leerlo, y admite en cambio alternar entre video normal e inverso u otro
// color. Por eso no se reutiliza start_alarm_blink(), que anima la opacidad
// del objeto entero y dejaria el texto ilegible medio ciclo.
static lv_obj_t *s_alarmBanner = NULL;
static lv_obj_t *s_alarmBannerLabel = NULL;
static int s_bannerPriority = -1;  // -1 = ninguna, para no reiniciar la anim
static lv_color_t s_bannerHi;      // color pleno de la prioridad
static lv_color_t s_bannerLo;      // el mismo, oscurecido

#define BANNER_HEIGHT_PX 52
// Medio periodo: 250 ms -> 2,0 Hz (ALTA), 750 ms -> 0,66 Hz (MEDIA). Ambos
// caen dentro de los rangos de la Tabla 2 con margen a los dos lados.
#define BANNER_HALF_PERIOD_MS_HIGH 250
#define BANNER_HALF_PERIOD_MS_MEDIUM 750

// Definida mas abajo en este mismo fichero.
void reset_alarm_detail_state();

// Pulsar el banner abre el centro de alarmas. Es un overlay sobre
// lv_layer_top(), no un cambio de pantalla: desde el bloqueo esto evita tener
// que desbloquear primero para ver que pasa, que era el camino largo justo
// cuando hay prisa.
static void AlarmBanner_cb(lv_event_t *e) {
  (void)e;
  reset_alarm_detail_state();
  AlarmCenter_Open();
}

static void banner_blink_cb(void *obj, int32_t v) {
  lv_obj_set_style_bg_color((lv_obj_t *)obj,
                            lv_color_mix(s_bannerHi, s_bannerLo, (uint8_t)v),
                            LV_PART_MAIN);
}

void alarm_banner_init(void) {
  s_alarmBanner = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(s_alarmBanner);
  lv_obj_set_width(s_alarmBanner, lv_pct(100));
  lv_obj_set_height(s_alarmBanner, BANNER_HEIGHT_PX);
  lv_obj_set_align(s_alarmBanner, LV_ALIGN_TOP_MID);
  lv_obj_set_style_bg_opa(s_alarmBanner, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(s_alarmBanner, LV_OBJ_FLAG_SCROLLABLE);
  // El banner SI es pulsable: lleva a la pantalla de alarmas, que es donde
  // estan el detalle, el historial y el boton de silencio. Es la ruta mas
  // corta desde "algo suena" hasta "puedo hacer algo", y no depende de que el
  // operador encuentre el boton de la barra superior.
  //
  // Consecuencia asumida: en la pantalla de bloqueo el banner ocupa la franja
  // donde esta ui_LockButton, asi que durante una alarma un toque ahi lleva a
  // alarmas en vez de desbloquear. Es el comportamiento util: si suena, lo que
  // se quiere es atenderla. El resto de la pantalla sigue desbloqueando.
  lv_obj_add_flag(s_alarmBanner, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s_alarmBanner, AlarmBanner_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(s_alarmBanner, LV_OBJ_FLAG_HIDDEN);

  s_alarmBannerLabel = lv_label_create(s_alarmBanner);
  lv_obj_set_align(s_alarmBannerLabel, LV_ALIGN_CENTER);
  lv_label_set_long_mode(s_alarmBannerLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_width(s_alarmBannerLabel, lv_pct(94));
  lv_obj_set_style_text_align(s_alarmBannerLabel, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN);
  lv_obj_set_style_text_font(s_alarmBannerLabel, &lv_font_montserrat_20,
                             LV_PART_MAIN);
}

// --- Chasquido de confirmacion al pulsar ---------------------------------
//
// Se engancha al feedback_cb del driver de entrada de LVGL, que es el punto
// que la propia libreria reserva para esto: se dispara con CUALQUIER pulsacion
// sobre CUALQUIER control, sin tener que acordarse de anadir la llamada en
// cada callback nuevo. Un `beep()` repartido por los cincuenta manejadores de
// este fichero se olvida en el cincuenta y uno.
//
// NO BLOQUEANTE a proposito. hmi_audio_module_beep() existe pero hace
// delay(): llamarlo desde aqui congelaria el despacho de eventos de LVGL
// durante todo el pitido. Aqui se enciende y se apaga desde el bucle de UI.
// 12 ms: un tic seco. Estuvo en 25 y el chasquido competia en protagonismo con
// las senales de alarma, que es justo al reves de lo que debe ser — y en este
// equipo, ademas, el zumbador del display se oye MAS que el de la placa (ver
// docs/alarms.md §9: pendiente de medida acustica).
#define CLICK_BEEP_MS 12u
static uint32_t s_clickBeepUntilMs = 0;

// --- Senal acustica del enlace perdido -----------------------------------
//
// La UNICA alarma que suena en el display. El resto llegan por el enlace desde
// la placa, que ya las esta sonando en su zumbador; esta no puede llegar por
// definicion, porque lo que se ha perdido es el enlace.
//
// Antes, la mitad audible de esta condicion se delegaba entera en la
// motherBoard, que declara ALARM_HMI_LINK_LOST por su cuenta. Funciona cuando
// lo roto es el cable, pero no cuando la que calla es la placa: con su firmware
// colgado o reiniciandose, securityCheck() deja de correr, driveAlarmBuzzer()
// con el, y no sonaba NADA — mientras el display seguia ensenando las ultimas
// cifras recibidas, que el operador lee como actuales.
//
// Suena aunque la placa tambien este sonando. El display no puede saber si
// sigue viva —averiguarlo pide justo el enlace que falta—, asi que con el cable
// roto se oiran los dos. Es redundancia, no la inconsistencia que prohibe
// 6.3.3.1: misma condicion, misma prioridad, mismo patron, porque las dos
// mitades salen de alarm_priority(ALARM_HMI_LINK_LOST) y de las constantes de
// shared/alarm_audio_pattern.h.
//
// Y por eso el patron es el de MEDIA sin retocar: su rafaga ocupa 850 ms de
// cada 25 s, un 3,4 % del tiempo. Ese silencio es lo que impide que este
// zumbador —que en este equipo se oye MAS que el de la placa— tape una alarma
// ALTA real, cuyas rafagas se repiten cada 10 s. Hacer el patron mas denso se
// cargaria la mitigacion.
static uint32_t s_linkAudioBurstStartMs = 0;
static bool     s_linkAudioActive = false;   // la condicion esta anunciandose
static bool     s_linkAudioPinOn = false;    // ultimo estado escrito al zumbador
// Pausa de audio local (AUDIO PAUSED). Estado del display, no del protocolo:
// con el enlace caido el boton de silenciar no llega a la placa, asi que una
// pausa que dependiera de ella seria inservible justo cuando hace falta.
static uint32_t s_linkAudioPauseUntilMs = 0;

static bool link_audio_paused(void) {
  return s_linkAudioPauseUntilMs != 0 &&
         (int32_t)(millis() - s_linkAudioPauseUntilMs) < 0;
}

// true mientras se esta emitiendo una rafaga — incluidos los huecos ENTRE sus
// pulsos. El chasquido de pulsacion tiene que callarse en todo ese tramo, no
// solo cuando el zumbador esta encendido: un tic de 12 ms metido en un hueco
// anade un cuarto pulso a una rafaga de tres.
static bool link_audio_burst_in_progress(void) {
  if (!s_linkAudioActive || link_audio_paused()) {
    return false;
  }
  const uint32_t elapsed =
      (millis() - s_linkAudioBurstStartMs) % ALARM_BURST_PERIOD_MS_MEDIUM;
  return elapsed < ALARM_BURST_LEN_MS_MEDIUM;
}

static void LinkAudio_Pause(void) {
  s_linkAudioPauseUntilMs = millis() + ALARM_AUDIO_PAUSE_MS;
  if (s_linkAudioPinOn) {
    buzzerOff();
    s_linkAudioPinOn = false;
  }
}

static bool LinkAudio_IsPaused(void) { return link_audio_paused(); }

static int LinkAudio_PauseRemainingS(void) {
  if (!link_audio_paused()) {
    return 0;
  }
  return (int)((s_linkAudioPauseUntilMs - millis()) / 1000u);
}

// Sirve el patron desde el bucle de UI. NO BLOQUEANTE por el mismo motivo que
// el chasquido, y ademas FUERA de LVGL_Lock: cada transicion es una
// transaccion I2C y bloquear ahi el mutex del renderer es el fallo
// ARQ-LOCK-001 que ya se corrigio para AlarmSound_Update().
static void link_audio_service(void) {
  const bool lost = Display_IsBoardLinkLost();

  if (!lost) {
    // La condicion se ha ido: callar y olvidar la pausa. Cesar aqui no
    // contradice 6.10 —que prohibe que el audio se agote por haber sonado
    // suficiente—: cesa porque la condicion ha desaparecido, que es la otra
    // via legitima. Y la pausa no sobrevive, para que una perdida posterior
    // suene desde el primer instante.
    if (s_linkAudioPinOn) {
      buzzerOff();
      s_linkAudioPinOn = false;
    }
    s_linkAudioActive = false;
    s_linkAudioPauseUntilMs = 0;
    return;
  }

  const uint32_t now = millis();

  if (!s_linkAudioActive) {
    // La primera rafaga arranca en este mismo instante y no espera al periodo.
    // Es el mismo freshStart del motor de la placa: sin el, el aviso llegaria
    // hasta 25 s tarde.
    s_linkAudioActive = true;
    s_linkAudioBurstStartMs = now;
  }

  if (link_audio_paused()) {
    if (s_linkAudioPinOn) {
      buzzerOff();
      s_linkAudioPinOn = false;
    }
    return;
  }
  if (s_linkAudioPauseUntilMs) {
    // Pausa expirada con la condicion aun presente: el audio se reanuda solo
    // (201.12.3.104), y lo hace con una rafaga entera desde su principio.
    s_linkAudioPauseUntilMs = 0;
    s_linkAudioBurstStartMs = now;
  }

  const uint32_t elapsed =
      (now - s_linkAudioBurstStartMs) % ALARM_BURST_PERIOD_MS_MEDIUM;
  const bool on = alarm_audio_pulse_on(elapsed, ALARM_PRIORITY_MEDIUM);

  // Solo en los CAMBIOS: llamar a buzzerOn()/buzzerOff() en cada pasada seria
  // una transaccion I2C por vuelta del bucle de UI.
  if (on != s_linkAudioPinOn) {
    s_linkAudioPinOn = on;
    if (on) {
      buzzerOn();
    } else {
      buzzerOff();
    }
  }
}

static void click_beep_start(void) {
  // El chasquido cede: comparten el unico zumbador del display, y lo cosmetico
  // no interrumpe a lo normativo.
  if (link_audio_burst_in_progress()) {
    return;
  }
  buzzerOn();
  s_clickBeepUntilMs = millis() + CLICK_BEEP_MS;
}

static void click_beep_service(void) {
  if (s_clickBeepUntilMs && (int32_t)(millis() - s_clickBeepUntilMs) >= 0) {
    // Si la rafaga ha arrancado durante estos 12 ms, el zumbador ya no es
    // nuestro: apagarlo aqui truncaria su primer pulso.
    if (!link_audio_burst_in_progress()) {
      buzzerOff();
    }
    s_clickBeepUntilMs = 0;
  }
}

// Si tocar este objeto HACE algo. Es la definicion util de "control".
//
// NO sirve mirar LV_OBJ_FLAG_CLICKABLE: LVGL lo pone en el constructor de
// TODOS los objetos (lv_obj.c, `obj->flags = LV_OBJ_FLAG_CLICKABLE`), asi que
// las pantallas y los paneles decorativos tambien lo llevan y el chasquido
// sonaba al tocar el fondo.
static bool obj_is_control(lv_obj_t *o) {
  // Una pantalla no tiene padre, y tocar el fondo nunca es pulsar un control.
  if (lv_obj_get_parent(o) == NULL) {
    return false;
  }
  // Widgets que son controles por definicion.
  if (lv_obj_check_type(o, &lv_btn_class) ||
      lv_obj_check_type(o, &lv_imgbtn_class) ||
      lv_obj_check_type(o, &lv_switch_class) ||
      lv_obj_check_type(o, &lv_checkbox_class) ||
      lv_obj_check_type(o, &lv_slider_class) ||
      lv_obj_check_type(o, &lv_dropdown_class) ||
      lv_obj_check_type(o, &lv_roller_class) ||
      lv_obj_check_type(o, &lv_btnmatrix_class)) {
    return true;
  }
  // Contenedores e imagenes con manejador propio: este proyecto los usa como
  // botones (el badge del bloqueo, el check de "todo OK", las tarjetas del
  // centro de alarmas). Un panel decorativo no tiene manejadores y cae aqui.
  return o->spec_attr != NULL && o->spec_attr->event_dsc_cnt > 0;
}

// Teclas: fuera del chasquido. Al escribir se pulsa muchas veces seguidas y un
// pitido por letra es ruido, no confirmacion.
//
// No basta con mirar lv_keyboard_class. El teclado del asistente de bebe es un
// lv_btnmatrix a proposito, no un lv_keyboard (BabyWizard.cpp explica por que:
// LVGL 8.3 guarda los keymaps en un global de fichero y lv_keyboard_set_map()
// reescribiria el mapa de TODOS los teclados de la app). Filtrando solo por
// lv_keyboard_class sonaba una tecla por letra.
//
// Excepcion: la matriz de botones interna de un lv_tabview son pestanas de
// navegacion, no teclas, y esas si deben confirmar.
static bool obj_is_keylike(lv_obj_t *o) {
  for (; o != NULL; o = lv_obj_get_parent(o)) {
    if (lv_obj_check_type(o, &lv_keyboard_class)) {
      return true;
    }
    if (lv_obj_check_type(o, &lv_btnmatrix_class)) {
      lv_obj_t *parent = lv_obj_get_parent(o);
      return parent == NULL || !lv_obj_check_type(parent, &lv_tabview_class);
    }
  }
  return false;
}

static void ui_click_feedback_cb(lv_indev_drv_t *drv, uint8_t event) {
  (void)drv;
  // PRESSED y no CLICKED: el chasquido tiene que salir cuando el dedo toca,
  // no al levantarlo. Con CLICKED se percibe como retardo del equipo.
  if (event != LV_EVENT_PRESSED) {
    return;
  }
  lv_obj_t *obj = lv_indev_get_obj_act();
  if (!obj || !obj_is_control(obj)) {
    return;
  }
  if (obj_is_keylike(obj)) {
    return;
  }
  click_beep_start();
}

// Resuelve el idioma con un helper local en vez de repetir el indexado de
// g_lang en cada cadena. Lo usan el banner y los avisos de esta tarea.
static const char *TXT_UI(const char *es, const char *en, const char *fr) {
  return (g_lang == LANG_ES) ? es : (g_lang == LANG_FR) ? fr : en;
}

// Indicador permanente de AUDIO PAUSED, en lv_layer_top() para que sobreviva a
// los lv_scr_load() y se vea en TODAS las pantallas.
//
// 201.12.3.104 exige que "las alarmas silenciadas deliberadamente mantengan
// indicacion visual", y 6.8.5 que el estado se marque con el simbolo de la
// Tabla 5 y sea legible a 1 m. El icono por fila del centro de alarmas dice
// CUAL esta callada (6.8.1), pero solo se ve con el centro abierto; este dice
// QUE HAY algo callado, siempre, sin depender de donde este el operador.
static lv_obj_t *s_audioPausedIcon = NULL;
static lv_obj_t *s_audioPausedTimer = NULL;
// Ultimo texto pintado en el banner. Existe para NO reescribirlo en cada
// pasada: ver el comentario en alarm_banner_update().
static char s_bannerText[64] = "";

void audio_paused_icon_init(void) {
  // La lamina de la norma (IEC 60417-5576, variante de X DISCONTINUA =
  // AUDIO PAUSED), convertida a mascara de 1 bit. La X continua significaria
  // AUDIO OFF, inactivacion permanente, que este equipo no ofrece.
  // A tamaño natural (48 px), sin zoom ni set_size: escalado con
  // lv_img_set_zoom() el pivote de la transformacion es el centro de la
  // imagen FUENTE, cae fuera del objeto reducido y la imagen se recortaba
  // entera. Mismo fallo que tenia el icono de la fila.
  s_audioPausedIcon = lv_img_create(lv_layer_top());
  lv_img_set_src(s_audioPausedIcon, &ui_img_audio_paused_sym);
  lv_obj_set_style_img_recolor(s_audioPausedIcon, lv_color_hex(0xFFB436), 0);
  lv_obj_set_style_img_recolor_opa(s_audioPausedIcon, LV_OPA_COVER, 0);
  // ABAJO a la derecha, no arriba: la esquina superior derecha es la barra de
  // navegacion de ui_ScreenMain y el icono se solapaba con sus botones. La
  // norma pide indicacion visual mantenida y legible a 1 m (6.8.5,
  // 201.12.3.104), no una posicion concreta, y la franja inferior esta libre
  // desde que el banner solo se pinta arriba y solo en el bloqueo.
  lv_obj_align(s_audioPausedIcon, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
  lv_obj_add_flag(s_audioPausedIcon, LV_OBJ_FLAG_HIDDEN);

  // Cuenta atras JUNTO al icono, que es donde la norma la quiere: "The use of
  // a countdown timer... adjoining the icon, is encouraged... so that they
  // can more easily be distinguished from ALARM OFF or AUDIO OFF" (racional
  // de 6.8.5). Con 10 min de pausa responde ademas a la pregunta util del
  // operador, que es cuando vuelve el sonido.
  s_audioPausedTimer = lv_label_create(lv_layer_top());
  lv_label_set_text(s_audioPausedTimer, "");
  lv_obj_set_style_text_color(s_audioPausedTimer, lv_color_hex(0xFFB436), 0);
  lv_obj_set_style_text_font(s_audioPausedTimer, &lv_font_montserrat_20, 0);
  // A la izquierda del icono, que mide 48 px y esta a -8 del borde inferior.
  lv_obj_align(s_audioPausedTimer, LV_ALIGN_BOTTOM_RIGHT, -60, -20);
  lv_obj_add_flag(s_audioPausedTimer, LV_OBJ_FLAG_HIDDEN);
}

void audio_paused_icon_update(void) {
  if (!s_audioPausedIcon) {
    return;
  }
  // Idempotente por el mismo motivo que el banner: corre en cada pasada, y
  // mover al frente o reescribir la etiqueta invalida y redibuja aunque nada
  // haya cambiado. Solo se toca LVGL cuando cambia la visibilidad o el
  // SEGUNDO que toca pintar — o sea, como mucho una vez por segundo.
  static bool s_wasVisible = false;
  static int s_lastLeft = -1;

  // La pausa LOCAL manda sobre la que cuenta la placa. Cuando esta armada, el
  // enlace esta caido y todo lo que venga en ctrl_state_msg es informacion
  // vieja: su silencedBitmask y su cuenta atras son de antes del corte.
  const bool localPause = LinkAudio_IsPaused();
  const bool visible = localPause || (ctrl_state_msg.silencedBitmask != 0u);
  const int left = localPause ? LinkAudio_PauseRemainingS()
                              : (visible ? ctrl_state_msg.silenceRemainingS : 0);

  if (visible != s_wasVisible) {
    s_wasVisible = visible;
    s_lastLeft = -1;  // fuerza repintar el numero al reaparecer
    if (visible) {
      lv_obj_clear_flag(s_audioPausedIcon, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(s_audioPausedIcon);
    } else {
      lv_obj_add_flag(s_audioPausedIcon, LV_OBJ_FLAG_HIDDEN);
      if (s_audioPausedTimer) {
        lv_obj_add_flag(s_audioPausedTimer, LV_OBJ_FLAG_HIDDEN);
      }
    }
  }

  if (!visible || !s_audioPausedTimer) {
    return;
  }

  // La cuenta atras solo si la placa la manda. Con 0 se enseña el icono a
  // secas: mejor sin numero que con uno inventado.
  if (left != s_lastLeft) {
    s_lastLeft = left;
    if (left > 0) {
      char buf[8];
      snprintf(buf, sizeof(buf), "%d:%02d", left / 60, left % 60);
      lv_label_set_text(s_audioPausedTimer, buf);
      lv_obj_clear_flag(s_audioPausedTimer, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(s_audioPausedTimer);
    } else {
      lv_obj_add_flag(s_audioPausedTimer, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

// Boton de SILENCIAR mientras el enlace esta caido.
//
// Quien decide normalmente si se ve es update_alarm_panels(), y esa corre solo
// cuando llega un CTRL,ALM. Con el enlace caido no llega ninguno, asi que la
// unica alarma que el display anuncia por su cuenta era tambien la unica que no
// se podia callar. Va aparte y en cada pasada, por el mismo motivo que el
// banner: depende de que NO llegue nada.
static void link_audio_mute_button_update(void) {
  if (!ui_MuteAlarm) {
    return;
  }
  static bool s_shownByLink = false;
  const bool want = Display_IsBoardLinkLost() && !LinkAudio_IsPaused();
  if (want == s_shownByLink) {
    return; // idempotente: no se toca LVGL si nada ha cambiado
  }
  s_shownByLink = want;
  if (want) {
    lv_obj_clear_flag(ui_MuteAlarm, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(ui_MuteAlarm, LV_OBJ_FLAG_HIDDEN);
    // Devolver la decision a la ruta normal en vez de dejar el boton como lo
    // hayamos dejado nosotros: si al volver el enlace hay una alarma real sin
    // silenciar, tiene que reaparecer sin esperar al siguiente CTRL,ALM.
    g_pendingAlarmUpdate = true;
  }
}

void alarm_banner_update(void) {
  if (!s_alarmBanner) {
    return;
  }

  // Alarma senalizando de mayor prioridad. La prioridad es la que MANDA la
  // motherBoard en el campo correspondiente de CTRL,ALM: el display no la
  // calcula ni la deduce del titulo. La placa es la dueña de la informacion de
  // alarmas y esta se limita a pintarla; cualquier copia local de la politica
  // de prioridades seria una segunda fuente de verdad esperando a
  // desincronizarse, que es un fallo que este proyecto ya ha tenido.
  int topIdx = -1;
  int topPrio = -1;
  for (int i = 0; i < MAX_ALARMS; i++) {
    if (!alarmList[i].state) {
      continue;
    }
    const int p = (int)alarmList[i].priority;
    if (p > topPrio) {
      topPrio = p;
      topIdx = i;
    }
  }

  // Prueba de funcionamiento (201.12.3.105): la clausula pide comprobar las
  // alarmas "audible AND visual", asi que la prueba tiene que ejercitar
  // tambien esta ruta y no solo el zumbador. Se pinta el banner con el color y
  // el parpadeo de la prioridad que la placa este reproduciendo.
  //
  // Nunca puede tapar una alarma real: la placa cancela la prueba en cuanto
  // aparece una condicion, y aqui ademas solo se entra si no hay ninguna.
  // Display_BoardEverSeen() ademas del centinela: la prueba corre EN LA PLACA,
  // asi que sin haber recibido nunca una linea suya no puede haber ninguna en
  // curso. Cubre la ventana entre la primera linea que llega (un CTRL,TEL,
  // por ejemplo) y el primer CTRL,STATE que trae de verdad el campo.
  const bool testing = (topIdx < 0) && Display_BoardEverSeen() &&
                       (ctrl_state_msg.alarmTestPriority != ALARM_TEST_IDLE_HMI);

  // Enlace con la placa perdido, visto desde este lado.
  //
  // Tiene que verse SIEMPRE y por delante de todo lo demas: cuando la placa
  // calla, lo que hay en pantalla ya no son medidas, son las ultimas que
  // llegaron. El operador las lee como actuales. Por eso este caso gana al
  // banner de cualquier alarma: esas alarmas tambien son informacion vieja.
  //
  // La placa declara ALARM_HMI_LINK_LOST por su cuenta y hace sonar SU
  // zumbador; lo de aqui es la mitad visual. La mitad audible ya NO depende
  // solo de ella: el display tiene la suya propia (link_audio_service(),
  // mas abajo en este fichero), precisamente porque esta es la unica
  // condicion cuyo aviso puede tener que salir cuando la placa ya no puede
  // emitir nada.
  const bool linkLost = Display_IsBoardLinkLost();

  // Con el centro de alarmas abierto el banner sobra: ese overlay ya lista
  // cada alarma activa con su titulo y su marca de prioridad delante, asi que
  // la senal de 1 m que pide 6.3.2.2.2 sigue presente de forma nativa. Y
  // ademas estorba, porque el banner vive tambien en lv_layer_top() y se
  // pintaria por encima de la tarjeta.
  const bool onAlarmsScreen =
      AlarmCenter_IsOpen() ||
      (ui_ScreenAlarms && lv_scr_act() == ui_ScreenAlarms);

  // La supresion con el centro abierto NO se aplica a la prueba ni a la
  // perdida de enlace.
  //
  // La prueba se lanza desde la propia cabecera del centro de alarmas, asi
  // que suprimir ahi el banner dejaba la prueba MUDA de vista: sonaba pero no
  // se veia nada, que es media clausula sin cumplir (201.12.3.105 pide
  // comprobar "audible AND visual"). Y con el enlace caido, el contenido del
  // centro es tan viejo como el resto: taparlo con el aviso es lo correcto.
  //
  // El banner es una franja fina arriba y se pone en primer plano en cada
  // pasada, asi que se lee sobre la tarjeta sin ocultar lo que importa.
  const bool suppressed = onAlarmsScreen && !testing && !linkLost;

  if ((topIdx < 0 && !testing && !linkLost) || suppressed) {
    lv_anim_del(s_alarmBanner, banner_blink_cb);
    lv_obj_add_flag(s_alarmBanner, LV_OBJ_FLAG_HIDDEN);
    s_bannerPriority = -1;
    return;
  }

  // SOLO en la pantalla de bloqueo, en la franja superior. Es donde el equipo
  // pasa la mayor parte del tiempo por el autolock, asi que la senal se lleva
  // la posicion buena justo donde mas se ve, y ahi arriba no tapa nada.
  //
  // En el resto de pantallas el banner ya no se pinta: en ellas hay alguien
  // delante operando, con el icono de alarmas de la barra a la vista, y una
  // franja fija en el borde inferior estorbaba mas de lo que avisaba.
  //
  // Se reevalua en cada pasada y no al cambiar de pantalla: el banner cuelga
  // de lv_layer_top(), que no se entera de los lv_scr_load().
  //
  // Excepcion: durante la prueba de funcionamiento SI se pinta en cualquier
  // pantalla. El operador la lanza desde ajustes y tiene que ver alli mismo
  // que la senal visual responde; obligarle a bloquear la pantalla para
  // comprobarlo haria inservible la prueba.
  const bool onLockScreen = (ui_ScreenLock && lv_scr_act() == ui_ScreenLock);
  if (!onLockScreen && !testing && !linkLost) {
    lv_anim_del(s_alarmBanner, banner_blink_cb);
    lv_obj_add_flag(s_alarmBanner, LV_OBJ_FLAG_HIDDEN);
    s_bannerPriority = -1;
    s_bannerText[0] = '\0';
    return;
  }
  lv_obj_set_align(s_alarmBanner, LV_ALIGN_TOP_MID);

  const char *wantText;
  if (linkLost) {
    // Por delante de cualquier alarma: si la placa calla, lo que se ve de
    // ella es viejo, incluidas sus alarmas. MEDIA para contar lo mismo que
    // ALARM_HMI_LINK_LOST al otro lado — un unico relato para el operador.
    topPrio = ALARM_PRIORITY_MEDIUM;
    wantText = TXT_UI("!! SIN ENLACE CON LA PLACA", "!! BOARD LINK LOST",
                      "!! LIAISON CARTE PERDUE");
  } else if (testing) {
    topPrio = ctrl_state_msg.alarmTestPriority;
    wantText = TXT_UI("PRUEBA DE ALARMA", "ALARM TEST", "TEST D'ALARME");
  } else {
    wantText = alarmList[topIdx].type;
  }

  // Solo se toca LVGL si el texto CAMBIO.
  //
  // Esta funcion corre en cada pasada del bucle de UI, y lv_label_set_text()
  // y lv_obj_move_foreground() invalidan y fuerzan redibujado aunque el
  // contenido sea identico. Llamarlos a la frecuencia del bucle repintaba la
  // franja y reordenaba lv_layer_top() sin parar, y arrastraba toda la
  // interfaz. No se noto al escribirlo porque el banner solo salia en la
  // pantalla de bloqueo, donde no hay nada mas compitiendo por el redibujado.
  if (strncmp(s_bannerText, wantText, sizeof(s_bannerText) - 1) != 0) {
    snprintf(s_bannerText, sizeof(s_bannerText), "%s", wantText);
    lv_label_set_text(s_alarmBannerLabel, s_bannerText);
    lv_obj_clear_flag(s_alarmBanner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_alarmBanner);
  }

  if (topPrio == s_bannerPriority) {
    return;  // misma prioridad: no reiniciar la animacion en cada ciclo
  }
  s_bannerPriority = topPrio;
  lv_anim_del(s_alarmBanner, banner_blink_cb);

  uint32_t halfPeriodMs = 0;
  lv_color_t textColor;
  switch (topPrio) {
    case ALARM_PRIORITY_HIGH:
      s_bannerHi = lv_color_hex(0xFF5468);
      s_bannerLo = lv_color_hex(0x5A1822);
      textColor = lv_color_hex(0xFFFFFF);
      halfPeriodMs = BANNER_HALF_PERIOD_MS_HIGH;
      break;
    case ALARM_PRIORITY_MEDIUM:
      s_bannerHi = lv_color_hex(0xFFB436);
      s_bannerLo = lv_color_hex(0x6A4810);
      textColor = lv_color_hex(0x1A1208);
      halfPeriodMs = BANNER_HALF_PERIOD_MS_MEDIUM;
      break;
    default:  // BAJA: constante, sin parpadeo (Tabla 2)
      s_bannerHi = lv_color_hex(0x4EC7FF);
      s_bannerLo = s_bannerHi;
      textColor = lv_color_hex(0x0A1D26);
      halfPeriodMs = 0;
      break;
  }
  lv_obj_set_style_text_color(s_alarmBannerLabel, textColor, LV_PART_MAIN);

  if (halfPeriodMs == 0) {
    lv_obj_set_style_bg_color(s_alarmBanner, s_bannerHi, LV_PART_MAIN);
    return;
  }

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, s_alarmBanner);
  lv_anim_set_values(&a, 255, 0);  // 255 = color pleno, 0 = oscurecido
  lv_anim_set_time(&a, halfPeriodMs);
  lv_anim_set_playback_time(&a, halfPeriodMs);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_exec_cb(&a, banner_blink_cb);
  lv_anim_start(&a);
}

void update_alarm_panels() {
  // Con el enlace caido, el check "OK" no puede decir la verdad: no sabemos
  // si hay una alarma real en curso, solo que hemos dejado de escuchar a la
  // placa. El banner de "LINK LOST" ya lo comunica (alarm_banner_update() SI
  // consulta Display_IsBoardLinkLost()); este check viejo no lo hacia, y por
  // eso se quedaba en verde justo cuando menos hay que fiarse de la pantalla.
  const bool linkLost = Display_IsBoardLinkLost();

  // Determine fan/heater alarm state first — used in multiple sections below
  bool fanHeaterAlarm = false;
  for (int i = 0; i < MAX_ALARMS; i++) {
    if ((alarmList[i].id == ALARM_FAN_FAILURE || alarmList[i].id == ALARM_HEATER_FAULT) && alarmList[i].state) {
      fanHeaterAlarm = true;
      break;
    }
  }

  int totalActiveAlarms = 0;
  int activeCount = 0;
  alarmSlotToIndex[0] = -1;
  alarmSlotToIndex[1] = -1;
  alarmSlotToIndex[2] = -1;
  alarmSlotToIndex[3] = -1;

  bool selectedStillActive = false;

  for (int i = 0; i < MAX_ALARMS; i++) {
    if (alarmList[i].state) {
      totalActiveAlarms++;
      if (activeCount < MAX_ALARM_DISPLAY) {
        alarmSlotToIndex[activeCount] = i;
        activeCount++;
      }
      if (alarmList[i].id == g_selectedAlarmId) {
        selectedStillActive = true;
      }
    }
  }

  // Limpiar descripción de alarma si ya no está activa (Casos A y C)
  if (g_selectedAlarmId != -1 && !selectedStillActive) {
    if (ui_AlarmDetailLabel)
      lv_label_set_text(ui_AlarmDetailLabel, "");
    g_selectedAlarmId = -1;
  }

  alarmActive = (totalActiveAlarms > 0);
  hmi_msg.muteAlarm = alarmActive ? (alarmsMuted ? 1 : 0) : 0;

  if (activeCount > NUM_ALARM_0) {
    lv_obj_clear_flag(ui_Alarm1Cont, LV_OBJ_FLAG_HIDDEN);
    int idx = alarmSlotToIndex[0];
    lv_label_set_text(ui_Alarm1Label, alarmList[idx].type);
    start_alarm_blink(ui_Alarm1Panel);
  }
  if (activeCount > NUM_ALARM_1) {
    lv_obj_clear_flag(ui_Alarm2Cont, LV_OBJ_FLAG_HIDDEN);
    int idx = alarmSlotToIndex[1];
    lv_label_set_text(ui_Alarm2Label, alarmList[idx].type);
    start_alarm_blink(ui_Alarm2Panel);
  }
  if (activeCount > NUM_ALARM_2) {
    lv_obj_clear_flag(ui_Alarm3Cont, LV_OBJ_FLAG_HIDDEN);
    int idx = alarmSlotToIndex[2];
    lv_label_set_text(ui_Alarm3Label, alarmList[idx].type);
    start_alarm_blink(ui_Alarm3Panel);
  }
  if (activeCount > NUM_ALARM_3) {
    lv_obj_clear_flag(ui_Alarm4Cont, LV_OBJ_FLAG_HIDDEN);
    int idx = alarmSlotToIndex[3];
    lv_label_set_text(ui_Alarm4Label, alarmList[idx].type);
    start_alarm_blink(ui_Alarm4Panel);
  }

  int pos = activeCount;
  if (pos < MAX_ALARM_DISPLAY) {
    if (pos <= NUM_ALARM_0)
      lv_obj_add_flag(ui_Alarm1Cont, LV_OBJ_FLAG_HIDDEN);
    if (pos <= NUM_ALARM_1)
      lv_obj_add_flag(ui_Alarm2Cont, LV_OBJ_FLAG_HIDDEN);
    if (pos <= NUM_ALARM_2)
      lv_obj_add_flag(ui_Alarm3Cont, LV_OBJ_FLAG_HIDDEN);
    if (pos <= NUM_ALARM_3)
      lv_obj_add_flag(ui_Alarm4Cont, LV_OBJ_FLAG_HIDDEN);
  }

  bool anyAlarm = (totalActiveAlarms > 0) || fanHeaterAlarm;
  if (totalActiveAlarms > 0) {
    char buf[10];
    itoa(totalActiveAlarms, buf, 10);
    // Main screen
    lv_obj_clear_flag(ui_AlarmButton, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_Panel10, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_NumAlarm, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ui_NumAlarm, buf);
    lv_obj_add_flag(ui_CheckImgMain, LV_OBJ_FLAG_HIDDEN);
    // Lock screen
    lv_obj_clear_flag(ui_AlarmLockCont, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ui_AlarmLockNumLabel, buf);
    lv_obj_add_flag(ui_CheckImg, LV_OBJ_FLAG_HIDDEN);
  } else {
    // Main screen — hide alarm button; OK only if no fan/heater alarm either,
    // y solo si la placa sigue hablando: "OK" es una afirmacion, no un valor
    // por defecto.
    lv_obj_add_flag(ui_Panel10, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_NumAlarm, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_AlarmButton, LV_OBJ_FLAG_HIDDEN);
    if (fanHeaterAlarm || linkLost)
      lv_obj_add_flag(ui_CheckImgMain, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_clear_flag(ui_CheckImgMain, LV_OBJ_FLAG_HIDDEN);
    // Lock screen
    lv_obj_add_flag(ui_AlarmLockCont, LV_OBJ_FLAG_HIDDEN);
    if (fanHeaterAlarm || linkLost)
      lv_obj_add_flag(ui_CheckImg, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_clear_flag(ui_CheckImg, LV_OBJ_FLAG_HIDDEN);
  }

  if (!anyAlarm) {
    alarmsMuted = false;
  }

  // Senal visual de 1 m, en todas las pantallas (6.3.2.2.2).
  alarm_banner_update();

  // El boton de silencio se creaba en ui_ScreenAlarms y se ocultaba al
  // arrancar, pero no habia ni un solo clear_flag en todo el fichero: nunca
  // llegaba a mostrarse, asi que el operador no tenia forma de silenciar una
  // alarma. Pasaba desapercibido porque el zumbador de la motherboard se
  // agotaba solo a los ~4 min; desde que el audio solo cesa por accion del
  // operador (IEC 60601-1-8 6.10) es la diferencia entre una alarma que se
  // puede callar y una que no.
  //
  // Visible mientras haya alarma activa y no este ya silenciada. Al pulsarlo,
  // MuteAlarm_cb() lo vuelve a ocultar y marca alarmsMuted; reaparece solo si
  // el silencio se cancela, que ocurre cuando se limpian todas las alarmas
  // (justo arriba) o cuando llega una alarma nueva (CommTask).
  if (ui_MuteAlarm) {
    if (alarmActive && !alarmsMuted) {
      lv_obj_clear_flag(ui_MuteAlarm, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(ui_MuteAlarm, LV_OBJ_FLAG_HIDDEN);
    }
  }

  // Warning overlays always hidden (replaced by TempCont visibility)
  if (ui_HeaterErrorTempCont)
    lv_obj_add_flag(ui_HeaterErrorTempCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_HeaterErrorHumCont)
    lv_obj_add_flag(ui_HeaterErrorHumCont, LV_OBJ_FLAG_HIDDEN);

  // Hide the entire temperature control container when fan/heater alarm is active
  if (ui_TempCont) {
    if (fanHeaterAlarm)
      lv_obj_add_flag(ui_TempCont, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_clear_flag(ui_TempCont, LV_OBJ_FLAG_HIDDEN);
  }
}

// Event handler for Heater Error Click
static void HeaterError_event_handler(lv_event_t *e) {
  // Go to Alarms Screen
  _ui_screen_change(&ui_ScreenAlarms, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                    &ui_ScreenAlarms_screen_init);
  lv_tabview_set_act(ui_AlarmsTabview, 1,
                     LV_ANIM_OFF); // Go to Description Tab (Page 1)

  // Set specific description
  const char *TXT_HEATER_ERROR_DESC[] = {
      "Hay un problema con el ventilador / calefactor: \n\n 1. Pruebe a "
      "desconectar y conectar el calefactor y posteriormente reinicie "
      "incubadora\n\n 2. Si no funciona el paso 1, arregle el calefactor.",
      "There is a problem with the fan / heater:\n\n 1. Try unplugging and "
      "plugging in the heater and then restart the incubator\n\n 2. If step 1 "
      "doesn't work, fix the heater.",
      "Il y a un probleme avec le ventilateur / chauffage :\n\n 1. Essayez de "
      "debrancher et de brancher le chauffage et redemarrez l'incubateur\n\n "
      "2. Si le pas 1 ne fonctionne pas, reparez le chauffage."};
  if (ui_AlarmDetailLabel) {
    lv_label_set_text(ui_AlarmDetailLabel, TXT_HEATER_ERROR_DESC[g_lang]);
    g_selectedAlarmId = ALARM_HEATER_FAULT;
  }
}

void show_alarm_detail_from_slot(int slot) {
  if (slot < 0 || slot >= MAX_ALARM_DISPLAY)
    return;
  int idx = alarmSlotToIndex[slot];
  if (idx < 0)
    return;
  lv_tabview_set_act(ui_AlarmsTabview, 1, LV_ANIM_ON);
  lv_label_set_text(ui_AlarmDetailLabel, alarmList[idx].description);
  g_selectedAlarmId = alarmList[idx].id;
}

// Estos cuatro solo abren el detalle de una alarma: no cambian nada de la
// trama, asi que no mandan estado.
void Alarm1Cont_cb(lv_event_t *e) {
  show_alarm_detail_from_slot(0);
}
void Alarm2Cont_cb(lv_event_t *e) {
  show_alarm_detail_from_slot(1);
}
void Alarm3Cont_cb(lv_event_t *e) {
  show_alarm_detail_from_slot(2);
}
void Alarm4Cont_cb(lv_event_t *e) {
  show_alarm_detail_from_slot(3);
}

void reset_alarm_detail_state() {
  if (ui_AlarmDetailLabel)
    lv_label_set_text(ui_AlarmDetailLabel, "");
  g_selectedAlarmId = -1;
}

void AlarmButton_cb(lv_event_t *e) {
  reset_alarm_detail_state();
  lv_tabview_set_act(ui_AlarmsTabview, 0, LV_ANIM_ON);
}

void AlarmsTabview_cb(lv_event_t *e) {
  if (!ui_AlarmsTabview)
    return;
  uint16_t act = lv_tabview_get_tab_act(ui_AlarmsTabview);
  if (act == 0) { // Si volvemos a la lista de alarmas
    reset_alarm_detail_state();
  }
  // Cambiar de pestana no cambia nada de la trama.
}

void chart_add_hum_value(float hum) {
  if (humSeries == NULL)
    return;
  if (hum < 0)
    hum = 0;
  if (hum > 100)
    hum = 100;
  lv_chart_set_next_value(ui_HumChart, humSeries, (lv_coord_t)hum);
}

void AlarmSound_Update() {
  // El display no emite sonido por las alarmas que le LLEGAN: esas las anuncia
  // la placa, que ya las esta sonando. La unica excepcion es la perdida de
  // enlace, que no puede llegar por el enlace y tiene su propio servicio
  // (link_audio_service).
  //
  // Por eso el apagado es condicional: esta funcion corre al final del bucle y
  // desde MuteAlarm_cb(), y un buzzerOff() a secas truncaria un pulso de esa
  // rafaga.
  if (!link_audio_burst_in_progress()) {
    buzzerOff();
  }
}

void MuteAlarm_cb(lv_event_t *e) {
  (void)e;
  alarmsMuted = true;
  g_muteHoldUntilMs = millis() + MUTE_HOLD_MS;
  hmi_msg.muteAlarm = 1;
  hmi_msg.shouldSendData = true;
  // Con el enlace caido, esta peticion NO llega a la placa — es justo lo que
  // se ha perdido. Se manda igualmente (no estorba, y si el enlace vuelve
  // dentro de la ventana el silencio se aplica), pero la senal del display la
  // calla su propia pausa, que se cuenta aqui.
  if (Display_IsBoardLinkLost()) {
    LinkAudio_Pause();
  }
  lv_obj_add_flag(ui_MuteAlarm, LV_OBJ_FLAG_HIDDEN);
  AlarmSound_Update();
}

static void show_targets_for_mode(void) {
  if (unlockTimeoutTimer) {
    lv_timer_del(unlockTimeoutTimer);
    unlockTimeoutTimer = NULL;
  }
  lv_obj_add_flag(ui_UnlockCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_HumLockDesiredCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ArrowAirLock, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ArrowSkinLock, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ArrowHumLock, LV_OBJ_FLAG_HIDDEN);

  if (tempSwitched) {
    if (selectedPanel == AIR_PANEL_SELECTED) {
      lv_obj_clear_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui_ArrowAirLock, LV_OBJ_FLAG_HIDDEN);
    } else if (selectedPanel == SKIN_PANEL_SELECTED) {
      lv_obj_clear_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui_ArrowSkinLock, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (humidityEnabled && switchHum) {
    lv_obj_clear_flag(ui_HumLockDesiredCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ArrowHumLock, LV_OBJ_FLAG_HIDDEN);
  }

  // FIRST COLUMN (Detected Values) ALWAYS VISIBLE
  if (ui_AirTempLockCont)
    lv_obj_clear_flag(ui_AirTempLockCont, LV_OBJ_FLAG_HIDDEN);
  if (ui_SkinTempLockCont)
    lv_obj_clear_flag(ui_SkinTempLockCont, LV_OBJ_FLAG_HIDDEN);
  if (humidityEnabled && ui_HumLockCont)
    lv_obj_clear_flag(ui_HumLockCont, LV_OBJ_FLAG_HIDDEN);
}

static void unlock_timeout_cb(lv_timer_t *t) {
  (void)t;
  locked = true;
  show_targets_for_mode();
  if (unlockTimeoutTimer) {
    lv_timer_del(unlockTimeoutTimer);
    unlockTimeoutTimer = NULL;
  }
}

static void show_unlock_only(void) {
  lv_obj_clear_flag(ui_UnlockCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(ui_UnlockCont);
  if (ui_LockButton) lv_obj_add_flag(ui_LockButton, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_HumLockDesiredCont, LV_OBJ_FLAG_HIDDEN);

  // Start or reset inactivity timer to hide UnlockCont
  if (unlockTimeoutTimer) {
    lv_timer_reset(unlockTimeoutTimer);
  } else {
    unlockTimeoutTimer =
        lv_timer_create(unlock_timeout_cb, UNLOCK_TIMEOUT_MS, NULL);
    lv_timer_set_repeat_count(unlockTimeoutTimer, 1);
  }
}

static void reset_border_bars(void) {
  if (ui_BorderTop)    lv_obj_set_width(ui_BorderTop, 0);
  if (ui_BorderRight)  lv_obj_set_height(ui_BorderRight, 0);
  if (ui_BorderBottom) {
    lv_obj_set_width(ui_BorderBottom, 0);
    lv_obj_set_x(ui_BorderBottom, UNLOCK_POPUP_W);
  }
  if (ui_BorderLeft) {
    lv_obj_set_height(ui_BorderLeft, 0);
    lv_obj_set_y(ui_BorderLeft, UNLOCK_POPUP_H);
  }
}

static void lock_progress_timer_cb(lv_timer_t *t) {
  (void)t;
  uint32_t elapsed = lv_tick_get() - lockProgressStart;
  if (elapsed > LOCK_PROGRESS_DURATION_MS)
    elapsed = LOCK_PROGRESS_DURATION_MS;

  // 4 phases, 25% each: top→right→bottom(RTL)→left(BTT)
  int pct = (int)(elapsed * 100 / LOCK_PROGRESS_DURATION_MS);

  if (pct <= 25) {
    if (ui_BorderTop)
      lv_obj_set_width(ui_BorderTop, (lv_coord_t)(UNLOCK_POPUP_W * pct / 25));
  } else if (pct <= 50) {
    if (ui_BorderTop)   lv_obj_set_width(ui_BorderTop, UNLOCK_POPUP_W);
    if (ui_BorderRight) lv_obj_set_height(ui_BorderRight,
                          (lv_coord_t)(UNLOCK_POPUP_H * (pct - 25) / 25));
  } else if (pct <= 75) {
    if (ui_BorderRight) lv_obj_set_height(ui_BorderRight, UNLOCK_POPUP_H);
    if (ui_BorderBottom) {
      lv_coord_t bw = (lv_coord_t)(UNLOCK_POPUP_W * (pct - 50) / 25);
      lv_obj_set_width(ui_BorderBottom, bw);
      lv_obj_set_x(ui_BorderBottom, UNLOCK_POPUP_W - bw);
    }
  } else {
    if (ui_BorderBottom) {
      lv_obj_set_width(ui_BorderBottom, UNLOCK_POPUP_W);
      lv_obj_set_x(ui_BorderBottom, 0);
    }
    if (ui_BorderLeft) {
      lv_coord_t lh = (lv_coord_t)(UNLOCK_POPUP_H * (pct - 75) / 25);
      lv_obj_set_height(ui_BorderLeft, lh);
      lv_obj_set_y(ui_BorderLeft, UNLOCK_POPUP_H - lh);
    }
  }

  if (elapsed >= LOCK_PROGRESS_DURATION_MS) {
    if (lockProgressTimer) {
      lv_timer_del(lockProgressTimer);
      lockProgressTimer = NULL;
    }
    locked = false;
    lv_scr_load(ui_ScreenMain);
    lv_obj_add_flag(ui_UnlockCont, LV_OBJ_FLAG_HIDDEN);
  }
}

static void start_lock_progress(void) {
  reset_border_bars();
  lockProgressStart = lv_tick_get();
  if (lockProgressTimer) {
    lv_timer_del(lockProgressTimer);
    lockProgressTimer = NULL;
  }
  lockProgressTimer = lv_timer_create(lock_progress_timer_cb, 50, NULL);
}

static void stop_lock_progress(void) {
  if (lockProgressTimer) {
    lv_timer_del(lockProgressTimer);
    lockProgressTimer = NULL;
  }
  reset_border_bars();
}

static void enter_lock_screen(void) {
  if (lv_scr_act() == ui_ScreenLock) {
    stop_lock_progress();
    locked = true;
    show_targets_for_mode();
    return;
  }
  stop_lock_progress();
  locked = true;
  lv_scr_load(ui_ScreenLock);
  show_targets_for_mode();
  lv_disp_trig_activity(NULL);
}

void ImgButton1_Lock_cb(lv_event_t *e) {
  (void)e;
  if (lv_scr_act() == ui_ScreenMain) {
    enter_lock_screen();
  }
  // El bloqueo es estado local del display: no viaja en HMI_Message.
}

void LockScreenAnyTouch_cb(lv_event_t *e) {
  if (lv_scr_act() != ui_ScreenLock)
    return;
  lv_obj_t *origin = lv_event_get_target(e);
  // Ignore touches that originate inside the popup itself
  if (ui_UnlockCont) {
    lv_obj_t *o = origin;
    while (o && o != lv_scr_act()) {
      if (o == ui_UnlockCont) return;
      o = lv_obj_get_parent(o);
    }
  }

  bool unlockVisible = !lv_obj_has_flag(ui_UnlockCont, LV_OBJ_FLAG_HIDDEN);

  if (!unlockVisible) {
    show_unlock_only();
    locked = false;
  } else {
    stop_lock_progress();
    show_targets_for_mode();
    locked = true;
  }
}

static void lock_stop_debounce_cb(lv_timer_t *t) {
  (void)t;
  lockStopDebounceTimer = NULL;
  stop_lock_progress();
  if (unlockTimeoutTimer) {
    lv_timer_resume(unlockTimeoutTimer);
    lv_timer_reset(unlockTimeoutTimer);
  }
}

static void UnlockCont_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED) {
    // Cancel any pending stop (handles finger drift between children)
    if (lockStopDebounceTimer) {
      lv_timer_del(lockStopDebounceTimer);
      lockStopDebounceTimer = NULL;
    }
    // Only start if not already running (prevents duplicate events)
    if (!lockProgressTimer) {
      start_lock_progress();
    }
    if (unlockTimeoutTimer) {
      lv_timer_pause(unlockTimeoutTimer);
    }
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    // Debounce: tolerate brief finger drift between children (150 ms window)
    if (!lockStopDebounceTimer) {
      lockStopDebounceTimer = lv_timer_create(lock_stop_debounce_cb, 20, NULL);
      lv_timer_set_repeat_count(lockStopDebounceTimer, 1);
    }
  }
}

static void add_unlock_press_cb_recursive(lv_obj_t *obj) {
  if (!obj)
    return;
  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(obj, UnlockCont_event_cb, LV_EVENT_ALL, NULL);
  uint32_t n = lv_obj_get_child_cnt(obj);
  for (uint32_t i = 0; i < n; i++) {
    lv_obj_t *child = lv_obj_get_child(obj, i);
    add_unlock_press_cb_recursive(child);
  }
}

// Ring around the main-screen padlock: fills clockwise as inactivity builds
// up, so a complete circle means the auto-lock is about to fire. Any touch
// resets lv_disp_get_inactive_time() and therefore empties the ring.
static void update_autolock_ring(uint32_t inactive_ms) {
  if (!ui_LockAutoArc)
    return;
  int16_t pct = (inactive_ms >= INACTIVITY_TIMEOUT_MS)
                    ? 100
                    : (int16_t)(inactive_ms * 100 / INACTIVITY_TIMEOUT_MS);
  if (lv_arc_get_value(ui_LockAutoArc) != pct)
    lv_arc_set_value(ui_LockAutoArc, pct);
}

void inactivity_timer_cb(lv_timer_t *timer) {
  // Exentos del auto-bloqueo: la pantalla de alarmas, y la ayuda (menu con
  // QR o tutorial guiado): leer un QR o seguir un paso lleva mas de
  // INACTIVITY_TIMEOUT_MS sin tocar, y bloquear a mitad lo tiraria todo.
  if (lv_scr_act() == ui_ScreenAlarms || HelpDialog_IsOpen() ||
      HelpTour_IsOpen()) {
    lv_disp_trig_activity(NULL);
    update_autolock_ring(0);
    return;
  }
  uint32_t inactive = lv_disp_get_inactive_time(NULL);
  update_autolock_ring(inactive);
  if (inactive > INACTIVITY_TIMEOUT_MS) {
    if (lv_scr_act() != ui_ScreenLock) {
      stop_lock_progress();
      locked = true;
      lv_scr_load(ui_ScreenLock);
      show_targets_for_mode();
    }
  }
}

void WifiConnectButton_cb(lv_event_t *e) {
  hmi_msg.shouldSendData = true;
  extern char pendingSSID[64];
  extern char pendingPass[64];
  strncpy(pendingSSID, wifi_ssid, sizeof(pendingSSID));
  strncpy(pendingPass, wifi_pass, sizeof(pendingPass));

  Communication_SendWiFiCredentials(pendingSSID, pendingPass);
  vTaskDelay(
      pdMS_TO_TICKS(100)); // Ensure serial is clear before WiFi logs start
  // El backoff acumulado por reintentos automáticos previos no debe penalizar a
  // quien pulsa "Conectar": si este intento falla, el siguiente automático debe
  // volver a los 30s base y no al tope.
  wifiResetReconnectBackoff();
  wifiInit();              // Trigger new connection attempt
  // isConnected se actualiza en el loop principal via WiFi.status()
}

void WifiDisconnectButton_cb(lv_event_t *e) {
  WiFi.disconnect();
  pendingReconnect = true;
  disconnectTimestampMs = millis();
  // Show the config form immediately for responsiveness. The loop will update
  // isConnected and button visibility once WiFi.status() confirms the disconnect.
  lv_obj_clear_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);
}

void Label9_cb(lv_event_t *e) {
  if (isFanHeaterAlarmActive()) return;
  lv_obj_add_state(ui_Switch1, LV_STATE_CHECKED);
  lv_event_send(ui_Switch1, LV_EVENT_VALUE_CHANGED, NULL);
}

void Label15_cb(lv_event_t *e) {
  lv_obj_clear_state(ui_Switch1, LV_STATE_CHECKED);
  lv_event_send(ui_Switch1, LV_EVENT_VALUE_CHANGED, NULL);
}

void Label13_cb(lv_event_t *e) {
  lv_obj_add_state(ui_Switch2, LV_STATE_CHECKED);
  lv_event_send(ui_Switch2, LV_EVENT_VALUE_CHANGED, NULL);
}

void Label16_cb(lv_event_t *e) {
  lv_obj_clear_state(ui_Switch2, LV_STATE_CHECKED);
  lv_event_send(ui_Switch2, LV_EVENT_VALUE_CHANGED, NULL);
}

void Label10_cb(lv_event_t *e) {
  lv_obj_add_state(ui_Switch3, LV_STATE_CHECKED);
  lv_event_send(ui_Switch3, LV_EVENT_VALUE_CHANGED, NULL);
}

void Label17_cb(lv_event_t *e) {
  lv_obj_clear_state(ui_Switch3, LV_STATE_CHECKED);
  lv_event_send(ui_Switch3, LV_EVENT_VALUE_CHANGED, NULL);
}

void TempToggleBtn_cb(lv_event_t *) {
  bool isOn = lv_obj_has_state(ui_Switch1, LV_STATE_CHECKED);
  if (!isOn && isFanHeaterAlarmActive()) return;
  if (isOn)
    lv_obj_clear_state(ui_Switch1, LV_STATE_CHECKED);
  else
    lv_obj_add_state(ui_Switch1, LV_STATE_CHECKED);
  lv_event_send(ui_Switch1, LV_EVENT_VALUE_CHANGED, NULL);
}

void HumToggleBtn_cb(lv_event_t *) {
  if (lv_obj_has_state(ui_Switch2, LV_STATE_CHECKED))
    lv_obj_clear_state(ui_Switch2, LV_STATE_CHECKED);
  else
    lv_obj_add_state(ui_Switch2, LV_STATE_CHECKED);
  lv_event_send(ui_Switch2, LV_EVENT_VALUE_CHANGED, NULL);
}

void PhotoToggleBtn_cb(lv_event_t *) {
  if (lv_obj_has_state(ui_Switch3, LV_STATE_CHECKED))
    lv_obj_clear_state(ui_Switch3, LV_STATE_CHECKED);
  else
    lv_obj_add_state(ui_Switch3, LV_STATE_CHECKED);
  lv_event_send(ui_Switch3, LV_EVENT_VALUE_CHANGED, NULL);
}

void ui_set_switch_state_silent(lv_obj_t *sw, bool on) {
  if (!sw)
    return;
  if (on)
    lv_obj_add_state(sw, LV_STATE_CHECKED);
  else
    lv_obj_clear_state(sw, LV_STATE_CHECKED);
}

void Settings_cb(lv_event_t *e) {
  reset_alarm_detail_state();
  _ui_screen_change(&ui_ScreenSettings, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                    &ui_ScreenSettings_screen_init);
}

void SPO2Button_cb(lv_event_t *e) {
  reset_alarm_detail_state();
  _ui_screen_change(&ui_ScreenPulseOxi, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                    &ui_ScreenPulseOxi_screen_init);
}

void ChartButton_cb(lv_event_t *e) {
  reset_alarm_detail_state();
  _ui_screen_change(&ui_ScreenCharts, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                    &ui_ScreenCharts_screen_init);

  if (g_ui_initialized) {
    // Si la pantalla de charts se acaba de inicializar, podemos acceder a
    // ui_TabView1 Nota: ui_TabView1 se crea en ui_ScreenCharts_screen_init
    lv_tabview_set_act(ui_TabViewMainCharts, 0,
                       LV_ANIM_OFF); // Ir a Tiempo Real por defecto

    // Ocultar/Mostrar páginas del TabView según estado de sensores
    if (tempSwitched && humSwitched) {
      lv_tabview_set_act(ui_TabView1, 0,
                         LV_ANIM_OFF); // Mostrar Temp por defecto
    } else if (tempSwitched) {
      lv_tabview_set_act(ui_TabView1, 0, LV_ANIM_OFF); // Forzar Temp
    } else if (humSwitched) {
      lv_tabview_set_act(ui_TabView1, 1, LV_ANIM_OFF); // Forzar Hum
    }
  }
}

void AlarmLockImg_cb(lv_event_t *e) {
  (void)e;
  reset_alarm_detail_state();
  AlarmCenter_Open();
}

// El check verde de "todo OK" tambien abre el centro de alarmas.
//
// Sin esto el registro era inaccesible en el caso mas comun: sin alarmas
// activas se ocultan las DOS vias (ui_AlarmButton en la barra y el badge del
// bloqueo) y en su sitio aparece este check, que no era pulsable. Es decir,
// el historial solo se podia consultar mientras algo estaba sonando — justo
// cuando a nadie le interesa ponerse a leer el registro.
void CheckImg_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  reset_alarm_detail_state();
  AlarmCenter_Open();
}

void AlarmLockCont_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    AlarmLockImg_cb(e);
  }
}

void ImgButton2_cb(lv_event_t *e) {
  _ui_screen_change(&ui_ScreenMain, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                    &ui_ScreenMain_screen_init);
}

void ImgButton7_cb(lv_event_t *e) {
  reset_alarm_detail_state();
  _ui_screen_change(&ui_ScreenMain, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                    &ui_ScreenMain_screen_init);
}

void ImgButton8_cb(lv_event_t *e) {
  _ui_screen_change(&ui_ScreenMain, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                    &ui_ScreenMain_screen_init);
}

void ImgButton9_cb(lv_event_t *e) {
  _ui_screen_change(&ui_ScreenMain, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                    &ui_ScreenMain_screen_init);
}

// ==========================================
// Callbacks
// ==========================================
void AudioTestBtn_cb(lv_event_t *e) {
  // Audio deshabilitado para migración Arduino 3.x
  // AudioManager::getInstance().playTone();
}

void AudioStopBtn_cb(lv_event_t *e) {
  // Audio deshabilitado para migración Arduino 3.x
  // AudioManager::getInstance().stop();
}

void VolumeUp_cb(lv_event_t *e) {
  // Audio deshabilitado para migración Arduino 3.x
}

void VolumeDown_cb(lv_event_t *e) {
  // Audio deshabilitado para migración Arduino 3.x
}

// ==========================================
// Power-Off Popup
// ==========================================
static void pwroff_create_popup(void) {
  if (ui_PwrOffOverlay)
    return; // already created

  // Semi-transparent dark overlay covering the whole screen
  ui_PwrOffOverlay = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(ui_PwrOffOverlay);
  lv_obj_set_size(ui_PwrOffOverlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  lv_obj_set_style_bg_color(ui_PwrOffOverlay, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ui_PwrOffOverlay, LV_OPA_70, LV_PART_MAIN);
  lv_obj_clear_flag(ui_PwrOffOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_center(ui_PwrOffOverlay);

  // Arc (progress ring) — 150x150, range 0..360
  ui_PwrOffArc = lv_arc_create(ui_PwrOffOverlay);
  lv_obj_set_size(ui_PwrOffArc, 150, 150);
  lv_obj_center(ui_PwrOffArc);
  lv_arc_set_rotation(ui_PwrOffArc, 270); // start from top
  lv_arc_set_range(ui_PwrOffArc, 0, 360);
  lv_arc_set_value(ui_PwrOffArc, 0);
  lv_arc_set_bg_angles(ui_PwrOffArc, 0, 360);            // full background ring
  lv_obj_remove_style(ui_PwrOffArc, NULL, LV_PART_KNOB); // hide knob
  lv_obj_clear_flag(ui_PwrOffArc, LV_OBJ_FLAG_CLICKABLE);

  // Arc style: white indicator on dark grey background
  lv_obj_set_style_arc_color(ui_PwrOffArc, lv_color_hex(0x333333),
                             LV_PART_MAIN); // bg ring
  lv_obj_set_style_arc_width(ui_PwrOffArc, 8, LV_PART_MAIN);
  lv_obj_set_style_arc_color(ui_PwrOffArc, lv_color_hex(0xFF3333),
                             LV_PART_INDICATOR); // progress ring (red)
  lv_obj_set_style_arc_width(ui_PwrOffArc, 8, LV_PART_INDICATOR);

  // Power symbol label centered inside the arc
  ui_PwrOffSymbol = lv_label_create(ui_PwrOffOverlay);
  lv_label_set_text(ui_PwrOffSymbol, LV_SYMBOL_POWER);
  lv_obj_set_style_text_color(ui_PwrOffSymbol, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_font(ui_PwrOffSymbol, &lv_font_montserrat_48,
                             LV_PART_MAIN);
  lv_obj_center(ui_PwrOffSymbol);

  lv_obj_add_flag(ui_PwrOffOverlay, LV_OBJ_FLAG_HIDDEN); // hidden by default
}

static void pwroff_show(void) {
  if (!ui_PwrOffOverlay)
    pwroff_create_popup();
  lv_arc_set_value(ui_PwrOffArc, 0);
  lv_obj_clear_flag(ui_PwrOffOverlay, LV_OBJ_FLAG_HIDDEN);
  pwrOffPopupVisible = true;
}

static void pwroff_hide(void) {
  if (ui_PwrOffOverlay) {
    lv_obj_add_flag(ui_PwrOffOverlay, LV_OBJ_FLAG_HIDDEN);
  }
  pwrOffPopupVisible = false;
}

static void pwroff_update(void) {
  if (g_pwrOffActive) {
    if (!pwrOffPopupVisible) {
      pwroff_show();
    }
    // Calculate arc angle: 0 at start → 360 when remaining reaches 0
    int elapsed = PWR_OFF_TOTAL_MS - g_pwrOffRemainingMs;
    if (elapsed < 0)
      elapsed = 0;
    if (elapsed > PWR_OFF_TOTAL_MS)
      elapsed = PWR_OFF_TOTAL_MS;
    int angle = (elapsed * 360) / PWR_OFF_TOTAL_MS;
    lv_arc_set_value(ui_PwrOffArc, angle);

    // Shutdown: remaining reached 0
    if (g_pwrOffRemainingMs <= 0) {
      lv_arc_set_value(ui_PwrOffArc, 360);
      lv_timer_handler(); // flush last frame

      // Turn off backlight via I2C
      Wire.beginTransmission(I2C_ADDR_BACKLIGHT);
      Wire.write(DISPLAY_BL_OFF_VALUE);
      Wire.endTransmission();

      // Halt — device power will be cut by motherboard
      while (true) {
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    }
  } else {
    if (pwrOffPopupVisible) {
      pwroff_hide();
    }
  }
}

// ==========================================
// Main Task
// ==========================================
void UI_Task(void *pvParameters) {
  ESP_LOGI(TAG, "UI Task Started");

  // CrowPanel STC8H1K28 init + Backlight (I2C 0x30)
  // On crash recovery the STC8 keeps its audio/buzzer state (no reset needed).
  // The brightness command is retried up to 5 times: a single silently-failing
  // I2C write (NACK due to STC8 settle time after ESP32 soft-reset) would leave
  // the screen black for the entire session with no way to recover.
  {
    if (!g_hmiRestoreState) {
      vTaskDelay(pdMS_TO_TICKS(100));
      uint8_t init_cmds[] = {I2C_CMD_BUZZER_OFF, I2C_CMD_SPEAKER_ON};
      for (uint8_t cmd : init_cmds) {
        Wire.beginTransmission(I2C_ADDR_BACKLIGHT);
        Wire.write(cmd);
        Wire.endTransmission();
        vTaskDelay(pdMS_TO_TICKS(20));
      }
    } else {
      // Brief settle time on crash recovery: the ESP32 I2C peripheral just
      // restarted and the STC8 may need a moment before it ACKs.
      vTaskDelay(pdMS_TO_TICKS(50));
    }

    // Según doc v1.3: 0 es Brillo Máximo, 245 es Apagado.
    bool bl_ok = false;
    for (int attempt = 0; attempt < 5 && !bl_ok; attempt++) {
      Wire.beginTransmission(I2C_ADDR_BACKLIGHT);
      Wire.write(DISPLAY_BL_ON_VALUE);
      uint8_t err = Wire.endTransmission();
      if (err == 0) {
        bl_ok = true;
        if (attempt > 0)
          ESP_LOGI(TAG, "Backlight ON: OK after %d retr%s", attempt,
                   attempt == 1 ? "y" : "ies");
      } else {
        ESP_LOGE(TAG, "Backlight ON: I2C err=%d (attempt %d/5)", err, attempt + 1);
        vTaskDelay(pdMS_TO_TICKS(50));
      }
    }
    if (!bl_ok)
      ESP_LOGE(TAG, "Backlight ON: FAILED after all retries — screen will be black");

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  // Display initialization — leer freq_write de Preferences antes de crear panel
  {
    uint32_t savedFreq = 0;
    { Preferences p; p.begin(HMI_NS_CFG, true);
      savedFreq = p.getUInt(HMI_KEY_DISP_FREQ, 0);
      p.end(); }
    if (savedFreq >= DISPLAY_FREQ_MIN && savedFreq <= DISPLAY_FREQ_MAX) {
      g_currentFreqWrite = savedFreq;
      ESP_LOGW("LCD", "freq_write from Preferences: %lu Hz", savedFreq);
    } else {
      g_currentFreqWrite = DISPLAY_FREQ_WRITE;
      ESP_LOGI("LCD", "freq_write default: %lu Hz",
               (uint32_t)DISPLAY_FREQ_WRITE);
    }
  }

  // Crear panel RGB con bounce buffers (anti-flicker)
  {
    esp_lcd_rgb_panel_config_t panel_cfg = {};

    panel_cfg.clk_src = LCD_CLK_SRC_DEFAULT;
    panel_cfg.timings.pclk_hz = g_currentFreqWrite;
    panel_cfg.timings.h_res = DISPLAY_WIDTH;
    panel_cfg.timings.v_res = DISPLAY_HEIGHT;

    panel_cfg.timings.hsync_pulse_width = DISPLAY_HSYNC_PULSE_WIDTH;
    panel_cfg.timings.hsync_back_porch = DISPLAY_HSYNC_BACK_PORCH;
    panel_cfg.timings.hsync_front_porch = DISPLAY_HSYNC_FRONT_PORCH;
    panel_cfg.timings.vsync_pulse_width = DISPLAY_VSYNC_PULSE_WIDTH;
    panel_cfg.timings.vsync_back_porch = DISPLAY_VSYNC_BACK_PORCH;
    panel_cfg.timings.vsync_front_porch = DISPLAY_VSYNC_FRONT_PORCH;

    panel_cfg.timings.flags.hsync_idle_low = !DISPLAY_HSYNC_POLARITY;
    panel_cfg.timings.flags.vsync_idle_low = !DISPLAY_VSYNC_POLARITY;
    panel_cfg.timings.flags.pclk_idle_high = DISPLAY_PCLK_IDLE_HIGH;
    panel_cfg.timings.flags.pclk_active_neg = DISPLAY_PCLK_ACTIVE_NEG;
    panel_cfg.timings.flags.de_idle_high = DISPLAY_DE_IDLE_HIGH;

    panel_cfg.data_width = 16; // RGB565
    panel_cfg.num_fbs = 1;
    panel_cfg.bounce_buffer_size_px = BOUNCE_BUF_SIZE_PX;
    panel_cfg.psram_trans_align = 64;
    panel_cfg.flags.fb_in_psram = 1;         // Framebuffer en PSRAM
    panel_cfg.flags.bb_invalidate_cache = 0; // DMA usa bounce buffer completo

    // Pines de datos RGB565: B[4:0], G[5:0], R[4:0]
    panel_cfg.data_gpio_nums[0] = DISPLAY_PIN_B0;
    panel_cfg.data_gpio_nums[1] = DISPLAY_PIN_B1;
    panel_cfg.data_gpio_nums[2] = DISPLAY_PIN_B2;
    panel_cfg.data_gpio_nums[3] = DISPLAY_PIN_B3;
    panel_cfg.data_gpio_nums[4] = DISPLAY_PIN_B4;
    panel_cfg.data_gpio_nums[5] = DISPLAY_PIN_G0;
    panel_cfg.data_gpio_nums[6] = DISPLAY_PIN_G1;
    panel_cfg.data_gpio_nums[7] = DISPLAY_PIN_G2;
    panel_cfg.data_gpio_nums[8] = DISPLAY_PIN_G3;
    panel_cfg.data_gpio_nums[9] = DISPLAY_PIN_G4;
    panel_cfg.data_gpio_nums[10] = DISPLAY_PIN_G5;
    panel_cfg.data_gpio_nums[11] = DISPLAY_PIN_R0;
    panel_cfg.data_gpio_nums[12] = DISPLAY_PIN_R1;
    panel_cfg.data_gpio_nums[13] = DISPLAY_PIN_R2;
    panel_cfg.data_gpio_nums[14] = DISPLAY_PIN_R3;
    panel_cfg.data_gpio_nums[15] = DISPLAY_PIN_R4;

    panel_cfg.hsync_gpio_num = DISPLAY_PIN_HSYNC;
    panel_cfg.vsync_gpio_num = DISPLAY_PIN_VSYNC;
    panel_cfg.de_gpio_num = DISPLAY_PIN_DE;
    panel_cfg.pclk_gpio_num = DISPLAY_PIN_PCLK;
    panel_cfg.disp_gpio_num = -1; // No separate enable pin

    ESP_LOGI("LCD", "Creating RGB panel: %lux%lu @ %lu Hz, bounce=%d px",
             (uint32_t)DISPLAY_WIDTH, (uint32_t)DISPLAY_HEIGHT,
             g_currentFreqWrite, BOUNCE_BUF_SIZE_PX);

    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_cfg, &lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_panel));

    esp_lcd_rgb_panel_event_callbacks_t lcd_cbs = {
        .on_vsync = lcd_on_vsync,
        .on_bounce_empty = lcd_on_bounce_empty,
        .on_bounce_frame_finish = lcd_on_bounce_frame_finish,
    };
    ESP_ERROR_CHECK(
        esp_lcd_rgb_panel_register_event_callbacks(lcd_panel, &lcd_cbs, NULL));
    ESP_LOGI("LCD", "Glitch diagnostics callbacks registered");

#if DISPLAY_ROTATE_180
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(lcd_panel, true, true));
#else
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(lcd_panel, false, false));
#endif

    ESP_LOGW("LCD", "RGB panel initialized OK  [HEAP] internal=%u PSRAM=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  }

  lv_init();

  // Try to initialize Touch
  bool touch_ok = false;
  for (int i = 0; i < 3; i++) {
    // Note: PCA9557 at 0x18 was not found in scan.
    // Touch reset is likely handled by STC8 (0x30) or already high.
    if (ts.begin()) {
      touch_ok = true;
      ESP_LOGI(TAG, "Touch controller initialized OK");
      break;
    }
    ESP_LOGW(TAG, "Touch init failed, retrying...");
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  if (!touch_ok) {
    ESP_LOGE(TAG, "Touch controller FAILED to init after retries. Touch may "
                  "trigger ghost clicks or not work.");
  }

  // ts.reset();
  ts.setRotation(TOUCH_ROTATION);
  // Rotation already handled by esp_lcd_panel_mirror() above

  screenWidth = DISPLAY_WIDTH;
  screenHeight = DISPLAY_HEIGHT;

  lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL,
                        DISPLAY_WIDTH * DISPLAY_HEIGHT / COLOR_DIVISOR);

  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  indev_drv.long_press_time = LOCK_PROGRESS_DURATION_MS;
  // Chasquido de confirmacion en cualquier control, teclado excluido.
  indev_drv.feedback_cb = ui_click_feedback_cb;
  lv_indev_drv_register(&indev_drv);

  ui_init();
  g_ui_initialized = true;
  Communication_UIReady(); // Sincronización robusta: avisar a la Board que ya
                           // podemos pintar alarmas

  /* Comentado para v1.3 (Control vía I2C)
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(TFT_BL_PIN, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, BRIGHTNESS_MAX);
  */

  UI_ApplyLanguage(g_lang);
  ui_set_switch_state_silent(ui_SwitchDarkMode, darkMode);
  ui_set_switch_state_silent(ui_SwitchHumidityMode, humidityEnabled);
  if (humidityEnabled) {
    lv_obj_clear_flag(ui_HumCont, LV_OBJ_FLAG_HIDDEN);
  }
  // UI_ApplyTheme() movida al final de la creación de elementos manuales para
  // que les afecte

  // Botón de PLAY de Audio (oculto — acceso de audio deshabilitado)
  ui_AudioPlayBtn = lv_btn_create(ui_ScreenSettings);
  lv_obj_set_size(ui_AudioPlayBtn, 120, 50);
  lv_obj_align(ui_AudioPlayBtn, LV_ALIGN_BOTTOM_RIGHT, -160, -20);
  ui_AudioPlayLabel = lv_label_create(ui_AudioPlayBtn);
  lv_label_set_text(ui_AudioPlayLabel, "Play Audio");
  lv_obj_center(ui_AudioPlayLabel);
  lv_obj_add_event_cb(ui_AudioPlayBtn, AudioTestBtn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(ui_AudioPlayBtn, LV_OBJ_FLAG_HIDDEN); // Oculto permanente
  lv_obj_clear_flag(ui_AudioPlayBtn, LV_OBJ_FLAG_CLICKABLE); // No interactuable

  // Botón de STOP de Audio (oculto — acceso de audio deshabilitado)
  ui_AudioStopBtn = lv_btn_create(ui_ScreenSettings);
  lv_obj_set_size(ui_AudioStopBtn, 120, 50);
  lv_obj_align(ui_AudioStopBtn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
  lv_obj_set_style_bg_color(ui_AudioStopBtn, lv_color_hex(0xFF0000),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_t *audioStopLabel = lv_label_create(ui_AudioStopBtn);
  lv_label_set_text(audioStopLabel, "Stop");
  lv_obj_center(audioStopLabel);
  lv_obj_add_event_cb(ui_AudioStopBtn, AudioStopBtn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(ui_AudioStopBtn, LV_OBJ_FLAG_HIDDEN); // Oculto permanente
  lv_obj_clear_flag(ui_AudioStopBtn, LV_OBJ_FLAG_CLICKABLE); // No interactuable

  // --- Fila de Volumen (oculta — acceso de audio deshabilitado) ---
  // Botón Vol-
  ui_VolumeDownBtn = lv_btn_create(ui_ScreenSettings);
  lv_obj_set_size(ui_VolumeDownBtn, 50, 40);
  lv_obj_align(ui_VolumeDownBtn, LV_ALIGN_BOTTOM_RIGHT, -260, -80);
  lv_obj_t *volDownLabel = lv_label_create(ui_VolumeDownBtn);
  lv_label_set_text(volDownLabel, LV_SYMBOL_MINUS);
  lv_obj_center(volDownLabel);
  lv_obj_add_event_cb(ui_VolumeDownBtn, VolumeDown_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(ui_VolumeDownBtn, LV_OBJ_FLAG_HIDDEN); // Oculto permanente
  lv_obj_clear_flag(ui_VolumeDownBtn, LV_OBJ_FLAG_CLICKABLE);

  // Label de volumen central
  ui_VolumeLabel = lv_label_create(ui_ScreenSettings);
  {
    lv_label_set_text(ui_VolumeLabel, "Vol: --");
  }
  lv_obj_set_style_text_font(ui_VolumeLabel, &lv_font_montserrat_16, 0);
  lv_obj_align(ui_VolumeLabel, LV_ALIGN_BOTTOM_RIGHT, -170, -90);
  lv_obj_add_flag(ui_VolumeLabel, LV_OBJ_FLAG_HIDDEN); // Oculto permanente

  // Botón Vol+
  ui_VolumeUpBtn = lv_btn_create(ui_ScreenSettings);
  lv_obj_set_size(ui_VolumeUpBtn, 50, 40);
  lv_obj_align(ui_VolumeUpBtn, LV_ALIGN_BOTTOM_RIGHT, -30, -80);
  lv_obj_t *volUpLabel = lv_label_create(ui_VolumeUpBtn);
  lv_label_set_text(volUpLabel, LV_SYMBOL_PLUS);
  lv_obj_center(volUpLabel);
  lv_obj_add_event_cb(ui_VolumeUpBtn, VolumeUp_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(ui_VolumeUpBtn, LV_OBJ_FLAG_HIDDEN); // Oculto permanente
  lv_obj_clear_flag(ui_VolumeUpBtn, LV_OBJ_FLAG_CLICKABLE);

  // Aplicar tema DESPUÉS de crear todos los elementos manuales (Play, Stop,
  // Volumen, etc.)
  UI_ApplyTheme();

  // --- Skin probe toast (RF-SKIN-010, UI-SKIN-004): mensaje de bloqueo al
  // activar modo piel sin sonda ---
  // ui_ScreenMain, no lv_scr_act(): ui_init() ya cargo ui_ScreenIntro, asi que
  // la pantalla activa aqui es el splash y el toast nunca llegaba a verse.
  // En lv_layer_top() y no colgando de ui_ScreenMain.
  //
  // Colgado de la pantalla principal el aviso no se veia en ninguna otra, y
  // sobre todo quedaba DEBAJO del centro de alarmas y del asistente, que son
  // overlays de la capa superior. Justo los sitios desde donde mas se avisa:
  // el aviso de la prueba de alarmas salia detras de la propia tarjeta.
  ui_SkinProbeToast = lv_label_create(lv_layer_top());
  lv_label_set_text(ui_SkinProbeToast, "");
  lv_label_set_long_mode(ui_SkinProbeToast, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(ui_SkinProbeToast, 460);
  lv_obj_align(ui_SkinProbeToast, LV_ALIGN_BOTTOM_MID, 0, -24);
  lv_obj_set_style_text_align(ui_SkinProbeToast, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(ui_SkinProbeToast, &lv_font_montserrat_20,
                             LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_SkinProbeToast, lv_color_hex(0xFF8C00),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ui_SkinProbeToast, LV_OPA_90, LV_PART_MAIN);
  lv_obj_set_style_text_color(ui_SkinProbeToast, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN);
  lv_obj_set_style_pad_all(ui_SkinProbeToast, 16, LV_PART_MAIN);
  lv_obj_set_style_radius(ui_SkinProbeToast, 8, LV_PART_MAIN);
  lv_obj_add_flag(ui_SkinProbeToast, LV_OBJ_FLAG_HIDDEN);


  // --- Phototherapy safety confirmation popup (ISO 7010 M025) ---
  create_photo_safety_popup();
  // --- Replace slider switches with single toggle buttons ---
  create_main_toggle_buttons();
  // --- Baby-data activation wizard (temp-control-activation-wizard spec) ---
  // Parent to ui_ScreenMain explicitly, never lv_scr_act(): ui_init() above
  // already loaded ui_ScreenIntro, so the active screen here is the splash —
  // an overlay parented to it is discarded the moment ui_ScreenMain loads.
  BabyWizard_Init(ui_ScreenMain);
  // --- Babies history screen (baby-history-viewer spec) ---
  BabyHistory_Init(ui_ScreenMain);
  BabyExitDialog_Init(ui_ScreenMain);
  // Ajuste manual de la hora, abierto desde ui_ClockButton.
  TimeDialog_Init(ui_ScreenMain);
  // Menu de ayuda, abierto desde ui_HelpButton (mismo criterio de parent).
  HelpDialog_Init(ui_ScreenMain);
  // Tutorial guiado: en lv_layer_top() porque recorre Main y Ajustes.
  HelpTour_Init();
  // --- Centro de alarmas (activas + registro) ---
  // Sin parent: cuelga de lv_layer_top() para abrirse desde cualquier pantalla,
  // el bloqueo incluido.
  AlarmCenter_Init();
  // --- Tendencia de telemetria (aire/piel/humedad), accesible desde el
  // bloqueo (ui_ChartLockImg). Mismo criterio de parent que AlarmCenter. ---
  TelemetryHistory_Init();
  // El check de "todo OK" es lo unico visible en el sitio de las alarmas
  // cuando no hay ninguna; hacerlo pulsable es lo que deja el registro
  // accesible en estado normal. Las imagenes de LVGL no son pulsables por
  // defecto, de ahi el flag explicito.
  //
  // Solo el de ui_ScreenMain. El gemelo de la pantalla de bloqueo (ui_CheckImg)
  // se deja quieto a proposito: alli el toque en cualquier punto desbloquea, y
  // hacerlo pulsable crearia una zona muerta permanente que no desbloquea. El
  // banner ya se toma esa licencia, pero solo mientras suena una alarma;
  // quedarsela siempre es otra cosa.
  if (ui_CheckImgMain) {
    lv_obj_add_flag(ui_CheckImgMain, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_CheckImgMain, CheckImg_cb, LV_EVENT_CLICKED, NULL);
  }
  // En lv_layer_top(), no colgado de una pantalla: la senal de alarma
  // tiene que sobrevivir a lv_scr_load().
  alarm_banner_init();
  audio_paused_icon_init();

  if (g_hmiRestoreState) {
    // Skip 5-second splash and go straight to lock screen on crash recovery
    lv_scr_load(ui_ScreenMain);
    enter_lock_screen();
    intro_timer = NULL;
  } else {
    intro_start_ms = millis();
    intro_timer = lv_timer_create(intro_timer_cb, 200, NULL);
  }

  // Visuals
  lv_bar_set_range(ui_AirTempBar, 0, TEMP_BAR_RANGE);
  lv_bar_set_range(ui_SkinTempBar, 0, TEMP_BAR_RANGE);
  lv_bar_set_range(ui_HumBar, HUM_BAR_MIN, HUM_BAR_MAX);

  lv_obj_add_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_MuteAlarm, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ModesConfigCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_LanguagesDropDown, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Keyboard1, LV_OBJ_FLAG_HIDDEN);
  lv_keyboard_set_textarea(ui_Keyboard1, NULL);

  lv_textarea_set_text(ui_TextArea1, wifi_ssid);
  lv_textarea_set_text(ui_TextArea2, wifi_pass);

  lv_color_t init_panel_col = darkMode ? COLOR_PANEL_DARK : COLOR_PANEL_WHITE;
  lv_color_t init_inactive_col = darkMode ? COLOR_PANEL_DARK : COLOR_PANEL_GRAY;

  lv_obj_set_style_bg_color(ui_Panel2, init_panel_col, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_Panel2, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_Panel5, init_panel_col, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_Panel5, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_Panel6, init_panel_col, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_Panel6, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_Panel4, init_panel_col, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_Panel4, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_set_style_bg_color(ui_Panel1, init_inactive_col, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_Panel1, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_Panel3, init_inactive_col, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_Panel3, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_set_style_bg_color(ui_ArrowDownTemp, COLOR_PANEL_GRAY, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_ArrowDownTemp, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_ArrowUpTemp, COLOR_PANEL_GRAY, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_ArrowUpTemp, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_ArrowDownHum, COLOR_PANEL_GRAY, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_ArrowDownHum, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_ArrowUpHum, COLOR_PANEL_GRAY, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_ArrowUpHum, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_set_style_bg_color(ui_AirPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_AirPanel, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui_SkinPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_SkinPanel, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_add_flag(ui_Alarm1Cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Alarm2Cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Alarm3Cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Alarm4Cont, LV_OBJ_FLAG_HIDDEN);

  lv_obj_add_flag(ui_Alarm1Cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Alarm2Cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Alarm3Cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Alarm4Cont, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_add_flag(ui_Alarm1Label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Alarm1Panel, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_add_flag(ui_Alarm2Label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Alarm2Panel, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_add_flag(ui_Alarm3Label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Alarm3Panel, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_add_flag(ui_Alarm4Panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Alarm4Label, LV_OBJ_FLAG_CLICKABLE);

  // lv_obj_add_flag(ui_ChartButton, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_SPO2Button, LV_OBJ_FLAG_HIDDEN);

  lv_obj_set_width(ui_AlarmDetailLabel, lv_pct(100));
  lv_label_set_long_mode(ui_AlarmDetailLabel, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(ui_AlarmDetailLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(ui_AlarmDetailLabel, LV_ALIGN_TOP_MID, 0, 20);

  lv_label_set_text(ui_AlarmDetailLabel, "");

  lv_obj_add_flag(ui_NumAlarm, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Panel10, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_AlarmButton, LV_OBJ_FLAG_HIDDEN);

  lv_obj_add_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_HumLockDesiredCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ArrowAirLock, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ArrowSkinLock, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ArrowHumLock, LV_OBJ_FLAG_HIDDEN);

  lv_obj_add_flag(ui_UnlockCont, LV_OBJ_FLAG_HIDDEN);

  add_unlock_press_cb_recursive(ui_UnlockCont);

  lv_obj_add_flag(ui_AlarmLockCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_CheckImg, LV_OBJ_FLAG_HIDDEN);

  airTempSeries = configure_temp_chart(ui_AirTempChart, LV_PALETTE_BLUE,
                                       TEMP_CHART_MIN, TEMP_CHART_MAX);
  skinTempSeries = configure_temp_chart(ui_SkinTempChart, LV_PALETTE_BLUE,
                                        TEMP_CHART_MIN, TEMP_CHART_MAX);

  lv_obj_add_flag(ui_AirTempChartCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_SkinTempChartCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_HumChartCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_TabView1, LV_OBJ_FLAG_HIDDEN);

  // Clean Hum chart series
  lv_chart_series_t *s = lv_chart_get_series_next(ui_HumChart, NULL);
  while (s != NULL) {
    lv_chart_series_t *next = lv_chart_get_series_next(ui_HumChart, s);
    s->x_points = NULL;
    s->x_ext_buf_assigned = 0;
    lv_chart_remove_series(ui_HumChart, s);
    s = next;
  }
  lv_chart_set_type(ui_HumChart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(ui_HumChart, 50);
  lv_chart_set_range(ui_HumChart, LV_CHART_AXIS_PRIMARY_Y, HUM_CHART_MIN,
                     HUM_CHART_MAX);
  humSeries = lv_chart_add_series(
      ui_HumChart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_axis_tick(ui_HumChart, LV_CHART_AXIS_SECONDARY_Y, 0, 0, 0, 0,
                         false, 0);
  lv_chart_set_axis_tick(ui_HumChart, LV_CHART_AXIS_PRIMARY_X, 0, 0, 0, 0,
                         false, 0);
  for (int i = 0; i < lv_chart_get_point_count(ui_HumChart); i++) {
    humSeries->y_points[i] = LV_CHART_POINT_NONE;
  }
  lv_chart_refresh(ui_HumChart);

  lv_timer_handler();

  update_labels();

  lv_obj_add_flag(ui_Label9, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Label15, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_add_flag(ui_Label13, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Label16, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_add_flag(ui_Label10, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui_Label17, LV_OBJ_FLAG_CLICKABLE);

  setup_panel_callbacks();
  setup_arrow_callbacks();
  setup_arrow_hum_callbacks();

  // Assign callbacks for Heater Error interaction
  if (ui_HeaterErrorTempCont)
    lv_obj_add_event_cb(ui_HeaterErrorTempCont, HeaterError_event_handler,
                        LV_EVENT_CLICKED, NULL);
  if (ui_HeaterErrorHumCont)
    lv_obj_add_event_cb(ui_HeaterErrorHumCont, HeaterError_event_handler,
                        LV_EVENT_CLICKED, NULL);
  if (ui_HeaterErrorTempLabel)
    lv_obj_add_event_cb(ui_HeaterErrorTempLabel, HeaterError_event_handler,
                        LV_EVENT_CLICKED, NULL);
  if (ui_HeaterErrorHumLabel)
    lv_obj_add_event_cb(ui_HeaterErrorHumLabel, HeaterError_event_handler,
                        LV_EVENT_CLICKED, NULL);

  // 200 ms instead of 1 s: same auto-lock behaviour, but the countdown ring
  // around the padlock advances in 1 % steps instead of 5 % jumps.
  lv_timer_create(inactivity_timer_cb, 200, NULL);
  pwroff_create_popup();

  for (;;) {
    LVGL_Lock();
    lv_timer_handler();
    pwroff_update();

    // Gestión de estado de botones de Audio — deshabilitada (UI oculta
    // permanentemente) Si se quiere reactivar, descomentar el bloque siguiente
    // y quitar LV_OBJ_FLAG_HIDDEN de los widgets:
    /*
    if(ui_AudioPlayBtn) {
        if(AudioManager::getInstance().isPlaying()) {
            if(!lv_obj_has_state(ui_AudioPlayBtn, LV_STATE_DISABLED)) {
                lv_obj_add_state(ui_AudioPlayBtn, LV_STATE_DISABLED);
            }
            if(ui_AudioStopBtn && lv_obj_has_flag(ui_AudioStopBtn,
    LV_OBJ_FLAG_HIDDEN)) { lv_obj_clear_flag(ui_AudioStopBtn,
    LV_OBJ_FLAG_HIDDEN);
            }
        } else {
            if(lv_obj_has_state(ui_AudioPlayBtn, LV_STATE_DISABLED)) {
                lv_obj_clear_state(ui_AudioPlayBtn, LV_STATE_DISABLED);
            }
            if(ui_AudioStopBtn && !lv_obj_has_flag(ui_AudioStopBtn,
    LV_OBJ_FLAG_HIDDEN)) { lv_obj_add_flag(ui_AudioStopBtn, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    */

    // AudioManager::getInstance().loop(); // Ahora corre en su propia tarea
    // (Core 0)

    // Debug Pulse
    // static uint32_t lastLoopMs = 0;
    static uint32_t lastSyncMs = 0;

    if (millis() - lastSyncMs > 1000) {
      lastSyncMs = millis();
      UI_SyncAll();
    }

    /*
    if (millis() - lastLoopMs > 5000) {
      lastLoopMs = millis();
      ESP_LOGD(TAG, "UI and Audio Loop active");
    }
    */
    LVGL_Unlock();

    // LCD diagnostics: log cada 10 segundos
    {
      static uint32_t lcd_diag_last_ms = 0;
      uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
      if (now_ms - lcd_diag_last_ms >= 3000) { // 3s: bench-debug, más resolución temporal
        lcd_diag_last_ms = now_ms;
        lcd_diagnostics_log();
      }
    }

    // Heap/stack high-water mark — every 60 s
    {
      static uint32_t s_diag_last_ms = 0;
      uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
      if (now_ms - s_diag_last_ms >= 60000) {
        s_diag_last_ms = now_ms;
        uint32_t heap_int     = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        uint32_t heap_int_min = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        uint32_t heap_psram   = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        UBaseType_t ui_hwm   = uxTaskGetStackHighWaterMark(NULL);
        TaskHandle_t comm_h  = CommTask_GetHandle();
        UBaseType_t comm_hwm = comm_h ? uxTaskGetStackHighWaterMark(comm_h) : 0;
        uint32_t ui_hwm_b   = (uint32_t)ui_hwm   * sizeof(StackType_t);
        uint32_t comm_hwm_b = (uint32_t)comm_hwm * sizeof(StackType_t);
        if (heap_int_min < 20480 || ui_hwm_b < 2048 || comm_hwm_b < 2048) {
          ESP_LOGE("DIAG", "LOW RESOURCES heap_int=%lu heap_int_min=%lu heap_psram=%lu ui_hwm=%lu B comm_hwm=%lu B",
                   (unsigned long)heap_int, (unsigned long)heap_int_min,
                   (unsigned long)heap_psram,
                   (unsigned long)ui_hwm_b, (unsigned long)comm_hwm_b);
        } else {
          ESP_LOGW("DIAG", "heap_int=%lu heap_int_min=%lu heap_psram=%lu ui_hwm=%lu B comm_hwm=%lu B",
                   (unsigned long)heap_int, (unsigned long)heap_int_min,
                   (unsigned long)heap_psram,
                   (unsigned long)ui_hwm_b, (unsigned long)comm_hwm_b);
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
    bool doAlarmSound = false;
    bool doNVSWrite = false;
    LVGL_Lock();

    // --- Lock screen: probe contact state from CTRL,PROBE ---
    if (ctrl_probe_msg.updated) {
      ctrl_probe_msg.updated = false;
      bool applied = (ctrl_probe_msg.state == SPO2_PROBE_APPLIED);
      // La visibilidad se deriva del estado ACTUAL, no de un flanco: si el
      // HMI se perdiera una transicion, el siguiente CTRL,PROBE (la placa lo
      // repite cada 2 s mientras no hay contacto) recolocaria la pantalla.
      // add_flag/clear_flag son idempotentes.
      if (applied) {
        if (ui_LockPPGChart)
          lv_obj_clear_flag(ui_LockPPGChart, LV_OBJ_FLAG_HIDDEN);
      } else {
        if (ui_LockPPGChart) {
          bool wasVisible =
              !lv_obj_has_flag(ui_LockPPGChart, LV_OBJ_FLAG_HIDDEN);
          lv_obj_add_flag(ui_LockPPGChart, LV_OBJ_FLAG_HIDDEN);
          // Vaciar la traza al ocultarla, no al volver a mostrarla: asi el
          // chart queda en blanco se muestre por donde se muestre. En modo
          // CIRCULAR la traza vieja ya no se va desplazando sola, se quedaria
          // congelada en pantalla hasta que el cursor la barriera entera.
          // set_all_value ya resetea start_point a 0 y refresca; el repintado
          // completo es gratis aqui porque el objeto esta oculto. Solo en la
          // transicion a oculto: repetirlo cada 2 s seria trabajo inutil.
          if (wasVisible && lockPPGSeries)
            lv_chart_set_all_value(ui_LockPPGChart, lockPPGSeries,
                                   LV_CHART_POINT_NONE);
        }
        if (ui_LockHRCont)
          lv_obj_add_flag(ui_LockHRCont, LV_OBJ_FLAG_HIDDEN);
      }
      spo2ProbeAttached = applied;
    }

    // --- Lock screen: PPG waveform ---
    if (ui_LockPPGChart && lockPPGSeries && ctrl_ppg_msg.updated) {
      float ppg_val = (float)ctrl_ppg_msg.ppg;
      ctrl_ppg_msg.updated = false;
      if (locked && spo2ProbeAttached) {
        lv_chart_set_next_value(ui_LockPPGChart, lockPPGSeries,
                                (lv_coord_t)ppg_val);
        // Modo CIRCULAR: mantener un hueco por delante del cursor. Solo hace
        // falta borrar el punto que entra al hueco en esta muestra — los
        // LOCK_PPG_GAP-1 anteriores ya se borraron en iteraciones previas —,
        // asi que el coste es una invalidacion extra, no LOCK_PPG_GAP.
        // start_point directamente, no lv_chart_get_x_start_point(): ese
        // getter devuelve 0 en modo CIRCULAR (es "por que indice empieza a
        // dibujar el eje X"), no la posicion del cursor de escritura.
        uint16_t blank =
            (lockPPGSeries->start_point + LOCK_PPG_GAP - 1) % LOCK_PPG_POINTS;
        lv_chart_set_value_by_id(ui_LockPPGChart, lockPPGSeries, blank,
                                 LV_CHART_POINT_NONE);
      }
    }

    // --- Lock screen: HR value ---
    if (locked && ctrl_vit_msg.updated) {
      ctrl_vit_msg.updated = false;
      if (!spo2ProbeAttached) {
        if (ui_LockHRCont) lv_obj_add_flag(ui_LockHRCont, LV_OBJ_FLAG_HIDDEN);
      } else {
        if (ui_LockHRCont) {
          lv_obj_clear_flag(ui_LockHRCont, LV_OBJ_FLAG_HIDDEN);
          if (ui_LockHRLabel) {
            if (ctrl_vit_msg.hr > 0) {
              char hr_buf[8];
              snprintf(hr_buf, sizeof(hr_buf), "%u", ctrl_vit_msg.hr);
              lv_label_set_text(ui_LockHRLabel, hr_buf);
            } else {
              lv_label_set_text(ui_LockHRLabel, "--");
            }
          }
        }
      }
    }

    if (pendingReconnect && (millis() - disconnectTimestampMs >= 5000)) {
      pendingReconnect = false;
      wifiInit();
    }

    if (wifiVisible) {
      bool actuallyConnected = (WiFi.status() == WL_CONNECTED);
      if (actuallyConnected != isConnected) {
        isConnected = actuallyConnected;
        updateButtonVisibility();
        if (isConnected) {
          lv_obj_clear_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);
          lv_label_set_text(ui_WifiSSIDLabel, WiFi.SSID().c_str());
        } else {
          lv_obj_add_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);
        }
      }
    }

    if (photoTimerActive) {
      unsigned long elapsed = millis() - photoTimerStartMs;
      long totalSeconds = photoTimerMinutes * SECONDS_PER_MINUTE;
      long remaining = totalSeconds - (elapsed / 1000);

      if (remaining <= 0) {
        // Timer finished
        photoTimerActive = false;
        hmi_msg.phototherapyMode = PHOTOTHERAPY_OFF;
        hmi_msg.shouldSendData = true;

        // Turn switch OFF visually and trigger callback
        lv_obj_clear_state(ui_Switch3, LV_STATE_CHECKED);
        lv_event_send(ui_Switch3, LV_EVENT_VALUE_CHANGED, NULL);

        if (ui_PhotoCancelBtn)
          lv_obj_add_flag(ui_PhotoCancelBtn, LV_OBJ_FLAG_HIDDEN);
      } else {
        // Update countdown display
        int totalMins =
            (remaining + (SECONDS_PER_MINUTE - 1)) /
            SECONDS_PER_MINUTE; // Round up to show minutes correctly
        int hours = totalMins / SECONDS_PER_MINUTE;
        int mins = totalMins % SECONDS_PER_MINUTE;
        char buf[16];
        snprintf(buf, sizeof(buf), "%d:%02d", hours, mins);

        // Update main screen
        lv_label_set_text(ui_PhotoTimeValueLabel, buf);

        // Update lock screen
        if (ui_PhotoLockTimeLabel) {
          lv_label_set_text(ui_PhotoLockTimeLabel, buf);
        }
        if (ui_PhotoLockCont) {
          lv_obj_clear_flag(ui_PhotoLockCont, LV_OBJ_FLAG_HIDDEN);
        }
      }
    } else if (hmi_msg.phototherapyMode == PHOTOTHERAPY_ON) {
      // Phototherapy is ON but no timer is active
      if (ui_PhotoLockTimeLabel) {
        // Use translation for "ON" if available, or just "ON"
        lv_label_set_text(ui_PhotoLockTimeLabel, "ON");
      }
      if (ui_PhotoLockCont) {
        lv_obj_clear_flag(ui_PhotoLockCont, LV_OBJ_FLAG_HIDDEN);
      }
    } else {
      lastPhotoMinutesSent = -1;
      // Timer not active and light is OFF, hide lock screen container
      if (ui_PhotoLockCont) {
        lv_obj_add_flag(ui_PhotoLockCont, LV_OBJ_FLAG_HIDDEN);
      }
    }

    // State sync: apply ctrl_state from CommTask (moved from CommTask)
    if (ctrl_state_msg.newState) {
      if (Display_ApplyCtrlState(ctrl_state_msg)) {
        ctrl_state_msg.newState = false;
        g_stateSynced = true;
        hmi_msg.shouldSendData = true;
        g_pendingAlarmUpdate = true; // Refresh alarm UI after state sync
      }
    }

    // Telemetry: update labels and charts (moved from CommTask)
    if (g_pendingTelemetryApply) {
      taskENTER_CRITICAL(&g_telemetry_mux);
      double snapAir  = airTempValueDetected;
      double snapSkin = skinTempValueDetected;
      int    snapHum  = humValueDetected;
      g_pendingTelemetryApply = false;
      taskEXIT_CRITICAL(&g_telemetry_mux);
      update_labels();
      if (tempSwitched) {
        chart_add_air_temp((float)snapAir);
        chart_add_skin_temp((float)snapSkin);
      }
      chart_add_hum_value((float)snapHum);
      // Mismo criterio de validez que update_labels() (airOk/skinOk/humOk
      // ahi arriba): un centinela PROTO_TEL_*_UNAVAILABLE o el enlace caido
      // nunca deben guardarse como si fueran una lectura real.
      {
        const bool linkLost = Display_IsBoardLinkLost();
        const bool airOkHist =
            !linkLost && !PROTO_TEL_TEMP_IS_UNAVAILABLE(snapAir);
        const bool skinOkHist =
            !linkLost && !PROTO_TEL_TEMP_IS_UNAVAILABLE(snapSkin);
        const bool humOkHist =
            !linkLost && snapHum != PROTO_TEL_HUM_UNAVAILABLE;
        TelemetryHistory_RecordSample((float)snapAir, airOkHist,
                                       (float)snapSkin, skinOkHist,
                                       (float)snapHum, humOkHist);
      }
    }

    if (g_pendingDutyApply) {
      g_pendingDutyApply = false;
      UI_UpdatePowerBars(g_tempDutyPwm, g_humDutyPwm);
    }

    if (g_pendingAlarmUpdate) {
      update_alarm_panels();
      g_pendingAlarmUpdate = false;
      doAlarmSound = true;
    }

    // Baby-data wizard: consumes CommTask's pending PROFILE_* messages,
    // timeouts, and the critical-alarm-interrupts-wizard rule (Section 4).
    BabyWizard_Poll();
    // Babies history screen: same polling contract (list/history/chart
    // responses, timeouts, critical-alarm-closes-screen).
    BabyHistory_Poll();
    // Centro de alarmas: respuestas del registro y del detalle, con sus
    // timeouts. A diferencia del resto, este NO se cierra con una alarma
    // critica — es precisamente la pantalla donde se atiende.
    AlarmCenter_Poll();
    // Tendencia de telemetria: sin peticiones a la motherBoard, solo cierra
    // si hay una alarma critica (mismo contrato que BabyHistory_Poll).
    TelemetryHistory_Poll();

    // Ajuste manual de hora (se abre tocando la hora de cabecera,
    // ClockButton_cb): mismo contrato de polling que el wizard — consume el
    // CTRL,TIME_ACK y aplica la regla de "una alarma critica se lleva la
    // pantalla por delante".
    TimeDialog_Poll();
    // Ayuda: resultado del envio a soporte (lo deja la tarea WiFi/OTA) y la
    // misma regla de alarma critica, para el menu y para el tutorial.
    HelpDialog_Poll();
    HelpTour_Poll();

    // El banner se reevalua en CADA pasada, no solo cuando cambia el conjunto
    // de alarmas. Su visibilidad depende de la pantalla activa y el banner
    // cuelga de lv_layer_top(), que no se entera de los lv_scr_load(): si solo
    // se recalculara desde update_alarm_panels() —que corre con
    // g_pendingAlarmUpdate, o sea cuando llega un CTRL,ALM—, al salir del
    // bloqueo hacia cualquier otra pantalla el banner se quedaba pintado hasta
    // el siguiente cambio de alarma. Es idempotente y barato.
    alarm_banner_update();
    // Se reevalua en cada pasada por lo mismo, y ademas depende de
    // silencedBitmask, que llega con CTRL,STATE y caduca solo en la placa.
    audio_paused_icon_update();
    // Y estos dos por el motivo opuesto: dependen de que NO llegue nada.
    link_lost_blank_update();
    clock_update();
    wifi_board_status_update();
    connectivity_heading_update();
    link_audio_mute_button_update();
    // Apaga el chasquido de la ultima pulsacion cuando le toca. Va aqui, y no
    // con un delay() dentro del callback, para no congelar LVGL.
    click_beep_service();
    // Baby-exit dialog: only the transition to a fully idle incubator
    // (no temperature, no humidity, no phototherapy) means the baby
    // actually came out; dropping one therapy among several does not.
    BabyExitDialog_Tick(UI_AnyControlActive());

    if (eepromDirty && (millis() - lastVarChangeTime > EEPROM_COMMIT_DELAY)) {
      eepromDirty = false;
      doNVSWrite = true;
    }
    LVGL_Unlock();

    // Senal acustica del enlace perdido. Fuera de LVGL_Lock porque cada
    // transicion del patron es una transaccion I2C (ARQ-LOCK-001), y ANTES de
    // la escritura en NVS porque esa puede tardar ~30 ms y desplazaria los
    // bordes de los pulsos.
    link_audio_service();

    // NVS writes run outside LVGL_Lock — avoids blocking the renderer during
    // wear-leveling erasure (~30ms worst case). Same pattern as AlarmSound_Update.
    if (doNVSWrite) {
      uint32_t t0 = millis(); // LCD_DIAG: correlar con glitches de pantalla (ver lcd_diagnostics_log)
      Preferences p;
      p.begin(HMI_NS_CFG, false);
      p.putFloat(HMI_KEY_AIR_TEMP,   (float)airTempValue);
      p.putFloat(HMI_KEY_SKIN_TEMP,  (float)skinTempValue);
      p.putUChar(HMI_KEY_HUMIDITY,   (uint8_t)humValue);
      p.putUChar(HMI_KEY_PHOTO_MIN,  (uint8_t)photoTimerMinutes);
      p.end();
      ESP_LOGI(TAG, "Preferences write cycle complete");
      ESP_LOGW(TAG, "LCD_DIAG: periodic Preferences write tomó %lu ms",
               (unsigned long)(millis() - t0));
    }

    // AlarmSound_Update uses I2C (Wire.endTransmission) — must run outside
    // LVGL_Lock to avoid blocking the LVGL mutex for ~50 ms (Fix: ARQ-LOCK-001)
    if (doAlarmSound) {
      AlarmSound_Update();
    }
  }
}

void CreateUITask() {
  xTaskCreatePinnedToCore(UI_Task, "UI", UI_TASK_STACK_SIZE, NULL,
                          UI_TASK_PRIORITY, NULL, CORE_ID_FREERTOS);
}

void UI_SyncAll() {
  if (!g_ui_initialized)
    return;

  lv_color_t active_col = darkMode ? COLOR_PANEL_GRAY : COLOR_PANEL_WHITE;
  lv_color_t inactive_col = darkMode ? COLOR_PANEL_DARK : COLOR_PANEL_GRAY;

  // 1. Update internal state flags from current switch visual state
  switchTemp = lv_obj_has_state(ui_Switch1, LV_STATE_CHECKED);
  tempSwitched = switchTemp;
  switchHum = lv_obj_has_state(ui_Switch2, LV_STATE_CHECKED);
  humSwitched = switchHum;
  skinPanelEnabled = lv_obj_has_state(ui_Switch4, LV_STATE_CHECKED);

  // The permanent skin-probe status label (RF-SKIN-004 / UI-SKIN-005) was
  // removed on request: it added standing clutter to the main screen. The
  // probe still blocks SKIN activation and reports why through UI_ShowToast.

  // 2. Temperature Logic
  if (tempSwitched) {
    if (selectedPanel == NO_PANEL_SELECTED) {
      selectedPanel = (lastSelectedPanel != NO_PANEL_SELECTED)
                          ? lastSelectedPanel
                          : AIR_PANEL_SELECTED;
    }

    if (selectedPanel == AIR_PANEL_SELECTED) {
      set_active_panel(ui_AirPanel, ui_SkinPanel);
      lv_obj_clear_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);

      // Target Screen Visibility
      if (ui_TargetAirTempCont)
        lv_obj_clear_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
      if (ui_ArrowAirLock)
        lv_obj_clear_flag(ui_ArrowAirLock, LV_OBJ_FLAG_HIDDEN);
      if (ui_TargetSkinTempCont)
        lv_obj_add_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
      if (ui_ArrowSkinLock)
        lv_obj_add_flag(ui_ArrowSkinLock, LV_OBJ_FLAG_HIDDEN);

      hmi_msg.controlMode = CONTROL_AIR;
    } else {
      set_active_panel(ui_SkinPanel, ui_AirPanel);
      lv_obj_clear_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);

      // Target Screen Visibility
      if (ui_TargetSkinTempCont)
        lv_obj_clear_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
      if (ui_ArrowSkinLock)
        lv_obj_clear_flag(ui_ArrowSkinLock, LV_OBJ_FLAG_HIDDEN);
      if (ui_TargetAirTempCont)
        lv_obj_add_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
      if (ui_ArrowAirLock)
        lv_obj_add_flag(ui_ArrowAirLock, LV_OBJ_FLAG_HIDDEN);

      hmi_msg.controlMode = CONTROL_SKIN;
    }

    // First Column (Detected Values) ALWAYS VISIBLE
    if (ui_AirTempLockCont)
      lv_obj_clear_flag(ui_AirTempLockCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_SkinTempLockCont)
      lv_obj_clear_flag(ui_SkinTempLockCont, LV_OBJ_FLAG_HIDDEN);
    if (humidityEnabled && ui_HumLockCont)
      lv_obj_clear_flag(ui_HumLockCont, LV_OBJ_FLAG_HIDDEN);

    // Enable arrows
    arrowsActive = true;
    lv_obj_add_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(ui_ArrowDownTemp, active_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_ArrowDownTemp, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowUpTemp, active_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_ArrowUpTemp, LV_OPA_COVER, LV_PART_MAIN);

    // Temperature Panel background
    lv_obj_set_style_bg_color(ui_Panel1, active_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_Panel1, LV_OPA_COVER, LV_PART_MAIN);

    temp_chart_show_for_selected_panel();
  } else {
    selectedPanel = NO_PANEL_SELECTED;
    arrowsActive = false;
    lv_obj_add_flag(ui_AirTempChartCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_SkinTempChartCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(ui_ArrowDownTemp, inactive_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_ArrowDownTemp, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowUpTemp, inactive_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_ArrowUpTemp, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_Panel1, inactive_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_Panel1, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_AirPanel, inactive_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_AirPanel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_SkinPanel, inactive_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_SkinPanel, LV_OPA_COVER, LV_PART_MAIN);

    // Visually show labels and thermometer of the last selected panel even if
    // OFF
    if (lastSelectedPanel == SKIN_PANEL_SELECTED) {
      lv_obj_clear_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);
    }

    // Lock Screen hide targets but keep detected values visible
    if (ui_TargetAirTempCont)
      lv_obj_add_flag(ui_TargetAirTempCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_TargetSkinTempCont)
      lv_obj_add_flag(ui_TargetSkinTempCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_ArrowAirLock)
      lv_obj_add_flag(ui_ArrowAirLock, LV_OBJ_FLAG_HIDDEN);
    if (ui_ArrowSkinLock)
      lv_obj_add_flag(ui_ArrowSkinLock, LV_OBJ_FLAG_HIDDEN);

    if (ui_AirTempLockCont)
      lv_obj_clear_flag(ui_AirTempLockCont, LV_OBJ_FLAG_HIDDEN);
    if (ui_SkinTempLockCont)
      lv_obj_clear_flag(ui_SkinTempLockCont, LV_OBJ_FLAG_HIDDEN);
    if (humidityEnabled && ui_HumLockCont)
      lv_obj_clear_flag(ui_HumLockCont, LV_OBJ_FLAG_HIDDEN);
  }

  // 3. Humidity Logic
  if (switchHum) {
    lv_obj_clear_flag(ui_HumPanelCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_Panel3, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_HumChartCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_HumLockDesiredCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ImgArrowDownHum, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_ImgArrowUpHum, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(ui_ArrowDownHum, active_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_ArrowDownHum, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowUpHum, active_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_ArrowUpHum, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_Panel3, active_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_Panel3, LV_OPA_COVER, LV_PART_MAIN);
  } else {
    lv_obj_add_flag(ui_HumPanelCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_Panel3, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_HumChartCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_HumLockDesiredCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ImgArrowDownHum, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_ImgArrowUpHum, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(ui_ArrowDownHum, inactive_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_ArrowDownHum, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowUpHum, inactive_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_ArrowUpHum, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_Panel3, inactive_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_Panel3, LV_OPA_COVER, LV_PART_MAIN);
  }

  // 4. Phototherapy Logic (Switch 3)
  bool photoOn = lv_obj_has_state(ui_Switch3, LV_STATE_CHECKED);

  if (photoOn) {
    // Normal state
    lv_obj_set_style_bg_color(ui_PhotoTimerPanel, active_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_PhotoTimerPanel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_flag(ui_PhotoTimeMinusBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_PhotoTimePlusBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_PhotoStartBtn, LV_OBJ_FLAG_CLICKABLE);

    // Restore button colors
    lv_obj_set_style_bg_color(ui_PhotoTimeMinusBtn, lv_color_hex(0x0075EE),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_PhotoTimePlusBtn, lv_color_hex(0x0075EE),
                              LV_PART_MAIN);

    if (photoTimerActive) {
      // Update visual running state
      const char *TXT_RUNNING[] = {"EJECUTANDO", "RUNNING", "EN COURS"};
      if (ui_PhotoStartLabel)
        lv_label_set_text(ui_PhotoStartLabel, TXT_RUNNING[g_lang]);
      if (ui_PhotoStartBtn)
        lv_obj_set_style_bg_color(ui_PhotoStartBtn, lv_color_hex(0x888888),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);

      // Ensure cancel button is visible
      if (ui_PhotoCancelBtn)
        lv_obj_clear_flag(ui_PhotoCancelBtn, LV_OBJ_FLAG_HIDDEN);

      // Ensure lock screen is visible if we are in lock screen
      if (ui_PhotoLockCont)
        lv_obj_clear_flag(ui_PhotoLockCont, LV_OBJ_FLAG_HIDDEN);
    } else {
      // Setup state
      const char *TXT_PHOTOSTART[] = {"EMPEZAR", "START", "DEMARRER"};
      if (ui_PhotoStartLabel)
        lv_label_set_text(ui_PhotoStartLabel, TXT_PHOTOSTART[g_lang]);
      if (ui_PhotoStartBtn)
        lv_obj_set_style_bg_color(ui_PhotoStartBtn, lv_color_hex(0x00AA00),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);

      // Ensure cancel button is hidden
      if (ui_PhotoCancelBtn)
        lv_obj_add_flag(ui_PhotoCancelBtn, LV_OBJ_FLAG_HIDDEN);

      if (ui_PhotoLockCont)
        lv_obj_add_flag(ui_PhotoLockCont, LV_OBJ_FLAG_HIDDEN);

      // Restore normal minutes display if not active
      char buf[16];
      snprintf(buf, sizeof(buf), "%d min", photoTimerMinutes);
      if (ui_PhotoTimeValueLabel)
        lv_label_set_text(ui_PhotoTimeValueLabel, buf);
    }
  } else {
    // Grayed out state
    lv_obj_set_style_bg_color(ui_PhotoTimerPanel, inactive_col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_PhotoTimerPanel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(ui_PhotoTimeMinusBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_PhotoTimePlusBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_PhotoStartBtn, LV_OBJ_FLAG_CLICKABLE);

    // Buttons to light gray as requested
    lv_obj_set_style_bg_color(ui_PhotoTimeMinusBtn, COLOR_PANEL_LIGHT_GRAY,
                              LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_PhotoTimePlusBtn, COLOR_PANEL_LIGHT_GRAY,
                              LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_PhotoStartBtn, COLOR_PANEL_LIGHT_GRAY,
                              LV_PART_MAIN);

    // Reset visual values
    if (!photoTimerActive) {
      char buf[16];
      snprintf(buf, sizeof(buf), "%d min", photoTimerMinutes);
      if (ui_PhotoTimeValueLabel)
        lv_label_set_text(ui_PhotoTimeValueLabel, buf);

      const char *TXT_PHOTOSTART[] = {"EMPEZAR", "START", "DEMARRER"};
      if (ui_PhotoStartLabel)
        lv_label_set_text(ui_PhotoStartLabel, TXT_PHOTOSTART[g_lang]);
    }

    if (ui_PhotoCancelBtn)
      lv_obj_add_flag(ui_PhotoCancelBtn, LV_OBJ_FLAG_HIDDEN);
    if (ui_PhotoLockCont)
      lv_obj_add_flag(ui_PhotoLockCont, LV_OBJ_FLAG_HIDDEN);
    photoTimerActive = false; // Safety
  }

  // Panel 2 is now handled above based on photoOn state
  // lv_obj_set_style_bg_color(ui_Panel2, active_col, LV_PART_MAIN);
  lv_obj_set_style_opa(ui_Panel2, LV_OPA_COVER, LV_PART_MAIN);

  // 5. Skin Block (Switch 4). Same condition as temp_content_set_visible():
  // the baby-temperature controller belongs to a running thermal control, so
  // the skin block alone must not put it on screen.
  if (skinPanelEnabled && tempSwitched) {
    lv_obj_clear_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);
  }

  // 6. TabView Management
  lv_obj_t *tab_btns_cont = lv_obj_get_child(ui_TabView1, 0);
  lv_obj_t *temp_tab_btn = NULL;
  lv_obj_t *hum_tab_btn = NULL;
  if (tab_btns_cont) {
    temp_tab_btn = lv_obj_get_child(tab_btns_cont, 0);
    hum_tab_btn = lv_obj_get_child(tab_btns_cont, 1);
  }

  if (!switchTemp && !switchHum) {
    lv_obj_add_flag(ui_TabView1, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(ui_TabView1, LV_OBJ_FLAG_HIDDEN);
    if (temp_tab_btn) {
      if (switchTemp)
        lv_obj_clear_flag(temp_tab_btn, LV_OBJ_FLAG_HIDDEN);
      else
        lv_obj_add_flag(temp_tab_btn, LV_OBJ_FLAG_HIDDEN);
    }
    if (hum_tab_btn) {
      if (switchHum)
        lv_obj_clear_flag(hum_tab_btn, LV_OBJ_FLAG_HIDDEN);
      else
        lv_obj_add_flag(hum_tab_btn, LV_OBJ_FLAG_HIDDEN);
    }
    if ((switchTemp && !switchHum) || (!switchTemp && switchHum)) {
      if (tab_btns_cont)
        lv_obj_add_flag(tab_btns_cont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(
          lv_tabview_get_content(ui_TabView1),
          LV_OBJ_FLAG_SCROLLABLE); // Bloquear swipe si solo hay uno
    } else {
      if (tab_btns_cont)
        lv_obj_clear_flag(tab_btns_cont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(lv_tabview_get_content(ui_TabView1),
                      LV_OBJ_FLAG_SCROLLABLE); // Permitir swipe si hay dos
    }

    if (switchTemp && !switchHum)
      lv_tabview_set_act(ui_TabView1, 0, LV_ANIM_OFF);
    else if (!switchTemp && switchHum)
      lv_tabview_set_act(ui_TabView1, 1, LV_ANIM_OFF);
  }

  hmi_msg.language = (int)g_lang;
  update_labels();
  update_main_toggle_buttons();
}

static void UI_ApplyStyleToLabelsRecursive(lv_obj_t *obj, lv_color_t color) {
  if (!obj)
    return;

  // Check if the object is a label.
  // Note: In some LVGL versions we use lv_obj_get_class(obj) == &lv_label_class
  // or lv_obj_check_type(obj, &lv_label_class).
  if (lv_obj_check_type(obj, &lv_label_class)) {
    lv_obj_set_style_text_color(obj, color, 0);
  }

  uint32_t i;
  uint32_t n = lv_obj_get_child_cnt(obj);
  for (i = 0; i < n; i++) {
    UI_ApplyStyleToLabelsRecursive(lv_obj_get_child(obj, i), color);
  }
}

void UI_ApplyTheme() {
  if (!g_ui_initialized)
    return;

  lv_color_t bg_col = darkMode
                          ? COLOR_BG_DARK
                          : lv_color_hex(0xF4F4F4); // Original light gray bg
  lv_color_t panel_col = darkMode ? COLOR_PANEL_DARK : COLOR_PANEL_WHITE;
  lv_color_t text_col = darkMode ? COLOR_TEXT_DARK : lv_color_hex(0x000000);

  // Apply to all screens background
  if (ui_ScreenMain)
    lv_obj_set_style_bg_color(ui_ScreenMain, bg_col, 0);
  if (ui_ScreenSettings)
    lv_obj_set_style_bg_color(ui_ScreenSettings, bg_col, 0);
  if (ui_ScreenAlarms)
    lv_obj_set_style_bg_color(ui_ScreenAlarms, bg_col, 0);
  if (ui_ScreenCharts)
    lv_obj_set_style_bg_color(ui_ScreenCharts, bg_col, 0);
  if (ui_ScreenPulseOxi)
    lv_obj_set_style_bg_color(ui_ScreenPulseOxi, bg_col, 0);

  // Main Screen Panels
  if (ui_Panel1)
    lv_obj_set_style_bg_color(ui_Panel1, panel_col, 0);
  if (ui_Panel2)
    lv_obj_set_style_bg_color(ui_Panel2, panel_col, 0);
  if (ui_Panel3)
    lv_obj_set_style_bg_color(ui_Panel3, panel_col, 0);
  if (ui_Panel4)
    lv_obj_set_style_bg_color(ui_Panel4, panel_col, 0);
  if (ui_Panel5)
    lv_obj_set_style_bg_color(ui_Panel5, panel_col, 0);
  if (ui_Panel6)
    lv_obj_set_style_bg_color(ui_Panel6, panel_col, 0);

  // Settings Panels
  if (ui_Panel7)
    lv_obj_set_style_bg_color(ui_Panel7, panel_col, 0);
  if (ui_Panel8)
    lv_obj_set_style_bg_color(ui_Panel8, panel_col, 0);
  if (ui_Panel9)
    lv_obj_set_style_bg_color(ui_Panel9, panel_col, 0);
  if (ui_PanelDarkMode)
    lv_obj_set_style_bg_color(ui_PanelDarkMode, panel_col, 0);
  if (ui_PanelHumidityMode)
    lv_obj_set_style_bg_color(ui_PanelHumidityMode, panel_col, 0);
  if (ui_InfoPanel)
    lv_obj_set_style_bg_color(ui_InfoPanel, panel_col, 0);

  // Apply text color to all labels in all screens
  if (ui_ScreenMain)
    UI_ApplyStyleToLabelsRecursive(ui_ScreenMain, text_col);
  if (ui_ScreenSettings)
    UI_ApplyStyleToLabelsRecursive(ui_ScreenSettings, text_col);
  if (ui_ScreenAlarms)
    UI_ApplyStyleToLabelsRecursive(ui_ScreenAlarms, text_col);
  if (ui_ScreenCharts)
    UI_ApplyStyleToLabelsRecursive(ui_ScreenCharts, text_col);
  if (ui_ScreenPulseOxi)
    UI_ApplyStyleToLabelsRecursive(ui_ScreenPulseOxi, text_col);
  // Lock Screen ALWAYS dark with white text
  if (ui_ScreenLock) {
    lv_obj_set_style_bg_color(ui_ScreenLock, lv_color_hex(0x1A1A1A), 0);
    UI_ApplyStyleToLabelsRecursive(ui_ScreenLock, lv_color_hex(0xFFFFFF));
  }

  // Settings Details (Info/Wifi) ALWAYS keep black text because their panels
  // are white
  if (ui_InfoDetailsCont)
    UI_ApplyStyleToLabelsRecursive(ui_InfoDetailsCont, lv_color_hex(0x000000));
  if (ui_WifiConfigCont)
    UI_ApplyStyleToLabelsRecursive(ui_WifiConfigCont, lv_color_hex(0x000000));
  if (ui_WifiConnectedCont)
    UI_ApplyStyleToLabelsRecursive(ui_WifiConnectedCont,
                                   lv_color_hex(0x000000));

  // Images recoloring for Dark Mode (Originals are black, we want white)
  lv_color_t img_recolor =
      darkMode ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x000000);
  lv_opa_t img_recolor_opa = darkMode ? LV_OPA_COVER : LV_OPA_TRANSP;

  if (ui_ChartButton) {
    lv_obj_set_style_img_recolor(ui_ChartButton, img_recolor, 0);
    lv_obj_set_style_img_recolor_opa(ui_ChartButton, img_recolor_opa, 0);
  }
  if (ui_ImgButton2) {
    lv_obj_set_style_img_recolor(ui_ImgButton2, img_recolor, 0);
    lv_obj_set_style_img_recolor_opa(ui_ImgButton2, img_recolor_opa, 0);
  }
  if (ui_ImgButton7) {
    lv_obj_set_style_img_recolor(ui_ImgButton7, img_recolor, 0);
    lv_obj_set_style_img_recolor_opa(ui_ImgButton7, img_recolor_opa, 0);
  }
  if (ui_ImgButton8) {
    lv_obj_set_style_img_recolor(ui_ImgButton8, img_recolor, 0);
    lv_obj_set_style_img_recolor_opa(ui_ImgButton8, img_recolor_opa, 0);
  }
  if (ui_ImgButton9) {
    lv_obj_set_style_img_recolor(ui_ImgButton9, img_recolor, 0);
    lv_obj_set_style_img_recolor_opa(ui_ImgButton9, img_recolor_opa, 0);
  }
  if (ui_Image4) {
    lv_obj_set_style_img_recolor(ui_Image4, img_recolor, 0);
    lv_obj_set_style_img_recolor_opa(ui_Image4, img_recolor_opa, 0);
  }

  // --- CHARTS DARK MODE APLICATION ---
  lv_color_t chart_bg_col =
      darkMode ? COLOR_PANEL_DARK : lv_color_hex(0xFFFFFF);
  lv_color_t chart_text_col =
      darkMode ? COLOR_TEXT_DARK : lv_color_hex(0x808080);
  lv_color_t chart_grid_col =
      darkMode ? lv_color_hex(0x222222) : lv_color_hex(0xEEEEEE);

  lv_obj_t *charts[] = {ui_AirTempChart, ui_SkinTempChart, ui_HumChart,
                        ui_OxChart};

  for (int i = 0; i < 4; i++) {
    if (charts[i] != NULL) {
      // Fondo del chart
      lv_obj_set_style_bg_color(charts[i], chart_bg_col, LV_PART_MAIN);

      // Color de la malla / cuadrícula
      lv_obj_set_style_line_color(charts[i], chart_grid_col, LV_PART_MAIN);

      // Color del texto de los ejes
      lv_obj_set_style_text_color(charts[i], chart_text_col, LV_PART_TICKS);
    }
  }

  // --- TABVIEWS DARK MODE APLICATION ---
  lv_color_t tab_bg_col = darkMode ? COLOR_BG_DARK : lv_color_hex(0xFFFFFF);
  lv_color_t tab_page_bg_col =
      darkMode ? COLOR_PANEL_DARK : lv_color_hex(0xFFFFFF);
  lv_color_t tab_btn_bg_col =
      darkMode ? COLOR_PANEL_DARK : lv_color_hex(0xFFFFFF);
  lv_color_t tab_text_col = darkMode ? COLOR_TEXT_DARK : lv_color_hex(0x000000);

  lv_obj_t *tabviews[] = {ui_AlarmsTabview, ui_TabViewMainCharts, ui_TabView1};

  for (int i = 0; i < 3; i++) {
    if (tabviews[i] != NULL) {
      // 1. Fondo Principal de la vista del Tab
      lv_obj_set_style_bg_color(tabviews[i], tab_bg_col, LV_PART_MAIN);

      // 2. Fondo de los Botones (Header)
      lv_obj_t *tab_btns = lv_tabview_get_tab_btns(tabviews[i]);
      if (tab_btns != NULL) {
        // El contenedor en sí
        lv_obj_set_style_bg_color(tab_btns, tab_btn_bg_col, LV_PART_MAIN);
        // Los items individuales (las paletas clickeables)
        lv_obj_set_style_bg_color(tab_btns, tab_btn_bg_col, LV_PART_ITEMS);
        lv_obj_set_style_text_color(tab_btns, tab_text_col, LV_PART_ITEMS);
      }

      // 3. Aplicar fondo a CADA página interna (LV_DIR_CONTENT sub-children)
      // LVGL estructura habitualmente tabview -> [0] tab_btns, [1]
      // tab_content_container -> tabs
      lv_obj_t *content_cont = lv_tabview_get_content(tabviews[i]);
      if (content_cont) {
        // El root donde están las páginas
        lv_obj_set_style_bg_color(content_cont, tab_page_bg_col, LV_PART_MAIN);

        uint32_t tab_cnt = lv_obj_get_child_cnt(content_cont);
        for (uint32_t t = 0; t < tab_cnt; t++) {
          lv_obj_t *real_page = lv_obj_get_child(content_cont, t);
          if (real_page) {
            lv_obj_set_style_bg_color(real_page, tab_page_bg_col, LV_PART_MAIN);
          }
        }
      }
    }
  }

  // --- DROPDOWNS DARK MODE APLICATION ---
  lv_color_t dd_bg_col = darkMode ? COLOR_PANEL_DARK : lv_color_hex(0xFFFFFF);
  lv_color_t dd_text_col = darkMode ? COLOR_TEXT_DARK : lv_color_hex(0x000000);
  lv_color_t dd_list_bg = darkMode ? COLOR_PANEL_DARK : lv_color_hex(0xFFFFFF);
  lv_color_t dd_list_sel =
      darkMode ? lv_color_hex(0x555555) : lv_palette_main(LV_PALETTE_BLUE);

  lv_obj_t *dropdowns[] = {ui_LanguagesDropDown};

  for (int i = 0; i < 1; i++) {
    if (dropdowns[i] != NULL) {
      // Estilo del botón principal del dropdown
      lv_obj_set_style_bg_color(dropdowns[i], dd_bg_col, LV_PART_MAIN);
      lv_obj_set_style_text_color(dropdowns[i], dd_text_col, LV_PART_MAIN);

      // Estilo de la lista que se despliega
      lv_obj_t *list = lv_dropdown_get_list(dropdowns[i]);
      if (list != NULL) {
        // Fondo de la lista al desplegar
        lv_obj_set_style_bg_color(list, dd_list_bg, LV_PART_MAIN);
        lv_obj_set_style_border_color(
            list, darkMode ? lv_color_hex(0x444444) : lv_color_hex(0xCCCCCC),
            LV_PART_MAIN);
        // Texto de las opciones
        lv_obj_set_style_text_color(list, dd_text_col, LV_PART_MAIN);

        // Color de la opción seleccionada
        lv_obj_set_style_bg_color(list, dd_list_sel, LV_PART_SELECTED);
      }
    }
  }


  UI_SyncAll();
}
