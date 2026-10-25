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

Si no hay hora en la captura o ahora, el transcurrido SHALL ser 0 (se muestra el
valor capturado, nunca uno inventado). Si `ageKnown=0`, el HMI SHALL NOT mostrar
días de vida ni edad corregida. Si las semanas de gestación son 0 (no
informadas), SHALL NOT mostrar edad corregida.

#### Scenario: Edad corregida el mismo día de la captura
- **WHEN** el bebé tiene 34 semanas de gestación y la placa responde
  `PROFILE_RANGE` con `ageKnown=1, ageDays=5`
- **THEN** los días de vida son 5
- **AND** la edad corregida es `34+5`

#### Scenario: Pasan días con el HMI encendido
- **WHEN** la captura fue con `ageDays=5` y han transcurrido 3 días y 2 horas
- **THEN** los días de vida son 8
- **AND** con 34 semanas de gestación la edad corregida es `35+1`

#### Scenario: Sin hora sincronizada
- **WHEN** `HMI_GetEpochNow()` devuelve 0 en la captura o en el momento de
  calcular
- **THEN** los días de vida son los capturados, sin sumar nada

#### Scenario: Edad desconocida
- **WHEN** `PROFILE_RANGE` llega con `ageKnown=0`
- **THEN** no se muestran días de vida ni edad corregida
- **AND** solo se muestra la gestación al nacer

*Cobertura: cálculo puro en `[env:native]` del HMI
(`test/test_patient_session`). Que `ageDays` se capture del mensaje correcto es
verificación manual en banco.*

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
nombre, semanas, `ageDays`, epoch de captura) y los acumulados (`seq` y segundos
por terapia), cada uno en **un único registro** para que ninguno pueda quedar a
medias. Los acumulados SHALL escribirse como mucho cada 60 s y solo si cambiaron
(más una vez al pararse todas las terapias). Todo acceso a la NVS SHALL hacerse
fuera de `LVGL_Lock`.

Al arrancar, el HMI SHALL restaurar la identidad si el registro es válido
(versión conocida, `seq != 0`, nombre terminado), y SHALL aceptar el registro de
acumulados solo si su `seq` es el de la identidad restaurada. Al dar de alta al
bebé (`BabyWizard_ClearActiveProfile()`) SHALL borrar ambos. Si la motherBoard
contradice el `seq` restaurado (`PROFILE_ACK` = 0), prevalece la placa.

#### Scenario: Reinicio con terapia en marcha
- **WHEN** el HMI se reinicia con un bebé activo y la placa reanuda sus terapias
- **THEN** al llegar el primer `CTRL,STATE` con terapia activa el HMI muestra el
  nombre y los acumulados guardados

#### Scenario: Registro corrupto
- **WHEN** el blob de identidad tiene versión desconocida, `seq = 0`, tamaño
  incorrecto o nombre sin terminador
- **THEN** se descarta y el HMI arranca sin identidad

#### Scenario: Acumulados de otro bebé
- **WHEN** el `seq` del registro de acumulados no es el de la identidad
  restaurada
- **THEN** los acumulados se descartan (arrancan en 0 hasta la siguiente lista)

#### Scenario: Alta del paciente
- **WHEN** el operador da de alta al bebé
- **THEN** se borran identidad y acumulados, en RAM y en NVS

*Cobertura: validación de los registros pura en `[env:native]` del HMI; lectura/
escritura real de NVS y el borrado en el alta son verificación manual.*

### Requirement: Identidad visible en Main y en el bloqueo

Mientras `BabyWizard_HasLiveSession()` sea verdadero, el HMI SHALL mostrar el
nombre del bebé, su edad corregida y sus días de vida en el botón «Bebés» del
heading de `ui_ScreenMain` y en una réplica del heading de `ui_ScreenLock`. Sin
sesión viva, el botón SHALL volver a mostrar su rótulo «Bebés» y la réplica del
bloqueo SHALL ocultarse. Las cadenas SHALL salir del catálogo en las cuatro
lenguas y sin caracteres fuera de ASCII. Ningún slot del heading SHALL cambiar de
posición.

#### Scenario: Terapia en marcha con bebé conocido
- **GIVEN** un bebé «Lucia», 34 semanas, 5 días de vida, con control de
  temperatura activo
- **WHEN** se mira Main
- **THEN** el botón «Bebés» muestra «Lucia» y «EGC 34+5 - 5d» (en español)

*Verificación manual en banco.*

#### Scenario: Pantalla de bloqueo
- **WHEN** se bloquea el mando con ese bebé activo
- **THEN** el heading de bloqueo muestra «Lucia» y «EGC 34+5 - 5d» junto a la
  conectividad, sin truncar

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

Con sesión viva, el HMI SHALL mostrar en la cabecera de cada panel de terapia
(temperatura, humedad, fototerapia) el acumulado de esa terapia para el bebé
activo con el formato compacto, bajo un rótulo «Total» del catálogo. Sin sesión
viva, esas etiquetas SHALL ocultarse. La etiqueta SHALL NOT recibir toques ni
solapar el título ni el botón de la cabecera.

#### Scenario: Tres terapias con acumulados distintos
- **GIVEN** un bebé con 200 min de termorregulación, 45 de humedad y 180 de
  fototerapia
- **WHEN** se mira Main
- **THEN** los paneles muestran `3h 20m`, `45m` y `3h`

*Verificación manual en banco, con las cuatro lenguas y con 12h 20m, el peor caso
de ancho.*

#### Scenario: El valor avanza
- **WHEN** una terapia lleva un minuto más en marcha
- **THEN** la etiqueta se repinta con el minuto nuevo y no antes

*Verificación manual en banco.*

