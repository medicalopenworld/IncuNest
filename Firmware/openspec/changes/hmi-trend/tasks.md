Todo el cambio es de **Display_HMI**. Los módulos `src/modules/trend/*` son
lógica pura y se cubren con tests Unity en `[env:native]` (`pio test -e
native` en `Firmware/Display_HMI`). El resto (LVGL, LittleFS, `UITask`,
`CommTask`, `AlarmCenter`) **no tiene entorno de test**: se verifica con
`pio run -e main` y con prueba manual en el CrowPanel real; las tareas de
banco están marcadas **(manual)** y ningún checkbox reclama cobertura
automatizada para ellas.

## 1. Display_HMI — bloque A: marcadores de alarma y consigna

Commits: `test(hmi): failing specs for trend markers` y
`feat(hmi): marcadores de alarma y consigna en la tendencia`.

- [ ] 1.1 Tests en rojo: `test/test_trend_store` (anillo de 6 series int16,
      huecos, duplicados, wrap, shift), `test/test_trend_events` (anillo de
      eventos), `test/test_trend_policy` (validez de consigna, mapeo
      slot->px de marcadores).
- [ ] 1.2 `src/modules/trend/{trend_types.h,trend_store,trend_events,
      trend_policy}` y alta en `[env:native]`.
- [ ] 1.3 `TelemetryHistory.cpp` sobre `trend_store`: seis series, consigna
      como segunda serie por gráfico, cola de flancos, dibujo de marcadores
      en `LV_EVENT_DRAW_POST_END`, leyenda, textos i18n.
- [ ] 1.4 `CommTask.cpp`: hook de flanco en `processReceivedAlarm()`.
- [ ] 1.5 `UITask.cpp`: consigna al llamar a `TelemetryHistory_RecordSample`.
- [ ] 1.6 `AlarmCenter.cpp`: botón TENDENCIA (punto de entrada).
- [ ] 1.7 `pio test -e native` en verde y `pio run -e main` en verde.
- [ ] 1.8 (manual) Banco: el botón abre la tendencia desde Main y desde
      bloqueo; las marcas alta/baja aparecen al provocar y resolver una
      alarma (p. ej. con la prueba de alarmas o desconectando la sonda de
      piel); la línea de consigna sigue a un cambio de consigna y desaparece
      al apagar el control.

## 2. Display_HMI — bloque B: tendencia persistente de 24 h

Commits: `test(hmi): failing specs for trend persistence` y
`feat(hmi): tendencia persistente de 24 h en LittleFS`.

- [ ] 2.1 Tests en rojo: `test/test_trend_clock`, `test/test_trend_decimate`,
      `test/test_trend_file` (CRC, ida y vuelta, corrupciones, restauración
      alineada por epoch, fichero del futuro, solape con datos vivos),
      `test/test_trend_policy` ampliado (cuándo volcar).
- [ ] 2.2 `src/modules/trend/{trend_clock,trend_decimate,trend_file,
      trend_model}` y `prepend` en `trend_store`/`trend_events`.
- [ ] 2.3 `src/modules/storage/hmi_fs.{h,cpp}`.
- [ ] 2.4 `TelemetryHistory.cpp`: capacidad 8640, ventanas 4/12/24 h,
      decimación a 1442 puntos, rejilla y etiquetas por ventana, servicio de
      volcado troceado y restauración, aviso de entrada en bloqueo.
- [ ] 2.5 `UITask.cpp`: servicio de volcado fuera de `LVGL_Lock`; aviso en
      las dos rutas de entrada a bloqueo.
- [ ] 2.6 `pio test -e native` y `pio run -e main` en verde; RAM/Flash
      anotados y comparados con la base.
- [ ] 2.7 (manual) Banco: corte de corriente en mitad de una sesión de >1 h
      y recuperación (la traza anterior aparece, el hueco del apagado es
      "sin dato", los marcadores persisten); corte *durante* un volcado (el
      fichero anterior sigue válido); ventanas 4/12/24 h; ausencia de
      parpadeo apreciable durante el volcado y con una alarma activa;
      heap interno antes/después del montaje (log `DIAG`).

## 3. Docs

Commit: `docs(hmi): update for trend`.

- [ ] 3.1 `docs/hmi.md`: sección de tendencia (entrada, ventanas, marcadores),
      almacenamiento (`hmi_fs`, partición, política de volcado) y formato del
      fichero.

## 4. Archivo

Commit: `chore(spec): archive hmi-trend`.

- [ ] 4.1 `openspec archive hmi-trend`.
