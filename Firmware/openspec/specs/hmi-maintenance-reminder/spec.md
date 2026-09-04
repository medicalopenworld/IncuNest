# hmi-maintenance-reminder Specification

## Purpose

Que la incubadora recuerde al personal cada cuánto hay que hacerle
mantenimiento (limpieza y desinfección) y se lo diga con un aviso en pantalla
que lleve el QR de los tutoriales, en vez de depender de que alguien lleve la
cuenta por su cuenta.

Afecta solo a `Display_HMI`. No toca el protocolo serie ni la `motherBoard`:
el HMI ya recibe de la placa el epoch (`CTRL,TIME`) y ya sabe qué bebé está al
mando (`BabyWizard_GetActiveSeq()`), que es todo lo que hace falta.

## Requirements

### Requirement: El recordatorio se dispara por plazo de calendario

El HMI SHALL registrar en NVS (`hmi_cfg`, clave `mnt_last`) el epoch UTC del
último mantenimiento registrado, y SHALL considerar que el plazo se ha
cumplido cuando hayan pasado `mnt_days` días de CALENDARIO desde esa fecha.

El plazo se mide con el reloj de la placa (`HMI_GetEpochNow()`) y NO con horas
de funcionamiento: una incubadora que ha estado un mes apagada necesita
limpieza igual antes de volver a usarse.

Mientras el HMI no tenga hora válida (`HMI_GetEpochNow()` devuelve 0) NO SHALL
haber aviso por plazo: sin reloj no hay nada que medir.

En el primer arranque con hora válida, si no hay ningún mantenimiento
registrado, el HMI SHALL sembrar `mnt_last` con la fecha de ese momento. Un
equipo recién fabricado tendría `mnt_last = 0`, es decir "vencido desde 1970",
y el aviso saldría en la primera pantalla que viese el operador.

Si el reloj se mueve hacia atrás y `mnt_last` queda en el futuro, el HMI SHALL
reanclar `mnt_last` a la fecha actual. Un registro en el futuro dejaría el
plazo sin cumplirse nunca.

#### Scenario: Se cumple el plazo y sale el aviso
- **WHEN** el intervalo configurado es de 30 días
- **AND** han pasado 31 días desde el último mantenimiento registrado
- **AND** el operador desbloquea la pantalla
- **THEN** sale el aviso de mantenimiento, con los días transcurridos y la
  fecha del último registro
- *(Verificación manual: Display_HMI no tiene entorno de test. En banco se
  provoca ajustando la hora del equipo con el diálogo del reloj.)*

#### Scenario: Sin hora válida no hay aviso por plazo
- **WHEN** la placa todavía no ha difundido un epoch sincronizado
- **THEN** no sale ningún aviso por plazo, y en cuanto llegue la hora el plazo
  empieza a contar desde ese momento si no había registro previo
- *(Verificación manual: arranque con la placa sin hora.)*

#### Scenario: Reloj movido hacia atrás
- **WHEN** el operador ajusta la fecha a un año anterior por error
- **AND** el último mantenimiento registrado queda en el futuro
- **THEN** el registro se reancla a la fecha actual y el plazo vuelve a contar
  desde ahí, en vez de quedarse sin vencer nunca
- *(Verificación manual: ajustar la hora hacia atrás y mirar el log `MNT`.)*

### Requirement: El recordatorio se dispara también con un bebé nuevo

El HMI SHALL avisar además cada vez que ponga al mando un perfil de bebé
distinto del último por el que ya avisó — alta de un paciente nuevo o cambio
de un bebé a otro — con independencia del plazo. Es la limpieza entre
pacientes.

El perfil por el que ya se avisó SHALL persistirse en NVS (`mnt_seq`), para
que un reinicio con el MISMO bebé dentro no vuelva a avisar.

Con el recordatorio desactivado (`mnt_days == 0`) NO SHALL avisar por ningún
motivo, tampoco por bebé nuevo: quien lo apaga no quiere unos avisos y no
otros.

#### Scenario: Alta de un bebé nuevo
- **WHEN** el operador activa el control de temperatura y el asistente da de
  alta un bebé distinto del anterior
- **AND** la pantalla se desbloquea después
- **THEN** sale el aviso de limpieza entre pacientes (texto propio, distinto
  del de plazo cumplido)
- *(Verificación manual en banco: alta de un bebé nuevo con el asistente.)*

#### Scenario: Reinicio con el mismo bebé dentro
- **WHEN** el equipo se reinicia y el operador vuelve a seleccionar el MISMO
  bebé que ya estaba
- **THEN** no sale ningún aviso por bebé nuevo
- *(Verificación manual en banco: reinicio y reselección del mismo perfil.)*

#### Scenario: Recordatorio desactivado
- **WHEN** el intervalo está en DESACTIVADO en Ajustes
- **THEN** no sale el aviso ni por plazo cumplido ni por bebé nuevo
- *(Verificación manual en banco.)*

### Requirement: El aviso lleva el QR de los tutoriales y dos respuestas

El aviso SHALL ser un diálogo modal sobre `ui_ScreenMain` con la misma tarjeta
que el menú de ayuda (780x460), y SHALL mostrar un QR de
`SUPPORT_TUTORIAL_URL` — el MISMO código que la vista "Vídeo tutorial" del
menú de ayuda, porque los tutoriales de limpieza viven en esa página.

El aviso SHALL ofrecer exactamente dos salidas y ninguna X:

- **MANTENIMIENTO HECHO**: registra la fecha de hoy en `mnt_last`, borra el
  "más tarde" y da por avisado al bebé actual.
- **MÁS TARDE**: calla los avisos durante 24 h (`mnt_snooze`).

Sin hora válida, MANTENIMIENTO HECHO SHALL callar el aviso pero NO SHALL
fechar nada: el registro sigue vacío y se muestra como "sin registrar".
Inventar una fecha sería peor que no tener ninguna.

Todos los textos SHALL salir del catálogo (`ui/i18n_strings.def`) en los
cuatro idiomas del equipo.

#### Scenario: Registrar el mantenimiento desde el aviso
- **WHEN** el operador pulsa MANTENIMIENTO HECHO
- **THEN** el aviso se cierra, sale un toast de confirmación, y la fecha de
  hoy queda registrada — el siguiente aviso por plazo no llega hasta que
  vuelvan a pasar `mnt_days` días
- *(Verificación manual en banco.)*

#### Scenario: Aplazar el aviso
- **WHEN** el operador pulsa MÁS TARDE
- **THEN** el aviso se cierra y no vuelve a salir durante 24 h, ni por plazo
  ni por el bebé que lo provocó
- *(Verificación manual en banco: aplazar y desbloquear varias veces.)*

#### Scenario: Registrar sin reloj
- **WHEN** el operador pulsa MANTENIMIENTO HECHO sin hora válida en el equipo
- **THEN** el aviso se calla pero la fecha del último mantenimiento sigue
  siendo "sin registrar"
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
desbloqueo, porque solo pulsar un botón lo calla.

El tope SHALL medirse desde el instante de apertura (`lv_tick_elaps()`) y NO
con `lv_disp_get_inactive_time()`: la propia exención del auto-bloqueo
reinicia ese contador cada 200 ms, así que un tope que lo leyera no llegaría
nunca.

#### Scenario: No se cuela encima de otro diálogo
- **WHEN** hay motivo de aviso pendiente y el operador desbloquea la pantalla
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

### Requirement: El intervalo se configura por equipo desde Ajustes

`ui_ScreenSettings` SHALL ofrecer una fila **MANTENIMIENTO**, debajo de MODOS,
con un panel propio en la misma columna que los demás (x=185). El panel SHALL
permitir elegir el intervalo entre DESACTIVADO, 7, 15, 30 y 90 días, SHALL
mostrar la fecha del último mantenimiento registrado, y SHALL ofrecer un botón
para registrar un mantenimiento sin esperar al aviso.

El intervalo SHALL persistirse en NVS (`mnt_days`) y el valor de fábrica SHALL
ser de 30 días. Un valor guardado que ya no esté en la lista SHALL caer al de
fábrica en vez de dejar el desplegable sin ninguna opción marcada.

Cambiar el intervalo SHALL borrar el "más tarde" vigente: el operador acaba de
decidir cada cuánto quiere el aviso, y la respuesta a la pregunta anterior ya
no vale.

Abrir la fila MANTENIMIENTO SHALL ocultar los paneles de las otras filas, y
abrir cualquier otra fila SHALL ocultar el de mantenimiento — misma regla que
ya cumplen Info, WiFi, Idioma y Modos entre ellas.

#### Scenario: Cambiar el intervalo
- **WHEN** el operador elige 7 DÍAS en el desplegable de la fila
  MANTENIMIENTO
- **THEN** el intervalo queda guardado y sobrevive a un reinicio
- **AND** un "más tarde" que estuviera vigente deja de aplicarse
- *(Verificación manual en banco: cambiar, reiniciar y volver a mirar.)*

#### Scenario: Registrar el mantenimiento a mano
- **WHEN** el operador acaba de limpiar la incubadora y pulsa el botón de
  registrar en la fila MANTENIMIENTO
- **THEN** la fecha del último mantenimiento pasa a ser la de hoy y el plazo
  vuelve a contar desde ahí
- *(Verificación manual en banco.)*

#### Scenario: Las filas de Ajustes no se pisan
- **WHEN** el operador abre MANTENIMIENTO y luego MODOS (o al revés)
- **THEN** solo se ve el panel de la fila abierta, y la columna de menú de
  Ajustes sigue visible a la izquierda
- *(Verificación manual en banco, tocando las cinco filas en cualquier orden.)*
