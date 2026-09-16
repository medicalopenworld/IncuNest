## ADDED Requirements

### Requirement: Jerarquía de fuentes de hora

La motherBoard SHALL clasificar toda fuente de hora con un rango, y un rango
mayor SHALL ganar siempre al menor:

| Rango | Fuente | Por qué ahí |
|---|---|---|
| 4 | Manual (`/config` o `HMI,SET_TIME`) | Es la única que conoce la hora local sin red, y cambiarla bajo los pies del operador es peor que un error de minutos |
| 3 | NTP / SNTP (WiFi o módem) | Precisión de segundos y fecha fiable |
| 2 | RTC del HMI | Conserva la hora entre apagados, pero deriva minutos al mes |
| 1 | NITZ del módem | Muchos operadores no lo emiten, o lo emiten con minutos de error |
| 0 | Ninguna | El reloj no está sincronizado |

Toda fijación del reloj SHALL pasar por un único punto de entrada en
`modules/util/system_clock`. Ninguna ruta —SNTP, módem, webserver, enlace del
HMI— SHALL llamar a `settimeofday()` por su cuenta.

Ante rangos iguales gana la fijación más reciente. El rango vigente SHALL
vivir solo en RAM: un ciclo de alimentación lo reinicia a 0, igual que hoy hace
la marca de reloj manual, de modo que tras un reinicio las fuentes automáticas
vuelven a tener vía libre.

#### Scenario: NTP llega después de la semilla del RTC
- **WHEN** el reloj está fijado con rango RTC
- **AND** SNTP resuelve la hora
- **THEN** el reloj adopta el epoch de SNTP
- **AND** el rango vigente pasa a NTP

*Cobertura: `[env:native]` de motherBoard.*

#### Scenario: NITZ no desplaza un reloj ya sembrado por el RTC
- **WHEN** el reloj está fijado con rango RTC
- **AND** el módem entrega un epoch por NITZ
- **THEN** el epoch del reloj no cambia
- **AND** el rango vigente sigue siendo RTC

*Cobertura: `[env:native]` de motherBoard.*

#### Scenario: El ajuste manual gana a todo
- **WHEN** el operador fija la hora por `HMI,SET_TIME` o por `/config`
- **AND** después llega una sincronización de SNTP
- **THEN** el epoch del reloj no cambia
- **AND** el rango vigente sigue siendo manual hasta el próximo reinicio

*Cobertura: `[env:native]` de motherBoard.*

#### Scenario: El rango se pierde al reiniciar
- **WHEN** el reloj estaba fijado a mano y el equipo se reinicia
- **THEN** el rango vigente arranca en 0
- **AND** la primera fuente automática que llegue fija el reloj

*Cobertura: `[env:native]` de motherBoard.*

### Requirement: El huso se ordena aparte del instante

La jerarquía de rangos ordena el **epoch**. El huso horario (`tzq`) SHALL
seguir su propio orden, en el que **NITZ gana a IP**, porque la antena está
físicamente donde está el equipo mientras que una IP puede ser de una VPN o de
la sede del operador en otro país.

En consecuencia, cuando llegue un NITZ que no pueda fijar el epoch por rango
insuficiente, la motherBoard SHALL aplicar de todas formas su offset de huso si
ese offset es mejor que el vigente. El RTC del HMI no aporta huso propio:
devuelve el que se le guardó, y entra con el mismo rango que el epoch que lo
acompaña.

Un ajuste manual SHALL seguir implicando offset `0` por diseño, porque el epoch
que teclea el operador ya es hora local.

#### Scenario: NITZ aporta huso aunque no aporte hora
- **WHEN** el reloj está fijado con rango NTP y huso de origen IP
- **AND** el módem entrega un NITZ con epoch y offset
- **THEN** el epoch del reloj no cambia
- **AND** el offset vigente pasa a ser el de NITZ, con `tzsrc=1`

*Cobertura: `[env:native]` de motherBoard.*

#### Scenario: El ajuste manual no hereda huso de la red
- **WHEN** el operador fija la hora a mano estando ya resuelto un huso por NITZ
- **THEN** el offset vigente pasa a `0` con `tzsrc=3`
- **AND** la hora mostrada es exactamente la que tecleó el operador

*Cobertura: `[env:native]` de motherBoard.*

### Requirement: `CTRL,TIME` publica el rango de la fuente

La motherBoard SHALL difundir el rango de la fuente que fijó el epoch vigente
como cuarto campo de datos de `CTRL,TIME`:

`CTRL,TIME,epoch,tzq,tzsrc,src`

El campo `src` SHALL ser opcional, con la misma regla de compatibilidad que ya
tienen `tzq` y `tzsrc`: un HMI que reciba la línea sin él SHALL interpretarlo
como rango 0, y un HMI antiguo que reciba la línea con él SHALL ignorarlo sin
error. `src` SHALL usar la escala de rangos de fuente de hora, que es distinta
e independiente de la escala de `tzsrc`.

La cadencia SHALL seguir siendo de 10 segundos: este cambio no añade ningún
mensaje periódico al enlace.

#### Scenario: Difusión con rango
- **WHEN** el reloj está fijado por SNTP con huso resuelto por NITZ
- **THEN** la motherBoard difunde `CTRL,TIME` con `src` igual al rango NTP
- **AND** con `tzsrc` igual a NITZ

*Cobertura: manual, observando el enlace en la unidad de banco.*

#### Scenario: HMI antiguo contra motherBoard nueva
- **WHEN** un HMI con firmware anterior a este cambio recibe un `CTRL,TIME` que
  incluye el campo `src`
- **THEN** el HMI parsea los campos que conoce y descarta el sobrante
- **AND** la hora mostrada es la correcta

*Cobertura: manual, cruzando versiones de firmware en el banco.*

### Requirement: Recepción de `HMI,RTC_TIME`

La motherBoard SHALL aceptar del HMI el mensaje
`HMI,RTC_TIME,epoch,tzq,tzsrc` y tratarlo como una fuente de rango RTC. Como
todo el protocolo, SHALL validar el número de campos y que cada uno sea un
número antes de indexarlos, y SHALL descartar en silencio una línea mal formada
o truncada.

La motherBoard SHALL rechazar la semilla, dejando el reloj intacto, cuando el
epoch caiga fuera de la ventana `[2021-01-01, 2100-01-01)` o cuando el rango
vigente ya sea mejor que RTC.

#### Scenario: Semilla aceptada en un arranque sin red
- **WHEN** la motherBoard arranca sin hora y recibe un `HMI,RTC_TIME` válido
- **THEN** fija el reloj con ese epoch y ese huso
- **AND** el rango vigente pasa a RTC
- **AND** el siguiente `CTRL,TIME` difunde ese epoch con `src` de rango RTC

*Cobertura: `[env:native]` de motherBoard para la decisión; manual para el
enlace completo.*

#### Scenario: Semilla descartada por línea mal formada
- **WHEN** llega `HMI,RTC_TIME` con menos campos de los esperados, o con un
  campo no numérico
- **THEN** la motherBoard descarta la línea en silencio
- **AND** el reloj queda intacto

*Cobertura: `[env:native]` de motherBoard.*

#### Scenario: Semilla descartada por epoch imposible
- **WHEN** llega `HMI,RTC_TIME` con un epoch de 1970
- **THEN** la motherBoard descarta la semilla
- **AND** el reloj queda intacto

*Cobertura: `[env:native]` de motherBoard.*
