#include "ui/TelemetryHistory.h"

#include <cmath>

#include "CommTask.h"
#include "UITask.h"
#include "main.h"
#include "ui.h"

// Definidas en ElementsCreation.cpp (sin header propio: hasta ahora solo las
// usaban las gráficas de esa misma unidad de compilación). Mismo patrón que
// AlarmCenter_Open en ui_event_AlarmLockImg: extern local en vez de un
// include nuevo para dos funciones.
extern void ui_apply_sparkline_style(lv_obj_t *chart, lv_color_t color);
extern void ui_add_chart_safe_zone(lv_obj_t *chart, float min_val,
                                    float max_val, float range_min,
                                    float range_max);

namespace {

// 4 h de techo a 10 s/muestra = 1440 puntos por canal — 1440*3*4 bytes =
// ~16.9 KB, sin apuro frente a los 320 KB de SRAM del ESP32-S3.
// Nombre especifico y no "BUFFER_SIZE": main.h ya declara una constante
// global con ese nombre (buffers de texto de labels) y, aunque esta vive en
// un namespace anonimo, ambas quedan visibles sin cualificar en este mismo
// TU — el nombre generico resultaba en "reference to BUFFER_SIZE is
// ambiguous" al compilar.
constexpr int HIST_BUF_SIZE = 1440;
// Por tiempo transcurrido (millis()), no por cuenta de llamadas: RecordSample
// solo se invoca cuando llega telemetria real (g_pendingTelemetryApply), asi
// que un contador de llamadas se estira si el bucle de UI coalesce mas de una
// linea CTRL,TEL entre pasadas. La base de tiempo real evita ese arrastre.
constexpr uint32_t DECIMATE_MS = 10000;  // ~10 s/muestra
// Un hueco > 2x el intervalo esperado es un corte real (EMI, reinicio de la
// placa — known_issues.md #1/#5), no jitter del bucle de UI.
constexpr uint32_t GAP_THRESHOLD_MS = 2 * DECIMATE_MS;
// 1h/2h/4h a razon de 1 punto/10s. El ultimo coincide con HIST_BUF_SIZE (el
// techo del buffer es tambien la ventana mas ancha ofrecida) — referencia la
// constante en vez de repetir 1440 para que no puedan divergir. 1h primero:
// es la ventana por defecto (indice 0 del dropdown, sin seleccion explicita).
constexpr int WINDOW_POINTS[3] = {360, 720, HIST_BUF_SIZE};

// NAN marca "sin dato valido en este instante": medida no disponible
// (PROTO_TEL_*_UNAVAILABLE), enlace HMI<->motherBoard caido, o hueco de
// tiempo detectado. Nunca se guarda ni se pinta el centinela crudo de la
// placa (-999.0/-1) como si fuera una lectura real — PROTOCOL.md es
// explicito sobre por que eso es un problema de seguridad, no cosmetico.
float s_bufAir[HIST_BUF_SIZE];
float s_bufSkin[HIST_BUF_SIZE];
float s_bufHum[HIST_BUF_SIZE];
int s_writeIdx = 0;
int s_sampleCount = 0;
uint32_t s_lastSampleMs = 0;
bool s_haveLastSample = false;

bool s_isOpen = false;

lv_obj_t *s_overlay = nullptr;
lv_obj_t *s_content = nullptr;
lv_obj_t *s_windowDd = nullptr;
lv_obj_t *s_title = nullptr;

lv_obj_t *s_chartAir = nullptr;
lv_obj_t *s_chartSkin = nullptr;
lv_obj_t *s_chartHum = nullptr;
lv_chart_series_t *s_serAir = nullptr;
lv_chart_series_t *s_serSkin = nullptr;
lv_chart_series_t *s_serHum = nullptr;
lv_obj_t *s_lblAir = nullptr;
lv_obj_t *s_lblSkin = nullptr;
lv_obj_t *s_lblHum = nullptr;

// true si algo con mas prioridad debe llevarse la pantalla por delante: una
// alarma activa (el banner y el icono de AUDIO PAUSED, ambos en
// lv_layer_top() como este overlay, tienen que verse y ser pulsables) o el
// enlace HMI<->motherBoard caido (su aviso "tiene que verse SIEMPRE por
// delante de todo lo demas", ver alarm_banner_update en UITask.cpp).
//
// A diferencia de BabyHistory/BabyWizard/TimeDialog, aqui no basta con
// UI_IsCriticalAlarmActive(): este panel es una vista de solo lectura sin
// ninguna informacion de alarma propia que compense taparla. AlarmCenter si
// es la excepcion legitima (es donde se atiende la alarma); este no.
bool mustYield() { return UI_IsAnyAlarmActive() || Display_IsBoardLinkLost(); }

void closeScreen() {
  // La lista desplegada del dropdown se reparenta a lv_layer_top() al abrir
  // (lv_dropdown_open) y lv_dropdown_close() no la reparenta de vuelta —
  // solo la oculta. Cerrarla explicitamente evita que quede huerfana y
  // visible por encima de todo tras ocultar el overlay.
  if (s_windowDd) lv_dropdown_close(s_windowDd);
  if (s_overlay) lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  s_isOpen = false;
}

void onClose(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  closeScreen();
}

// Repinta las 3 series desde el buffer circular segun la ventana elegida en
// el dropdown. Mismo algoritmo que el panel retirado: rango de ejes fijo,
// solo cambia cuantos puntos recientes se muestran.
void redraw() {
  if (!s_chartAir || !s_serAir || !s_serSkin || !s_serHum) return;

  uint16_t idx = lv_dropdown_get_selected(s_windowDd);
  if (idx > 2) idx = 0;
  int point_count = WINDOW_POINTS[idx];
  if (point_count > s_sampleCount) point_count = s_sampleCount;

  int drawCount = point_count > 0 ? point_count : 1;
  lv_chart_set_point_count(s_chartAir, drawCount);
  lv_chart_set_point_count(s_chartSkin, drawCount);
  lv_chart_set_point_count(s_chartHum, drawCount);

  for (int i = 0; i < drawCount; i++) {
    s_serAir->y_points[i] = LV_CHART_POINT_NONE;
    s_serSkin->y_points[i] = LV_CHART_POINT_NONE;
    s_serHum->y_points[i] = LV_CHART_POINT_NONE;
  }
  s_serAir->start_point = 0;
  s_serSkin->start_point = 0;
  s_serHum->start_point = 0;

  // Sin datos aun (equipo recien encendido): dejar los ejes vacios en vez de
  // dibujar una linea plana en 0, que se leeria como una medida real.
  if (point_count > 0) {
    int start_idx = (s_writeIdx - point_count + HIST_BUF_SIZE) % HIST_BUF_SIZE;
    for (int i = 0; i < point_count; i++) {
      int b = (start_idx + i) % HIST_BUF_SIZE;
      // NAN (medida no disponible / hueco de tiempo) se pinta como
      // LV_CHART_POINT_NONE: LVGL corta la linea ahi en vez de unir con una
      // recta que implicaria una medida o continuidad que no hubo.
      lv_coord_t vAir = std::isnan(s_bufAir[b])
                            ? LV_CHART_POINT_NONE
                            : (lv_coord_t)s_bufAir[b];
      lv_coord_t vSkin = std::isnan(s_bufSkin[b])
                             ? LV_CHART_POINT_NONE
                             : (lv_coord_t)s_bufSkin[b];
      lv_coord_t vHum = std::isnan(s_bufHum[b])
                             ? LV_CHART_POINT_NONE
                             : (lv_coord_t)s_bufHum[b];
      lv_chart_set_next_value(s_chartAir, s_serAir, vAir);
      lv_chart_set_next_value(s_chartSkin, s_serSkin, vSkin);
      lv_chart_set_next_value(s_chartHum, s_serHum, vHum);
    }
  }

  lv_chart_refresh(s_chartAir);
  lv_chart_refresh(s_chartSkin);
  lv_chart_refresh(s_chartHum);
}

void onWindowChanged(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  redraw();
}

lv_obj_t *makeChartLabel(lv_coord_t y, const char *text) {
  lv_obj_t *lbl = lv_label_create(s_content);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(lbl, 14, y);
  return lbl;
}

lv_obj_t *makeChart(lv_coord_t y, int rangeLo, int rangeHi, double safeLo,
                     double safeHi, lv_color_t color,
                     lv_chart_series_t **outSeries) {
  lv_obj_t *chart = lv_chart_create(s_content);
  // Las etiquetas del eje Y se dibujan A LA IZQUIERDA DEL PROPIO BORDE del
  // chart (lv_chart.c: draw_y_ticks, p2.x = obj->coords.x1 - major_len,
  // label a la izquierda de ahi), no dentro de su padding interno — asi que
  // lo que de verdad importa es el hueco fisico hasta el borde de s_content,
  // no pad_left/LV_PART_MAIN (ese no es el "part" que lee label_gap). Con
  // x=14 solo habia 14 px antes de topar con s_content y "40"/"100" se
  // recortaban por la izquierda, dejando visible solo el ultimo digito (de
  // ahi el "todo 0": 20/30/40/10/100 acaban todos en 0). Los charts de
  // Tiempo Real no lo sufren porque quedan centrados con ~35 px de margen
  // (771 px de contenedor - 700 px de chart) / 2; aqui se replica ese mismo
  // margen con numeros, no con centrado, porque el ancho es fijo.
  lv_obj_set_size(chart, 686, 96);
  lv_obj_set_pos(chart, 44, y);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, rangeLo, rangeHi);
  ui_apply_sparkline_style(chart, color);
  // Menos marcas que las 4 de ui_apply_sparkline_style() (pensada para los
  // charts de 280 px de Tiempo Real): con solo 96 px de alto, 4 marcas
  // quedaban muy juntas verticalmente. pad_left aqui SI es el "part"
  // correcto (LV_PART_TICKS, no MAIN): separacion entre la marca y el
  // numero, no el margen fisico (ese lo da el x=44 de arriba).
  lv_obj_set_style_pad_left(chart, 6, LV_PART_TICKS);
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 2, 1, 3, 1, true, 34);
  ui_add_chart_safe_zone(chart, (float)safeLo, (float)safeHi,
                          (float)rangeLo, (float)rangeHi);
  *outSeries = lv_chart_add_series(chart, color, LV_CHART_AXIS_PRIMARY_Y);
  return chart;
}

}  // namespace

void TelemetryHistory_Init(void) {
  // lv_layer_top(), no una pantalla: igual que AlarmCenter, para poder
  // abrirse desde ui_ScreenLock sin desbloquear.
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

  lv_obj_t *card = lv_obj_create(s_overlay);
  lv_obj_set_size(card, 760, 450);
  lv_obj_center(card);
  lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
  lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  s_content = lv_obj_create(card);
  lv_obj_remove_style_all(s_content);
  lv_obj_set_size(s_content, 740, 430);
  lv_obj_center(s_content);
  lv_obj_clear_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);

  s_title = lv_label_create(s_content);
  lv_label_set_text(s_title, TR(STR_TREND));
  lv_obj_set_style_text_font(s_title, &lv_font_montserrat_20, 0);
  lv_obj_align(s_title, LV_ALIGN_TOP_LEFT, 4, 6);

  lv_obj_t *close = lv_btn_create(s_content);
  lv_obj_set_size(close, 44, 44);
  lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_set_style_bg_color(close, lv_color_hex(0xAA3333), LV_PART_MAIN);
  lv_obj_set_style_radius(close, 8, LV_PART_MAIN);
  lv_obj_add_event_cb(close, onClose, LV_EVENT_CLICKED, NULL);
  lv_obj_t *closeLbl = lv_label_create(close);
  lv_label_set_text(closeLbl, "X");
  lv_obj_center(closeLbl);

  s_windowDd = lv_dropdown_create(s_content);
  // "1 h" primero: indice 0 es la seleccion por defecto de un dropdown sin
  // lv_dropdown_set_selected() explicito.
  lv_dropdown_set_options(s_windowDd, "1 h\n2 h\n4 h");
  lv_obj_set_width(s_windowDd, 110);
  lv_obj_align(s_windowDd, LV_ALIGN_TOP_RIGHT, -54, 2);
  lv_obj_add_event_cb(s_windowDd, onWindowChanged, LV_EVENT_VALUE_CHANGED,
                       NULL);

  // Mismos colores que las graficas de ui_ScreenCharts (Tiempo Real): aire
  // verde, piel cian, humedad azul — un lenguaje visual ya establecido en
  // este HMI, no uno nuevo por pantalla.
  s_lblAir = makeChartLabel(40, TR(STR_AIR));
  s_chartAir = makeChart(58, TEMP_CHART_MIN, TEMP_CHART_MAX, AIR_SAFE_ZONE_MIN,
                         AIR_SAFE_ZONE_MAX, lv_color_hex(0x00FF00),
                         &s_serAir);

  s_lblSkin = makeChartLabel(160, TR(STR_SKIN));
  s_chartSkin =
      makeChart(178, TEMP_CHART_MIN, TEMP_CHART_MAX, SKIN_SAFE_ZONE_MIN,
                SKIN_SAFE_ZONE_MAX, lv_color_hex(0x00E0E0), &s_serSkin);

  s_lblHum = makeChartLabel(280, TR(STR_HUMIDITY));
  s_chartHum = makeChart(298, HUM_CHART_MIN, HUM_CHART_MAX, HUM_SAFE_ZONE_MIN,
                         HUM_SAFE_ZONE_MAX, lv_color_hex(0x3B82F6),
                         &s_serHum);
}

void TelemetryHistory_ApplyLanguage(void) {
  // Se llama despues de que UI_ApplyLanguage() ya actualizo g_lang, asi que
  // TR() aqui lee el idioma nuevo.
  if (s_title)
    lv_label_set_text(s_title, TR(STR_TREND));
  if (s_lblAir) lv_label_set_text(s_lblAir, TR(STR_AIR));
  if (s_lblSkin) lv_label_set_text(s_lblSkin, TR(STR_SKIN));
  if (s_lblHum)
    lv_label_set_text(s_lblHum, TR(STR_HUMIDITY));
}

void TelemetryHistory_Open(void) {
  if (!s_overlay || s_isOpen) return;
  // Evita el parpadeo de abrir y cerrar en el mismo tick: si ya hay algo con
  // mas prioridad, ni se abre (mismo criterio que BabyExitDialog.cpp).
  if (mustYield()) return;
  s_isOpen = true;
  redraw();
  lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(s_overlay);
}

void TelemetryHistory_Poll(void) {
  if (!s_isOpen) return;
  if (mustYield()) closeScreen();
}

void TelemetryHistory_RecordSample(float airTempC, bool airOk,
                                    float skinTempC, bool skinOk,
                                    float humPct, bool humOk) {
  const uint32_t now = millis();
  if (s_haveLastSample && (now - s_lastSampleMs) < DECIMATE_MS) return;

  // Hueco real detectado (enlace caido varias muestras, reinicio de la
  // placa — known_issues.md #1/#5): no unir con una linea recta que
  // implicaria continuidad falsa. Se sacrifica esta muestra como marca de
  // corte (NAN en los 3 canales); la siguiente ya retoma con normalidad.
  const bool gap =
      s_haveLastSample && (now - s_lastSampleMs) > GAP_THRESHOLD_MS;
  s_lastSampleMs = now;
  s_haveLastSample = true;

  s_bufAir[s_writeIdx] = (gap || !airOk) ? NAN : airTempC;
  s_bufSkin[s_writeIdx] = (gap || !skinOk) ? NAN : skinTempC;
  s_bufHum[s_writeIdx] = (gap || !humOk) ? NAN : humPct;
  s_writeIdx = (s_writeIdx + 1) % HIST_BUF_SIZE;
  if (s_sampleCount < HIST_BUF_SIZE) s_sampleCount++;

  // Redibujar solo si esta visible: el mismo ahorro que ya hacia el panel
  // retirado (evitar repintar un chart que nadie esta mirando).
  if (s_isOpen) redraw();
}
