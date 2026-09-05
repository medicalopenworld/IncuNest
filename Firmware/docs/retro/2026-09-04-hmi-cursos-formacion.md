# Retro — cursos de formación, fase 1 (2026-09-04)

Rama `feat/hmi-cursos-formacion`, worktree `Firmware/.worktrees/cursos-formacion`.
Modalidad `auto`. Cambio OpenSpec `hmi-cursos-formacion` (spec
`hmi-training-courses`, delta sobre `hmi-help-center`). Fase 1 de 3.

## Qué se hizo

Modo formación en el lado HMI del protocolo (ADR-0002): `CommTask` congela el
keepalive, no envía perfil ni hora, simula las respuestas en local y no aplica
`CTRL,STATE` a la UI. Motor de lecciones declarativo (explicar / hacer con
hueco permeable al toque y objetivo por estado / pregunta con reintento /
paso libre), gate clínico con demostración, franja MODO FORMACION, selector
con alumno, progreso en NVS y certificado QR. Lecciones E0, E1, E5 y T0.
Solo Display_HMI; `PROTOCOL.md` y `shared/` intactos. Build +26 KB de flash.

## Aprendizajes con coste observable

### 1. Eximir del auto-bloqueo con `lv_disp_trig_activity()` mata cualquier tope de inactividad (SEGUNDA ocurrencia del patrón, primera vez detectada)

`inactivity_timer_cb` eximía a `ui_ScreenAlarms`, a la ayuda y a la
formación llamando a `lv_disp_trig_activity(NULL)` cada 200 ms. Eso reinicia
el contador global de LVGL, que es justo el que `HelpDialog_Poll()` y
`Training_Poll()` leen para cerrarse solos a los 3 min. El tope de la ayuda
(`HELP_IDLE_TIMEOUT_MS`, añadido en `hmi-boton-ayuda` tras un bloqueante de
seguridad) **nunca funcionó**, y pasó dos revisiones. Lo cazó `qa-engineer`
siguiendo el flujo del contador, no leyendo el callback.

**Aplicado**: el callback exime a ayuda y formación **sin** tocar el
contador (solo `ui_ScreenAlarms` lo reinicia, que no tiene tope y es
intencional). Regla en `Firmware/.claude/rules/embedded-display-hmi.md`:
una exención del auto-bloqueo con tope propio no puede reiniciar el
contador de LVGL. Checklist de banco 6.11 ampliado a la ayuda.

### 2. El sandbox del protocolo no cubre los efectos de UI que no pasan por el protocolo

La revisión de seguridad enumeró cada `COMM_SERIAL.print*` y confirmó el
sandbox completo, pero encontró que en un paso libre el alumno podía tocar
CONECTAR WiFi: `Communication_SendWiFiCredentials` se tragaba, pero
`wifiInit()` y la persistencia de credenciales en NVS (`Wifi_OTA.cpp`) no
pasan por `CommTask`. Un "no recibe órdenes" verdadero exige repasar también
NVS, WiFi y cualquier salida que no sea la UART.

**Aplicado**: los botones de red se rechazan en formación; ADR-0002 y design
lo recogen. Regla en `embedded-display-hmi.md` (misma entrada que el punto 1,
apartado "modo formación"): toda acción de UI con efecto fuera del protocolo
(WiFi, NVS, backlight, frecuencia del panel) comprueba `Training_IsActive()`.

### 3. Un aborto por alarma no debe dejar ningún overlay de formación

La primera versión reabría el selector al abortar la lección por alarma y el
selector no cedía ante nada ni tenía tope: alarma → selector tapando la
pantalla clínica indefinidamente. Es el mismo tipo de hallazgo que el punto 2
de la retro anterior (banner solo en Lock + exención sin tope), aplicado a un
overlay nuevo: **segunda ocurrencia** de "un overlay nuevo se olvida de
ceder". La regla de `mustYield()` ya existía; lo que faltaba era aplicarla a
*todos* los overlays del cambio, no solo al principal.

**Aplicado**: `TrainingSelector_Poll()` con la misma cesión y tope; el aborto
no reabre nada. Regla: al añadir un overlay, listar cada overlay del cambio
y darle cesión y tope (añadido a la entrada "Overlays y alarmas" de la regla).

## Descartado con motivo

- **`const` a nivel de namespace tiene enlace interno**: `LESSON_INTRO`
  necesitó `extern` en el header para verse desde otras TUs. Primera
  ocurrencia, lo cazó el diseño antes del build; no se sistematiza.
- **Índice z guardado para restaurar el orden** (`lv_obj_move_to_index` con
  un índice capturado antes de subir el overlay): frágil porque los toasts
  reordenan `lv_layer_top()`. Sustituido por `lv_obj_move_background()` +
  `UI_RaiseAlarmIndicators()`. Es un detalle de LVGL, no una convención;
  queda documentado en el código.
- **Paso "toca AIRE" que siempre se saltaba**: el asistente deja AIRE
  seleccionado, y el mecanismo "objetivo ya cumplido se salta" lo convertía
  en invisible. Lo cazó `qa-engineer`. Es un error de contenido de una
  lección, no del motor; el mecanismo es correcto y se queda.
- **Textos > 200 caracteres en el bocadillo ancho**: uno (E5, ES, 208). Se
  acortó. Cuando haya 21 lecciones valdrá la pena un `static_assert`
  de longitud o un script; hoy un solo caso no lo justifica.

## Fases 2 y 3 (2026-09-05): cursos completos

### 4. La global `locked` no significa "pantalla bloqueada"

`locked` arranca en `true` con la principal cargada y `unlock_timeout_cb` la
rearma a `true` 5 s después de cada desbloqueo, ya en la principal; además
`LockScreenAnyTouch_cb` la pone a `false` con solo mostrar el pop-up. Se
exportó como `UI_IsScreenLocked()` y se usó en dos sitios con el significado
intuitivo: el motor no volvía a la principal tras un aborto (dejaba Ajustes
sin señal de alarma) y las lecciones de bloqueo se daban por hechas sin
tocar nada. Lo cazó la revisión de seguridad trazando los escritores de la
variable.

**Aplicado**: objetivos y decisión de pantalla por `lv_scr_act()`, función
retirada, comentario en `UITask.cpp` explicando qué significa `locked`.
Regla en `embedded-display-hmi.md`. El ciclo de vida de `unlockTimeoutTimer`
(que hace mentir a `locked`) queda como deuda anotada, fuera de este cambio.

### 5. Objetivos por visibilidad sin pantalla activa

`lv_obj_is_visible()` no comprueba que el objeto pertenezca a la pantalla
cargada: un panel de Ajustes destapado en una sesión anterior daba el paso
por hecho antes de tocarlo. Todos los objetivos de visibilidad llevan ahora
`lv_scr_act() == <pantalla>`. Mismo tipo de error que el punto 4: un
predicado que parece decir una cosa y dice otra.

### 6. Objetivos inalcanzables por el estado inicial

"Sube la consigna" con la consigna ya al máximo, "toca el panel de humedad"
con el panel oculto, "NUEVO BEBE" cuando el alumno pulsa SALTAR (gesto que
acababa de aprender como válido en la lección anterior). Cada objetivo
necesita una salida para el estado inicial que lo hace imposible: aceptar
el tope, un paso previo que lo habilite, o aceptar la alternativa y pedir
la buena en el texto. Dos revisores distintos lo encontraron por caminos
distintos; vale como criterio de revisión de lecciones (añadido a la regla).

### 7. El sandbox total no era lo que el usuario quería (decisión de producto, 2026-09-05)

Con los cursos completos, el usuario pidió lecciones **funcionales**: que
la lámpara y el calefactor se enciendan de verdad, y un bebé de prácticas
fijo (ZOE) obligatorio en el asistente que nunca quede en el historial. El
ADR-0002 original virtualizaba la actuación; la revisión la hace real y
virtualiza solo el bebé y los registros. El cambio fue pequeño porque el
interruptor estaba centralizado (`training_mode.cpp` + gates en
`CommTask`): quitar dos gates, forzar el envío al salir y rellenar la lista
con ZOE.

**Descartado como regla**: es la segunda vez en este proyecto que una
decisión de alcance del design se recorta o gira al final (la primera fue
el contacto por ThingsBoard). La regla que saldría ("confirmar las
decisiones de arquitectura del design con el usuario antes de implementar,
incluso en `auto`") ya se anotó en la retro anterior como candidata a la
segunda ocurrencia. **Ahora es la segunda**: se propone en
`Firmware/.claude/skills/loop-modes` un gate ligero en `auto` para las
decisiones marcadas como "Decisions" en el design.md. No se aplica en esta
retro porque tocar el skill de modalidades merece revisión humana.

## Pendiente que hereda la fase 2

- Migrar los `TXT(es,en,fr)` / `Txt3` de la ayuda y los cursos al catálogo
  X-macro de `feat/hmi-i18n-catalogo` cuando se integre (tarea 7.4 de
  `tasks.md`). Los textos de las lecciones son ~15 KB ya en fase 1.
- La demostración con bebé dentro puede confundir (franja "DEMOSTRACION");
  revisar en banco si hace falta un aviso más fuerte.

## Proceso

- Cuatro subagentes en paralelo (verify, review calidad, review seguridad,
  docs) tras el commit `feat`: el patrón vuelve a rendir. Esta vez verify y
  seguridad encontraron cosas distintas y complementarias (contador de
  inactividad vs rutas fuera del protocolo); mantener los dos siempre.
- El mapa de código previo (`Explore`, 11 preguntas) evitó releer los 5000
  líneas de `UITask.cpp` y permitió diseñar el sandbox en el sitio correcto
  a la primera; el único hueco del mapa fue la exención del auto-bloqueo,
  que ya era defectuosa y nadie había cuestionado.
