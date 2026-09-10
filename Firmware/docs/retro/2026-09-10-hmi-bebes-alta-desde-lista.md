# Retro — registro de bebés desde Bebes y lección 1 (2026-09-10)

Rama `feat/hmi-bebes-alta-desde-lista`, worktree
`Firmware/.worktrees/hmi-bebes-alta-desde-lista`. Modalidad `auto`. Cambio
OpenSpec `hmi-bebes-alta-desde-lista` (delta sobre `baby-history-viewer` y
`hmi-training-courses`). Solo Display_HMI.

## Qué se hizo

La pantalla Bebes registra bebés nuevos (BEBE NUEVO → nombre → semanas →
peso o SIN PESO → REGISTRAR, único punto de envío; guarda de tres activos).
Los teclados del asistente salen a `ui/InputKeypad.{h,cpp}` y se comparten.
En formación el registro crea un segundo bebé de prácticas (`0xFFFE`) que
la lista y el asistente ofrecen junto a ZOE; la lección 1 de Enfermería se
reescribe alrededor de ese flujo. Build: +6,8 KB de flash, +72 B de RAM.

## Aprendizajes

### 1. La nota de persistencia de `PROTOCOL.md` describía una versión anterior del código

`PROTOCOL.md` decía que "hasta `PROFILE_WEIGHT` nada se persiste del
wizard", y la spec `baby-profile-storage` tiene un escenario con esa misma
idea, pero `babyStore_createProfile()` escribe el slot y `nextSeq` en NVS en
el propio `PROFILE_NEW`. La decisión de diseño de este cambio (SIN PESO no
manda `SKIP`; la X después del ACK no "deshace" nada) dependía de saberlo.
Se leyó el código antes de fiarse del documento, que es lo que ya pide
`tooling.md` para los artefactos que no escribiste tú.

**Aplicado**: la nota corregida en `PROTOCOL.md` (persiste en `PROFILE_NEW`;
`SKIP` no cambia nada). Descartado generalizar: la regla ya existe; lo que
faltaba era aplicarla al protocolo, y la corrección queda en el documento.

### 2. Un hook de simulación muerto es una pista de diseño, no basura

`Training_SimProfileNew()` existía desde la fase 1 de los cursos pero era
inalcanzable (el asistente rechaza BEBE NUEVO en formación). Reutilizarlo
para el bebé de prácticas registrado costó 30 líneas y dio la experiencia
realista que el usuario quería (ve aparecer el bebé que tecleó), frente a la
alternativa de interceptar REGISTRAR con un aviso. Antes de borrar un hook
sin llamadas, mirar si es la mitad de una feature que no llegó.

**Descartado** como regla: es una observación de una vez.

### 3. El sandbox de formación tenía una fuga de NVS por un módulo que no lo consulta

`Maintenance_Tick()` sigue `BabyWizard_GetActiveSeq()` y escribe en NVS
(`HMI_KEY_MNT_SEQ`, `HMI_KEY_MNT_TPEND`) cuando cambia; lo llama
`MaintenanceDialog_Poll()` **antes** de su propia comprobación de
`Training_IsActive()`. Con ZOE al mando ya escribía; con el segundo bebé de
prácticas también lo habría hecho. Es la misma familia que el punto 2 de la
retro anterior (efectos fuera del protocolo): la revisión de seguridad de
entonces enumeró `COMM_SERIAL`, `Preferences` y `WiFi.*`, pero un módulo que
escribe NVS a partir de un *estado* que la formación cambia (el seq al
mando) no aparece con ese grep.

**Aplicado**: guarda en `Maintenance_Tick()`; regla ampliada en
`embedded-display-hmi.md` (apartado "Modo formación"): enumerar también los
consumidores de `BabyWizard_GetActiveSeq()` / `HasLiveSession()` y de
cualquier estado que la formación virtualice, no solo las salidas.

### 4. Una respuesta compartida sin identificador es de quien la espere: descartar antes de pedir y al abandonar

`CTRL,PROFILE_ACK` no lleva nada con lo que comprobar a qué petición
responde, y `g_pendingProfileAck` lo consumen el asistente y Bebes. El
código nuevo descartaba lo pendiente **antes** de enviar, pero no **al
abandonar** una espera: un ACK del registro llegado tarde (enlace degradado
más de 3 s) quedaba puesto y el asistente lo adoptaba como el ACK de su
`PROFILE_SELECT`, con lo que rango, peso y minutos iban a otro bebé. Lo
cazó la revisión de seguridad siguiendo el flag entre módulos; el revisor
de código lo vio como riesgo preexistente. El patrón "ACK huérfano" ya
existía con el alta, pero allí el seq huérfano es uno archivado y el
asistente se atasca visiblemente; con el registro es un seq activo y todo
funciona en silencio sobre el bebé equivocado.

**Aplicado**: `discardPendingReplies()` en cada abandono de espera y
descarte justo antes de cada envío en los dos módulos. Regla nueva en
`embedded-display-hmi.md` (apartado "Respuestas compartidas"): quien espera
un `g_pending*` compartido lo descarta antes de pedir y al abandonar, y un
timeout que rellena datos por defecto no habilita acciones que dependan de
ellos (la guarda de tres activos con `count = 0` por timeout dejaba pasar el
`PROFILE_NEW` con los slots llenos: mismo hallazgo, misma raíz).

### 5. Compartir el teclado, no la pantalla

La tentación era reutilizar `buildInputStep()` entero del asistente (título,
textarea, ATRAS / SALTAR / CONTINUAR, teclado). Bebes no tiene SALTAR y su
última pantalla lleva SIN PESO / REGISTRAR; parametrizar tres botones para
dos usos habría costado más que duplicar 40 líneas de layout. Lo que sí se
comparte es lo que tiene reglas de seguridad (mapa sin coma, filtro, rango).

**Descartado** como regla: es el criterio habitual de extraer lo que tiene
una razón de ser propia; queda anotado en `design.md` decisión 3.

### 6. El revisor de seguridad volvió a encontrar lo que el de código no (y viceversa)

Tercera iteración seguida en que el par verify/security encuentra defectos
distintos: código señaló el fix sin commitear mezclado con docs, un include
huérfano y `design.md` desactualizado; seguridad, los tres hallazgos altos
(ACK huérfano, guarda negativa, `s_wizardSeq` con terapia en curso) y el
flanco falso del mantenimiento tras la lección. La regla de lanzarlos en
paralelo ya existe; se confirma. Descartado cambiar nada.

## Fricciones

- Ninguna de herramientas: worktree limpio compiló a la primera (~100 s la
  referencia, ~50 s incremental).
- Los dos revisores tardaron 10 y 12 minutos sobre un diff de ~900 líneas;
  asumible, pero conviene lanzarlos justo después del último commit de
  implementación y no esperar a tener las docs listas.
- El commit de la propuesta salió con `user.name` distinto del configurado
  (se puso a mano); se corrigió con `--reset-author` antes de seguir. Usar
  siempre la configuración del repo (`pablo18393`), sin `-c user.name`.
