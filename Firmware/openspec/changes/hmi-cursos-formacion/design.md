## Context

Display_HMI, LVGL 8.3. Parte del tutorial pasivo de `hmi-help-center`
(`src/ui/HelpTour.cpp`: overlay en `lv_layer_top()`, cuatro sombras, marco,
bocadillo; `STEPS[]` de punteros a globales `ui_*`). Hechos del código que
fijan el diseño (mapa del 2026-09-04):

- **Toque a través del overlay.** `lv_indev_search_obj()` (LVGL 8.3.11)
  busca en los hijos siempre que el punto caiga dentro del objeto, y solo
  devuelve el propio objeto si es `CLICKABLE`. Un overlay raíz transparente y
  **no** clicable con sombras clicables alrededor de un hueco deja pasar el
  toque del hueco a la pantalla real y se traga el resto.
- **Orden z.** Bajo `lv_layer_top()` y por tanto por debajo del overlay del
  curso: `BabyWizard`, `BabyHistory`, `BabyExitDialog`, `TimeDialog`,
  `HelpDialog`, `ui_PhotoSafetyOverlay` (hijos de `ui_ScreenMain`). Por
  encima: `AlarmCenter`, `TelemetryHistory`, banner de alarma, AUDIO PAUSED,
  apagado. El overlay del curso nunca llama a `lv_obj_move_foreground()`.
- **La placa manda.** Consignas, toggles y modos ponen `shouldSendData` y
  viajan en la siguiente trama; `Display_ApplyCtrlState()`
  (`CommTask.cpp:925+`) aplica `CTRL,STATE` (~1 Hz) a switches, consignas y
  `hmi_msg`, con una gracia de 2,5 s solo para `actuation`, `controlMode`,
  `phototherapyMode`, `muteAlarm`, `skinModeEnabled`.
- **El alta crea el registro a mitad del asistente**
  (`Communication_SendProfileNew`, `BabyWizard.cpp:461`). El resto de
  respuestas de la placa que esperan los asistentes llegan por flags
  `g_pending*` que pone el parser de `CommTask`.
- **Faltan APIs**: `BabyWizard`, `BabyHistory`, `TelemetryHistory`,
  `TimeDialog`, `BabyExitDialog` no exponen `_IsOpen()` ni cierre público.
- **NVS**: escrituras desde la tarea UI, fuera de `LVGL_Lock()`, con
  `eepromDirty` + `EEPROM_COMMIT_DELAY` (`UITask.cpp:4542-4568`). Namespaces
  `hmi_cfg`/`hmi_wifi`/`hmi_gprs` en `EEPROM_defines.h`.
- **Auto-bloqueo** exento hoy para `ui_ScreenAlarms`, `HelpDialog_IsOpen()`
  y `HelpTour_IsOpen()` (`UITask.cpp:3365`).
- **Idiomas**: `TXT(es,en,fr)` local por fichero; la rama
  `feat/hmi-i18n-catalogo` (portugués, catálogo X-macro) no incluye la ayuda.

## Goals / Non-Goals

**Goals**

- Que el personal practique sobre la interfaz real, pulsando los controles
  reales, sin que la incubadora cambie nada.
- Acreditar quién ha superado qué, sin cuentas ni backend.
- Un motor de lecciones declarativo: añadir una lección es añadir una tabla.
- Cero cambios de protocolo y cero cambios en `shared/`.

**Non-Goals**

- Un simulador de la incubadora (temperaturas que evolucionan). En formación
  las medidas que se ven son las reales de la placa; los textos lo dicen.
- Formación sobre el hardware físico (sonda, puertas, limpieza) más allá de
  lo que la UI muestra.
- Gestión de alumnos, caducidad de certificados, servidor.

## Decisions

### 1. Modo formación en el lado HMI del protocolo (ADR-0002)

**Opciones**: (a) actuar de verdad y restaurar al salir; (b) flag de
formación en el protocolo para que la motherBoard no actúe (toca `shared/`
y ambas placas); (c) demostración pasiva para todo lo que toque actuadores;
(d) **cortar en la HMI**: mientras dura la lección, `CommTask` no envía
cambios ni peticiones de perfil y no aplica el estado de la placa a la UI.

**Decisión: (d).** `training_mode.{h,cpp}` (`src/state/`) con
`Training_Enter()` / `Training_Exit()` / `Training_IsActive()`. Efectos:

- `CommTask`: el keepalive/estado periódico sigue saliendo (para no perder
  el enlace ni disparar `ALARM_HMI_LINK_LOST`) pero con la **instantánea de
  `hmi_msg` tomada al entrar**, no con el `hmi_msg` vivo. Las funciones
  `Communication_Send*` de perfil, hora, prueba y silencio de alarma
  **no envían** y en su lugar encolan una **respuesta simulada** local que
  pone los mismos flags `g_pending*` que pondría el parser: lista de
  perfiles vacía, ACK de perfil nuevo con `seq` de formación (`0xFFFF`),
  rango NTE calculado con `shared/include/nte_table.h`, `TIME_ACK`
  aceptado. Así `BabyWizard`, `BabyExitDialog` y `TimeDialog` funcionan
  exactamente igual y la placa no se entera.
- `Display_ApplyCtrlState()`: actualiza `ctrl_state_msg` (alarmas, enlace,
  `linkBars`) y **retorna antes de tocar la UI o `hmi_msg`**.
- `UITask`: no persiste en NVS nada cambiado durante la formación
  (`eepromDirty` se descarta al salir; las escrituras inmediatas de idioma,
  modo oscuro y humedad se saltan con `Training_IsActive()`).
- `Training_Exit()`: restaura la instantánea local (consignas, panel,
  switches vía `ui_set_switch_state_silent`, `darkMode`, `humidityEnabled`,
  `skinPanelEnabled`, `g_lang`, fototerapia), pone a cero los sellos de la
  gracia de eco para que el siguiente `CTRL,STATE` mande, `UI_SyncAll()`,
  `UI_ApplyLanguage(g_lang)` si cambió, y `computeAndSendActuation()` **no**
  se llama (nada que mandar: la placa nunca cambió).

**Por qué no (a)**: la lección de temperatura encendería el calefactor de
verdad; aunque se restaure, es inaceptable como mecanismo en un dispositivo
médico. **Por qué no (b)**: es la solución "limpia" pero es un cambio
grande de protocolo para un fin no clínico, y la HMI puede garantizar lo
mismo por construcción: si no envía, la placa no actúa. **Por qué no (c)**:
el usuario quiere interactivo de verdad, y con (d) se puede.

### 2. Gate clínico, además del modo formación

El modo formación evita que la incubadora cambie, pero no que la pantalla
deje de mostrar el estado real durante minutos. Por eso una lección
interactiva solo arranca si:

```
!UI_AnyControlActive() && !UI_IsAnyAlarmActive() && !Display_IsBoardLinkLost()
&& BabyWizard_GetActiveSeq() == 0 && !g_pwrOffActive
```

Si falla, la lección se ofrece en **modo demostración**: los pasos "hacer"
se muestran como "explicar" (con el prefijo "Demostracion:") y no se entra
en modo formación. Una lección en demostración **no cuenta como superada**.
Durante una lección interactiva, `Training_Poll()` aborta y restaura ante
cualquier alarma, enlace perdido o `g_pwrOffActive`.

Rótulo fijo: franja ámbar de 20 px en el borde inferior (`lv_layer_top()`,
y 460-480, zona libre en todas las pantallas) con "MODO FORMACION: la
incubadora no recibe ordenes". Se crea en `Training_Init()` después del
banner de alarma para no taparlo (no compiten: el banner va arriba).

### 3. Motor de lecciones: tablas declarativas

```c
enum StepKind { STEP_EXPLAIN, STEP_DO, STEP_QUIZ };
struct Step {
  StepKind kind;
  lv_obj_t **target;        // control a resaltar / dejar tocar (nullptr = ninguno)
  lv_obj_t **screen;        // pantalla que debe estar cargada
  const char *es, *en, *fr; // texto (instruccion o pregunta)
  bool (*goal)(void);       // STEP_DO: verdadero cuando esta hecho
  uint8_t flags;            // STEP_FREE (sin sombras, bocadillo plegado), STEP_NO_SKIP
  const Quiz *quiz;         // STEP_QUIZ: 3 opciones, indice correcto, explicacion
};
struct Lesson { uint8_t id; const char *es,*en,*fr; const Step *steps; uint8_t count; uint8_t flags; };
struct Course { uint8_t id; const char *es,*en,*fr; const Lesson *lessons; uint8_t count; };
```

- **Objetivos por estado**, nunca por evento: `goal()` lee el estado local
  (`UI_GetControlSnapshot()`, nueva función en `UITask.h` que copia
  `airTempValue`, `selectedPanel`, `switchTemp`, `photoTimerMinutes`,
  `photoTimerActive`, `hmi_msg.phototherapyMode`, etc.), `lv_scr_act()`,
  `AlarmCenter_IsOpen()`, `BabyWizard_IsOpen()`/`_GetStep()`,
  `TimeDialog_IsOpen()`, `HelpDialog_IsOpen()`, `locked`. Se evalúa en
  `Training_Poll()` cada vuelta de UI. Un objetivo que ya se cumple al
  entrar en el paso lo salta (evita pedir algo ya hecho).
- **STEP_DO con hueco**: el overlay raíz pasa a no clicable; las sombras se
  ajustan a los coords exactos del control (sin `FRAME_PAD`) y el marco se
  dibuja encima con su pad. Solo se puede tocar el control del paso.
- **STEP_FREE**: sombras y marco ocultos; bocadillo plegado (800×72, abajo,
  encima de la franja) con la instrucción, el contador y SALIR. Para pasos
  cuyo objetivo exige varios toques dentro de un asistente ("rellena nombre
  y semanas y pulsa CONTINUAR").
- **STEP_QUIZ**: bocadillo con la pregunta y tres botones; acierto → texto
  de refuerzo y SIGUIENTE; fallo → explicación, `attempts++` y se repite.
- **Cancelación segura de asistentes**: si el alumno sale a mitad de un
  asistente, `Training_Exit()` llama a los nuevos `BabyWizard_Cancel()`,
  `BabyExitDialog_Cancel()`, `TimeDialog_Close()`, `BabyHistory_Close()`,
  `TelemetryHistory_Close()`, `AlarmCenter_Close()`, `HelpDialog` cerrado.
- **ANTERIOR** solo entre pasos "explicar"; un paso "hacer" completado no
  se deshace (el estado ya cambió). SIGUIENTE en un "hacer" no existe: se
  avanza al cumplir el objetivo. SALTAR solo en modo demostración.

### 4. Alta y salida del bebé: real hasta donde no deja rastro

Con el modo formación, `Communication_SendProfileNew` no sale y el ACK se
simula con `seq = 0xFFFF`: el asistente completo (nombre, semanas, peso,
edad, resumen, APLICAR) es interactivo de verdad y **no crea ningún
registro** en la placa ni en ThingsBoard. Igual la salida: `BabyExitDialog`
llega hasta la confirmación y `Communication_SendProfileDischarge` se traga.
No hace falta interceptar el último paso: el sandbox lo cubre entero. Al
salir, `BabyWizard_ClearActiveProfile()` deja `s_sessionSeq = 0`.

### 5. Identidad, progreso y certificado

- Al elegir un curso se pide nombre o iniciales (teclado de letras de
  `BabyWizard`, 24 caracteres). Si en NVS hay un alumno con progreso en ese
  curso, el selector ofrece "Continuar como ANA (4/12)" o "Nuevo alumno"
  (borra el progreso).
- NVS, namespace `hmi_train`, escrito desde el bucle de UI fuera del lock:
  por curso `n_name` (string), `n_done` (uint32 bitmask de lecciones),
  `n_att` (uint16 intentos acumulados); certificados `cert_N` (blob
  `{name[24], course, epoch, attempts, lessons}`, anillo de 16) y `cert_cnt`.
- Curso superado = todas las lecciones interactivas superadas (una lección
  en demostración no cuenta). Se muestra la pantalla de certificado con QR
  `mailto:` a `TRAINING_EMAIL` (default `SUPPORT_EMAIL`, en
  `Credentials_public.h`), asunto `IncuNest SN 0042 - Certificado
  <curso> - <nombre>` y cuerpo con fecha, lecciones, intentos y versión de
  firmware. La lista de certificados es consultable en el selector.

### 6. Lecciones (objetivos)

Fase 1 (validación del motor):

- **E5 Atender una alarma**: abrir el centro de alarmas desde el icono
  (`AlarmCenter_IsOpen()`), leer título y acción (explicar), pausar el audio
  (`hmi_msg.muteAlarm` local; en formación no sale), volver, consultar el
  registro, pregunta: "¿qué hace la pausa de audio?".
- **E1 Temperatura por aire**: tocar AIRE (`selectedPanel == AIR`), subir
  la consigna dos pasos (`airTempValue >= inicial + 0.4`), activar con el
  toggle (aparece el asistente: STEP_FREE hasta `switchTemp` verdadero por
  SALTAR o por completar), leer el valor grande frente a la consigna,
  apagar (`!switchTemp`), pregunta.

Fase 2, enfermería: E0 intro (tabla actual), E2 piel y sonda, E3 humedad,
E4 fototerapia (incluye el pop-up de seguridad ocular: STEP_DO sobre
`ui_PhotoSafetyModal`), E6 alta y seguimiento (asistente completo, peso,
canguro en `BabyHistory`), E7 salida (`BabyExitDialog`), E8 bloqueo
(`locked`, desbloqueo por pulsación larga), E9 tendencia, E10 hora
(`TimeDialog`, ACK simulado), E11 soporte (abrir el QR).

Fase 3, técnico: T0 intro, T1 información y versiones, T2 WiFi y servidor
(en formación las credenciales no se aplican: `wifiApplyNewCredentials` se
salta), T3 idioma y modos (idioma sí cambia de verdad y se restaura al
salir), T4 hora, T5 alarmas técnicas (explicar cada condición técnica y su
acción: sensor, calefactor, ventilador, enlace, red; pregunta), T6
actualización por servidor web (explicar; la IP no se muestra en la UI hoy:
la lección la enseña en el informe de soporte y propone mostrarla en
Información como tarea aparte), T7 informe de soporte, T8 apagado seguro
(explicar `CTRL,PWR_OFF`).

## Risks / Trade-offs

- **Pantalla desincronizada durante la lección**: la UI muestra el estado
  simulado, no el real. Mitigación: gate clínico (sin bebé), franja
  FORMACION siempre visible, aborto por alarma/enlace, tope de inactividad
  de 3 min (`HELP_IDLE_TIMEOUT_MS`).
- **Respuestas simuladas divergen de la placa** si el protocolo cambia.
  Mitigación: las simulaciones ponen los mismos `g_pending*` que el parser y
  viven junto a él en `CommTask.cpp`, con comentario cruzado.
- **Progreso por equipo, no por persona en la nube**: si dos alumnos
  alternan en el mismo equipo, uno borra el progreso del otro al elegir
  "Nuevo alumno". Asumido: el certificado es lo que acredita, y se guarda.
- **Tamaño**: 21 lecciones × 3 idiomas. Estimación 120 KB; margen 630 KB.
  Si la rama i18n aterriza antes de la fase 2, los textos van al catálogo.
- **ANTERIOR limitado**: no se deshacen pasos "hacer". Aceptado y explicado
  en el bocadillo.

## Migration Plan

Sin migración de datos. `hmi_train` es un namespace nuevo; si no existe,
no hay progreso. El firmware anterior ignora el namespace. `HelpTour.cpp`
se renombra/absorbe en `src/ui/training/`; `HelpDialog` llama a
`Training_OpenSelector()` en vez de `HelpTour_Start()`.

## Open Questions

- Dirección del responsable de formación (`TRAINING_EMAIL`): por defecto la
  de soporte; se puede fijar en `Credentials.h`.
- Mostrar la IP en Información (tarea T6): fuera de este cambio salvo que se
  decida incluirla.
