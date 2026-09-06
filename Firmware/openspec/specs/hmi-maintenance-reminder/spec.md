# hmi-maintenance-reminder Specification

## Purpose

Que la incubadora recuerde al personal el protocolo de limpieza y
desinfección — sus tres niveles y sus plazos — y se lo diga con un aviso en
pantalla que lleve el QR de los tutoriales, en vez de depender de que alguien
lleve la cuenta por su cuenta.

Afecta solo a `Display_HMI`. No toca el protocolo serie ni la `motherBoard`:
el HMI ya recibe de la placa el epoch (`CTRL,TIME`) y ya sabe qué bebé está al
mando (`BabyWizard_GetActiveSeq()`), que es todo lo que hace falta.

## Requirements

### Requirement: El protocolo tiene tres niveles con cadencia fija

El recordatorio SHALL manejar tres niveles, con estas cadencias, que NO son
configurables — son el protocolo de limpieza del equipo, no una preferencia:

| Nivel | Vence cuando |
|---|---|
| **DIARIA** | hay paciente dentro y ha pasado 1 día desde el último registro diario |
| **SEMANAL** | han pasado 7 días desde el último registro semanal |
| **TERMINAL** | hay un alta de paciente pendiente, o han pasado 7 días desde el último registro terminal |

"Paciente dentro" SHALL ser el criterio de `BabyWizard_HasLiveSession()`
(identidad conocida Y terapia en marcha ahora mismo), no un perfil recordado:
una incubadora vacía no se ensucia a diario.

El "o antes si hay suciedad visible" de la semanal es criterio del operador y
no algo que el firmware pueda detectar: SHALL aparecer como texto en el aviso,
y el botón de registrar SHALL estar disponible siempre, para poder anotar una
limpieza adelantada.

Los niveles se contienen: registrar un nivel SHALL poner al día también los
menos profundos (terminal ⊃ semanal ⊃ diaria). Si acaba de hacerse la
terminal, seguir pidiendo la diaria sería ruido.

Cada nivel SHALL persistir en NVS la fecha de su último registro
(`mnt_daily`, `mnt_weekly`, `mnt_term`), en epoch UTC.

#### Scenario: La diaria solo con paciente dentro
- **WHEN** ha pasado más de un día desde el último registro diario
- **AND** no hay terapia en marcha para ningún bebé
- **THEN** la diaria no aparece como vencida
- **AND** en cuanto se activa la terapia para un bebé, sí
- *(Verificación manual: Display_HMI no tiene entorno de test. Banco.)*

#### Scenario: Registrar la terminal pone al día las otras dos
- **WHEN** las tres están vencidas y el operador pulsa HECHO en la terminal
- **THEN** las tres pasan a estar al día con la fecha de hoy
- *(Verificación manual en banco.)*

#### Scenario: Registrar la diaria no toca las otras dos
- **WHEN** las tres están vencidas y el operador pulsa HECHO en la diaria
- **THEN** solo la diaria queda al día; semanal y terminal siguen vencidas
- *(Verificación manual en banco.)*

### Requirement: El alta del paciente deja pendiente una limpieza terminal

El HMI SHALL dar por vencida la limpieza terminal cuando el perfil que tiene
al mando (`BabyWizard_GetActiveSeq()`) pase de un bebé a ninguno — el alta que
da el diálogo de salida, vía `BabyWizard_ClearActiveProfile()` — o de un bebé
a otro distinto, que es cambiar de paciente sin pasar por el alta.

Empezar con el primer paciente (de ninguno a un bebé) NO SHALL dejar nada
pendiente: ahí no ha salido nadie.

Ese "terminal pendiente" SHALL persistirse en NVS (`mnt_tpend`): el equipo
puede apagarse entre el alta y la limpieza, y no depende de que haya reloj.

#### Scenario: Alta del paciente
- **WHEN** el diálogo de salida da el alta al bebé que estaba dentro
- **AND** la pantalla se desbloquea después
- **THEN** el aviso sale con la terminal marcada como TOCA AHORA
- *(Verificación manual en banco: apagar los controles y dar el alta.)*

#### Scenario: Cambio de un bebé a otro
- **WHEN** el asistente pone al mando un bebé distinto del que estaba
- **THEN** queda pendiente una limpieza terminal
- *(Verificación manual en banco.)*

#### Scenario: El pendiente sobrevive al reinicio
- **WHEN** se da el alta y el equipo se reinicia antes de registrar la
  limpieza
- **THEN** al arrancar la terminal sigue pendiente
- *(Verificación manual en banco: alta, reinicio y desbloqueo.)*

### Requirement: El aviso muestra los tres niveles, con el QR y su registro

El aviso SHALL ser un diálogo modal sobre `ui_ScreenMain` con la misma tarjeta
que el menú de ayuda (780x460), y SHALL mostrar un QR de
`SUPPORT_TUTORIAL_URL` — el MISMO código que la vista "Vídeo tutorial" del
menú de ayuda, porque los tutoriales de limpieza viven en esa página.

SHALL mostrar SIEMPRE los tres niveles, vencidos o no, cada uno con su
cadencia, la fecha de su último registro, su estado (TOCA AHORA / al día) y su
propio botón HECHO. El operador tiene que poder ver el protocolo completo, no
solo lo que le toca hoy.

Registrar un nivel NO SHALL cerrar el aviso: SHALL repintarlo con las fechas
nuevas. La diaria y la semanal vencen juntas cada 7 días y el operador puede
haber hecho las dos.

El aviso SHALL ofrecer un único botón de salida y ninguna X: **MÁS TARDE**
(calla los tres niveles 24 h) cuando hay algo vencido, y **CERRAR** cuando no
lo hay — abierto a mano, o con todo ya registrado. Un recordatorio que se
puede cerrar sin contestar no deja constancia de nada.

Sin hora válida, HECHO SHALL callar el aviso pero NO SHALL fechar nada: el
registro sigue mostrándose como "sin registrar". Inventar una fecha sería peor
que no tener ninguna.

Todos los textos SHALL salir del catálogo (`ui/i18n_strings.def`) en los
cuatro idiomas del equipo.

#### Scenario: El aviso enseña el protocolo completo
- **WHEN** sale el aviso porque vence la semanal
- **THEN** se ven las tres líneas —diaria, semanal y terminal— con su cadencia
  y su fecha, y solo la semanal marcada como TOCA AHORA
- *(Verificación manual en banco.)*

#### Scenario: Registrar dos niveles seguidos
- **WHEN** la diaria y la semanal están vencidas y el operador pulsa HECHO en
  las dos, una detrás de otra
- **THEN** el aviso sigue abierto y se repinta tras cada registro
- **AND** al no quedar nada vencido, el botón de abajo pasa de MÁS TARDE a
  CERRAR
- *(Verificación manual en banco.)*

#### Scenario: Aplazar el aviso
- **WHEN** el operador pulsa MÁS TARDE
- **THEN** el aviso se cierra y no vuelve a salir durante 24 h, por ningún
  nivel
- *(Verificación manual en banco: aplazar y desbloquear varias veces.)*

#### Scenario: Registrar sin reloj
- **WHEN** el operador pulsa HECHO sin hora válida en el equipo
- **THEN** el aviso se calla pero la fecha de ese nivel sigue siendo "sin
  registrar"
- *(Verificación manual: arranque sin hora de la placa.)*

### Requirement: El aviso espera su turno y cede la pantalla

El aviso SHALL armarse solo al DESBLOQUEAR la pantalla (y una vez en la
inicialización de la UI, porque quien enciende el equipo está delante), nunca
en medio de una maniobra.

Estando armado, SHALL abrirse únicamente cuando la pantalla activa sea
`ui_ScreenMain` y NINGÚN otro diálogo modal esté abierto (menú de ayuda,
tutorial guiado, centro de alarmas, asistente de bebé, diálogo de salida o
ajuste de hora).

El aviso SHALL ceder la pantalla con el mismo criterio que los overlays de
solo lectura (`TelemetryHistory`, `HelpDialog`): cualquier alarma activa
(`UI_IsAnyAlarmActive()`, sin filtrar por prioridad) o la pérdida del enlace
con la placa (`Display_IsBoardLinkLost()`) lo cierran en la siguiente pasada
del bucle de UI. No lleva información de alarma propia que compense tapar la
pantalla.

Mientras el aviso esté abierto, la inactividad NO SHALL llevar la pantalla a
`ui_ScreenLock` durante `MNT_IDLE_TIMEOUT_MS` (3 min): leer el QR con el móvil
lleva más de los 20 s del auto-bloqueo. Pasado ese tope el aviso SHALL
cerrarse solo, SIN contar como respuesta — volverá a salir en el siguiente
desbloqueo si sigue habiendo algo vencido, porque solo pulsar un botón lo
calla.

El tope SHALL medirse desde el instante de apertura (`lv_tick_elaps()`) y NO
con `lv_disp_get_inactive_time()`: la propia exención del auto-bloqueo
reinicia ese contador cada 200 ms, así que un tope que lo leyera no llegaría
nunca.

#### Scenario: No se cuela encima de otro diálogo
- **WHEN** hay algo vencido y el operador desbloquea la pantalla
- **AND** el asistente de bebé o el diálogo de salida está abierto
- **THEN** el aviso espera, y sale cuando ese diálogo se cierra
- *(Verificación manual en banco.)*

#### Scenario: Cierre por alarma o enlace perdido
- **WHEN** el aviso está abierto
- **AND** la placa anuncia una alarma de cualquier prioridad, o se pierde el
  enlace con la placa
- **THEN** el aviso se cierra en la siguiente pasada del bucle de UI y no se
  reabre mientras la condición dure
- *(Verificación manual en banco: provocar una alarma con el aviso abierto.)*

#### Scenario: Sin auto-bloqueo con el aviso abierto, con tope
- **WHEN** el aviso lleva abierto más de `INACTIVITY_TIMEOUT_MS` (20 s) pero
  menos de `MNT_IDLE_TIMEOUT_MS` (3 min) sin ningún toque
- **THEN** la pantalla NO se bloquea
- **AND** pasados los 3 min el aviso se cierra solo, el auto-bloqueo vuelve a
  contar, y el aviso vuelve a salir en el siguiente desbloqueo
- *(Verificación manual en banco, cronómetro en mano.)*

### Requirement: En Ajustes solo se activan o desactivan los avisos

`ui_ScreenSettings` SHALL ofrecer una fila **MANTENIMIENTO**, debajo de MODOS,
con un panel propio en la misma columna que los demás (x=185). El panel SHALL
contener:

- un **interruptor** único de avisos, y nada más de configuración: los plazos
  no se eligen;
- los tres niveles en **solo lectura**, con su cadencia y la fecha de su
  último registro;
- un botón **VER RECORDATORIO** que abre el propio pop-up, que es donde están
  los tres botones HECHO. No SHALL haber un segundo sitio donde registrar lo
  mismo.

Con los avisos desactivados NO SHALL salir el aviso por ningún nivel: quien lo
apaga no quiere unos avisos y no otros. Las fechas SHALL seguir registrándose
igual.

El estado del interruptor SHALL persistirse en NVS (`mnt_en`) y el valor de
fábrica SHALL ser activado. Volver a activarlo SHALL borrar un "más tarde"
vigente: no tiene sentido arrastrar el aplazamiento de hace tres semanas.

Abrir la fila MANTENIMIENTO SHALL ocultar los paneles de las otras filas, y
abrir cualquier otra fila SHALL ocultar el de mantenimiento — misma regla que
ya cumplen Info, WiFi, Idioma y Modos entre ellas.

#### Scenario: Desactivar los avisos
- **WHEN** el operador desactiva el interruptor
- **AND** hay niveles vencidos
- **THEN** no sale ningún aviso al desbloquear
- **AND** el ajuste sobrevive a un reinicio
- *(Verificación manual en banco: desactivar, reiniciar y desbloquear.)*

#### Scenario: Registrar a mano desde Ajustes
- **WHEN** el operador acaba de limpiar la incubadora y pulsa VER
  RECORDATORIO
- **THEN** se abre el pop-up con los tres niveles y sus botones HECHO
- **AND** el botón de abajo es CERRAR, no MÁS TARDE, si no hay nada vencido
- *(Verificación manual en banco.)*

#### Scenario: Las filas de Ajustes no se pisan
- **WHEN** el operador abre MANTENIMIENTO y luego MODOS (o al revés)
- **THEN** solo se ve el panel de la fila abierta, y la columna de menú de
  Ajustes sigue visible a la izquierda
- *(Verificación manual en banco, tocando las cinco filas en cualquier orden.)*
