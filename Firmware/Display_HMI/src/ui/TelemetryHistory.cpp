#include "ui/TelemetryHistory.h"

#include <cmath>
#include <cstdio>
#include <ctime>

#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#include "CommTask.h"
#include "UITask.h"
#include "main.h"
#include "modules/trend/trend_events.h"
#include "modules/trend/trend_policy.h"
#include "modules/trend/trend_store.h"
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

// ---------------------------------------------------------------------------
// Modelo de datos: una ranura de 10 s por muestra, indice == tiempo.
//
// El anillo (seis series int16: medida y consigna de aire/piel/humedad, ver
// modules/trend/) vive en PSRAM, no en .bss: la SRAM interna es el recurso
// escaso del HMI. Los dos bounce buffers del panel RGB necesitan 38,4 KB
// contiguos y DMA-capaces CADA UNO, y el driver los pide en UI_Task, despues
// de que WiFi se haya llevado su parte del heap interno; 17 KB de .bss aqui
// eran parte de lo que dejaba esa reserva sin sitio (ESP_ERR_NO_MEM en
// esp_lcd_new_rgb_panel → abort en el arranque). Estos buffers solo los lee
// y escribe la CPU, nunca DMA, asi que PSRAM les vale de sobra.
// Nombre especifico y no "BUFFER_SIZE": main.h ya declara una constante
// global con ese nombre (buffers de texto de labels) y, aunque esta vive en
// un namespace anonimo, ambas quedan visibles sin cualificar en este mismo
// TU — el nombre generico resultaba en "reference to BUFFER_SIZE is
// ambiguous" al compilar.
constexpr int HIST_BUF_SIZE = 1440;
// Ranuras de tiempo absolutas (esp_timer / SLOT_MS), no "10 s desde la
// ultima muestra": con el criterio relativo cada muestra caia 10 s + el
// periodo de la telemetria despues de la anterior, y ese medio segundo de
// media acumulado sobre 1440 muestras desplazaba el eje X hasta 12 min. Con
// ranuras absolutas el indice del buffer ES el tiempo (±10 s), que es lo que
// hace fiables las lineas de rejilla y las horas del eje.
constexpr int64_t SLOT_MS = TREND_SLOT_MS;  // 10 s/ranura
// 1h/2h/4h en ranuras. El ultimo coincide con HIST_BUF_SIZE (el techo del
// buffer es tambien la ventana mas ancha ofrecida) — referencia la constante
// en vez de repetir 1440 para que no puedan divergir. 1h primero: es la
// ventana por defecto (indice 0 del dropdown, sin seleccion explicita).
constexpr int WINDOW_POINTS[3] = {360, 720, HIST_BUF_SIZE};
// Eje X: rejilla vertical cada 15 min en las tres ventanas (90 ranuras) y
// siempre 5 etiquetas de hora (4 tramos): cada 15 min en 1 h, cada 30 min
// en 2 h, cada hora en 4 h. Las lineas que coinciden con una etiqueta se
// pintan mas marcadas que las intermedias (ver onChartDrawPart).
constexpr int GRID_STEP_SLOTS = 90;
constexpr int LABEL_SEGMENTS = 4;
// Lineas horizontales: alineadas con las 3 etiquetas del eje Y (la
// ui_apply_sparkline_style pone 4, que no cuadran con 3 numeros).
constexpr uint16_t HDIV_LINES = 3;

// Valores en decimas (36.7 C → 367) para que la traza se mueva con 0.1 C:
// lv_coord_t es entero y con el rango en grados enteros una variacion de
// medio grado no se veia hasta cruzar el entero. Es la misma unidad en la que
// guarda el modelo (trend_tenths), asi que no hay conversion al pintar. Las
// etiquetas del eje Y deshacen la escala en onChartDrawPart.
constexpr int Y_SCALE = 10;

// Geometria. Las etiquetas del eje Y se dibujan A LA IZQUIERDA DEL PROPIO
// BORDE del chart (lv_chart.c: draw_y_ticks), asi que lo que importa es el
// hueco fisico hasta el borde de s_content: con x=14 "40"/"100" se
// recortaban por la izquierda (822132e). Ancho 672 y no 686: la ultima
// etiqueta del eje X va centrada en el borde derecho del area de datos y
// necesita ~16 px mas alla del chart antes de topar con s_content (740).
constexpr lv_coord_t CHART_X = 44;
constexpr lv_coord_t CHART_W = 672;
constexpr lv_coord_t CHART_H = 96;
// Solo el chart de abajo (humedad) lleva las marcas y horas del eje X: entre
// un chart y la etiqueta del siguiente hay 6 px, no cabe un eje por chart.
// Las tres rejillas verticales comparten geometria, asi que la hora leida
// abajo vale para las tres trazas.
constexpr lv_coord_t X_TICK_DRAW_SIZE = 24;

// Colores de las marcas y de la consigna (la tarjeta es blanca, asi que nada
// de amarillos claros). La consigna es morada para que no se confunda con
// ninguna prioridad de alarma (rojo/naranja/amarillo) ni con las tres medidas
// (verde/cian/azul).
constexpr uint32_t COLOR_SETPOINT = 0x8E24AA;
constexpr uint32_t COLOR_MARK_HIGH = 0xD32F2F;
constexpr uint32_t COLOR_MARK_MEDIUM = 0xF57C00;
constexpr uint32_t COLOR_MARK_LOW = 0xC9A000;
constexpr uint32_t COLOR_MARK_CLEARED = 0x2E7D32;

// Tres graficos: aire, piel, humedad. Cada uno con su serie de medida y la de
// consigna del mismo canal en el modelo.
constexpr int N_CHARTS = 3;
constexpr int MEAS_SERIES[N_CHARTS] = {TREND_AIR, TREND_SKIN, TREND_HUM};
constexpr int SP_SERIES[N_CHARTS] = {TREND_SP_AIR, TREND_SP_SKIN, TREND_SP_HUM};

int16_t *s_block = nullptr;
TrendStore s_store = {};
TrendEvents s_events = {};
bool s_allocFailed = false;

// Reserva unica y perpetua de las seis series en un solo bloque de PSRAM: un
// fallo parcial con mallocs sueltos dejaria el historico a medias. Si no hay
// PSRAM (o falla), la tendencia se queda sin datos pero el equipo arranca y
// monitoriza igual — es una vista de consulta, no una funcion de seguridad.
bool ensureBuffers() {
  if (s_block) return true;
  if (s_allocFailed) return false;
  const size_t bytes = trend_store_bytes(HIST_BUF_SIZE);
  s_block = (int16_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
  if (!s_block) {
    s_allocFailed = true;
    ESP_LOGE("TelHist", "sin PSRAM para el historico (%u B) — tendencia vacia",
             (unsigned)bytes);
    return false;
  }
  trend_store_init(&s_store, s_block, HIST_BUF_SIZE);
  return true;
}

int64_t s_drawnSlot = -1;  // ranura "ahora" del ultimo repintado
int s_axisIdx = -1;        // ventana a la que estan configurados los ejes
// Ventana pintada por el ultimo redraw(): las marcas de alarma se colocan
// contra ella, no contra el "ahora" del momento de dibujar, para que queden
// alineadas con las series que ya estan escritas en y_points[].
int64_t s_drawStart = 0;
int s_drawN = 0;

bool s_isOpen = false;

lv_obj_t *s_overlay = nullptr;
lv_obj_t *s_content = nullptr;
lv_obj_t *s_windowDd = nullptr;
lv_obj_t *s_title = nullptr;

lv_obj_t *s_chart[N_CHARTS] = {nullptr, nullptr, nullptr};
lv_chart_series_t *s_serMeas[N_CHARTS] = {nullptr, nullptr, nullptr};
lv_chart_series_t *s_serSp[N_CHARTS] = {nullptr, nullptr, nullptr};
lv_obj_t *s_lblAir = nullptr;
lv_obj_t *s_lblSkin = nullptr;
lv_obj_t *s_lblHum = nullptr;
lv_obj_t *s_legRaised = nullptr;
lv_obj_t *s_legCleared = nullptr;
lv_obj_t *s_legSetpoint = nullptr;

// ---------------------------------------------------------------------------
// Flancos de alarma. CommTask los detecta (processReceivedAlarm) y NO toca el
// modelo: aqui solo se encolan, con el instante en esp_timer, bajo una seccion
// critica de unos pocos ciclos. UI_Task los drena en TelemetryHistory_Poll y
// los convierte a ranura con el instante del flanco, no el del drenaje.
struct PendingEdge {
  int64_t ms;
  uint8_t id;
  uint8_t priority;
  bool raised;
};
constexpr int EDGE_QUEUE = 16;
PendingEdge s_edgeQ[EDGE_QUEUE];
int s_edgeHead = 0;
int s_edgeCount = 0;
portMUX_TYPE s_edgeMux = portMUX_INITIALIZER_UNLOCKED;

// Devuelve cuantos flancos paso al anillo de eventos.
int drainEdges() {
  int n = 0;
  for (;;) {
    PendingEdge e;
    taskENTER_CRITICAL(&s_edgeMux);
    const bool have = s_edgeCount > 0;
    if (have) {
      e = s_edgeQ[s_edgeHead];
      s_edgeHead = (s_edgeHead + 1) % EDGE_QUEUE;
      s_edgeCount--;
    }
    taskEXIT_CRITICAL(&s_edgeMux);
    if (!have) break;
    trend_events_add(&s_events, e.ms / SLOT_MS, e.id, e.priority, e.raised);
    n++;
  }
  return n;
}

int64_t nowSlot() { return (esp_timer_get_time() / 1000) / SLOT_MS; }

int windowIdx() {
  uint16_t idx = s_windowDd ? lv_dropdown_get_selected(s_windowDd) : 0;
  return idx > 2 ? 0 : (int)idx;
}

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

// Texto de la etiqueta k (0..LABEL_SEGMENTS) del eje X para la ventana idx.
// Hora local si la placa ya ha mandado la hora (mismo criterio que el reloj
// del heading: HMI_GetEpochNow + HMI_ToLocal); si no, tiempo hacia atras
// "-h:mm" ("-0:45", "-2:00") con "0:00" en el borde derecho, que es "ahora".
void xLabelText(int idx, int k, char *out, size_t outLen) {
  const int n = WINDOW_POINTS[idx];
  const int64_t backSec =
      (int64_t)(n - (n * k) / LABEL_SEGMENTS) * (SLOT_MS / 1000);
  const uint32_t epoch = HMI_GetEpochNow();
  if (epoch != 0) {
    const uint32_t local = HMI_HasLocalTime() ? HMI_ToLocal(epoch) : epoch;
    const time_t t = (time_t)((int64_t)local - backSec);
    struct tm tmv;
    gmtime_r(&t, &tmv);
    snprintf(out, outLen, "%02d:%02d", tmv.tm_hour, tmv.tm_min);
    return;
  }
  const int mins = (int)(backSec / 60);
  if (mins == 0)
    snprintf(out, outLen, "0:00");
  else
    snprintf(out, outLen, "-%d:%02d", mins / 60, mins % 60);
}

// Un solo hook de dibujo para los tres charts:
//  - etiquetas del eje Y: deshacer Y_SCALE (367 → "36"; LVGL solo sabe
//    imprimir el entero del rango).
//  - etiquetas del eje X (solo el chart de humedad las tiene): hora.
//  - lineas verticales de la rejilla: las que caen en una etiqueta de hora
//    mas marcadas que las intermedias de 15 min, para que en 4 h (17 lineas)
//    la hora entera se distinga de un vistazo.
void onChartDrawPart(lv_event_t *e) {
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
  if (!dsc || dsc->class_p != &lv_chart_class) return;

  if (dsc->type == LV_CHART_DRAW_PART_TICK_LABEL && dsc->text) {
    if (dsc->id == LV_CHART_AXIS_PRIMARY_Y) {
      lv_snprintf(dsc->text, dsc->text_length, "%d",
                  (int)(dsc->value / Y_SCALE));
    } else if (dsc->id == LV_CHART_AXIS_PRIMARY_X) {
      xLabelText(windowIdx(), (int)dsc->value, dsc->text, dsc->text_length);
    }
    return;
  }

  if (dsc->type == LV_CHART_DRAW_PART_DIV_LINE_VER && dsc->line_dsc) {
    const int n = WINDOW_POINTS[windowIdx()];
    const int linesPerLabel = (n / LABEL_SEGMENTS) / GRID_STEP_SLOTS;  // 1/2/4
    const bool major = (dsc->id % linesPerLabel) == 0;
    dsc->line_dsc->opa = major ? LV_OPA_40 : LV_OPA_20;
  }
}

lv_color_t markColor(TrendMarkKind kind) {
  switch (kind) {
    case TREND_MARK_HIGH:
      return lv_color_hex(COLOR_MARK_HIGH);
    case TREND_MARK_MEDIUM:
      return lv_color_hex(COLOR_MARK_MEDIUM);
    case TREND_MARK_LOW:
      return lv_color_hex(COLOR_MARK_LOW);
    case TREND_MARK_CLEARED:
    default:
      return lv_color_hex(COLOR_MARK_CLEARED);
  }
}

// Marcas de alarma: una linea vertical por evento visible, sobre series y
// rejilla (DRAW_POST_END, cuando el chart ya ha pintado todo lo suyo). Alta =
// continua con el color de la prioridad; baja (resuelta) = discontinua verde.
// Se colocan contra la ventana del ULTIMO redraw() (s_drawStart/s_drawN), la
// misma con la que se escribieron las series.
void onChartDrawPost(lv_event_t *e) {
  lv_obj_t *chart = lv_event_get_target(e);
  lv_draw_ctx_t *dc = lv_event_get_draw_ctx(e);
  if (!chart || !dc || s_drawN <= 0 || s_events.count == 0) return;

  lv_area_t a;
  lv_obj_get_content_coords(chart, &a);
  const int w = lv_area_get_width(&a);
  if (w <= 0) return;

  for (int i = 0; i < s_events.count; i++) {
    TrendEvent ev;
    if (!trend_events_get(&s_events, i, &ev)) break;
    const int px = trend_slot_to_px(ev.slot, s_drawStart, s_drawN, w);
    if (px < 0) continue;

    const TrendMarkKind kind = trend_mark_kind(ev.priority, ev.raised);
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = markColor(kind);
    d.width = 2;
    d.opa = LV_OPA_COVER;
    if (kind == TREND_MARK_CLEARED) {
      d.dash_width = 4;
      d.dash_gap = 3;
    }
    lv_point_t p1 = {(lv_coord_t)(a.x1 + px), a.y1};
    lv_point_t p2 = {(lv_coord_t)(a.x1 + px), a.y2};
    lv_draw_line(dc, &d, &p1, &p2);
  }
}

// Ejes y rejilla para la ventana idx. Solo hace trabajo cuando cambia la
// ventana: lv_chart_set_point_count realoja las series (y antes se llamaba
// en cada muestra) y set_axis_tick/set_div_line_count invalidan el chart.
void applyAxis(int idx) {
  if (idx == s_axisIdx) return;
  s_axisIdx = idx;
  const int n = WINDOW_POINTS[idx];
  const uint16_t vdiv = (uint16_t)(n / GRID_STEP_SLOTS + 1);  // 5/9/17
  const uint16_t minor =
      (uint16_t)((n / LABEL_SEGMENTS) / GRID_STEP_SLOTS);  // 1/2/4
  for (lv_obj_t *c : s_chart) {
    lv_chart_set_point_count(c, (uint16_t)n);
    lv_chart_set_div_line_count(c, HDIV_LINES, vdiv);
  }
  lv_chart_set_axis_tick(s_chart[N_CHARTS - 1], LV_CHART_AXIS_PRIMARY_X, 6, 3,
                         LABEL_SEGMENTS + 1, minor, true, X_TICK_DRAW_SIZE);
}

// Repinta las series desde el anillo. El punto j (0..N-1) del chart es la
// ranura now-(N-1)+j: el eje X es siempre la ventana completa y los datos
// quedan pegados al borde derecho ("ahora"). Con el equipo recien encendido
// se ve la ventana vacia con 10 min de traza a la derecha, no 10 min
// estirados a todo el ancho como si fueran 4 h. Y si el enlace lleva un
// rato caido, la traza se va desplazando a la izquierda con el tiempo
// (TelemetryHistory_Poll repinta al cambiar de ranura) en vez de quedarse
// pegada al borde fingiendo ser actual.
//
// Escribe y_points[] directamente y refresca una vez por chart: la version
// anterior encadenaba lv_chart_set_next_value, que en modo SHIFT invalida el
// chart ENTERO dos veces por punto — 8640 invalidaciones por repintado con
// la ventana de 4 h, cada una con consultas de estilo en PSRAM. Eso era el
// "va lenta" con la vista abierta, y de paso tenia bloqueada LVGL_Lock (y
// con ella la tarea Comm) durante el repintado.
void redraw() {
  for (int c = 0; c < N_CHARTS; c++)
    if (!s_chart[c] || !s_serMeas[c] || !s_serSp[c]) return;
  const int64_t t0 = esp_timer_get_time();

  const int idx = windowIdx();
  const int n = WINDOW_POINTS[idx];
  const int64_t now = nowSlot();
  s_drawnSlot = now;
  applyAxis(idx);
  s_drawStart = now - (n - 1);
  s_drawN = n;

  for (int c = 0; c < N_CHARTS; c++) {
    s_serMeas[c]->start_point = 0;
    s_serSp[c]->start_point = 0;
  }

  const bool haveData = s_block != nullptr && s_store.count > 0;
  for (int c = 0; c < N_CHARTS; c++) {
    lv_coord_t *yMeas = s_serMeas[c]->y_points;
    lv_coord_t *ySp = s_serSp[c]->y_points;
    for (int j = 0; j < n; j++) {
      // NO_DATA (medida no disponible / ranura sin telemetria / consigna que
      // no gobierna nada) se pinta como LV_CHART_POINT_NONE: LVGL corta la
      // linea ahi en vez de unir con una recta que implicaria una medida o
      // continuidad que no hubo.
      int16_t m = TREND_NO_DATA;
      int16_t sp = TREND_NO_DATA;
      if (haveData) {
        const int64_t slot = s_drawStart + j;
        m = trend_store_value(&s_store, MEAS_SERIES[c], slot);
        sp = trend_store_value(&s_store, SP_SERIES[c], slot);
      }
      yMeas[j] = (m == TREND_NO_DATA) ? LV_CHART_POINT_NONE : (lv_coord_t)m;
      ySp[j] = (sp == TREND_NO_DATA) ? LV_CHART_POINT_NONE : (lv_coord_t)sp;
    }
  }

  for (lv_obj_t *c : s_chart) lv_chart_refresh(c);

  // Medida para el banco: solo se emite con la vista abierta (una linea
  // cada 10 s). Es el coste de preparar el frame, no de pintarlo — el pintado
  // lo hace lv_timer_handler y se ve en lcd_diagnostics_log (slow_frames).
  ESP_LOGI("TelHist", "redraw %d pts, %d muestras, %d eventos, %lld us", n,
           s_store.count, s_events.count,
           (long long)(esp_timer_get_time() - t0));
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

void makeChart(int slot, lv_coord_t y, int rangeLo, int rangeHi, double safeLo,
               double safeHi, lv_color_t color) {
  lv_obj_t *chart = lv_chart_create(s_content);
  lv_obj_set_size(chart, CHART_W, CHART_H);
  lv_obj_set_pos(chart, CHART_X, y);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, rangeLo * Y_SCALE,
                     rangeHi * Y_SCALE);
  ui_apply_sparkline_style(chart, color);
  // Menos marcas que las 4 de ui_apply_sparkline_style() (pensada para los
  // charts de 280 px de Tiempo Real): con solo 96 px de alto, 4 marcas
  // quedaban muy juntas verticalmente. pad_left aqui SI es el "part"
  // correcto (LV_PART_TICKS, no MAIN): separacion entre la marca y el
  // numero, no el margen fisico (ese lo da CHART_X).
  lv_obj_set_style_pad_left(chart, 6, LV_PART_TICKS);
  lv_obj_set_style_pad_bottom(chart, 3, LV_PART_TICKS);  // hueco marca→hora
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 2, 1, 3, 1, true, 34);
  // Rejilla visible sobre la tarjeta blanca: los 15/255 de la sparkline
  // (pensados para el fondo oscuro de Tiempo Real) no se ven aqui. Las
  // verticales ajustan su opacidad por linea en onChartDrawPart.
  lv_obj_set_style_line_opa(chart, LV_OPA_30, LV_PART_MAIN);
  lv_obj_add_event_cb(chart, onChartDrawPart, LV_EVENT_DRAW_PART_BEGIN, NULL);
  lv_obj_add_event_cb(chart, onChartDrawPost, LV_EVENT_DRAW_POST_END, NULL);
  ui_add_chart_safe_zone(chart, (float)(safeLo * Y_SCALE),
                          (float)(safeHi * Y_SCALE),
                          (float)(rangeLo * Y_SCALE),
                          (float)(rangeHi * Y_SCALE));
  // La consigna se anade ANTES que la medida: LVGL pinta las series en orden
  // de creacion, asi que la medida queda encima de la linea de consigna.
  s_serSp[slot] = lv_chart_add_series(chart, lv_color_hex(COLOR_SETPOINT),
                                      LV_CHART_AXIS_PRIMARY_Y);
  s_serMeas[slot] = lv_chart_add_series(chart, color, LV_CHART_AXIS_PRIMARY_Y);
  s_chart[slot] = chart;
}

enum class LegendGlyph { VerticalSolid, VerticalDashed, HorizontalLine };

// Una entrada de la leyenda: el trazo tal y como se ve en el grafico (las
// marcas son verticales, la consigna horizontal) y su texto. Devuelve el
// label para poder cambiarle el idioma en caliente.
lv_obj_t *makeLegendItem(lv_obj_t *row, lv_color_t color, LegendGlyph glyph,
                         const char *text) {
  lv_obj_t *item = lv_obj_create(row);
  lv_obj_remove_style_all(item);
  lv_obj_set_size(item, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(item, 5, 0);
  lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  // Caja de 18x14 con uno o dos segmentos: marca vertical continua = una
  // barra de 3x14; discontinua = dos de 3x6; consigna = una barra de 18x3.
  lv_obj_t *sw = lv_obj_create(item);
  lv_obj_remove_style_all(sw);
  lv_obj_set_size(sw, 18, 14);
  lv_obj_clear_flag(sw, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  struct Seg {
    lv_coord_t x, y, w, h;
  };
  Seg segs[2] = {};
  int nSegs = 0;
  switch (glyph) {
    case LegendGlyph::VerticalSolid:
      segs[nSegs++] = {7, 0, 3, 14};
      break;
    case LegendGlyph::VerticalDashed:
      segs[nSegs++] = {7, 0, 3, 6};
      segs[nSegs++] = {7, 8, 3, 6};
      break;
    case LegendGlyph::HorizontalLine:
      segs[nSegs++] = {0, 6, 18, 3};
      break;
  }
  for (int i = 0; i < nSegs; i++) {
    lv_obj_t *seg = lv_obj_create(sw);
    lv_obj_remove_style_all(seg);
    lv_obj_set_size(seg, segs[i].w, segs[i].h);
    lv_obj_set_pos(seg, segs[i].x, segs[i].y);
    lv_obj_set_style_bg_color(seg, color, 0);
    lv_obj_set_style_bg_opa(seg, LV_OPA_COVER, 0);
    lv_obj_clear_flag(seg, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  }

  lv_obj_t *lbl = lv_label_create(item);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0x333333), 0);
  return lbl;
}

}  // namespace

void TelemetryHistory_Init(void) {
  trend_events_init(&s_events);

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

  // Leyenda de las marcas y la consigna, entre el titulo y el selector de
  // ventana (hay ~440 px libres; las cuatro lenguas caben).
  lv_obj_t *legend = lv_obj_create(s_content);
  lv_obj_remove_style_all(legend);
  lv_obj_set_size(legend, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(legend, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(legend, 14, 0);
  lv_obj_clear_flag(legend, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(legend, LV_ALIGN_TOP_LEFT, 120, 10);
  s_legRaised = makeLegendItem(legend, lv_color_hex(COLOR_MARK_HIGH),
                               LegendGlyph::VerticalSolid,
                               TR(STR_TREND_LEG_RAISED));
  s_legCleared = makeLegendItem(legend, lv_color_hex(COLOR_MARK_CLEARED),
                                LegendGlyph::VerticalDashed,
                                TR(STR_TREND_LEG_CLEARED));
  s_legSetpoint = makeLegendItem(legend, lv_color_hex(COLOR_SETPOINT),
                                 LegendGlyph::HorizontalLine,
                                 TR(STR_TREND_LEG_SETPOINT));

  // Mismos colores que las graficas de ui_ScreenCharts (Tiempo Real): aire
  // verde, piel cian, humedad azul — un lenguaje visual ya establecido en
  // este HMI, no uno nuevo por pantalla.
  s_lblAir = makeChartLabel(40, TR(STR_AIR));
  makeChart(0, 58, TEMP_CHART_MIN, TEMP_CHART_MAX, AIR_SAFE_ZONE_MIN,
            AIR_SAFE_ZONE_MAX, lv_color_hex(0x00FF00));

  s_lblSkin = makeChartLabel(160, TR(STR_SKIN));
  makeChart(1, 178, TEMP_CHART_MIN, TEMP_CHART_MAX, SKIN_SAFE_ZONE_MIN,
            SKIN_SAFE_ZONE_MAX, lv_color_hex(0x00E0E0));

  s_lblHum = makeChartLabel(280, TR(STR_HUMIDITY));
  makeChart(2, 298, HUM_CHART_MIN, HUM_CHART_MAX, HUM_SAFE_ZONE_MIN,
            HUM_SAFE_ZONE_MAX, lv_color_hex(0x3B82F6));

  // Ejes de la ventana por defecto ya en Init: asi la rejilla y las horas
  // estan listas en la primera apertura sin depender de que redraw() corra
  // antes del primer frame.
  applyAxis(windowIdx());
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
  if (s_legRaised) lv_label_set_text(s_legRaised, TR(STR_TREND_LEG_RAISED));
  if (s_legCleared) lv_label_set_text(s_legCleared, TR(STR_TREND_LEG_CLEARED));
  if (s_legSetpoint)
    lv_label_set_text(s_legSetpoint, TR(STR_TREND_LEG_SETPOINT));
}

bool TelemetryHistory_IsOpen(void) { return s_isOpen; }

void TelemetryHistory_Close(void) {
  if (s_isOpen) closeScreen();
}

void TelemetryHistory_Open(void) {
  if (!s_overlay || s_isOpen) return;
  // Evita el parpadeo de abrir y cerrar en el mismo tick: si ya hay algo con
  // mas prioridad, ni se abre (mismo criterio que BabyExitDialog.cpp).
  if (mustYield()) return;
  s_isOpen = true;
  drainEdges();
  redraw();
  lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(s_overlay);
}

void TelemetryHistory_Poll(void) {
  // Los flancos se drenan con la vista cerrada tambien: el anillo de eventos
  // tiene que estar al dia para cuando se abra.
  const int newEdges = drainEdges();
  if (!s_isOpen) return;
  if (mustYield()) {
    closeScreen();
    return;
  }
  // El eje avanza con el reloj aunque no llegue telemetria: un repintado
  // por ranura (10 s), que con y_points[] directo es barato. Un flanco nuevo
  // repinta al momento para que la marca aparezca sin esperar.
  if (newEdges > 0 || nowSlot() != s_drawnSlot) redraw();
}

void TelemetryHistory_RecordAlarmEdge(uint8_t id, uint8_t priority,
                                      bool raised) {
  // Contexto: CommTask. Nada de LVGL ni de modelo aqui, solo el hand-off.
  const int64_t ms = esp_timer_get_time() / 1000;
  taskENTER_CRITICAL(&s_edgeMux);
  if (s_edgeCount >= EDGE_QUEUE) {  // la UI no drena: se pierde el mas antiguo
    s_edgeHead = (s_edgeHead + 1) % EDGE_QUEUE;
    s_edgeCount--;
  }
  PendingEdge &slotRef = s_edgeQ[(s_edgeHead + s_edgeCount) % EDGE_QUEUE];
  slotRef.ms = ms;
  slotRef.id = id;
  slotRef.priority = priority;
  slotRef.raised = raised;
  s_edgeCount++;
  taskEXIT_CRITICAL(&s_edgeMux);
}

void TelemetryHistory_RecordSample(float airTempC, bool airOk,
                                    float skinTempC, bool skinOk,
                                    float humPct, bool humOk,
                                    float spAirC, bool spAirOk,
                                    float spSkinC, bool spSkinOk,
                                    float spHumPct, bool spHumOk) {
  if (!ensureBuffers()) return;

  TrendSample sample;
  sample.v[TREND_AIR] = trend_tenths(airTempC, airOk);
  sample.v[TREND_SKIN] = trend_tenths(skinTempC, skinOk);
  sample.v[TREND_HUM] = trend_tenths(humPct, humOk);
  sample.v[TREND_SP_AIR] = trend_tenths(spAirC, spAirOk);
  sample.v[TREND_SP_SKIN] = trend_tenths(spSkinC, spSkinOk);
  sample.v[TREND_SP_HUM] = trend_tenths(spHumPct, spHumOk);

  // Una muestra por ranura: la primera telemetria de cada 10 s manda, y los
  // huecos (enlace caido, reinicio de la placa — known_issues.md #1/#5) se
  // rellenan con "sin dato" dentro de trend_store_record.
  if (!trend_store_record(&s_store, nowSlot(), &sample)) return;

  // Redibujar solo si esta visible: el mismo ahorro que ya hacia el panel
  // retirado (evitar repintar un chart que nadie esta mirando).
  if (s_isOpen) redraw();
}
