# HMI Robustness — Crash Fix & Long-term Stability

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminar el crash confirmado en `ui_ScreenSettings_screen_init` y garantizar operación continua del display HMI durante semanas sin reboot, como requiere una incubadora neonatal.

**Architecture:** El crash ocurre porque CommTask (stack 8 KB) ejecuta operaciones LVGL profundas que desbordan su stack y corrompen memoria adyacente. La solución tiene tres capas: (1) mitigación inmediata aumentando el stack y arreglando bugs confirmados; (2) eliminación arquitectural moviendo todas las llamadas LVGL exclusivamente al UITask; (3) observabilidad permanente con high-water marks de heap/stack para detectar regresiones antes de que fallen.

**Tech Stack:** ESP32-S3, Arduino/ESP-IDF, LVGL 8.3, FreeRTOS, PlatformIO (`pio run -e main`)

**Contexto del crash (NO TOCAR, ya analizado):**
- Crash site: `ElementsCreation.cpp:2653–2655` → `lv_obj_add_flag(NULL, ...)` porque `lv_btn_create(ui_WifiConfigCont)` devolvió NULL
- SP corrupto en el frame llamador contiene bytes ASCII de mensajes `"CTRL,..."` del CommTask
- El CommTask llama `Display_ApplyCtrlState()` que ejecuta ~50 operaciones LVGL desde un stack de 8 KB
- `_ui_screen_delete` en `ui_helpers.c` tiene dos bugs que impiden liberación correcta de pantallas
- `ui_event_Settings` llama `_ui_screen_change` dos veces con el mismo callback

---

## FASE 1 — Fixes de emergencia (desplegar hoy)

> Estas tres tareas son quirúrgicas. No cambian arquitectura. Se pueden compilar, flashear y validar en menos de 1 hora.

---

### Tarea 1: Aumentar stack del CommTask

**Objetivo:** Dar margen inmediato mientras se hace el fix arquitectural (Fase 2). El CommTask llama funciones LVGL que profundizan el call stack; 8 KB es insuficiente.

**Archivos:**
- Modificar: `Display_HMI/include/main.h:208`

- [ ] **Paso 1: Editar `main.h`**

Cambiar línea 208:
```cpp
// ANTES:
constexpr int COMM_TASK_STACK_SIZE = 8192;

// DESPUÉS:
constexpr int COMM_TASK_STACK_SIZE = 16384;   // 16 KB — margen para calls LVGL profundas
```

- [ ] **Paso 2: Compilar**

```bash
cd Display_HMI
pio run -e main
```

Esperado: `SUCCESS`. Si hay errores de enlazado por OOM, reducir a 12288 como compromiso.

- [ ] **Paso 3: Commit**

```bash
git add Display_HMI/include/main.h
git commit -m "fix(hmi): increase CommTask stack 8→16 KB to prevent overflow in LVGL calls"
```

---

### Tarea 2: Corregir `_ui_screen_delete` (dos bugs)

**Objetivo:** La función tiene la condición invertida Y asigna el puntero local en lugar de `*target`. Como consecuencia, las pantallas nunca se liberan correctamente, y si alguna vez se nulificara un puntero de pantalla (tras un fix futuro o un LVGL auto-delete), `_ui_screen_change` llamaría a `target_init()` sobre objetos LVGL en estado inconsistente.

**Archivos:**
- Modificar: `Display_HMI/src/ui_helpers.c:59-65`

- [ ] **Paso 1: Leer el archivo para confirmar las líneas exactas**

```bash
grep -n "_ui_screen_delete" Display_HMI/src/ui_helpers.c
```

Esperado: muestra la función en torno a línea 59.

- [ ] **Paso 2: Corregir `_ui_screen_delete`**

Reemplazar el cuerpo completo de la función (líneas 59–65):

```c
// ANTES (buggy):
void _ui_screen_delete(lv_obj_t ** target)
{
    if(*target == NULL) {          // BUG: condición invertida
        lv_obj_del(*target);
        target = NULL;             // BUG: asigna puntero local, no *target
    }
}

// DESPUÉS (correcto):
void _ui_screen_delete(lv_obj_t ** target)
{
    if(*target != NULL) {
        lv_obj_del(*target);
        *target = NULL;
    }
}
```

- [ ] **Paso 3: Compilar**

```bash
pio run -e main
```

Esperado: `SUCCESS` sin nuevas advertencias.

- [ ] **Paso 4: Commit**

```bash
git add Display_HMI/src/ui_helpers.c
git commit -m "fix(hmi): _ui_screen_delete — correct inverted NULL check and pointer assignment"
```

---

### Tarea 3: Eliminar la llamada duplicada a `_ui_screen_change` en `ui_event_Settings`

**Objetivo:** `ui_event_Settings` llama `_ui_screen_change(..., &ui_ScreenSettings_screen_init)` directamente, y después llama `Settings_cb(e)` que lo vuelve a llamar. Tras corregir Tarea 2, esta doble llamada provocaría que `ui_ScreenSettings_screen_init` se ejecute dos veces en el segundo acceso a Settings (cuando el puntero ya se habría nulificado correctamente). La primera llamada del par ya está dentro de `Settings_cb`, que también invoca `reset_alarm_detail_state()`.

**Archivos:**
- Modificar: `Display_HMI/src/ElementsCreation.cpp:381-389`

- [ ] **Paso 1: Leer el contexto**

```bash
grep -n "ui_event_Settings\|Settings_cb\|_ui_screen_change.*Settings" Display_HMI/src/ElementsCreation.cpp | head -20
```

- [ ] **Paso 2: Eliminar la llamada directa redundante**

El bloque actual en `ui_event_Settings` (≈líneas 381–389):

```cpp
// ANTES:
void ui_event_Settings(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    _ui_screen_change(&ui_ScreenSettings, LV_SCR_LOAD_ANIM_FADE_ON,
                      ANIM_TIME_MS, 0, &ui_ScreenSettings_screen_init);  // ← ELIMINAR
    _ui_screen_delete(&ui_ScreenMain);
    Settings_cb(e);          // ← ya hace _ui_screen_change internamente
    hmi_msg.shouldSendData = true;
  }
}

// DESPUÉS:
void ui_event_Settings(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    _ui_screen_delete(&ui_ScreenMain);
    Settings_cb(e);
    hmi_msg.shouldSendData = true;
  }
}
```

- [ ] **Paso 3: Verificar que `Settings_cb` realiza `_ui_screen_change` correctamente**

```bash
grep -n "void Settings_cb" Display_HMI/src/UITask.cpp
grep -A5 "void Settings_cb" Display_HMI/src/UITask.cpp
```

Esperado: muestra que `Settings_cb` llama `reset_alarm_detail_state()` y después `_ui_screen_change`.

- [ ] **Paso 4: Compilar**

```bash
pio run -e main
```

Esperado: `SUCCESS`.

- [ ] **Paso 5: Commit**

```bash
git add Display_HMI/src/ElementsCreation.cpp
git commit -m "fix(hmi): remove duplicate _ui_screen_change call in ui_event_Settings"
```

---

### Tarea 4: NULL guards en `ui_ScreenSettings_screen_init`

**Objetivo:** Si algún `lv_obj_create` falla (OOM, parent inválido), la función debe loguear el error y retornar en lugar de desreferenciar el puntero nulo. En una incubadora neonatal, un crash no es aceptable; una pantalla parcialmente inicializada con un mensaje de error sí lo es.

**Archivos:**
- Modificar: `Display_HMI/src/ElementsCreation.cpp`

- [ ] **Paso 1: Confirmar líneas exactas de los puntos críticos**

```bash
grep -n "ui_ScreenSettings\s*=\|ui_WifiConfigCont\s*=\|ui_WifiDisconnectButton\s*=" \
     Display_HMI/src/ElementsCreation.cpp
```

Esperado output (aproximado):
```
2264:  ui_ScreenSettings = lv_obj_create(NULL);
2539:  ui_WifiConfigCont = lv_obj_create(ui_ScreenSettings);
2647:  ui_WifiDisconnectButton = lv_btn_create(ui_WifiConfigCont);
```

- [ ] **Paso 2: Añadir macro de guard al inicio de la función**

Justo antes de la línea `void ui_ScreenSettings_screen_init(void) {` (≈línea 2263), añadir la macro:

```cpp
// Guard macro — si un alloc LVGL falla, loguea y aborta la inicialización
// en lugar de desreferenciar NULL y crashear.
#define LVGL_INIT_GUARD(ptr, name)                                           \
    do {                                                                     \
        if (!(ptr)) {                                                        \
            ESP_LOGE("UI_INIT", "LVGL OOM: " name " == NULL — partial init");\
            return;                                                          \
        }                                                                    \
    } while (0)
```

Añadirla cerca del inicio del archivo (después de los includes, antes de la primera función) para que esté disponible globalmente si es necesario, o justo antes de `ui_ScreenSettings_screen_init`.

- [ ] **Paso 3: Añadir guard en `ui_ScreenSettings` (línea ~2265)**

```cpp
// ANTES:
  ui_ScreenSettings = lv_obj_create(NULL);
  lv_obj_clear_flag(ui_ScreenSettings, LV_OBJ_FLAG_SCROLLABLE);

// DESPUÉS:
  ui_ScreenSettings = lv_obj_create(NULL);
  LVGL_INIT_GUARD(ui_ScreenSettings, "ui_ScreenSettings");
  lv_obj_clear_flag(ui_ScreenSettings, LV_OBJ_FLAG_SCROLLABLE);
```

- [ ] **Paso 4: Añadir guard en `ui_WifiConfigCont` (línea ~2540)**

```cpp
// ANTES:
  ui_WifiConfigCont = lv_obj_create(ui_ScreenSettings);
  lv_obj_remove_style_all(ui_WifiConfigCont);

// DESPUÉS:
  ui_WifiConfigCont = lv_obj_create(ui_ScreenSettings);
  LVGL_INIT_GUARD(ui_WifiConfigCont, "ui_WifiConfigCont");
  lv_obj_remove_style_all(ui_WifiConfigCont);
```

- [ ] **Paso 5: Añadir guard en `ui_WifiDisconnectButton` — el punto de crash exacto (línea ~2648)**

```cpp
// ANTES:
  ui_WifiDisconnectButton = lv_btn_create(ui_WifiConfigCont);
  lv_obj_set_width(ui_WifiDisconnectButton, 130);

// DESPUÉS:
  ui_WifiDisconnectButton = lv_btn_create(ui_WifiConfigCont);
  LVGL_INIT_GUARD(ui_WifiDisconnectButton, "ui_WifiDisconnectButton");
  lv_obj_set_width(ui_WifiDisconnectButton, 130);
```

- [ ] **Paso 6: Compilar**

```bash
pio run -e main
```

Esperado: `SUCCESS`.

- [ ] **Paso 7: Commit**

```bash
git add Display_HMI/src/ElementsCreation.cpp
git commit -m "fix(hmi): add NULL guards in ui_ScreenSettings_screen_init to prevent crash on LVGL OOM"
```

---

### Tarea 4b: Test de validación de Fase 1

**Objetivo:** Verificar que los cuatro fixes de la Fase 1 funcionan antes de continuar.

- [ ] **Paso 1: Compilar build completo limpio**

```bash
pio run -e main --target clean && pio run -e main
```

Esperado: `SUCCESS`, sin warnings nuevos.

- [ ] **Paso 2: Flashear y observar boot**

```bash
pio run -e main --target upload
pio device monitor --baud 115200
```

Esperado en los primeros 10 segundos del log:
```
[DIAG] HMI bootCount=N lastRst=...
BOOT heap: internal=... SPIRAM=...
UI Task Started
```
No debe aparecer ningún `Backtrace:` ni `GURU MEDITATION`.

- [ ] **Paso 3: Prueba de navegación repetida (stress manual)**

En el dispositivo físico:
1. Navegar a pantalla Settings → presionar Back → repetir 20 veces
2. Observar el monitor UART en busca de cualquier `E (...)` error o crash
3. Verificar que la pantalla Settings se muestra correctamente en cada visita

Esperado: sin crash, sin errores LVGL en consola.

- [ ] **Paso 4: Soak test de 8 horas**

Dejar el dispositivo encendido 8 horas con comunicación UART activa desde el motherboard. Monitorear con:

```bash
pio device monitor --baud 115200 2>&1 | tee /tmp/hmi_soak_8h.log
```

Al final verificar:
```bash
grep -i "backtrace\|guru meditation\|abort\|panic" /tmp/hmi_soak_8h.log
```

Esperado: sin resultados.

---

## FASE 2 — Eliminación arquitectural (CommTask sin LVGL)

> Esta es la corrección del **root cause estructural**: el CommTask nunca debe llamar funciones LVGL. Todo lo visual debe ejecutarse en el UITask. La Fase 1 da tiempo para hacerlo con calma y con el dispositivo funcionando.

---

### Tarea 5: Mover todas las llamadas LVGL del CommTask al UITask

**Objetivo:** CommTask solo debe (a) parsear UART, (b) actualizar structs compartidos, (c) activar flags booleanos. UITask lee esos flags en su loop y ejecuta los cambios de UI. Esto elimina definitivamente el riesgo de stack overflow en CommTask relacionado con LVGL.

**Archivos:**
- Modificar: `Display_HMI/src/CommTask.cpp`
- Modificar: `Display_HMI/include/CommTask.h`
- Modificar: `Display_HMI/src/UITask.cpp`

#### 5a: Añadir flag `g_pendingTelemetryApply` en CommTask.h

- [ ] **Paso 1: Añadir declaración extern en CommTask.h** (después de la declaración de `g_skinProbeState`, ≈línea 129):

```cpp
// Flag para que UITask ejecute la actualización LVGL de telemetría
// (CommTask solo actualiza valores numéricos; UITask hace los lv_label_set_text)
extern volatile bool g_pendingTelemetryApply;
```

#### 5b: Eliminar llamadas LVGL de `applyHMIData` en CommTask.cpp

- [ ] **Paso 2: Añadir definición del flag** al inicio de `CommTask.cpp` (junto con las demás variables globales, ≈línea 27):

```cpp
volatile bool g_pendingTelemetryApply = false;
```

- [ ] **Paso 3: Reescribir `applyHMIData`** (≈líneas 427–442) para eliminar las llamadas LVGL:

```cpp
// ANTES:
static void applyHMIData() {
  if (!g_ui_initialized)
    return;
  airTempValueDetected = ctrl_tel_msg.detectedAirTemperature;
  skinTempValueDetected = ctrl_tel_msg.detectedSkinTemperature;
  humValueDetected = (int)ctrl_tel_msg.detectedHumidity;
  LVGL_Lock();
  update_labels();
  if (tempSwitched) {
    chart_add_air_temp((float)airTempValueDetected);
    chart_add_skin_temp((float)skinTempValueDetected);
  }
  chart_add_hum_value((float)humValueDetected);
  chart_save_history();
  LVGL_Unlock();
}

// DESPUÉS:
static void applyHMIData() {
  if (!g_ui_initialized)
    return;
  // Solo actualiza valores numéricos compartidos — UITask hace el render
  airTempValueDetected   = ctrl_tel_msg.detectedAirTemperature;
  skinTempValueDetected  = ctrl_tel_msg.detectedSkinTemperature;
  humValueDetected       = (int)ctrl_tel_msg.detectedHumidity;
  g_pendingTelemetryApply = true;
}
```

#### 5c: Mover `Display_ApplyCtrlState` del CommTask al UITask

- [ ] **Paso 4: Hacer `Display_ApplyCtrlState` accesible desde UITask**

En `CommTask.h`, añadir la declaración pública (después de las funciones existentes, ≈línea 138):

```cpp
// Llamada desde UITask cuando ctrl_state_msg.newState == true
bool Display_ApplyCtrlState(const ControlBoard_Message_State &st);
```

- [ ] **Paso 5: Simplificar `Display_StateSync_Service` en CommTask.cpp** (≈líneas 410–425) para NO llamar `Display_ApplyCtrlState`:

```cpp
// ANTES:
static void Display_StateSync_Service(void) {
  if (g_stateSynced)
    return;
  uint32_t now = millis();
  if (now - g_lastStateReqMs >= COMM_STATE_SYNC_MS) {
    Communication_RequestState();
    g_lastStateReqMs = now;
  }
  if (ctrl_state_msg.newState) {
    if (Display_ApplyCtrlState(ctrl_state_msg)) {
      ctrl_state_msg.newState = false;
      g_stateSynced = true;
      hmi_msg.shouldSendData = true;
    }
  }
}

// DESPUÉS:
static void Display_StateSync_Service(void) {
  if (g_stateSynced)
    return;
  uint32_t now = millis();
  if (now - g_lastStateReqMs >= COMM_STATE_SYNC_MS) {
    Communication_RequestState();
    g_lastStateReqMs = now;
  }
  // Display_ApplyCtrlState se ejecuta en UITask cuando ctrl_state_msg.newState == true
}
```

- [ ] **Paso 6: Quitar `static` de `Display_ApplyCtrlState`** en CommTask.cpp (≈línea 280):

```cpp
// ANTES:
static bool Display_ApplyCtrlState(const ControlBoard_Message_State &st) {

// DESPUÉS:
bool Display_ApplyCtrlState(const ControlBoard_Message_State &st) {
```

#### 5d: Mover las llamadas LVGL de `AlarmSound_Update` del CommTask

- [ ] **Paso 7: Verificar si `AlarmSound_Update` usa LVGL**

```bash
grep -n "lv_\|LVGL" Display_HMI/src/buzzer.cpp Display_HMI/src/modules/audio/hmi_audio_module.cpp 2>/dev/null
```

Si hay llamadas LVGL: mover las llamadas a `AlarmSound_Update()` del CommTask al UITask (junto con `update_alarm_panels()`).

Si no hay llamadas LVGL: puede permanecer en CommTask.

- [ ] **Paso 8: Si `AlarmSound_Update` tiene LVGL — mover a UITask**

En `CommTask.cpp`, en `processReceivedAlarm` (≈línea 481) y en la sección de bitmask sync (≈línea 400):

```cpp
// ANTES (en processReceivedAlarm):
  g_pendingAlarmUpdate = true;
  AlarmSound_Update();

// DESPUÉS:
  g_pendingAlarmUpdate = true;
  // AlarmSound_Update() se llama desde UITask junto con update_alarm_panels()
```

En `CommTask.cpp`, en la sección de bitmask sync (≈línea 400):

```cpp
// ANTES:
      if (changed) {
          g_pendingAlarmUpdate = true;
          AlarmSound_Update();
      }

// DESPUÉS:
      if (changed) {
          g_pendingAlarmUpdate = true;
          // AlarmSound_Update() ejecutado por UITask
      }
```

#### 5e: Añadir los handlers en el UITask main loop

- [ ] **Paso 9: Añadir include de CommTask.h en UITask.cpp** si no existe:

```bash
grep "CommTask.h" Display_HMI/src/UITask.cpp
```

Si no está: añadir al bloque de includes al inicio de `UITask.cpp`:
```cpp
#include "CommTask.h"
```

- [ ] **Paso 10: Añadir handlers en el UITask main loop**

En `UITask.cpp`, dentro del loop principal (≈línea 3720, junto al bloque de `g_pendingAlarmUpdate`), añadir los dos nuevos bloques. El loop completo del área a modificar debe quedar:

```cpp
    // --- State sync: aplica estado recibido del motherboard (antes en CommTask) ---
    if (ctrl_state_msg.newState) {
      if (Display_ApplyCtrlState(ctrl_state_msg)) {
        ctrl_state_msg.newState = false;
        g_stateSynced = true;
        hmi_msg.shouldSendData = true;
      }
    }

    // --- Telemetry: actualiza labels y charts (antes en CommTask) ---
    if (g_pendingTelemetryApply) {
      g_pendingTelemetryApply = false;
      update_labels();
      if (tempSwitched) {
        chart_add_air_temp((float)airTempValueDetected);
        chart_add_skin_temp((float)skinTempValueDetected);
      }
      chart_add_hum_value((float)humValueDetected);
      chart_save_history();
    }

    // --- Alarms: ya existía, solo añadir AlarmSound_Update si se movió ---
    if (g_pendingAlarmUpdate) {
      update_alarm_panels();
      AlarmSound_Update();   // añadir solo si se eliminó de CommTask en paso 8
      g_pendingAlarmUpdate = false;
    }
```

**Nota:** Los tres bloques anteriores deben estar dentro del bloque `LVGL_Lock()` / `LVGL_Unlock()` del loop, ya que acceden a objetos LVGL.

- [ ] **Paso 11: Compilar**

```bash
pio run -e main
```

Esperado: `SUCCESS`. Si hay errores por símbolos no encontrados, verificar que:
- `Display_ApplyCtrlState` tiene declaración pública en CommTask.h
- `g_pendingTelemetryApply` tiene `extern volatile bool` en CommTask.h
- CommTask.h está incluido en UITask.cpp

- [ ] **Paso 12: Commit**

```bash
git add Display_HMI/src/CommTask.cpp Display_HMI/include/CommTask.h Display_HMI/src/UITask.cpp
git commit -m "refactor(hmi): move all LVGL calls from CommTask to UITask — CommTask now flag-only"
```

#### 5f: Validación de Fase 2

- [ ] **Paso 13: Verificar que CommTask ya no llama LVGL**

```bash
grep -n "lv_\|LVGL_Lock\|LVGL_Unlock" Display_HMI/src/CommTask.cpp
```

Esperado: cero resultados (o solo comentarios).

- [ ] **Paso 14: Soak test de 24 horas**

```bash
pio run -e main --target upload
pio device monitor --baud 115200 2>&1 | tee /tmp/hmi_soak_24h.log
```

Ejercitar periódicamente la navegación entre pantallas mientras el log corre. Al final:

```bash
grep -ic "backtrace\|panic\|guru\|abort\|stack overflow\|OOM\|INIT ERROR" /tmp/hmi_soak_24h.log
```

Esperado: `0`.

---

## FASE 3 — Observabilidad (operación durante semanas)

> Sin visibilidad de tendencias de memoria y stack, no se puede detectar una regresión antes de que cause un crash. Esta fase añade diagnósticos permanentes que se emiten al log UART.

---

### Tarea 6: High-water marks de heap y stack

**Objetivo:** Emitir cada 60 segundos los niveles mínimos de heap libre y los high-water marks de stack de las tareas críticas. Si algún valor cae por debajo de un umbral, emitir una alerta antes de que cause un crash.

**Archivos:**
- Modificar: `Display_HMI/src/UITask.cpp`
- Modificar: `Display_HMI/src/CommTask.cpp`

#### 6a: Exponer handle del CommTask

- [ ] **Paso 1: Añadir variable para el task handle en CommTask.cpp** (junto con las globales del archivo, ≈línea 27):

```cpp
static TaskHandle_t s_comm_task_handle = NULL;
```

- [ ] **Paso 2: Capturar el handle en `CreateCommTask`** (≈línea 516):

```cpp
// ANTES:
void CreateCommTask() {
  xTaskCreatePinnedToCore(Comm_Task, "Comm", COMM_TASK_STACK_SIZE, NULL,
                          COMM_TASK_PRIORITY, NULL, CORE_ID_FREERTOS);
}

// DESPUÉS:
void CreateCommTask() {
  xTaskCreatePinnedToCore(Comm_Task, "Comm", COMM_TASK_STACK_SIZE, NULL,
                          COMM_TASK_PRIORITY, &s_comm_task_handle, CORE_ID_FREERTOS);
}
```

- [ ] **Paso 3: Declarar en CommTask.h** (junto a `CreateCommTask`, ≈línea 135):

```cpp
TaskHandle_t CommTask_GetHandle(void);
```

- [ ] **Paso 4: Implementar getter en CommTask.cpp** (al final del archivo):

```cpp
TaskHandle_t CommTask_GetHandle(void) { return s_comm_task_handle; }
```

#### 6b: Añadir diagnóstico periódico en UITask

- [ ] **Paso 5: Añadir bloque de diagnóstico en el UITask loop** en `UITask.cpp`

En el area de diagnósticos del loop (junto a `lcd_diagnostics_log`, ≈línea 3583), añadir después del bloque de `lcd_diag`:

```cpp
    // --- Diagnóstico de memoria y stack (cada 60 s) ---
    // Umbrales: heap interno < 20 KB o stack HWM < 2 KB son señales de alerta
    {
      static uint32_t diag_last_ms = 0;
      uint32_t now_diag = xTaskGetTickCount() * portTICK_PERIOD_MS;
      if (now_diag - diag_last_ms >= 60000) {
        diag_last_ms = now_diag;

        uint32_t heap_int  = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        uint32_t heap_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        UBaseType_t ui_hwm   = uxTaskGetStackHighWaterMark(NULL);
        TaskHandle_t comm_h  = CommTask_GetHandle();
        UBaseType_t comm_hwm = comm_h ? uxTaskGetStackHighWaterMark(comm_h) : 0;

        if (heap_int < 20480 || ui_hwm < 512 || comm_hwm < 512) {
          ESP_LOGE("DIAG", "⚠ LOW RESOURCES heap_int=%lu heap_psram=%lu ui_hwm=%u comm_hwm=%u",
                   (unsigned long)heap_int, (unsigned long)heap_psram,
                   (unsigned)ui_hwm * sizeof(StackType_t),
                   (unsigned)comm_hwm * sizeof(StackType_t));
        } else {
          ESP_LOGW("DIAG", "heap_int=%lu heap_psram=%lu ui_hwm=%u B comm_hwm=%u B",
                   (unsigned long)heap_int, (unsigned long)heap_psram,
                   (unsigned)ui_hwm * sizeof(StackType_t),
                   (unsigned)comm_hwm * sizeof(StackType_t));
        }
      }
    }
```

- [ ] **Paso 6: Añadir include de CommTask.h en UITask.cpp** si aún no está (ya cubierto en Tarea 5 paso 9).

- [ ] **Paso 7: Compilar**

```bash
pio run -e main
```

Esperado: `SUCCESS`.

- [ ] **Paso 8: Verificar output de diagnóstico en log**

Flashear y esperar 60 segundos. Buscar en log:

```bash
pio device monitor --baud 115200 | grep DIAG
```

Esperado: una línea con los valores de heap y stack (sin `⚠ LOW RESOURCES`).

- [ ] **Paso 9: Commit**

```bash
git add Display_HMI/src/CommTask.cpp Display_HMI/include/CommTask.h Display_HMI/src/UITask.cpp
git commit -m "feat(hmi): add heap/stack high-water mark diagnostics every 60s"
```

---

### Tarea 7: Stack overflow hook de FreeRTOS

**Objetivo:** Si alguna vez un task desborda su stack (independientemente del tamaño), el hook loguea el nombre del task y hace `esp_restart()` en lugar de corrompir memoria silenciosamente. En una incubadora, un reboot controlado es preferible a una corrupción silenciosa.

**Archivos:**
- Modificar: `Display_HMI/src/main.cpp`

- [ ] **Paso 1: Añadir el hook en `main.cpp`** (después de los includes, antes de `setup()`):

```cpp
// FreeRTOS stack overflow detection hook.
// Requiere configCHECK_FOR_STACK_OVERFLOW >= 1 en FreeRTOS config (activo por
// defecto en ESP-IDF). Si un task desborda su stack, este hook loguea el nombre
// y hace restart limpio antes de corromper memoria adyacente.
extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask,
                                               char *pcTaskName) {
  (void)xTask;
  // Solo operaciones seguras: uart directo, sin heap, sin LVGL
  ESP_EARLY_LOGE("STACK", "OVERFLOW in task '%s' — restarting", pcTaskName);
  esp_restart();
}
```

- [ ] **Paso 2: Compilar**

```bash
pio run -e main
```

Esperado: `SUCCESS`. Si el compilador advierte sobre redefinición del símbolo, es porque ESP-IDF ya define una versión por defecto — en ese caso usar `__attribute__((weak))` no es la solución; verificar que el proyecto no incluya otro hook en otra unidad de compilación.

- [ ] **Paso 3: Commit**

```bash
git add Display_HMI/src/main.cpp
git commit -m "feat(hmi): add FreeRTOS stack overflow hook — log task name and restart cleanly"
```

---

### Tarea 8: Soak test de semana de operación

**Objetivo:** Validación final de que el sistema puede operar durante el tiempo real de uso clínico.

- [ ] **Paso 1: Build de producción limpio**

```bash
pio run -e main --target clean && pio run -e main
```

Verificar que no hay `-D CRASH_TEST_HMI` activo:

```bash
grep "CRASH_TEST_HMI" Display_HMI/platformio.ini
```

La línea debe estar comentada en el env `main`.

- [ ] **Paso 2: Flashear y capturar log continuo durante 72 horas**

```bash
pio run -e main --target upload
pio device monitor --baud 115200 2>&1 | tee /tmp/hmi_soak_72h.log &
```

Durante ese tiempo, ejercitar el dispositivo cada pocas horas:
- Navegar Settings → Back → Settings (repetir 10 veces)
- Activar/desactivar alarms desde el motherboard
- Activar WiFi OTA si es posible

- [ ] **Paso 3: Analizar el log al final**

```bash
# Crashes o panics
grep -c "Backtrace\|GURU MEDITATION\|abort\|panic" /tmp/hmi_soak_72h.log

# Alertas de recursos bajos
grep "LOW RESOURCES" /tmp/hmi_soak_72h.log

# Tendencia de heap (comparar primeros vs últimos valores)
grep "DIAG" /tmp/hmi_soak_72h.log | head -5
grep "DIAG" /tmp/hmi_soak_72h.log | tail -5
```

Criterios de éxito:
- `0` crashes
- Sin `LOW RESOURCES` alerts
- El heap libre no decrece significativamente entre el primer y último log de DIAG (leak < 1 KB/hora)

---

## Resumen de archivos modificados

| Fase | Archivo | Cambio |
|------|---------|--------|
| 1 | `Display_HMI/include/main.h` | `COMM_TASK_STACK_SIZE` 8192 → 16384 |
| 1 | `Display_HMI/src/ui_helpers.c` | Fix `_ui_screen_delete` (condición + puntero) |
| 1 | `Display_HMI/src/ElementsCreation.cpp` | Eliminar `_ui_screen_change` duplicado en `ui_event_Settings` |
| 1 | `Display_HMI/src/ElementsCreation.cpp` | `LVGL_INIT_GUARD` macro + 3 guards en `ui_ScreenSettings_screen_init` |
| 2 | `Display_HMI/src/CommTask.cpp` | Eliminar todas las llamadas LVGL; añadir flags |
| 2 | `Display_HMI/include/CommTask.h` | Declarar `g_pendingTelemetryApply` y `Display_ApplyCtrlState` públicos |
| 2 | `Display_HMI/src/UITask.cpp` | Handlers para `ctrl_state_msg.newState` y `g_pendingTelemetryApply` |
| 3 | `Display_HMI/src/CommTask.cpp` | Handle del task + getter |
| 3 | `Display_HMI/include/CommTask.h` | Declarar `CommTask_GetHandle` |
| 3 | `Display_HMI/src/UITask.cpp` | Diagnóstico cada 60s |
| 3 | `Display_HMI/src/main.cpp` | Stack overflow hook |

## Criterios de aceptación finales

1. `pio run -e main` compila sin errores ni warnings nuevos
2. La secuencia "Settings → Back" ejecutada 50 veces no produce crash
3. 72 horas de operación continua sin crash ni `LOW RESOURCES`
4. El log DIAG muestra heap libre > 20 KB y HWMs de stack > 2 KB en todo momento
5. `grep "LVGL_Lock\|lv_" Display_HMI/src/CommTask.cpp` devuelve cero hits de código activo
