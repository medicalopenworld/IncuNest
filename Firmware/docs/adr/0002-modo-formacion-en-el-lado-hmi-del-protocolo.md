# ADR-0002: El modo formación se implementa en el lado HMI del protocolo, sin que la motherBoard lo sepa

- **Estado**: aceptado (revisado 2026-09-05: la actuación pasa a ser real y
  el bebé se virtualiza como ZOE, ver "Revisión")
- **Fecha**: 2026-09-04
- **Placas afectadas**: Display_HMI
- **Cambio OpenSpec**: `openspec/changes/hmi-cursos-formacion`

## Contexto

Los cursos de formación (`hmi-training-courses`) piden que el personal
pulse los controles reales de la pantalla: subir una consigna, encender el
control de temperatura, dar de alta a un bebé. En el firmware actual no hay
ninguna separación entre "la UI cambia" y "la incubadora cambia":

- Cualquier cambio local pone `hmi_msg.shouldSendData` y sale en la
  siguiente trama (`CommTask.cpp:1190-1196`); la motherBoard actúa sobre
  calefactor, humidificador y fototerapia.
- `Display_ApplyCtrlState()` (`CommTask.cpp:925+`) aplica el `CTRL,STATE`
  de la placa (~1 Hz) a switches, consignas y `hmi_msg`, con una gracia de
  eco de 2,5 s solo para cinco campos.
- El asistente de alta crea el perfil en la placa a mitad del recorrido
  (`Communication_SendProfileNew`, `BabyWizard.cpp:461`) y de ahí sale a
  ThingsBoard con `baby_seq`.

Es un dispositivo médico: una lección no puede encender un calefactor ni
crear un paciente.

## Opciones consideradas

| Opción | A favor | En contra |
|---|---|---|
| A. Actuar de verdad y restaurar al salir | Sin código nuevo en CommTask | El calefactor se enciende durante la lección; si el firmware se cae a mitad, el estado queda cambiado; los perfiles creados hay que borrarlos y ya han salido a ThingsBoard |
| B. Flag de formación en el protocolo; la motherBoard ignora órdenes y no persiste | Solución "limpia" en el sitio correcto | Toca `shared/`, `PROTOCOL.md` y las dos placas (cambio grande y coordinado) para un fin no clínico; hasta que las dos placas lo lleven no hay formación |
| C. Demostración pasiva para todo lo que toque actuadores | Cero riesgo | No es "interactivo de verdad", que es el requisito |
| **D. Cortar en la HMI**: mientras dura la lección, `CommTask` no envía cambios ni peticiones y no aplica el estado de la placa a la UI; las respuestas que esperan los asistentes se simulan localmente | Seguridad por construcción (si no se envía, la placa no actúa); cero cambios de protocolo; la UI se comporta igual que en real | La pantalla muestra estado simulado durante la lección; hay que mantener las simulaciones alineadas con el parser |

## Decisión

**D.** `training_mode.{h,cpp}` en `src/state/` con `Training_Enter()`,
`Training_Exit()`, `Training_IsActive()`. Mientras está activo:

1. El envío periódico a la placa sigue (keepalive y detección de enlace)
   pero con la **instantánea de `hmi_msg` tomada al entrar**: la placa
   recibe exactamente lo que ya tenía.
2. `Communication_Send*` de perfil y de hora no envían; encolan una
   respuesta simulada que pone los mismos flags `g_pending*` que pondría el
   parser (`seq` de formación `0xFFFF`, rango NTE calculado con
   `shared/include/nte_table.h`, `TIME_ACK` aceptado). Alta, salida, canguro
   y credenciales WiFi se tragan. **Excepción deliberada**: `ALM_SILENCE` y
   `ALM_TEST` siguen saliendo, porque son interacciones con el sistema de
   alarmas (60601-1-8), no con la terapia, no persisten nada, y un botón
   muerto justo cuando suena algo es peor que una orden inocua (una alarma
   real aborta la lección en la siguiente pasada de UI de todas formas).
3. `Display_ApplyCtrlState()` actualiza `ctrl_state_msg` (alarmas, enlace,
   barras) y aplica identidad, etiquetas y bitmask de alarmas; retorna antes
   de tocar el estado de control de la UI o `hmi_msg`.
4. No se persiste en NVS nada cambiado durante la lección, y los controles
   de red (CONECTAR / DESCONECTAR WiFi) se rechazan con un aviso: cambiarían
   el equipo de verdad y no viajan por el protocolo.
5. `Training_Exit()` **restaura `hmi_msg` desde la instantánea** y baja el
   flag en la misma función (el invariante lo garantiza el módulo dueño del
   flag, no el llamador); el estado de la UI lo restaura antes el motor con
   `UI_RestoreControlSnapshot()`. La gracia de eco de `CommTask` no se toca:
   al detectar la restauración protege 2,5 s unos valores que son
   exactamente los que tiene la placa.

Se combina con un **gate clínico** (sin terapia activa, sin alarma, con
enlace, sin perfil activo, sin apagado en curso) y una franja fija
"MODO FORMACION" en pantalla: el sandbox evita que la incubadora cambie; el
gate evita que se tape el estado real con un bebé dentro.

## Revisión 2026-09-05: actuación real, bebé virtual

El usuario pidió que las lecciones de temperatura y fototerapia sean
**funcionales**: que la lámpara y el calefactor se enciendan de verdad, y
que el alumno practique siempre con un bebé de prácticas llamado **ZOE**
que nunca quede en el historial. Eso reparte el sandbox en dos mitades:

- **Actuación real.** `SendMessageToOtherESP()` vuelve a enviar el
  `hmi_msg` vivo y `Display_ApplyCtrlState()` se aplica entero también en
  formación. La incubadora responde a consignas, toggles y fototerapia como
  en operación normal. Lo hace posible el gate clínico (sin terapia activa,
  sin alarma, con enlace, sin perfil real, sin apagado): la cabina está
  vacía. Al salir, `Training_Exit()` restaura `hmi_msg` desde la
  instantánea y fuerza el envío: la placa vuelve al estado previo (todo
  apagado si así estaba) en la siguiente vuelta de `CommTask`.
- **Bebé virtual.** La lista de perfiles en formación trae un único bebé,
  ZOE (`TRAINING_BABY_SEQ = 0xFFFF`, 32 semanas, 1500 g), y el asistente
  rechaza BEBE NUEVO y SALTAR con un aviso: el alumno tiene que
  seleccionarla. Selección, peso y edad se contestan en local (rango NTE con
  la misma función pura que la placa); alta, salida, canguro, hora y
  credenciales siguen sin salir. ZOE nunca llega a la placa, al historial ni
  a ThingsBoard; `BabyWizard_ClearActiveProfile()` la borra al salir.
- **Lo que no cambia**: nada persiste en NVS, los botones de red se
  rechazan, `ALM_SILENCE`/`ALM_TEST` salen, cualquier alarma o enlace
  perdido aborta y restaura.

La franja pasa a decir "FORMACION: LA INCUBADORA CALIENTA E ILUMINA DE
VERDAD. CABINA VACIA. Bebe de practica ZOE." El riesgo que se asume es
calentar una cabina vacía y encender la lámpara sin bebé durante unos
minutos, con los límites de seguridad de la propia placa vigentes; a cambio
el alumno ve la incubadora responder de verdad, que es lo que el curso
quiere enseñar.

**Salvaguardas añadidas tras la revisión de seguridad de esta revisión:**

- **El gate de la HMI no puede ver un bebé sin terapia.** Antes de cada
  lección interactiva el selector pide a la placa su lista **real** de
  bebés activos (`HMI,PROFILE_LIST_REQ`, con el modo formación aún
  inactivo) y rechaza la lección si hay alguno o si la placa no contesta en
  2,5 s; y el alumno tiene que confirmar expresamente "SI, LA CABINA ESTA
  VACIA" en un diálogo que nombra lo que va a pasar (calentar, encender la
  lámpara). La confirmación queda en el log.
- **Watchdog de la lámpara.** Si la HMI se reinicia con la lámpara
  encendida, la placa mantiene la terapia (`ALARM_HMI_LINK_LOST` no la
  corta) y solo el temporizador la apaga. En formación la trama de estado
  nunca sale con fototerapia ON y 0 minutos: se sustituye por
  `TRAINING_PHOTO_TIMER_MIN` (5 min).
- **Guarda de restauración.** Las consignas no tienen gracia de eco en
  `CommTask`; durante `TRAINING_RESTORE_GUARD_MS` (2,5 s) tras salir, un
  `CTRL,STATE` en vuelo con el estado de la lección no se aplica a la UI.
  El envío del estado restaurado lo fuerza `CommTask` desde su propia tarea
  (`Training_TakeForceSend`), sin tocar `shouldSendData` desde la UI.
- El alta de un bebé real desde Bebés, SIN PESO y SALTAR/BEBE NUEVO del
  asistente se rechazan con aviso en formación.

**Deuda que este ADR deja explícita en la motherBoard** (fuera de este
cambio, hay que abrir su propia rama): `CommTask.cpp` de la placa conserva
`s_wizardSeq` tras un alta (`BABY_MSG_DISCHARGE` solo limpia
`s_activeSeq`), y en el flanco de terapia ON hace
`babyStore_setActiveSeq(s_wizardSeq)`: una terapia posterior sin asistente,
formación incluida, **reactiva y acredita minutos al último bebé dado de
alta**. La comprobación de "sin bebés activos" mitiga el caso frecuente
pero no ese. También conviene registrar que las lecciones consumen horas
reales de lámpara (`KEY_RT_PHOTO`) y que la placa persiste en su NVS la
consigna de la lección si el control termina apagado (`KEY_CTRL_TEMP` solo
se reescribe con el control encendido); ambas son notas, no bloqueantes.

## Consecuencias

- La motherBoard no cambia ni una línea. `PROTOCOL.md` intacto.
- `CommTask.cpp` gana un punto de decisión por cada `Communication_Send*`
  y uno en `Display_ApplyCtrlState()`. Las simulaciones viven junto al
  parser con comentario cruzado: si cambia el formato de una respuesta, hay
  que tocar los dos sitios.
- El estado que ve el alumno durante la lección es simulado (consignas,
  toggles), pero las **medidas** (temperaturas, humedad) siguen siendo las
  reales de la placa, porque la telemetría (`CTRL,TEL`) no se corta. Los
  textos de las lecciones lo dicen.
- **Deuda explícita**: si más adelante se quiere que la motherBoard sepa que
  hay formación (p. ej. para registrar en ThingsBoard "formación en curso"
  o para bloquear su propio botón físico), se pasa a la opción B como
  cambio de `shared/` aparte; este ADR no lo impide, la HMI ya tiene el
  interruptor centralizado.
- **Revisar este ADR si**: la placa deja de aceptar el estado repetido como
  keepalive, o si alguna respuesta nueva del protocolo deja de tener
  simulación (síntoma: un asistente se queda esperando en formación).
