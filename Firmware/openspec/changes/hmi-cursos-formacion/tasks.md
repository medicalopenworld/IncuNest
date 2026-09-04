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
      retardo; `Discharge`, `Kangaroo`, `AlarmTest`, `AlarmSilence` y
      `WiFiCredentials` se tragan. Las peticiones de solo lectura (historiales,
      descripción de alarma) siguen yendo a la placa. Comentario cruzado con
      el parser.
- [x] 1.3 `Display_ApplyCtrlState()`: la parte de identidad, etiquetas y
      bitmask de alarmas se extrae a `applyCtrlStateInfoAndAlarms()` y es lo
      único que se aplica en formación; el estado de control retorna antes.
- [x] 1.4 `UITask.cpp`: no escribir NVS en formación (`doNVSWrite`,
      `UI_ApplyLanguage`, `Switch_cb` de modo oscuro y humedad).
- [x] 1.5 `pio run -e main` en verde (Flash 2 517 704 B, +1 912 B sobre dev).

### 2. APIs que faltan en los diálogos

Commit: `refactor(hmi): exponer IsOpen/Cancel en asistentes y dialogos`.

- [ ] 2.1 `BabyWizard_IsOpen()`, `BabyWizard_Cancel()`,
      `BabyWizard_GetStep()` (enum público mínimo: Closed / Identity /
      Weight / Age / Summary).
- [ ] 2.2 `BabyExitDialog_IsOpen()`/`_Cancel()`, `TimeDialog_IsOpen()`/
      `_Close()`, `BabyHistory_IsOpen()`/`_Close()`,
      `TelemetryHistory_IsOpen()`/`_Close()`.
- [ ] 2.3 `UI_GetControlSnapshot(UiControlSnapshot*)` y
      `UI_RestoreControlSnapshot(const UiControlSnapshot*)` en `UITask.h`
      (consignas, panel, switches vía `ui_set_switch_state_silent`, modos,
      fototerapia, idioma) + `UI_SyncAll()`.
- [ ] 2.4 `pio run -e main` en verde.

### 3. Motor de lecciones

Commit: `feat(hmi): motor de lecciones interactivas (explicar, hacer, pregunta)`.

- [ ] 3.1 `src/ui/training/training_engine.{h,cpp}`: absorbe `HelpTour.cpp`
      (sombras, marco, bocadillo, salto de pasos ocultos, navegación de
      pantallas). Tipos `Step`/`Lesson`/`Course`. Overlay raíz **no
      clicable**; sombras clicables; hueco a coords exactos del control en
      `STEP_DO`; bocadillo plegado en `STEP_FREE`; pregunta con tres
      opciones y reintento en `STEP_QUIZ`.
- [ ] 3.2 `Training_Poll()`: evaluar `goal()` cada vuelta; salto de objetivos
      ya cumplidos; aborto por alarma / enlace / apagado / inactividad
      (`HELP_IDLE_TIMEOUT_MS`); al abortar o terminar, `Training_Exit()` +
      restaurar instantánea + cancelar asistentes abiertos.
- [ ] 3.3 Gate clínico al arrancar una lección; modo demostración si falla
      (pasos "hacer" → "explicar" con prefijo); aviso previo.
- [ ] 3.4 Franja "MODO FORMACION" (20 px, borde inferior, `lv_layer_top()`,
      creada tras el banner de alarma).
- [ ] 3.5 Exención de auto-bloqueo en `inactivity_timer_cb` con
      `Training_IsOpen()`; `HelpTour_*` desaparece o queda como alias.
- [ ] 3.6 `pio run -e main` en verde.

### 4. Selector, identidad, progreso y certificado

Commit: `feat(hmi): selector de cursos, alumno, progreso NVS y certificado QR`.

- [ ] 4.1 `training_selector.cpp`: pantalla de cursos → lista de lecciones
      con estado → pantalla de alumno (teclado de letras de `BabyWizard`,
      24 chars) → "Continuar como X" / "Nuevo alumno".
- [ ] 4.2 `training_progress.{h,cpp}`: namespace `hmi_train` en
      `EEPROM_defines.h`; claves por curso (`n_name`, `n_done`, `n_att`) y
      anillo `cert_N`/`cert_cnt`; escritura desde el bucle de UI fuera del
      lock (flag `g_trainingDirty` junto a `eepromDirty`).
- [ ] 4.3 Certificado: pantalla con resumen y QR `mailto:` a
      `TRAINING_EMAIL` (default `SUPPORT_EMAIL` en `Credentials_public.h`),
      reutilizando el percent-encoding de `support_report.cpp`
      (`support_report_build_mailto_custom(to, subject, body)` o similar).
      Lista de certificados en el selector.
- [ ] 4.4 `HelpDialog`: TUTORIAL GUIADO abre `Training_OpenSelector()`.
- [ ] 4.5 `pio run -e main` en verde.

### 5. Lecciones de validación

Commit: `feat(hmi): lecciones E0 intro, E1 temperatura por aire y E5 alarmas`.

- [ ] 5.1 `lessons_nurse.cpp`: E0 (tabla actual de `HelpTour` como
      `STEP_EXPLAIN`), E1 temperatura por aire (objetivos del design §6),
      E5 atender una alarma. Textos ASCII ES/EN/FR.
- [ ] 5.2 Curso Técnico con solo T0 intro, para que el selector muestre los
      dos cursos.
- [ ] 5.3 `pio run -e main` en verde; delta de flash anotado.

### 6. Verificación manual en banco — **manual**

- [ ] 6.1 Modo formación: con el monitor serie de la motherBoard, subir la
      consigna y activar el toggle en E1; la placa no cambia
      `desiredAirTemperature` ni `actuation`; el calefactor no arranca.
- [ ] 6.2 Alta en formación (E1 vía asistente completo): ningún
      `HMI,PROFILE_*` en el monitor; ThingsBoard sin `baby_seq` nuevo.
- [ ] 6.3 Salir a mitad con el asistente abierto: se cierra, consigna y
      toggles vuelven, la franja desaparece, en <2 s la pantalla refleja el
      `CTRL,STATE` real; NVS sin cambios (reiniciar y comprobar consigna).
- [ ] 6.4 Hueco permeable: en "toca AIRE" solo responde AIRE; PIEL, candado
      y Ajustes no.
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
- [ ] 6.11 Auto-bloqueo: 3 min sin tocar en una lección → cierra y restaura;
      20 s después bloquea.

### 7. Documentación y archive de la fase 1

Commit: `docs: update for hmi-cursos-formacion (fase 1)`.

- [ ] 7.1 `docs/hmi.md` §6: cursos, modo formación, gate, progreso.
- [ ] 7.2 ADR-0002 (ya escrito en propose) revisado contra el código.
- [ ] 7.3 `docs/communication.md` o `PROTOCOL.md` **no** cambian; anotar en
      `docs/hmi.md` que el modo formación es solo HMI.
- [ ] 7.4 Textos: anotar en la retro que al integrar `feat/hmi-i18n-catalogo`
      los `TXT(es,en,fr)` de la ayuda y los cursos deben migrar al catálogo.

## Fase 2 — curso de Enfermería completo

Commit por lección o por pares: `feat(hmi): leccion E2 piel y sonda`, …

- [ ] 8.1 E2 piel y sonda (Ajustes > Modos > piel; PIEL; sonda).
- [ ] 8.2 E3 humedad.
- [ ] 8.3 E4 fototerapia segura (pop-up de seguridad ocular como `STEP_DO`).
- [ ] 8.4 E6 alta y seguimiento (asistente completo, peso, canguro).
- [ ] 8.5 E7 salida del bebé.
- [ ] 8.6 E8 bloqueo, E9 tendencia, E10 hora (ACK simulado), E11 soporte.
- [ ] 8.7 Banco: cada lección de principio a fin; certificado de Enfermería.

## Fase 3 — curso Técnico completo

- [ ] 9.1 T1 información y versiones; T2 WiFi y servidor (sin aplicar
      credenciales en formación); T3 idioma y modos (restaurar idioma).
- [ ] 9.2 T4 hora; T5 alarmas técnicas; T6 actualización por servidor web
      (decidir si se muestra la IP en Información: tarea aparte); T7
      informe de soporte; T8 apagado seguro.
- [ ] 9.3 Banco y certificado Técnico.
