Todo el cambio es de **Display_HMI**. Los módulos `src/modules/trend/*` son
lógica pura y se cubren con tests Unity en `[env:native]` (`pio test -e
native` en `Firmware/Display_HMI`). El resto (LVGL, LittleFS, `UITask`,
`CommTask`, `AlarmCenter`) **no tiene entorno de test**: se verifica con
`pio run -e main` y con prueba manual en el CrowPanel real; las tareas de
banco están marcadas **(manual)** y ningún checkbox reclama cobertura
automatizada para ellas. **Las tareas manuales 1.8 y 2.7 NO se han ejecutado
todavía** (no había hardware en esta sesión): quedan abiertas a propósito.

Medidas de `pio run -e main` (Display_HMI, RAM de la app 327 680 B, flash de
app 5 242 880 B):

| Estado | RAM usada | Flash usada |
|---|---|---|
| Base (`994d7f65`, dev) | 129 428 B | 2 699 364 B (51,5 %) |
| Tras el bloque A | 130 764 B | 2 701 928 B |
| Tras el bloque B + refactor | 129 956 B (+528 B) | 2 750 040 B (+50 676 B, 52,5 %) |

Los +528 B de `.bss` son la cola de flancos (256 B) y el estado del nuevo
código (volcado, `hmi_fs`, estáticos de LittleFS). El modelo de 104,8 KB está
entero en PSRAM. Los +50,7 KB de flash son sobre todo la librería LittleFS.

## 1. Display_HMI — bloque A: marcadores de alarma y consigna

Commits: `test(hmi): failing specs for trend markers` y
`feat(hmi): marcadores de alarma y consigna en la tendencia`.

- [x] 1.1 Tests en rojo: `test/test_trend_store` (anillo de 6 series int16,
      huecos, duplicados, wrap, shift), `test/test_trend_events` (anillo de
      eventos), `test/test_trend_policy` (validez de consigna, mapeo
      slot->px de marcadores). Verificado en rojo: `pio test -e native` falla
      al enlazar.
- [x] 1.2 `src/modules/trend/{trend_types.h,trend_store,trend_events,
      trend_policy}` y alta en `[env:native]`.
- [x] 1.3 `TelemetryHistory.cpp` sobre `trend_store`: seis series, consigna
      como segunda serie por gráfico, cola de flancos, dibujo de marcadores
      en `LV_EVENT_DRAW_POST_END`, leyenda, textos i18n.
- [x] 1.4 `CommTask.cpp`: hook de flanco en `processReceivedAlarm()`.
- [x] 1.5 `UITask.cpp`: consigna al llamar a `TelemetryHistory_RecordSample`.
- [x] 1.6 `AlarmCenter.cpp`: botón TENDENCIA (punto de entrada).
- [x] 1.7 `pio test -e native` en verde (77 casos tras el bloque A) y
      `pio run -e main` en verde.
- [ ] 1.8 (manual) Banco: el botón abre la tendencia desde Main y desde
      bloqueo; las marcas alta/baja aparecen al provocar y resolver una
      alarma (p. ej. con la prueba de alarmas o desconectando la sonda de
      piel); la línea de consigna sigue a un cambio de consigna y desaparece
      al apagar el control; el botón se ve apagado con una alarma activa.

## 2. Display_HMI — bloque B: tendencia persistente de 24 h

Commits: `test(hmi): failing specs for trend persistence` y
`feat(hmi): tendencia persistente de 24 h en LittleFS`, más
`fix(hmi): hmi_fs no abre un fichero inexistente solo para medirlo` y
`refactor(hmi): el modelo de la tendencia entero en un bloque de PSRAM`.

- [x] 2.1 Tests en rojo: `test/test_trend_clock`, `test/test_trend_decimate`,
      `test/test_trend_file` (CRC, ida y vuelta, corrupciones, restauración
      alineada por epoch, fichero del futuro, solape con datos vivos),
      `test/test_trend_policy` ampliado (cuándo volcar), prepend en
      `test_trend_store`/`test_trend_events`. Verificado en rojo (solo
      referencias sin definir, ningún error de compilación).
- [x] 2.2 `src/modules/trend/{trend_clock,trend_decimate,trend_file,
      trend_model}` y `prepend` en `trend_store`/`trend_events`.
- [x] 2.3 `src/modules/storage/hmi_fs.{h,cpp}`.
- [x] 2.4 `TelemetryHistory.cpp`: capacidad 8640, ventanas 4/12/24 h,
      decimación a 1442 puntos, rejilla y etiquetas por ventana, servicio de
      volcado troceado y restauración (`trend_persist`), aviso de entrada en
      bloqueo.
- [x] 2.5 `UITask.cpp`: servicio de volcado fuera de `LVGL_Lock`; aviso en
      las dos rutas de entrada a bloqueo.
- [x] 2.6 `pio test -e native` (155 casos) y `pio run -e main` en verde; RAM y
      flash anotados y comparados con la base (tabla de arriba).
- [ ] 2.7 (manual) Banco: corte de corriente en mitad de una sesión de >1 h
      y recuperación (la traza anterior aparece, el hueco del apagado es
      "sin dato", los marcadores persisten); corte *durante* un volcado (el
      fichero anterior sigue válido y no queda `/trend.bin` a medias);
      ventanas 4/12/24 h; ausencia de parpadeo apreciable durante el volcado
      y con una alarma activa; primer arranque con la partición virgen (se
      formatea una vez y monta); log `HmiFs`: heap interno antes/después del
      montaje y tiempo de montaje; log `TrendPersist`: duración y trozos de un
      volcado; log `TelHist`: coste de `redraw` en las tres ventanas.

## 3. Docs

Commit: `docs(hmi): update for trend`.

- [ ] 3.1 `docs/hmi.md`: sección de tendencia (entrada, ventanas, marcadores),
      almacenamiento (`hmi_fs`, partición, política de volcado) y formato del
      fichero.

## 4. Archivo

Commit: `chore(spec): archive hmi-trend`.

- [ ] 4.1 `openspec archive hmi-trend`.

## Deuda anotada (para el scribe / retro)

- `ui_ChartButton` y `ui_ScreenCharts` (pantalla antigua de "Tiempo Real")
  siguen creados y alimentados (`chart_add_*`) pero son inalcanzables: se
  pueden retirar en otro cambio.
- La lección E9 de formación (tendencia) sigue retirada: ahora la tendencia
  tiene entrada (centro de alarmas), así que podría volver a existir una
  lección que la abra desde ahí.
- Tras un reinicio, los marcadores vienen del fichero; `CTRL,ALM_HISTORY`
  (últimas 10, con epochs de la placa) podría repoblar marcas anteriores al
  primer volcado, sin implementar.
- Una alarma ya activa al arrancar el HMI se marca en el instante en que el
  HMI la conoce, no en el que empezó.
