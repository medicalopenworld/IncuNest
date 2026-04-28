# Problemas Conocidos — Causas Raíz e Inestabilidad

> Fuentes: `Firmware/docs/known_issues.md`, `src/UITask.cpp`, `src/CommTask.cpp`, `include/lv_conf.h`

## 1. Síntomas Reportados

Los tres síntomas principales reportados durante el uso en campo son:
1. **Parpadeos del display** — la pantalla parpadea o se ve artefactos durante operación normal
2. **Reinicios del HMI** — el HMI se reinicia inesperadamente sin acción del usuario
3. **No respuesta al toque** — la pantalla táctil deja de responder durante varios segundos

---

## 2. Análisis por Síntoma

### 2.1 Parpadeos del Display

**Causa raíz principal**: Conflicto de acceso DMA entre el bus RGB y el bus I2S (audio).

El ESP32-S3 tiene un sistema de buses DMA compartido. Cuando el driver de audio I2S activa transferencias DMA intensivas para reproducir MP3s, puede interrumpir el flujo continuo del DMA del bus RGB, causando que algunos píxeles lleguen al panel fuera de sincronía y produciendo parpadeos visuales.

**Evidencia en código** (`include/display_config.h:94`):
```c
// @brief Frecuencia pixel clock (18 MHz).
// Valor del factory code de Elecrow para CrowPanel Advance 7" (V1.2+).
// Si hay jitter con Audio I2S activo, probar bajando a 15 MHz.
#define DISPLAY_FREQ_WRITE 15000000UL  // Reducido de 21→18→15 MHz
```

El comentario en `display_config.h:91` documenta explícitamente el historial: la frecuencia ha bajado de 21 MHz (especificación oficial) a 15 MHz precisamente para reducir la competencia DMA con I2S.

**Causa raíz secundaria**: Tamaño insuficiente del bounce buffer en relación al pixel clock.

A 15 MHz con 800×480 píxeles, el LCD necesita alimentarse a ~29 MB/s. Los bounce buffers de 20 líneas (32 KB cada uno) minimizan pero no eliminan el problema cuando la PSRAM está siendo accedida simultáneamente por otros periféricos.

**Causa raíz terciaria**: `EEPROM.commit()` en el bucle principal durante `lcd_set_freq_write()` (ver `src/UITask.cpp:162`):
```cpp
void lcd_set_freq_write(uint32_t freq_hz) {
    g_currentFreqWrite = freq_hz;
    EEPROM.put(EEPROM_DISPLAY_FREQ, freq_hz);
    EEPROM.commit();   // ← Flash write: puede pausar todos los accesos durante ~ms
    delay(200);        // ← Pausa activa en tarea de alta prioridad
    ESP.restart();
}
```

**Solución en ESP-IDF nativo**: Usar el driver I2S nativo del IDF con configuración de prioridad DMA explícita, separando el bus I2S del DMA del RGB mediante asignación a diferentes buses DMA (`gdma_new_ahb_channel` con distintos canales).

---

### 2.2 Reinicios Inesperados del HMI

**Causa raíz 1 — Watchdog por `while(1)` en assert handler**:

En `include/lv_conf.h:271`:
```c
#define LV_ASSERT_HANDLER while(1);  // Halt by default
```

Cuando LVGL dispara un assert (por ejemplo, al intentar acceder a un objeto nulo), el firmware entra en un bucle infinito que dispara el **Task Watchdog Timer** (TWDT) del ESP32-S3. El TWDT causa un reset del sistema. Esto es silencioso — no produce un backtrace identificable en los logs.

**Causa raíz 2 — CommTask modifica objetos LVGL sin mutex adecuado**:

`CommTask` (Prio 3) llama a funciones LVGL dentro de `Display_ApplyCtrlState()` (`src/CommTask.cpp:259`) usando `LVGL_Lock()`. Sin embargo, el mutex es **recursivo** y `CommTask` tiene **mayor prioridad que UITask** (Prio 3 > Prio 2).

```cpp
// src/CommTask.cpp:413
LVGL_Lock();
ui_set_switch_state_silent(ui_Switch1, st.actuation & 0x01);
lv_obj_clear_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);
// ... múltiples llamadas LVGL
LVGL_Unlock();
```

Si `CommTask` adquiere el mutex y es preemptado por la tarea OTA (Prio 4), y OTA intenta también un `LVGL_Lock()`, el sistema puede entrar en una inversión de prioridad que cause un timeout del watchdog.

**Causa raíz 3 — `volatile` variables sin sincronización completa**:

```cpp
// src/CommTask.h:122
extern volatile bool g_pwrOffActive;
extern volatile int  g_pwrOffRemainingMs;
```

El tipo `volatile` en C++ no garantiza atomicidad en operaciones de múltiples bytes en CPUs multi-core. Un acceso a `g_pwrOffRemainingMs` (int, 4 bytes) desde Core 1 (CommTask) puede ser interrumpido por UITask (también Core 1) entre los accesos a los bytes. Aunque ambas tareas corren en el mismo Core, FreeRTOS puede hacer preemption entre ellas.

**Causa raíz 4 — Fallo de inicialización del touch sin manejo**:

Si el GT911 no responde tras 3 reintentos (`src/UITask.cpp:2899`), el código continúa sin abortar:
```cpp
if (!touch_ok) {
    ESP_LOGE(TAG, "Touch controller FAILED to init after retries.");
    // No hay manejo de error — el sistema continúa sin touch
}
```
Esto no causa reinicios directamente, pero el touch deja de funcionar y el usuario no puede interactuar, lo que puede llevar a timeouts de watchdog si algún proceso espera input.

---

### 2.3 No Respuesta al Toque

**Causa raíz 1 — CommTask (Prio 3) bloquea UITask (Prio 2) con el mutex LVGL**:

Cuando llega una ráfaga de mensajes UART (por ejemplo, `CTRL,STATE` + `CTRL,TEL` + múltiples `CTRL,ALM` en rápida sucesión), CommTask puede mantener el mutex LVGL durante 10-50 ms mientras aplica el estado. Durante ese tiempo, el `lv_timer_handler()` en UITask no puede ejecutarse, lo que impide el procesamiento de los eventos de toque.

La cadena de llamadas en CommTask que tarda más:
1. `applyHMIData()` → `LVGL_Lock()` → `update_labels()` (múltiples `lv_label_set_text`)
2. `processReceivedAlarm()` → `update_alarm_panels()` (reconstruir paneles de alarma)
3. `Display_ApplyCtrlState()` → `UI_SyncAll()` (sync completo de toda la UI)

**Causa raíz 2 — Polling del touch a 30ms sin interrupción**:

El GT911 se usa en modo polling (`LV_INDEV_DEF_READ_PERIOD = 30ms`) en lugar de modo interrupción. Esto significa que el touch solo se lee cada 30ms. Si CommTask mantiene el mutex durante 30ms, se pierde una lectura completa de touch, haciendo el sistema sentirse "laggy".

**Causa raíz 3 — Draw buffer insuficiente (24 KB)**:

Con `COLOR_DIVISOR = 16`, el draw buffer es 800×480/16 = 24000 px = 48 KB. Esto significa que LVGL necesita hacer **muchos más flushes** para actualizar una pantalla completa (hasta 32 flush calls para un repintado completo). Cada flush llama a `esp_lcd_panel_draw_bitmap()`, que bloquea hasta que el DMA transfiere los datos. Esto aumenta el tiempo total en el que UITask está ocupado en flush, dejando menos tiempo para procesar el toque.

---

## 3. Problemas Secundarios Identificados

### 3.1 Inconsistencia de Pines entre `display_config.h` y `main.h`

**Archivo**: `include/main.h:88-91` vs `include/display_config.h:51-54`

Los valores de `PIN_HENABLE`, `PIN_VSYNC`, `PIN_HSYNC`, `PIN_PCLK` en `main.h` están **invertidos** respecto a los correctos en `display_config.h`. Si se compila con los valores de `main.h`, el display no funcionará.

### 3.2 Alarma "Fantasma" — Resuelto parcialmente

**Descripción**: Antes de implementar el alarmBitmask, las alarmas podían quedar "pintadas" en pantalla aunque ya estuvieran extinguidas en la Motherboard, si el paquete `CTRL,ALM,id,txt,0` se perdía.

**Solución implementada**: `Display_ApplyCtrlState()` en `src/CommTask.cpp:364` limpia visualmente las alarmas no presentes en el bitmask recibido.

**Riesgo residual**: Si el bitmask llega como `(uint32_t)-1` (campo no presente en versiones antiguas del protocolo), la limpieza no se ejecuta. Ver `src/CommTask.cpp:154`:
```cpp
else ctrl_state_msg.alarmBitmask = (uint32_t)-1; // Valor nulo si no viene
```

### 3.3 PCA9557 No Encontrado en Scan I2C

**Archivo**: `src/UITask.cpp:2888`
```cpp
// Note: PCA9557 at 0x18 was not found in scan.
// Touch reset is likely handled by STC8 (0x30) or already high.
```

El expansor I/O PCA9557 (librería `PCA9557-arduino`) está declarado como dependencia pero **no se encontró en el bus I2C** en hardware v1.3/v1.4. Si el touch reset no se ejecuta correctamente, el GT911 puede quedar en estado indefinido tras un power-on no limpio, causando que el touch no responda.

### 3.4 AudioManager Deshabilitado

```cpp
// src/main.cpp:2
// #include "AudioManager.h"  // Deshabilitado para migración Arduino 3.x
```
```ini
# platformio.ini:26
build_src_filter = +<*> -<AudioManager.cpp>
```

El gestor de audio está completamente excluido del build. Las alarmas auditivas se gestionan a través del STC8H1K28 (buzzer de la placa) mediante comandos I2C. El speaker I2S que se indica en la arquitectura no está activo.

---

## 4. Por qué la Migración a ESP-IDF Nativo Mejora la Estabilidad

| Problema | Causa con Arduino | Solución en ESP-IDF nativo |
|---|---|---|
| Parpadeos DMA | Competencia entre I2S y RGB DMA en capa Arduino | Asignación explícita de canales GDMA independientes |
| Reinicios por WDT | `while(1)` en LV_ASSERT_HANDLER dispara TWDT | Implementar assert handler con `esp_restart()` y log |
| Bloqueo de touch | CommTask (Prio 3) > UITask (Prio 2) comparten mutex | Usar colas de mensajes (IPC queue) en lugar de mutex LVGL directo entre tareas |
| Pixel clock inestable | Capa Arduino limita configuración DMA | Control directo `esp_lcd_rgb_panel_config_t` con mejor timing |
| EEPROM write bloquea DMA | `EEPROM.commit()` con flash write en task crítica | NVS en background task separada |
| Draw buffer insuficiente | `COLOR_DIVISOR=16` por limitación de SRAM Arduino | Con IDF: configurar PSRAM para draw buffer (`MALLOC_CAP_SPIRAM`) |
