# hmi-patient-identity Specification

## Purpose
TBD - created by archiving change hmi-patient-identity. Update Purpose after archive.
## Requirements
### Requirement: Edad corregida y días de vida calculados en el HMI

El HMI SHALL calcular los días de vida del bebé a partir del último
`CTRL,PROFILE_RANGE` con `ageKnown=1` (campo `ageDays`) y de la hora vigente en
el momento de recibirlo, sin que la motherBoard envíe fecha de nacimiento:

- `díasVida = ageDays + floor((ahora - epochCaptura) / 86400)`;
- `edadCorregida = (semanasGestación * 7 + díasVida)` expresada como
  `semanas+días`.

La edad SHALL ser **desconocida** (sin días de vida ni edad corregida) si no hubo
hora en la captura, si falta la hora ahora, si `ageKnown=0`, o si han pasado más
de 365 días desde la captura. Si las semanas de gestación son 0 (no
informadas), SHALL NOT mostrarse edad corregida. Si la hora fue hacia atrás, no
se suma nada. Los días de vida mostrados SHALL acotarse a `99+`.

#### Scenario: Edad corregida el mismo día de la captura
- **WHEN** el bebé tiene 34 semanas de gestación y la placa responde
  `PROFILE_RANGE` con `ageKnown=1, ageDays=5`
- **THEN** los días de vida son 5
- **AND** la edad corregida es `34+5`

#### Scenario: Pasan días con el HMI encendido
- **WHEN** la captura fue con `ageDays=5` y han transcurrido 3 días y 2 horas
- **THEN** los días de vida son 8
- **AND** con 34 semanas de gestación la edad corregida es `35+1`

#### Scenario: Sin hora en la captura
- **WHEN** `HMI_GetEpochNow()` devolvió 0 al capturar
- **THEN** la edad es desconocida

#### Scenario: Sin hora ahora
- **WHEN** hubo captura con hora y ahora no hay hora
- **THEN** la edad es desconocida

#### Scenario: Captura demasiado vieja
- **WHEN** han pasado más de 365 días desde la captura
- **THEN** la edad es desconocida

#### Scenario: Edad desconocida
- **WHEN** `PROFILE_RANGE` llega con `ageKnown=0`
- **THEN** no se muestran días de vida ni edad corregida
- **AND** solo se muestra la gestación al nacer

#### Scenario: Días acotados
- **WHEN** los días de vida son 150
- **THEN** se muestran como `99+`

*Cobertura: cálculo puro en `[env:native]` del HMI. Que `ageDays` se capture del
mensaje correcto es verificación manual en banco.*

### Requirement: Acumulación local de minutos de terapia por bebé

El HMI SHALL acumular, para el bebé activo (`seq != 0`), los segundos de cada
terapia a partir del `CTRL,STATE` recibido, con la misma regla que la
motherBoard: **termorregulación** con `act` 1 o 3, **humedad** con `act` 2 o 3,
**fototerapia** con `photo != 0`. Cada paso SHALL acreditar como mucho 5 s, y SHALL
NOT acreditar nada mientras el enlace con la placa esté perdido, con `seq = 0`
ni en modo formación.

El HMI SHALL sembrar el acumulador al consumir `CTRL,PROFILE_ACK` con los
minutos del bebé en la última lista del asistente (0 si es nuevo), salvo que ya
estuviera acumulando para ese mismo `seq`.

Cada `CTRL,PROFILE_LIST` recibido, lo consuma quien lo consuma, SHALL
resincronizar el acumulador sin consumir `g_pendingProfileList`: un contador que
corre en el HMI toma el mayor de los dos valores; uno parado toma el de la placa.

#### Scenario: Temperatura y humedad a la vez
- **WHEN** llegan 120 segundos de `CTRL,STATE` con `act=3` y `photo=0`
- **THEN** los minutos de termorregulación y de humedad suben 2
- **AND** los de fototerapia no cambian

#### Scenario: Solo humedad
- **WHEN** llegan 60 segundos con `act=2`
- **THEN** solo suben los minutos de humedad

#### Scenario: Fototerapia sin control térmico
- **WHEN** llegan 60 segundos con `act=0` y `photo=1`
- **THEN** solo suben los minutos de fototerapia

#### Scenario: Enlace perdido
- **WHEN** el enlace con la placa está caído durante 30 s con terapias activas
- **THEN** ningún contador avanza

#### Scenario: Atasco de la interfaz
- **WHEN** entre dos pasos consecutivos transcurren 60 s
- **THEN** se acreditan como mucho 5 s por contador activo

#### Scenario: Resincronización con la lista de la placa
- **WHEN** el contador de fototerapia está parado en el HMI en 100 min
- **AND** llega una `PROFILE_LIST` con ese bebé a 130 min de fototerapia
- **THEN** el contador pasa a 130 min

#### Scenario: La lista va por detrás de un contador en marcha
- **WHEN** el contador de termorregulación corre en el HMI y marca 125 min
- **AND** llega una `PROFILE_LIST` con 120 min para ese bebé
- **THEN** el contador conserva 125 min

#### Scenario: Lista sin el bebé activo
- **WHEN** llega una `PROFILE_LIST` que no contiene el `seq` activo
- **THEN** el acumulador no cambia

*Cobertura: reglas de acumulación, tope, congelado y resincronización puras en
`[env:native]` del HMI. La lectura de `ctrl_state_msg`, el espejo de la lista en
el parser y la no-competencia con el asistente son verificación manual.*

### Requirement: Formato compacto de duración

Las duraciones de terapia SHALL mostrarse como `Xh Ym` (p. ej. `3h 20m`), `Ym`
por debajo de una hora (`45m`) y `Xh` cuando son horas exactas (`3h`), el mismo
formato que ya usa el visor de bebés.

#### Scenario: Formatos
- **WHEN** se formatean 200, 45, 180 y 0 minutos
- **THEN** el resultado es `3h 20m`, `45m`, `3h` y `0m`

*Cobertura: `[env:native]` del HMI.*

### Requirement: Persistencia de la identidad y de los acumulados

El HMI SHALL guardar en la NVS (`hmi_cfg`) la identidad del bebé activo (`seq`,
nombre, semanas, `ageDays`, epoch de captura y el `sn` de la placa) y los
acumulados (`seq` y segundos por terapia), cada uno en **un único registro**
para que ninguno pueda quedar a medias. Los acumulados SHALL escribirse como
mucho cada 60 s y solo si cambiaron (más una vez al pararse todas las
terapias). Todo acceso a la NVS SHALL hacerse fuera de `LVGL_Lock`.

Al arrancar, el HMI SHALL restaurar la identidad si el registro es válido
(versión 2, `seq != 0`, nombre terminado, semanas 0 o 20-44), y SHALL aceptar el
registro de acumulados solo si su `seq` es el de la identidad restaurada. Un
registro de versión 1 SHALL descartarse y borrarse: no está atado a ninguna
placa. La identidad restaurada nace sin confirmar. Al dar de alta al bebé
(`BabyWizard_ClearActiveProfile()`) SHALL borrar ambos registros.

#### Scenario: Reinicio con terapia en marcha
- **WHEN** el HMI se reinicia con un bebé activo y la placa reanuda sus terapias
- **THEN** al llegar el primer `CTRL,STATE` que lo confirma el HMI muestra el
  nombre y los acumulados guardados

#### Scenario: Registro corrupto
- **WHEN** el blob de identidad tiene versión desconocida, `seq = 0`, tamaño
  incorrecto, nombre sin terminador o semanas fuera de rango
- **THEN** se descarta y el HMI arranca sin identidad

#### Scenario: Registro de la versión 1
- **WHEN** hay un `pat_id` de 40 bytes de la versión 1
- **THEN** se descarta y se borra

#### Scenario: Acumulados de otro bebé
- **WHEN** el `seq` del registro de acumulados no es el de la identidad
  restaurada
- **THEN** los acumulados se descartan (arrancan ocultos hasta la siguiente lista)

#### Scenario: Alta del paciente
- **WHEN** el operador da de alta al bebé
- **THEN** se borran identidad y acumulados, en RAM y en NVS

*Cobertura: validación de los registros pura en `[env:native]` del HMI; lectura/
escritura real de NVS y el borrado en el alta son verificación manual.*

### Requirement: Identidad visible en Main y en el bloqueo

El HMI SHALL mostrar, mientras `BabyWizard_HasLiveSession()` sea verdadero **y la
identidad esté confirmada**, el nombre del bebé, su edad corregida y sus
días de vida en el botón «Bebés» del heading de `ui_ScreenMain` y en una réplica
del heading de `ui_ScreenLock`. Una identidad no confirmada SHALL NOT mostrarse
como confirmada: si la placa es antigua y el bebé figura en su lista, SHALL
mostrarse el nombre en gris con «?», sin edad. Sin sesión viva, el botón SHALL
volver a mostrar su rótulo «Bebés» y la réplica del bloqueo SHALL ocultarse. Las
cadenas SHALL salir del catálogo en las cuatro lenguas y sin caracteres fuera de
ASCII. Ningún slot del heading SHALL cambiar de posición. El detalle del botón
SHALL truncar con puntos si no cabe.

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

### Requirement: Terapia acumulada visible en los paneles de Main

Con sesión viva e identidad confirmada, y con minutos conocidos, el HMI SHALL
mostrar en la cabecera de cada panel de terapia (temperatura, humedad,
fototerapia) el acumulado de esa terapia para el bebé activo con el formato
compacto, **en una sola línea y sin rótulo**, bajo el título de la cabecera y
alineada como él. Sin sesión viva, sin confirmar o con minutos desconocidos,
esas etiquetas SHALL ocultarse. La etiqueta SHALL NOT recibir toques ni solapar
el título ni el botón de la cabecera, tampoco con `100h 5m` y el título francés.

#### Scenario: Tres terapias con acumulados distintos
- **GIVEN** un bebé confirmado con 200 min de termorregulación, 45 de humedad y
  180 de fototerapia
- **WHEN** se mira Main
- **THEN** los paneles muestran `3h 20m`, `45m` y `3h`

*Verificación manual en banco, con las cuatro lenguas y con `100h 5m`, el peor
caso de ancho.*

#### Scenario: El valor avanza
- **WHEN** una terapia lleva un minuto más en marcha
- **THEN** la etiqueta se repinta con el minuto nuevo y no antes

*Verificación manual en banco.*

#### Scenario: Minutos desconocidos
- **WHEN** no hay `pat_ther` válido ni lista que los siembre
- **THEN** las etiquetas permanecen ocultas

*Verificación manual en banco.*

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
| el `sn` de la placa no es el guardado | se borra |
| `activeSeq == seq` | se confirma |
| `activeSeq != 0` y `!= seq` | se borra |
| `activeSeq == 0` y la placa informa de terapia | se borra |
| `activeSeq == 0` y sin terapia | se espera |

Con una placa que **no** manda el campo, el HMI SHALL pedir
`PROFILE_LIST` una sola vez, con el enlace vivo. Si el bebé no está en la lista,
o está con otro nombre u otras semanas, se borra. Si está, la identidad SHALL
mostrarse **en gris y con «?» tras el nombre**, sin edad ni minutos, nunca como
confirmada.

Borrar la identidad SHALL borrar también sus acumulados, en RAM y en NVS.

#### Scenario: Reinicio del HMI, mismo bebé
- **GIVEN** una identidad restaurada con `seq` 7
- **WHEN** llega un `CTRL,STATE` con `activeSeq = 7` y el mismo `sn`
- **THEN** la identidad queda confirmada y se muestra con sus minutos

#### Scenario: HMI trasladado a otra placa
- **WHEN** el `sn` de la placa conectada no es el guardado
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
del HMI. El pegamento (lectura de `ctrl_state_msg`, petición única de lista,
pintado gris) es verificación manual en banco.*

### Requirement: Sin atajo del asistente con una identidad sin confirmar

El asistente de bebé SHALL NOT saltar el selector con `selectExisting()` sobre
una identidad no confirmada, aunque haya terapia viva: SHALL ofrecer el selector.

#### Scenario: Identidad restaurada sin confirmar y terapia viva
- **WHEN** el operador pulsa activar una terapia con la identidad sin confirmar
- **THEN** aparece el selector de bebés

*Verificación manual en banco.*

### Requirement: Los minutos solo se acreditan al bebé que la placa tiene al mando

El HMI SHALL acreditar minutos de terapia solo si la placa manda `activeSeq` y
vale el `seq` del acumulador; si la placa no lo manda, solo con la identidad
confirmada. Cuando los minutos no proceden de una siembra, de un registro válido
o de una lista, SHALL ocultarse y NOT contarse.

#### Scenario: La placa tiene a otro bebé al mando
- **WHEN** `activeSeq` es distinto del `seq` del acumulador
- **THEN** ningún contador avanza

#### Scenario: Sin `pat_ther` válido
- **WHEN** se restaura una identidad sin registro de acumulados válido
- **THEN** los minutos no se muestran hasta que una lista los siembre

#### Scenario: Contador en marcha y lista antigua
- **WHEN** un contador en marcha marca 125 min y llega una lista con 120
- **THEN** el contador no retrocede
- **AND** tampoco al llegar otra lista más antigua después

*Cobertura: `[env:native]` del HMI.*

### Requirement: Mismo seq con otro nombre es otro bebé

El HMI SHALL tratar una sesión con el mismo `seq` pero distinto nombre o semanas
como un bebé nuevo: reinicia edad, acumulados y confirmación.

#### Scenario: La placa reasignó el seq
- **WHEN** la identidad es (7, "Lucia", 34) y llega (7, "Marta", 30)
- **THEN** se descartan la edad y los acumulados de Lucia

*Cobertura: `[env:native]` del HMI (decisión pura).*

### Requirement: PROFILE_NEW rechazado no borra la identidad

Un `PROFILE_ACK 0` en respuesta a `PROFILE_NEW` SHALL NOT borrar la identidad del
bebé que sigue en la incubadora. Solo un `PROFILE_ACK 0` en respuesta a
`PROFILE_SELECT` SHALL hacerlo.

#### Scenario: Placa llena
- **WHEN** la placa rechaza el alta de un bebé nuevo con `PROFILE_ACK 0`
- **THEN** la identidad del bebé que está al mando se conserva

*Verificación manual en banco.*

