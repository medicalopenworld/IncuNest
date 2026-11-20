# hmi-weigh-reminder Specification

## Purpose
TBD - created by archiving change hmi-weigh-reminder. Update Purpose after archive.
## Requirements
### Requirement: Se avisa si el bebé activo lleva 24 h sin pesada registrada

El HMI SHALL avisar de la pesada cuando se cumplan TODAS estas condiciones:

- hay sesión viva (`BabyWizard_HasLiveSession()`: identidad conocida y terapia
  en marcha ahora mismo);
- hay hora sincronizada (`HMI_GetEpochNow() != 0`);
- existe una referencia y han pasado 24 h o más desde ella
  (`ahora - referencia >= 24 h`);
- no hay un "más tarde" vigente.

La **referencia** SHALL ser, por este orden: el epoch de la última pesada
sellada para ESE `seq`; si no lo hay, el instante en que el HMI vio a ese
`seq` por primera vez al mando (persistido en NVS). Si no hay ninguno, no hay
referencia y no se avisa.

La decisión SHALL ser una función pura sin LVGL ni Arduino
(`modules/maintenance/weigh_reminder`), cubierta por la suite
`test/test_weigh_reminder` en `[env:native]`. Sin hora sincronizada NO SHALL
avisarse: no se inventan tiempos con `millis()`, que no sobreviven a un
reinicio. Una referencia en el futuro (reloj corregido hacia atrás) SHALL
tratarse como "no vencido".

#### Scenario: Justo antes de las 24 h no avisa
- **GIVEN** un bebé activo con sesión viva y última pesada hace 23 h 59 min 59 s
- **WHEN** se evalúa el aviso
- **THEN** no toca avisar
- *(Cubierto por `[env:native]`, `test_weigh_reminder`.)*

#### Scenario: A las 24 h exactas avisa
- **GIVEN** un bebé activo con sesión viva y última pesada hace exactamente 24 h
- **WHEN** se evalúa el aviso
- **THEN** toca avisar
- *(Cubierto por `[env:native]`.)*

#### Scenario: Sin hora no se avisa
- **GIVEN** un bebé activo cuya última pesada fue hace 3 días
- **WHEN** el HMI no tiene hora sincronizada
- **THEN** no toca avisar
- *(Cubierto por `[env:native]`.)*

#### Scenario: Sin sesión viva no se avisa
- **GIVEN** un bebé recordado pero sin terapia en marcha, con pesada de hace 3 días
- **WHEN** se evalúa el aviso
- **THEN** no toca avisar
- *(Cubierto por `[env:native]`.)*

#### Scenario: Sin sello usa el momento en que se vio al bebé
- **GIVEN** un bebé activo sin pesada sellada para su `seq`
- **WHEN** el HMI lo vio por primera vez hace 25 h
- **THEN** toca avisar
- **AND** si lo vio hace 2 h no toca avisar
- *(Cubierto por `[env:native]`.)*

#### Scenario: Un sello de otro bebé no cuenta
- **GIVEN** un sello de pesada de hace 3 días perteneciente a otro `seq`
- **WHEN** se evalúa el aviso del bebé activo, visto por primera vez hace 2 h
- **THEN** no toca avisar, porque la referencia es el "visto" y no el sello ajeno
- *(Cubierto por `[env:native]`.)*

#### Scenario: Referencia en el futuro
- **GIVEN** un sello posterior a la hora actual
- **WHEN** se evalúa el aviso
- **THEN** no toca avisar
- *(Cubierto por `[env:native]`.)*

#### Scenario: Un "más tarde" vigente calla el aviso
- **GIVEN** una pesada vencida y un snooze que expira dentro de 1 h
- **WHEN** se evalúa el aviso
- **THEN** no toca avisar
- **AND** al llegar el instante de expiración vuelve a tocar
- *(Cubierto por `[env:native]`.)*

### Requirement: Las pesadas se sellan con la hora UTC al enviarlas

El HMI SHALL sellar en la NVS (`hmi_cfg`: `wgh_seq` y `wgh_last`) el epoch UTC
y el `seq` de cada `HMI,PROFILE_WEIGHT` con un peso real (gramos > 0) para el
bebé que está al mando. El momento del sello depende de quién envía:

- El asistente (incluida la pesada de admisión, de modo que el sello de un
  bebé nuevo empiece con ella) y la pantalla Bebés no esperan respuesta de la
  placa: SHALL sellar AL ENVIAR (`Communication_SendProfileWeight()` con
  `stampOnSend=true`). Si la trama se pierde, el aviso saldrá de más, nunca de
  menos.
- El diálogo PESAR AHORA SHALL enviar con `stampOnSend=false` y sellar solo al
  recibir `CTRL,PROFILE_RANGE` con el mismo `seq` (la única respuesta a
  `PROFILE_WEIGHT`) en una ventana de 3 s. El diálogo NO SHALL sellar nunca sin
  esa confirmación.

Además:

- `SKIP` (0 g) NO SHALL sellar: no es una pesada.
- Hay un ÚNICO hueco de sello, del bebé al mando. Un peso para un `seq`
  distinto del activo (alta desde la pantalla Bebés) NO SHALL tocar ese
  sello. Al volver a un bebé cuyo sello se descartó, el plazo cuenta desde su
  "visto", no desde su última pesada real.
- En formación (`Training_IsActive()` o un `seq` de práctica) NO SHALL
  escribirse nada en NVS (ADR-0002).
- Registrar un peso SHALL cancelar el "más tarde" vigente.
- Sin hora en ese instante SHALL anotarse la pesada como pendiente de fecha y
  sellarse con la primera hora válida; mientras tanto no se avisa.
- Al dar de alta al bebé (`BabyWizard_GetActiveSeq()` pasa a 0) se SHALL borrar
  el sello, el "visto" y el "más tarde". Al pasar a un bebé distinto se SHALL
  descartar lo que no le pertenezca.
- Un sello o "visto" más de 60 s por delante de la hora actual SHALL
  reanclarse a ella; una diferencia menor SHALL dejarse como está.
- Si `Init()` no pudo abrir la NVS, el HMI NO SHALL escribir ninguna clave
  `wgh_*` en esa sesión, para no pisar valores guardados con los de fábrica.
  Ante una NVS virgen (espacio de nombres aún sin crear) SHALL reintentar la
  apertura en escritura.
- Las escrituras NVS SHALL hacerse fuera de `LVGL_Lock`.

La máquina de estados SHALL ser lógica pura con el reloj, el bebé al mando y la
formación inyectados (`weigh_state`), cubierta en `[env:native]` por
`test/test_weigh_state`.

Claves de `hmi_cfg` (≤ 15 caracteres): `wgh_seq`, `wgh_last`, `wgh_sseq`,
`wgh_sfirst`, `wgh_snooze`, `wgh_en`.

#### Scenario: La pesada de admisión inicia el sello
- **WHEN** el operador admite un bebé nuevo y teclea 1850 g en el asistente
- **THEN** `wgh_seq` es el `seq` del bebé y `wgh_last` el epoch UTC de ese momento
- *(Verificación manual en banco: leer las claves en el log serie `WGH`.)*

#### Scenario: SIN PESO no sella
- **WHEN** el operador pulsa SIN PESO en el asistente
- **THEN** `wgh_last` no cambia
- *(Cubierto por `[env:native]`, `test_weigh_state`, y manual en banco.)*

#### Scenario: El alta borra el sello
- **WHEN** el operador da de alta al bebé en el diálogo de salida
- **THEN** `wgh_seq`, `wgh_last`, `wgh_sseq`, `wgh_sfirst` y `wgh_snooze` quedan a 0
- *(Cubierto por `[env:native]`; confirmación de NVS manual en banco.)*

#### Scenario: Formación no escribe NVS
- **WHEN** el alumno registra un peso durante una lección
- **THEN** ninguna clave `wgh_*` cambia
- *(Cubierto por `[env:native]`; manual en banco.)*

#### Scenario: Reancla con tolerancia
- **WHEN** un sello está 61 s por delante de la hora actual
- **THEN** se reancla a la hora actual
- **AND** a 60 s de diferencia se deja como está
- *(Cubierto por `[env:native]`, `test_weigh_state`.)*

#### Scenario: NVS ilegible no se sobrescribe
- **WHEN** la NVS no se puede abrir al arrancar
- **THEN** no se escribe ninguna clave `wgh_*` durante la sesión
- *(Verificación manual en banco o por inspección: no hay entorno de test de NVS.)*

### Requirement: El aviso ofrece PESAR AHORA y MÁS TARDE

El aviso SHALL usar la misma tarjeta que el de mantenimiento (780x460, overlay
reutilizado), con el nombre del bebé, las horas transcurridas desde la
referencia y dos botones, sin X:

- **PESAR AHORA** abre en el mismo diálogo una entrada numérica (teclado de
  dígitos) para el bebé activo, con el rango 400-5000 g del asistente. Al
  registrar envía `HMI,PROFILE_WEIGHT`, muestra "Registrando el peso..." y
  espera hasta 3 s a `CTRL,PROFILE_RANGE` del mismo `seq`. Solo al recibirlo
  sella la pesada, cancela el "más tarde" y muestra "Peso registrado". Si no
  llega, SHALL mostrar un toast de error, NO sellar, y volver al aviso para
  reintentar. En formación SHALL rechazarse con el aviso estándar.
- **MÁS TARDE** calla el aviso 24 h (`wgh_snooze`, epoch UTC), y solo si hay
  hora.

Antes de enviar, el diálogo SHALL descartar `g_pendingProfileRange`. Una
respuesta de rango que llegue después de que el diálogo se haya cerrado o haya
agotado la espera SHALL descartarse en una ventana de 3 s; si en esa ventana
se abre el asistente, la pantalla Bebés o una lección, la ventana SHALL
cancelarse y `g_pendingProfileRange` SHALL limpiarse en ese instante, porque el
rezagado es del mismo bebé y el nuevo peticionario lo adoptaría.
Las cadenas SHALL existir en español, inglés, francés y portugués.

#### Scenario: PESAR AHORA registra y calla el aviso
- **GIVEN** el aviso abierto para un bebé con 26 h sin pesar
- **WHEN** el operador pulsa PESAR AHORA, teclea 1900 y pulsa REGISTRAR
- **THEN** el HMI envía `HMI,PROFILE_WEIGHT,<seq>,1900`
- **AND** al recibir `CTRL,PROFILE_RANGE` el aviso se cierra, el peso queda sellado y no vuelve a salir hasta pasadas 24 h
- *(Verificación manual en banco.)*

#### Scenario: Sin confirmación de la placa no se sella
- **GIVEN** el aviso abierto y el enlace con la placa caído
- **WHEN** el operador registra 1900 g
- **THEN** a los 3 s se muestra un toast de error y se vuelve al aviso
- **AND** `wgh_last` no cambia y el aviso no se calla
- *(Verificación manual en banco: desconectar la UART de la placa.)*

#### Scenario: Peso fuera de rango
- **WHEN** el operador teclea 300 o 6000 en la entrada
- **THEN** se muestra el error de rango y no se envía nada
- *(Verificación manual en banco.)*

#### Scenario: MÁS TARDE calla 24 h
- **WHEN** el operador pulsa MÁS TARDE
- **THEN** no vuelve a avisar en los siguientes desbloqueos durante 24 h
- *(Verificación manual en banco.)*

#### Scenario: El rango rezagado no lo adopta el asistente
- **GIVEN** un registro sin confirmar cuya respuesta llega tarde
- **WHEN** se abre el asistente dentro de los 3 s siguientes
- **THEN** `g_pendingProfileRange` queda limpio y el asistente no lo toma como suyo
- *(Verificación manual en banco.)*

#### Scenario: Idiomas
- **WHEN** el equipo está en cada una de las cuatro lenguas
- **THEN** el aviso y la entrada muestran sus cadenas en esa lengua sin texto cortado
- *(Verificación manual en banco.)*

### Requirement: El aviso de pesada espera su turno y cede la pantalla como el de mantenimiento

El aviso de pesada SHALL armarse con el mismo desbloqueo que el de
mantenimiento, con una bandera de armado propia e independiente, y SHALL
cumplir sus mismas reglas: abrirse solo en `ui_ScreenMain`, sin ningún otro
diálogo modal abierto (ayuda, tutorial, centro de alarmas, asistente de bebé,
diálogo de salida, ajuste de hora, prueba de fábrica y **pantalla Bebés**),
fuera de formación; cerrarse sin contestar ante cualquier alarma, enlace
perdido, cambio de pantalla o `MNT_IDLE_TIMEOUT_MS` sin toque.

Su armado SHALL consumirse la primera vez que se evalúe en un momento en que
pueda abrirse, toque o no, de modo que el aviso sea "al desbloquear" y no a
media tarde porque venza el plazo. NO SHALL consumirse mientras no haya bebé
al mando (`BabyWizard_GetActiveSeq() == 0`, p. ej. al encender), para que el
primer desbloqueo tras identificar al bebé sí avise.

Encadenado con el de mantenimiento:

- Si coinciden, SHALL salir primero mantenimiento y, una vez CONTESTADO, el de
  pesada, sin bucles.
- Un mantenimiento cerrado sin contestar por tope de inactividad o cambio de
  pantalla NO SHALL encadenar el de pesada.
- Un mantenimiento cerrado por alarma o enlace perdido SHALL conservar su
  armado para volver cuando la causa cese, y NO SHALL encadenar el de pesada
  entre medias. El aviso de pesada cerrado por alarma SHALL conservar el suyo
  igualmente.
- Un aviso abierto a mano desde Ajustes NO SHALL tocar ningún armado.

#### Scenario: No se cuela sobre la pantalla Bebés
- **GIVEN** una pesada vencida y el desbloqueo hecho
- **WHEN** la pantalla Bebés está abierta
- **THEN** el aviso espera y sale al cerrarla
- *(Verificación manual en banco.)*

#### Scenario: Cede ante una alarma y vuelve
- **WHEN** el aviso de pesada o el de mantenimiento está abierto y la placa anuncia una alarma
- **THEN** se cierra en la siguiente pasada sin contar como respuesta
- **AND** vuelve a salir cuando la alarma cesa
- *(Verificación manual en banco.)*

#### Scenario: Mantenimiento y pesada vencidos a la vez
- **WHEN** ambos están vencidos y se desbloquea
- **THEN** sale el de mantenimiento; al pulsar MÁS TARDE sale el de pesada
- *(Verificación manual en banco.)*

#### Scenario: Mantenimiento cerrado por inactividad no encadena la pesada
- **WHEN** ambos están vencidos y el de mantenimiento se cierra por el tope de 3 min
- **THEN** el de pesada no se abre
- *(Verificación manual en banco.)*

#### Scenario: Arranque sin bebé identificado
- **GIVEN** un equipo recién encendido con una pesada que vencerá
- **WHEN** aún no hay bebé al mando
- **THEN** el armado de pesada no se consume
- **AND** el primer desbloqueo tras identificar al bebé avisa si toca
- *(Verificación manual en banco.)*

### Requirement: Un interruptor en Ajustes activa o desactiva el aviso de pesada

Ajustes > Mantenimiento SHALL incluir, bajo el interruptor de los avisos de
limpieza, un interruptor "AVISO DE PESADA" con textos en español, inglés,
francés y portugués. Su estado SHALL persistir en la clave `wgh_en` de
`hmi_cfg`, con valor por defecto ACTIVADO también en una NVS virgen (es un
aviso clínico, a diferencia de `mnt_en`). Desactivado, el aviso NO SHALL salir,
pero los sellos SHALL seguir anotándose. Volver a activarlo SHALL borrar un
"más tarde" anterior. En formación (`Training_IsActive()`) el cambio SHALL
rechazarse con el toast estándar y el interruptor SHALL volver a su posición
real, porque escribe NVS (ADR-0002).

#### Scenario: Activado por defecto
- **WHEN** una unidad con NVS virgen arranca
- **THEN** el interruptor está activado y el aviso puede salir
- *(Verificación manual en banco.)*

#### Scenario: Desactivado no avisa
- **GIVEN** una pesada vencida con el interruptor desactivado
- **WHEN** se desbloquea la pantalla
- **THEN** no sale el aviso
- *(Cubierto por `[env:native]`, `test_weigh_state`, para la decisión; manual en banco para la UI.)*

#### Scenario: Rechazado en formación
- **WHEN** el alumno pulsa el interruptor durante una lección
- **THEN** sale el toast estándar y el interruptor vuelve a su posición
- *(Verificación manual en banco.)*

