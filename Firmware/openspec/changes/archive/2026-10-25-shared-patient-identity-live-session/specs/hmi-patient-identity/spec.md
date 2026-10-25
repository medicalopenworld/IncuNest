## ADDED Requirements

### Requirement: La sesión viva exige una identidad confirmada

El HMI SHALL considerar «sesión viva» (`BabyWizard_HasLiveSession()`) solo cuando
hay un bebé conocido, alguna terapia en marcha **y la identidad confirmada** por
la placa. Toda ruta que actúe sobre el `seq` hacia la placa SHALL pasar por esa
condición: el alta (`PROFILE_DISCHARGE`) del diálogo de salida, el reenvío de
`PROFILE_SELECT` tras registrar un bebé en la pantalla de bebés, el atajo
`selectExisting` del asistente y el gating de los asistentes de humedad y
fototerapia. `BabyExitDialog_Tick` SHALL además comprobar la confirmación por sí
mismo antes de ofrecer el alta.

Lo que solo necesita saber que hay un paciente dentro, sea quien sea, SHALL
usar `BabyWizard_HasAnySession()` (bebé conocido y terapia en marcha, sin exigir
confirmación), y SHALL NOT actuar sobre el `seq` hacia la placa: el recordatorio
de limpieza diaria y el pintado gris de una identidad sin confirmar.

#### Scenario: Reinicio con terapia en marcha y la placa aún sin confirmar
- **GIVEN** una identidad restaurada con `seq` 7, sin confirmar, y una terapia en
  marcha
- **WHEN** la terapia se detiene
- **THEN** el diálogo de salida no se ofrece y no se envía `PROFILE_DISCHARGE`

#### Scenario: Registrar un bebé con una identidad sin confirmar
- **WHEN** se registra un bebé nuevo en la pantalla de bebés
- **AND** la identidad restaurada no está confirmada
- **THEN** no se reenvía `PROFILE_SELECT` con ese `seq`

#### Scenario: Activar otra terapia sin confirmar
- **WHEN** el operador activa una terapia y la identidad no está confirmada
- **THEN** el asistente muestra el selector de bebés, no el atajo

#### Scenario: Limpieza diaria
- **WHEN** hay una terapia en marcha con un bebé conocido sin confirmar
- **THEN** el recordatorio de limpieza diaria sí cuenta que hay un paciente dentro

*Cobertura: la regla es `patient_live_session()`, lógica pura en `[env:native]`
del HMI. Los usuarios (diálogo de salida, pantalla de bebés, asistente) son
verificación manual en banco.*

### Requirement: Descartar una identidad sin confirmar no es un alta

Cuando el HMI descarte una identidad que la placa nunca confirmó, SHALL
avisarlo al recordatorio de mantenimiento (`Maintenance_NoteSeqDropped()`) antes
de limpiar el perfil recordado, de modo que la transición de ese `seq` a 0 SHALL
NOT dejar una limpieza terminal pendiente. La excusa SHALL valer para una sola
transición, SHALL cubrir solo el paso de ese mismo `seq` a «ninguno», y SHALL NOT
ocultar el alta real de otro bebé. Descartar una identidad que sí se había
confirmado SHALL seguir contando como alta.

#### Scenario: Otra placa
- **GIVEN** una identidad restaurada con `seq` 7, sin confirmar
- **WHEN** el HMI la descarta porque la placa tiene otro bebé al mando
- **THEN** no queda limpieza terminal pendiente

#### Scenario: Alta real
- **WHEN** el bebé `seq` 7 se da de alta y no hubo descarte
- **THEN** queda una limpieza terminal pendiente

#### Scenario: Descarte de otro seq
- **WHEN** se descartó el `seq` 3 y lo que se va ahora es el 7
- **THEN** el paso del 7 a «ninguno» sigue siendo un alta

*Cobertura: `mnt_seq_decide()` es lógica pura en `[env:native]` del HMI
(`test_maintenance_seq`). La anotación desde `patient_state` es verificación
manual.*

### Requirement: La petición de lista del HMI caduca y se reintenta

El HMI SHALL dar por caducada la petición propia de `PROFILE_LIST` con la que contrasta una identidad o
fija unos minutos si no hay respuesta en 10 s, y SHALL
reintentarse hasta un máximo de 3 intentos, con el enlace vivo y sin el asistente
ni la pantalla de bebés abiertos. Un intento en vuelo SHALL NOT bloquear los
reintentos más allá de ese plazo, y el cálculo SHALL resistir la vuelta del
contador de milisegundos.

#### Scenario: Se pierde la respuesta
- **WHEN** pasan 10 s sin respuesta a la petición
- **THEN** el HMI pide la lista de nuevo

#### Scenario: Intentos agotados
- **WHEN** ya se han hecho 3 peticiones
- **THEN** no se pide una cuarta

#### Scenario: Petición en vuelo
- **WHEN** hay una petición sin contestar de hace menos de 10 s
- **THEN** no se pide otra

*Cobertura: `patient_list_request_due()` es lógica pura en `[env:native]` del
HMI.*

### Requirement: El descarte se aplaza con el asistente abierto

El HMI SHALL NOT descartar una identidad mientras el asistente de bebé esté
abierto: el bebé en curso del asistente SHALL conservarse. El descarte SHALL
quedar pendiente y aplicarse al cerrarse el asistente, y SHALL anularse si el
propio asistente confirma o sustituye la identidad entretanto.

#### Scenario: La placa contradice la identidad con el asistente abierto
- **WHEN** la placa contradice la identidad restaurada mientras el asistente
  está abierto
- **THEN** el alta en curso no se interrumpe
- **AND** el descarte se aplica al cerrarse el asistente

#### Scenario: El asistente confirma al bebé
- **WHEN** un `PROFILE_ACK` del asistente confirma el mismo bebé mientras había un
  descarte pendiente
- **THEN** el descarte se anula

*Verificación manual en banco.*

### Requirement: Contraste de lista tras confirmar por activeSeq

Tras confirmar una identidad restaurada por `activeSeq`, el HMI SHALL contrastar
su nombre y semanas con la siguiente `PROFILE_LIST` que llegue, propia o de
cualquier consumidor, y SHALL descartarla si el `seq` no figura con el mismo
nombre y semanas: un `seq` se reutiliza tras `babyStore_wipeAll()`, que reinicia
la cuenta en 1. Una identidad nacida de un `PROFILE_ACK` SHALL NOT necesitar este
contraste.

#### Scenario: Seq reutilizado tras borrar la placa
- **GIVEN** una identidad restaurada «Lucia» `seq` 3, confirmada por `activeSeq` 3
- **WHEN** la lista trae el `seq` 3 con el nombre «Marta»
- **THEN** la identidad se descarta

#### Scenario: Coincide
- **WHEN** la lista trae el `seq` 3 como «Lucia» con las mismas semanas
- **THEN** la identidad sigue confirmada

*Cobertura: el veredicto es `patient_list_has_baby()`, lógica pura en
`[env:native]` del HMI. El flujo (petición, descarte) es verificación manual.*

### Requirement: Un número de serie de 0 no es verificable

Un `sn` de 0, en el registro guardado o en la placa conectada, SHALL tratarse
como no verificable: SHALL NOT interpretarse como «otra placa» ni como «la misma».
Con un `sn` no verificable, `activeSeq` por sí solo SHALL NOT confirmar la
identidad: SHALL exigirse además el contraste de nombre y semanas con la lista, y
hasta entonces la identidad SHALL permanecer oculta. Con el enlace caído, el HMI
SHALL NOT decidir nada con el último `CTRL,STATE`, que está obsoleto.

#### Scenario: Identidad guardada sin sn
- **WHEN** el registro tiene `sn` 0 y la placa conectada tiene `sn` 555 y
  `activeSeq` igual al `seq`
- **THEN** no se descarta por `sn`
- **AND** no se confirma hasta que la lista traiga al bebé

#### Scenario: Placa sin sn
- **WHEN** la placa conectada tiene `sn` 0
- **THEN** pasa lo mismo: contraste de lista obligatorio

#### Scenario: Sigue valiendo el resto de la tabla
- **WHEN** el `sn` no es verificable y `activeSeq` es otro bebé
- **THEN** la identidad se descarta

#### Scenario: Enlace caído
- **WHEN** el enlace con la placa está perdido
- **THEN** no se confirma ni se descarta ninguna identidad

*Cobertura: `patient_confirm_decide()` y `patient_confirm_requires_list()` son
lógica pura en `[env:native]` del HMI. El enlace caído es verificación manual.*

## MODIFIED Requirements

### Requirement: Una identidad restaurada se confirma con la placa

Una identidad restaurada de la NVS SHALL nacer **sin confirmar**. El HMI SHALL
mostrarla como confirmada solo cuando la placa lo confirme, y SHALL NOT mostrar
ni identidad ni minutos mientras no lo esté, salvo el caso de placa antigua
(más abajo). Una identidad que proviene de un `PROFILE_ACK` del asistente SHALL
nacer confirmada.

Con una placa que manda `activeSeq` (campo 24 de `CTRL,STATE`), y una vez
recibido el primer `CTRL,STATE`:

| Situación | Resultado |
|---|---|
| el `sn` de la placa y el guardado son ambos distintos de 0 y distintos entre sí | se borra |
| `activeSeq == seq` | se confirma (con `sn` verificable; si no, ver «Un número de serie de 0 no es verificable») |
| `activeSeq != 0` y `!= seq` | se borra |
| `activeSeq == 0` y la placa informa de terapia | se borra |
| `activeSeq == 0` y sin terapia | se espera |

Con una placa que **no** manda el campo, el HMI SHALL pedir `PROFILE_LIST` con el
enlace vivo, con caducidad y reintentos (ver «La petición de lista del HMI caduca
y se reintenta»). Si el bebé no está en la lista, o está con otro nombre u otras
semanas, se borra. Si está, la identidad SHALL mostrarse **en gris y con «?» tras
el nombre**, sin edad ni minutos, nunca como confirmada.

Borrar la identidad SHALL borrar también sus acumulados, en RAM y en NVS.

#### Scenario: Reinicio del HMI, mismo bebé
- **GIVEN** una identidad restaurada con `seq` 7
- **WHEN** llega un `CTRL,STATE` con `activeSeq = 7` y el mismo `sn`
- **THEN** la identidad queda confirmada y se muestra con sus minutos

#### Scenario: HMI trasladado a otra placa
- **WHEN** el `sn` de la placa conectada y el guardado son distintos y no nulos
- **THEN** la identidad se descarta y no se muestra

#### Scenario: Los `seq` colisionan
- **GIVEN** una identidad restaurada con `seq` 7
- **WHEN** la placa dice `activeSeq = 3`
- **THEN** la identidad se borra

#### Scenario: La placa ya no tiene a ese bebé
- **WHEN** la placa dice `activeSeq = 0` y la terapia está en marcha
- **THEN** la identidad se borra

#### Scenario: Placa antigua con el bebé en la lista
- **WHEN** la placa no manda `activeSeq` y la lista trae el `seq` con el mismo
  nombre
- **THEN** se muestra el nombre en gris con «?», sin edad ni minutos

#### Scenario: Placa antigua con el bebé ausente
- **WHEN** la lista no trae el `seq`, o lo trae con otro nombre
- **THEN** la identidad se borra

*Cobertura: la tabla y el veredicto de lista son lógica pura en `[env:native]`
del HMI. El pegamento (lectura de la instantánea de la placa, petición de lista,
pintado gris) es verificación manual en banco.*

### Requirement: Identidad visible en Main y en el bloqueo

El HMI SHALL mostrar, mientras `BabyWizard_HasLiveSession()` sea verdadero (bebé
conocido, terapia en marcha **e identidad confirmada**), el nombre del bebé, su
edad corregida y sus días de vida en el botón «Bebés» del heading de
`ui_ScreenMain` y en una réplica del heading de `ui_ScreenLock`. Una identidad no
confirmada SHALL NOT mostrarse como confirmada: si la placa es antigua y el bebé
figura en su lista, SHALL mostrarse el nombre en gris con «?», sin edad (para
ello, y solo para ello, se usa `BabyWizard_HasAnySession()`). Sin sesión viva, el
botón SHALL volver a mostrar su rótulo «Bebés» y la réplica del bloqueo SHALL
ocultarse. Las cadenas SHALL salir del catálogo en las cuatro lenguas y sin
caracteres fuera de ASCII. Ningún slot del heading SHALL cambiar de posición. El
detalle del botón SHALL truncar con puntos si no cabe.

#### Scenario: Terapia en marcha con bebé conocido
- **GIVEN** un bebé «Lucia», 34 semanas, 5 días de vida, confirmado, con control
  de temperatura activo
- **WHEN** se mira Main
- **THEN** el botón «Bebés» muestra «Lucia» y «EGC 34+5 - 5d» (en español)

*Verificación manual en banco.*

#### Scenario: Pantalla de bloqueo
- **WHEN** se bloquea el mando con ese bebé activo
- **THEN** el heading de bloqueo muestra «Lucia» y «EGC 34+5 - 5d» junto a la
  conectividad, sin truncar

*Verificación manual en banco.*

#### Scenario: Identidad sin confirmar
- **WHEN** la identidad restaurada aún no está confirmada por la placa
- **THEN** el botón muestra «Bebés», sin nombre

*Verificación manual en banco.*

#### Scenario: Placa antigua con el bebé en la lista
- **WHEN** la placa no manda `activeSeq` y la lista contiene al bebé
- **THEN** el botón muestra «Lucia ?» en gris, sin edad

*Verificación manual en banco.*

#### Scenario: Incubadora inactiva
- **WHEN** ninguna terapia está activa
- **THEN** el botón muestra «Bebés» y no hay identidad en el bloqueo

*Verificación manual en banco.*

#### Scenario: Cambio de idioma
- **WHEN** el operador cambia de español a francés
- **THEN** el detalle pasa a `AGC 34+5 - 5j` en el siguiente refresco

*Verificación manual en banco.*
