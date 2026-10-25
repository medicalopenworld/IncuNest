Un commit por stage y por placa. Los tests de host corren con `pio test -e native` desde PowerShell (nunca Git Bash: un run bajo MSYS da falsos verdes), con `C:\msys64\mingw64\bin` en el PATH (lo añaden los `pre_native.py`). `test_sensorboard_frame` de la motherBoard falla con `0xC0000139` en esta máquina desde antes de este cambio: se mira el SUMMARY de cada suite.

## 1. motherBoard: informe y pacer (rojo primero)

- [x] 1.1 Constantes `PROTO_ENV_*` en `shared/include/protocol.h`.
- [x] 1.2 Tests Unity `test_sensorboard_env_report` (formato, centinelas, fallo de puerta, frescura, pacer, vuelta del contador) y alta de `sb_env_report.cpp` en `[env:native]`; esqueleto de `sb_env_report.{h,cpp}` que compila y falla.
- [x] 1.3 Implementar `sb_env_report.cpp` hasta verde.
- [x] 1.4 `CommTask.cpp`: snapshot cada 200 ms, `sb_env_report_make()`, pacer, `hmiSerial.print()`; solo con `SENSOR_SOURCE_SENSORBOARD`.

## 2. Display_HMI: parser y clasificación (rojo primero)

- [x] 2.1 Tests Unity `test_sb_env` (parser, clasificación, `sbenv_view`) y alta en el nuevo `[env:native]` de `Display_HMI/platformio.ini`; esqueleto de `sb_env.{h,cpp}`.
- [x] 2.2 Implementar `sb_env.cpp` hasta verde.
- [x] 2.3 `CommTask.cpp`: rama `CTRL,ENV` en `parse_message` y estado compartido con la UI.
- [x] 2.4 Textos en 4 lenguas en `i18n_strings.def`.
- [x] 2.5 Indicador sobre la franja libre de `ui_ClockButton` (Main) y réplica en Lock; `sb_env_heading_update()` en `UITask.cpp` con el patrón de `clock_update()`; invalidar tras `UI_ApplyTheme()`.

## 3. Verificación

- [x] 3.1 `pio test -e native` en motherBoard y en Display_HMI.
- [x] 3.2 `pio run -e IncuNest_V18` (motherBoard) y `pio run -e main` (Display_HMI) en verde; tamaños anotados.
- [ ] 3.3 **Banco (pendiente, sin entorno de test)**:
  - Equipo con SensorBoard: la línea `CTRL,ENV` aparece cada ~5 s en el monitor de la MB y el HMI la recibe.
  - Acercar un imán al hall: "Abierta" aparece en el botón del reloj en < 1 s; quitarlo: desaparece.
  - Flapping del hall ≥ 4 transiciones/60 s: salta `ALARM_SENSORBOARD_DOOR_FAULT` y el indicador de puerta **no** se pinta.
  - Aplaudir/hablar cerca del micro: el nivel sube de Bajo a Medio/Alto; en reposo (~60 dB crudos) debe verse Bajo.
  - Desenchufar el USB de la SensorBoard: los indicadores desaparecen en ≤ 1 s (`link=0`) y suena la alarma de enlace.
  - Equipo antiguo (I2C2): no aparece ningún indicador y no sale `CTRL,ENV`.
  - HMI con MB antigua (sin este cambio): nada pintado, sin errores en el log.
  - Pantalla de bloqueo: el indicador se replica, y no hay toques que lo afecten.
  - Cambio de idioma (ES/EN/FR/PT) con el indicador visible; modo oscuro sin que cambien los colores del indicador.
  - Reloj en "Sin hora": el indicador no pisa el texto.
  - Que la franja de 66 px (x=195..261) quepa de verdad: la hora y la fecha no tocan el indicador, "Ouverte" (la palabra más ancha) no llega al borde del botón, y el pictograma de la puerta se reconoce como puerta entornada. Las medidas salen de los avances de `lv_font_montserrat_*` y del `pad_hor` del tema por defecto; no se han visto en pantalla.
  - El toque sobre el indicador abre el diálogo de hora (el indicador no es clicable).

## 4. Documentación

- [x] 4.1 `PROTOCOL.md` (sección `CTRL,ENV`) y `docs/hmi.md`.
- [ ] 4.2 Deuda: sustituir los umbrales de ruido al calibrar el micrófono; actualizar `SensorBoard_v2/ESTADO.md` ("no llegan al HMI") — fuera de este OpenSpec root, lo anota el scribe.
