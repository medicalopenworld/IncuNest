## Why

Afecta solo a **Display_HMI**. No toca `shared/` ni `motherBoard`, y no cambia
ningún mensaje del protocolo serie (`PROTOCOL.md` queda igual).

La vista de tendencia (`TelemetryHistory`: aire, piel y humedad de las
últimas horas) existe y se mantiene al día en segundo plano, pero hoy **no
sirve a nadie**, por tres motivos que se han comprobado en el código:

1. **No tiene punto de entrada.** `TelemetryHistory_Open()` no la llama
   nadie: el botón TREND de la pantalla de bloqueo se retiró a petición para
   la tanda de fabricación del 2026-09-17 (`8b62b23b`) y el botón de gráficos
   de Main (`ui_ChartButton`) lleva años oculto y, además, abre la pantalla
   antigua `ui_ScreenCharts`, no esta vista.
2. **Solo muestra medidas.** El operador ve que la temperatura se fue, pero
   no *qué alarma saltó* ni *cuál era la consigna* en ese momento: para
   correlacionar tiene que salir de la pantalla y abrir el centro de
   alarmas.
3. **Muere con el reinicio.** El buffer de 4 h vive en RAM: un corte de red
   eléctrica o un reinicio por watchdog lo borra justo cuando más falta
   hace entender qué pasó. Y 4 h no cubren una guardia completa.

## What Changes

**A. Marcadores de alarma y consigna en la tendencia**

- Sobre los tres gráficos (aire, piel, humedad): cada **evento de alarma**
  (alta = sale, baja = se resuelve) como una marca vertical, y la **consigna**
  como una línea propia del gráfico, histórica (se guarda como serie
  paralela), no solo el valor actual.
- Los eventos se sellan en el flanco de `CTRL,ALM` (`processReceivedAlarm`),
  con prioridad y sentido, en un anillo pequeño de eventos.
- **Punto de entrada**: un botón "TENDENCIA" en la cabecera del centro de
  alarmas (`AlarmCenter`), que se alcanza con un toque desde el icono de
  alarmas del heading, desde el check de "todo OK" de Main y desde el icono
  de alarma de la pantalla de bloqueo. Razón y alternativas descartadas en
  `design.md` (§Punto de entrada).

**B. Tendencia persistente y más larga**

- Buffer ampliado a **24 h** (8640 ranuras de 10 s × 6 series, en PSRAM) y
  ventanas **4 h / 12 h / 24 h** (antes 1 h / 2 h / 4 h), con decimación
  min/max por ventana para que LVGL no pinte 8640 puntos.
- **Persistencia en LittleFS**: volcado binario con cabecera y CRC cada
  5 min y al entrar en bloqueo, escritura atómica (temporal + rename) y
  troceada para no frenar el refresco. Al arrancar, restaura alineando por
  epoch; el hueco entre el apagado y el arranque queda como "sin dato".
- Nuevo módulo **`hmi_fs`** (`src/modules/storage/`): monta LittleFS en la
  partición `spiffs` que ya existe (sin tocar la tabla de particiones) y
  ofrece helpers de lectura y escritura atómica reutilizables por el diario
  de eventos que vendrá en otra rama.
- La base de tiempo de las ranuras pasa de `esp_timer` a **epoch** cuando
  hay hora, con una transición limpia (`design.md` §Reloj).
- La lógica pura (anillo, eventos, reloj, decimación, formato de fichero,
  restauración) se extrae a `src/modules/trend/` sin LVGL ni Arduino y se
  cubre con tests Unity en `[env:native]`.

## Capabilities

### New Capabilities

- `hmi-trend`: vista de tendencia de aire/piel/humedad con marcadores de
  alarma y línea de consigna, ventanas de 4/12/24 h, y su persistencia
  sobreviviendo a reinicios y cortes de red.

### Modified Capabilities

(ninguna: `TelemetryHistory` no tenía spec previa.)

## Impact

- **Display_HMI**: `src/ui/TelemetryHistory.cpp` (reescritura del modelo de
  datos sobre los módulos nuevos, ventanas, marcadores), `src/ui/AlarmCenter.cpp`
  (botón de entrada), `src/tasks/CommTask.cpp` (un hook en el flanco de
  alarma), `src/tasks/UITask.cpp` (consigna al registrar la muestra, servicio
  de volcado fuera de `LVGL_Lock`, aviso de entrada en bloqueo),
  `include/ui/i18n_strings.def` (textos), `platformio.ini` (`[env:native]`).
  Ficheros nuevos: `src/modules/trend/*`, `src/modules/storage/hmi_fs.*`,
  `test/test_trend_*`.
- **Memoria**: +~104 KB de PSRAM permanentes (modelo de datos, frente a
  17 KB hoy) y ~104 KB de PSRAM transitorios durante un volcado o la
  restauración. **Cero SRAM interna nueva por diseño**; el montaje de
  LittleFS asigna una cantidad pequeña de heap interno que se mide y se
  reporta en `tasks.md`.
- **Flash**: la partición `spiffs` (≈5,9 MB) pasa a usarse de verdad; el
  volcado cada 5 min cuesta ~27 borrados de bloque, ≈5 ciclos/bloque/día
  con el reparto de desgaste de LittleFS.
- **Sin cambios** de `FWversion`, de la tabla de particiones, de
  `shared/` ni de la motherBoard.
