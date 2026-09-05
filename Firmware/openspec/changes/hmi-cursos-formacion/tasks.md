Todo el cambio es de **Display_HMI**. Sin entorno de test: cada fase se
verifica con `pio run -e main` y prueba **manual** en el CrowPanel con la
motherBoard conectada y sin bebé. Ningún checkbox reclama cobertura
automatizada.

## Fase 1 — motor, modo formación y dos lecciones de validación

### 1. Modo formación (CommTask, estado)

Commit: `feat(hmi): modo formacion que congela las ordenes a la placa`.

- [x] 1.1 `src/state/training_mode.{h,cpp}`: `Training_Enter()`,
      `Training_Exit()`, `Training_IsActive()`, instantánea de `hmi_msg` al
      entrar (`Training_FrozenHmiMsg()`). No hace falta tocar la gracia de
      eco de `CommTask`: al restaurar `hmi_msg` a la instantánea, los valores
      protegidos 2,5 s son los mismos que tiene la placa.
- [x] 1.2 `CommTask.cpp`: el envío periódico usa la instantánea cuando
      `Training_IsActive()`; `Communication_SendProfile{ListReq,New,Select,
      Weight,AgeManual}` y `SendSetTime` encolan respuesta simulada (lista
      vacía, ACK con `seq 0xFFFF`, rango NTE de `shared/include/nte_table.h`
      con edad desconocida hasta `AgeManual`, `TIME_ACK` aceptado) que
      entrega `Training_ServiceReplies()` en el bucle de UI con 250 ms de
      retardo; `Discharge`, `Kangaroo` y `WiFiCredentials` se tragan.
      `AlarmTest` y `AlarmSilence` **no** se gatean (review de seguridad: son
      órdenes al sistema de alarmas, no a la terapia, y un botón mudo es
      peor). Las peticiones de solo lectura (historiales, descripción de
      alarma) siguen yendo a la placa. Comentario cruzado con el parser.
      `Training_Exit()` restaura `hmi_msg` desde la instantánea (review R1).
- [x] 1.3 `Display_ApplyCtrlState()`: la parte de identidad, etiquetas y
      bitmask de alarmas se extrae a `applyCtrlStateInfoAndAlarms()` y es lo
      único que se aplica en formación; el estado de control retorna antes.
- [x] 1.4 `UITask.cpp`: no escribir NVS en formación (`doNVSWrite`,
      `UI_ApplyLanguage`, `Switch_cb` de modo oscuro y humedad).
- [x] 1.5 `pio run -e main` en verde (Flash 2 517 704 B, +1 912 B sobre dev).

### 2. APIs que faltan en los diálogos

Commit: `refactor(hmi): exponer IsOpen/Cancel en asistentes y dialogos`.

- [x] 2.1 `BabyWizard_IsOpen()`, `BabyWizard_Cancel()`,
      `BabyWizard_GetStep()` (enum público `BabyWizardStep`: CLOSED / PICKER /
      IDENTITY / WEIGHT / AGE / SUMMARY).
- [x] 2.2 `BabyExitDialog_IsOpen()`/`_Cancel()`, `TimeDialog_IsOpen()`/
      `_Close()`, `BabyHistory_IsOpen()`/`_Close()`,
      `TelemetryHistory_IsOpen()`/`_Close()`.
- [x] 2.3 `UI_GetControlSnapshot(UiControlSnapshot*)`,
      `UI_RestoreControlSnapshot(const UiControlSnapshot*)` y
      `UI_IsScreenLocked()` en `UITask.h` (consignas, panel, switches vía
      `ui_set_switch_state_silent`, modos, fototerapia, idioma) +
      `UI_SyncAll()`. `hmi_msg` lo guarda y restaura `training_mode.cpp`.
- [x] 2.4 `pio run -e main` en verde.

### 3. Motor de lecciones

Commit: `feat(hmi): motor de lecciones interactivas (explicar, hacer, pregunta)`.

- [x] 3.1 `src/ui/training/training_engine.cpp` + `include/ui/training/
      {training,lesson_types}.h`: absorbe y retira `HelpTour.cpp` (sombras,
      marco, bocadillo, salto de pasos ocultos, navegación de pantallas).
      Tipos `Step`/`Lesson`/`Course` con macros `EXPLAIN/DO/DO_ENTER/FREE/
      QUIZ`. Raíz clicable en explicar/pregunta y **no clicable** en hacer;
      sombras clicables; hueco = `lv_obj_get_click_area()` del control (zona
      táctil ampliada incluida) en `STEP_DO`; bocadillo oculto y franja con
      la instrucción en `STEP_FREE`; tres opciones y reintento en `STEP_QUIZ`.
- [x] 3.2 `Training_Poll()`: `goal()` cada vuelta; objetivos ya cumplidos se
      saltan al entrar; aborto por alarma / enlace / apagado / inactividad;
      en paso libre sube el overlay por encima de AlarmCenter/TelemetryHistory
      y lo devuelve a su índice al cerrarse. `endLesson()`: cancelar
      asistentes → `UI_RestoreControlSnapshot` → `hmi_msg` = instantánea →
      `Training_Exit` → `BabyWizard_ClearActiveProfile` → `ui_ScreenMain`.
- [x] 3.3 Gate clínico (`gateOk()`); modo demostración con prefijo y toast;
      la demostración no cuenta como superada.
- [x] 3.4 Franja inferior de 32 px (no 20: cabe `montserrat_14` y el botón
      SALIR de los pasos libres), hija del overlay, con MODO FORMACION /
      DEMOSTRACION / TUTORIAL o la instrucción del paso libre.
- [x] 3.5 Exención de auto-bloqueo con `Training_IsOpen()` (selector o
      lección); `HelpTour.{cpp,h}` eliminados; `HelpDialog` abre el selector.
- [x] 3.6 `pio run -e main` en verde.

### 4. Selector, identidad, progreso y certificado

Commit: `feat(hmi): selector de cursos, alumno, progreso NVS y certificado QR`.

- [x] 4.1 `training_selector.cpp`: cursos (dos tarjetas con progreso) →
      "Continuar como X (n/N)" / "Nuevo alumno" → nombre (teclado de letras
      de `BabyWizard`, 24 chars, mínimo dos letras) → lecciones con estado
      (OK verde / flecha) → arranque; `TrainingSelector_OnLessonEnd()`
      marca progreso, certifica al completar y reabre en lecciones o en el
      certificado.
- [x] 4.2 `training_progress.{h,cpp}`: namespace `hmi_train` en
      `EEPROM_defines.h`; claves `c<N>_name/done/att`, anillo `cert_<slot>`
      + `cert_cnt`/`cert_next`; `TakeDirty()` bajo lock y `Flush()` fuera,
      junto al bloque `doNVSWrite` de `UITask.cpp`.
- [x] 4.3 Certificado: QR `mailto:` a `TRAINING_EMAIL` (default
      `SUPPORT_EMAIL`, `Credentials_public.h`) con `mailto_build()` genérico
      añadido a `support_report.cpp`; asunto `IncuNest SN 0042 - Certificado
      <curso> - <nombre>`; lista de certificados desde el selector.
- [x] 4.4 `HelpDialog`: TUTORIAL GUIADO abre `Training_OpenSelector()`;
      subtítulo de la tarjeta actualizado.
- [x] 4.5 `pio run -e main` en verde.

### 5. Lecciones de validación

Commit: `feat(hmi): lecciones E0 intro, E1 temperatura por aire y E5 alarmas`.

- [x] 5.1 `lessons_intro.cpp` (E0 = tabla de `HelpTour` como `EXPLAIN`,
      compartida por ambos cursos), `lessons_nurse.cpp`: E1 temperatura por
      aire (toggle → asistente en paso libre → AIRE → dos flechas → explicar
      → apagar → pregunta) y E5 atender una alarma (icono → centro en paso
      libre → pausa de audio → check → registro → pregunta). El orden de E1
      cambia respecto al design: AIRE solo es seleccionable con el control
      encendido (`AirPanel_cb` retorna si `!tempSwitched`).
- [x] 5.2 `lessons_tech.cpp`: curso Técnico con solo T0 intro.
- [x] 5.3 `pio run -e main` en verde. Flash 2 540 812 B (+25 020 B sobre dev
      en `6819108`, 80,8 %); RAM 127 588 B (+2 600 B).

### 6. Verificación manual en banco — **manual**

- [ ] 6.1 Modo formación: con el monitor serie de la motherBoard, subir la
      consigna y activar el toggle en E1; la placa no cambia
      `desiredAirTemperature` ni `actuation`; el calefactor no arranca.
- [ ] 6.2 Alta en formación (E1 vía asistente completo): ningún
      `HMI,PROFILE_*` en el monitor; ThingsBoard sin `baby_seq` nuevo.
- [ ] 6.3 Salir a mitad con el asistente abierto: se cierra, consigna y
      toggles vuelven, la franja desaparece, en <2 s la pantalla refleja el
      `CTRL,STATE` real; NVS sin cambios (reiniciar y comprobar consigna).
- [ ] 6.4 Hueco permeable: en "toca el boton de encendido" (E1) solo
      responde el toggle de temperatura; candado, Ajustes, flechas y PIEL no.
      (El paso "toca AIRE" se convirtió en explicar: el asistente deja AIRE
      seleccionado y el objetivo se cumpliría siempre al entrar.)
- [ ] 6.5 Objetivo por estado: dos pulsaciones de flecha, avanza en la
      segunda.
- [ ] 6.6 Gate: con control activo, E3 se ofrece en demostración y no marca
      superada.
- [ ] 6.7 Aborto: prueba de alarmas durante E1 → cierra, restaura, alarma
      visible.
- [ ] 6.8 Pregunta: fallar, ver explicación, repetir, acertar; intentos
      contados.
- [ ] 6.9 Progreso: superar E1 y E5, reiniciar, siguen superadas a nombre
      del alumno; "Nuevo alumno" las borra.
- [ ] 6.10 Certificado (curso Técnico con solo T0 para probar el final):
      QR abre el correo con asunto y cuerpo correctos.
- [ ] 6.11 Auto-bloqueo: 3 min sin tocar en una lección → cierra y restaura
      sin reabrir el selector; 20 s después bloquea. Lo mismo con el selector
      abierto y con el menú de ayuda (el defecto de `inactivity_timer_cb` que
      reiniciaba el contador cada 200 ms afectaba también a la ayuda).
- [ ] 6.12 Aborto por alarma con el selector abierto: se cierra solo y no
      queda nada de formación sobre la pantalla.
- [ ] 6.13 En un paso libre (asistente abierto), tocar CONECTAR en Ajustes >
      WiFi muestra "No disponible en modo formacion" y no cambia la red.

### 7. Documentación y archive de la fase 1

Commit: `docs: update for hmi-cursos-formacion (fase 1)`.

- [x] 7.1 `docs/hmi.md` §6: cursos, modo formación, gate, progreso.
- [x] 7.2 ADR-0002 (ya escrito en propose) revisado contra el código: sin
      discrepancias (gate, simulaciones, `applyCtrlStateInfoAndAlarms`,
      restauración e instantánea coinciden con `training_mode.{h,cpp}`,
      `CommTask.cpp` y `UITask.cpp`); no ha hecho falta corregirlo.
- [x] 7.3 `PROTOCOL.md` no cambia; se añadió un párrafo corto en
      `docs/communication.md` (§A.3) explicando el modo formación desde el
      lado HMI, con referencia a ADR-0002; `docs/hmi.md` §6 deja explícito
      que es solo HMI.
- [ ] 7.4 Textos: anotar en la retro que al integrar `feat/hmi-i18n-catalogo`
      los `TXT(es,en,fr)` de la ayuda y los cursos deben migrar al catálogo.
      **Pendiente**: la anotación en `docs/retro/` no está hecha; no es un
      fichero de docs cruzados/README de placa, queda fuera del alcance de
      esta pasada (stage DOCS).

## Fase 2 — curso de Enfermería completo

Commit por lección o por pares: `feat(hmi): leccion E2 piel y sonda`, …

Commit real: `feat(hmi): cursos completos de Enfermeria y Tecnico (fases 2 y 3)`
(una sola tabla por curso; separar por lección no aportaba nada).

- [x] 8.1 E2 piel y sonda: Ajustes > Modos > interruptor de piel (con la
      sonda real de la placa: sin sonda el paso se da por cumplido y se
      explica), volver, encender temperatura, PIEL, explicar colocación de la
      sonda, apagar, pregunta.
- [x] 8.2 E3 humedad: toggle (asistente en paso libre), flecha +5 %, explicar
      depósito, apagar, pregunta.
- [x] 8.3 E4 fototerapia: toggle → asistente y pop-up de seguridad ocular en
      **paso libre** (el pop-up cuelga de `ui_ScreenMain`, bajo el overlay:
      con sombras no se podría tocar), +minutos, INICIAR, cancelar, apagar,
      pregunta.
- [x] 8.4 E6 alta y seguimiento: Bebés (historial real, solo lectura),
      asistente completo con bebé de formación (`seq 0xFFFF`), explicar
      consigna propuesta y curva de peso, pregunta.
- [x] 8.5 E7 salida del bebé: asistente, apagar en paso libre con el diálogo
      de salida permitido solo en ese paso (`Training_SetExitDialogAllowed`),
      objetivo = diálogo visto y cerrado con el control apagado; pregunta
      canguro vs alta.
- [x] 8.6 E8 bloqueo (candado real, toque en la pantalla, pulsación larga),
      E9 tendencia desde el bloqueo (overlay subido sobre TelemetryHistory),
      E10 hora (`TIME_ACK` simulado), E11 soporte (menú de ayuda en paso
      libre; `Training_OpenSelector` no anida con lección abierta). Si una
      lección termina con la pantalla bloqueada de verdad, se respeta y el
      selector no se reabre hasta volver a la principal.
- [x] 8.8 Revisión 2026-09-05 (petición del usuario): actuación real en
      formación (la lámpara y el calefactor se encienden), bebé de prácticas
      **ZOE** único en la lista y obligatorio (BEBE NUEVO y SALTAR rechazados
      con aviso), `Training_Exit()` fuerza el envío del estado previo.
      Textos de E1/E2/E3/E4/E6/E7 adaptados. ADR-0002 y spec revisados.
- [x] 8.9 Review de seguridad de la revisión: confirmación previa de cabina
      vacía con lista real de bebés activos de la placa (rechazo si hay
      alguno o no responde), watchdog de lámpara (`TRAINING_PHOTO_TIMER_MIN`),
      guarda de restauración de 2,5 s en `Display_ApplyCtrlState`, envío
      forzado desde `CommTask` (`Training_TakeForceSend`), `eepromDirty`
      pospuesto en vez de descartado, SIN PESO y alta desde Bebés rechazados,
      nombre de ZOE copiado al seleccionarla, franja con lo peligroso delante.
      Deuda anotada en la motherBoard (`s_wizardSeq` no se limpia al dar de
      alta): rama aparte.
- [ ] 8.7 Banco: cada lección de principio a fin; certificado de Enfermería.
      Además: la lista del asistente muestra solo a ZOE; BEBE NUEVO, SALTAR y
      SIN PESO dan el aviso; el diálogo de confirmación aparece antes de cada
      lección interactiva y no deja empezar con un bebé activo en la placa;
      la lámpara se enciende en E4 y se apaga al salir; el calefactor arranca
      en E1 y se apaga al salir (monitor serie: `actuation` vuelve al valor
      previo en <1 s, y la consigna restaurada no se revierte en los 2,5 s
      siguientes); con la lámpara ON y el temporizador cancelado la línea
      `HMI,...` lleva 5 en el último campo; ZOE no aparece en Bebés ni en
      ThingsBoard; el diálogo de salida de E7 dice "ZOE".
- [ ] 8.10 Banco: resetear solo la HMI con fototerapia real activa (fuera de
      formación) y mirar en el log de la placa si el primer keepalive de la
      HMI (hmi_msg a ceros) apaga la terapia; si es así, es un bug
      preexistente de `known_issues.md #4` que hay que abrir aparte.

## Fase 3 — curso Técnico completo

- [x] 9.1 T1 información y versiones (SN, firmware HMI y MB); T2 WiFi y
      servidor (los botones de red están desactivados en formación; se
      explica); T3 idioma y modos (cambio real de idioma en paso libre y
      modo oscuro, ambos restaurados al salir).
- [x] 9.2 T4 hora; T5 alarmas técnicas (sensor, ventilador/salida, calefactor,
      red, enlace; pregunta); T6 actualización por servidor web (explicar; la
      IP se lee en el informe de soporte, mostrarla en Información queda como
      tarea aparte); T7 informe de soporte; T8 apagado seguro.
- [ ] 9.3 Banco y certificado Técnico.
- [x] 9.4 `pio run -e main` en verde con los dos cursos completos (12 + 9
      lecciones): Flash 2 590 792 B (82,4 %; +49 KB de textos respecto a la
      fase 1), RAM 128 012 B. Sin caracteres fuera de ASCII; ningún texto de
      bocadillo supera 200 (ancho) / 180 (estrecho) caracteres
      (`scratchpad/lesson_text_len.py`).
