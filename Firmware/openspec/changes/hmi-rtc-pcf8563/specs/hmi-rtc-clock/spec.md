## ADDED Requirements

### Requirement: Lectura validada del PCF8563

El HMI SHALL leer la hora del PCF8563 (I2C `0x51`, bus compartido con el
táctil en IO15/IO16) solo tras comprobar que el contenido del chip es creíble.
Una lectura SHALL descartarse, tratando el RTC como "sin hora", cuando se dé
cualquiera de estas condiciones:

- el bit `VL` del registro de segundos está a 1, que indica que el oscilador se
  detuvo o la tensión de pila cayó y el contenido no es fiable;
- el epoch resultante cae fuera de la ventana `[2021-01-01, 2100-01-01)`, la
  misma que ya impone `civil_to_unix_utc()`;
- los campos BCD contienen nibbles fuera de rango decimal.

El PCF8563 no tiene doble búfer, así que la lectura SHALL repetirse cuando el
registro de segundos cambie entre dos lecturas consecutivas, para no componer
una hora a caballo de un incremento.

#### Scenario: Pila agotada, el chip arranca con basura
- **WHEN** el HMI arranca y el registro de segundos del PCF8563 tiene `VL=1`
- **THEN** el driver devuelve "sin hora"
- **AND** el HMI no envía `HMI,RTC_TIME` a la motherBoard
- **AND** la interfaz sigue mostrando el aviso «Sin hora» hasta que llegue una
  fuente de red o el operador teclee la hora

*Cobertura: la decisión sobre `VL` y la validación de ventana son lógica pura y
se cubren en `[env:native]` de motherBoard. La lectura real del chip es
verificación manual sobre la unidad de banco, retirando la pila.*

#### Scenario: Lectura a caballo de un incremento de segundo
- **WHEN** el driver lee los registros de tiempo y el segundo cambia entre la
  primera y la segunda lectura
- **THEN** el driver repite la lectura
- **AND** devuelve solo un instante cuyos dos muestreos coinciden en el segundo

*Cobertura: manual, instrumentando el driver en la unidad de banco.*

#### Scenario: Hora válida en el arranque
- **WHEN** el HMI arranca, `VL=0` y el epoch reconstruido cae dentro de la
  ventana válida
- **THEN** el driver devuelve ese epoch
- **AND** el HMI lo envía a la motherBoard como `HMI,RTC_TIME`

*Cobertura: manual sobre la unidad de banco.*

### Requirement: Convención de siglo del PCF8563

El firmware SHALL escribir siempre el bit de siglo del PCF8563 a `0` e
interpretar el año de dos dígitos BCD como `2000 + YY`. El chip guarda el año
en dos dígitos más ese bit de siglo, alojado en el registro de meses. Un epoch
que caiga en 2100 o más tarde SHALL rechazarse antes de escribirse, porque
queda fuera de la ventana válida y no es representable con esta convención.

#### Scenario: Año dentro de la convención
- **WHEN** se escribe en el RTC un epoch correspondiente a 2026-09-16
- **THEN** el chip queda con el bit de siglo a `0` y el año a `26`
- **AND** una lectura posterior reconstruye 2026-09-16

*Cobertura: conversión BCD ↔ epoch en `[env:native]` de motherBoard.*

#### Scenario: Año fuera de la convención
- **WHEN** se intenta escribir en el RTC un epoch correspondiente a 2100 o
  posterior
- **THEN** la escritura se rechaza
- **AND** el contenido previo del chip queda intacto

*Cobertura: `[env:native]` de motherBoard.*

### Requirement: Persistencia del huso en la NVS del HMI

El HMI SHALL persistir en su NVS, junto a cada escritura del RTC, el offset de
huso (`tzq`), su origen (`tzsrc`) y el rango de la fuente que fijó ese epoch
(`src`). Hace falta porque el PCF8563 no tiene RAM de usuario respaldada por
pila y solo puede guardar el instante. Esa terna SHALL escribirse y leerse
**como una unidad**: una siembra posterior nunca combina el epoch del RTC con
un huso guardado en otra escritura.

Si el RTC tiene hora válida pero la NVS no tiene terna guardada, el HMI SHALL
enviar el epoch con `tzq=0`, `tzsrc=0` y `src` de rango RTC, que es exactamente
el caso «hay hora, no hay zona» que el protocolo ya contempla.

#### Scenario: Ciclo de alimentación conservando el huso
- **WHEN** el equipo tenía hora y huso resueltos y se le corta la alimentación
- **AND** vuelve a arrancar sin red
- **THEN** el HMI recupera del RTC el epoch y de la NVS el huso
- **AND** los envía juntos en un único `HMI,RTC_TIME`
- **AND** la interfaz pinta hora local, no UTC

*Cobertura: manual sobre la unidad de banco, con un ciclo de alimentación real.*

#### Scenario: RTC con hora pero NVS sin huso
- **WHEN** el RTC devuelve un epoch válido y la NVS no tiene terna guardada
- **THEN** el HMI envía ese epoch con `tzq=0` y `tzsrc=0`
- **AND** la interfaz pinta la hora sin aplicar offset, como ya hace hoy ante
  `tzsrc=0`

*Cobertura: manual sobre la unidad de banco, borrando la clave de NVS.*

### Requirement: Escritura acotada del RTC

El HMI SHALL escribir el RTC **solo** cuando el `CTRL,TIME` recibido cumpla al
menos una de estas condiciones:

- el rango de su fuente (`src`) es mejor que el rango guardado en la NVS;
- el rango es igual al guardado y el epoch difiere en más de 2 segundos del que
  el RTC tiene en ese momento.

Un `CTRL,TIME` con rango peor que el guardado, o con `epoch=0`, NUNCA SHALL
escribir el RTC. El HMI no SHALL escribir el RTC de forma periódica: el bus
I2C es el mismo del táctil y una escritura innecesaria compite con la lectura
de toques.

#### Scenario: Llega NTP sobre un reloj sembrado por el propio RTC
- **WHEN** la motherBoard difunde `CTRL,TIME` con rango NTP
- **AND** la NVS tiene guardado rango RTC
- **THEN** el HMI escribe el epoch recibido en el PCF8563
- **AND** actualiza la terna de la NVS con el rango NTP y el huso recibido

*Cobertura: la decisión de escribir es lógica pura y se cubre en `[env:native]`
de motherBoard. La escritura real es verificación manual.*

#### Scenario: Llega NITZ sobre un reloj ya fijado por NTP
- **WHEN** la motherBoard difunde `CTRL,TIME` con rango NITZ
- **AND** la NVS tiene guardado rango NTP
- **THEN** el HMI no escribe el PCF8563
- **AND** la terna de la NVS queda intacta

*Cobertura: `[env:native]` de motherBoard.*

#### Scenario: Difusión de rutina sin deriva apreciable
- **WHEN** llega un `CTRL,TIME` del mismo rango que el guardado y su epoch
  difiere en 1 segundo del que tiene el RTC
- **THEN** el HMI no escribe el PCF8563

*Cobertura: `[env:native]` de motherBoard.*

#### Scenario: La motherBoard aún no tiene hora
- **WHEN** llega un `CTRL,TIME` con `epoch=0`
- **THEN** el HMI no escribe el PCF8563
- **AND** el HMI reenvía su `HMI,RTC_TIME` si tiene hora válida guardada

*Cobertura: `[env:native]` de motherBoard para la decisión; manual para el
reenvío.*

### Requirement: Siembra episódica, nunca periódica

El HMI SHALL enviar `HMI,RTC_TIME` únicamente al arrancar, y después solo
mientras la motherBoard siga difundiendo `epoch=0`, con una cadencia no
inferior a la de `CTRL,TIME` (10 s). En cuanto la motherBoard anuncie un epoch
distinto de cero, el HMI SHALL dejar de enviarlo hasta el siguiente reinicio.

Esta restricción responde a `docs/known_issues.md` #2 (UART Flooding): el
enlace no admite tráfico periódico evitable.

#### Scenario: La motherBoard adopta la semilla
- **WHEN** el HMI ha enviado `HMI,RTC_TIME` y la motherBoard responde con un
  `CTRL,TIME` de epoch distinto de cero
- **THEN** el HMI deja de enviar `HMI,RTC_TIME`

*Cobertura: manual, observando el enlace en la unidad de banco.*

#### Scenario: La motherBoard nunca adopta la semilla
- **WHEN** la motherBoard sigue difundiendo `epoch=0` durante 10 minutos
- **THEN** el HMI ha enviado como mucho un `HMI,RTC_TIME` por cada `CTRL,TIME`
  recibido
- **AND** el enlace no acumula retraso ni pierde líneas

*Cobertura: manual, midiendo el tráfico del enlace.*
