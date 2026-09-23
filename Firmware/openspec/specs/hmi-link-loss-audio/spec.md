# hmi-link-loss-audio Specification

## Purpose
TBD - created by archiving change hmi-audio-enlace-caido. Update Purpose after archive.
## Requirements
### Requirement: El display emite señal acústica propia cuando pierde el enlace

El display SHALL emitir una señal acústica de alarma por su propio transductor
mientras `Display_IsBoardLinkLost()` sea verdadero, sin depender de ninguna
trama procedente de la motherBoard.

Es la única condición para la que el display emite audio. El resto de alarmas
llegan por el enlace desde la motherBoard, que ya las está sonando en su propio
zumbador; duplicarlas ahí no aporta nada y multiplicaría el riesgo de
enmascaramiento.

#### Scenario: La placa deja de emitir con el display vivo
- **WHEN** han transcurrido más de `BOARD_LINK_TIMEOUT_MS` (5000 ms) desde la
  última línea válida recibida de la motherBoard
- **AND** el enlace había estado establecido previamente (`Display_BoardEverSeen()`)
- **THEN** el display comienza a emitir el patrón acústico de prioridad MEDIA
  por su zumbador
- **AND** el banner visual `!! SIN ENLACE CON LA PLACA` sigue mostrándose como
  hasta ahora, sin cambios
- *(Verificación manual: Display_HMI no tiene entorno de test. Banco — caso 17
  de `docs/alarms_bench_verification.md`.)*

#### Scenario: La motherBoard cuelga sin poder anunciarlo
- **WHEN** el firmware de la motherBoard se detiene, reinicia o entra en reset
  loop, de forma que su propio zumbador no puede sonar
- **THEN** el display emite igualmente la señal acústica, porque su detector y
  su transductor son independientes de la placa
- *(Verificación manual en banco: desconectar la alimentación de la motherBoard
  manteniendo alimentado el display.)*

#### Scenario: Ninguna otra alarma hace sonar el display
- **WHEN** la motherBoard anuncia cualquier alarma distinta de la pérdida de
  enlace y el enlace está vivo
- **THEN** el zumbador del display permanece en silencio, salvo el chasquido de
  confirmación al pulsar
- *(Verificación manual en banco.)*

### Requirement: El patrón acústico es el de prioridad MEDIA de la Tabla 3

La señal SHALL usar el patrón de ráfaga y pulso de prioridad MEDIA de la Tabla 3
de IEC 60601-1-8, con las mismas constantes que emplea la motherBoard para
anunciar `ALARM_HMI_LINK_LOST`: 3 pulsos por ráfaga, pulso de 150 ms, espaciado
`y` = 200 ms entre pulsos, y una ráfaga cada 25 000 ms.

La prioridad MEDIA no es una elección independiente: es la que
`alarm_priority(ALARM_HMI_LINK_LOST)` asigna en `shared/src/alarm_policy.cpp` y
la que ya usa el banner (`topPrio = ALARM_PRIORITY_MEDIUM`). Las dos mitades del
anuncio, y los dos transductores, tienen que contar lo mismo.

Las constantes SHALL residir en un único lugar consumido por ambas placas; el
display no las duplica.

#### Scenario: Temporización de la ráfaga
- **WHEN** la señal está activa y han transcurrido `t` ms desde el inicio de la
  ráfaga actual
- **THEN** el zumbador está encendido exactamente durante los intervalos
  [0, 150), [350, 500) y [700, 850) ms
- **AND** permanece apagado desde 850 ms hasta el inicio de la siguiente ráfaga
  en `t` = 25 000 ms
- *(Cubierto por test: lógica pura en `shared/`, ejercitada con Unity en el
  entorno `[env:native]` de motherBoard.)*

#### Scenario: La primera ráfaga no espera al periodo
- **WHEN** la condición de enlace perdido pasa de falsa a verdadera
- **THEN** la primera ráfaga arranca en ese mismo instante, sin esperar los
  25 000 ms del periodo
- *(Cubierto por test en `[env:native]`. Es el mismo `freshStart` que ya aplica
  el motor de la motherBoard: sin él el zumbador quedaría mudo hasta 25 s.)*

#### Scenario: Una sola definición de las constantes
- **WHEN** se compilan ambas placas
- **THEN** las constantes de temporización del patrón proceden del mismo header
  de `shared/`, y los `static_assert` de las ventanas de las Tablas 3 y 4 que ya
  existen en `motherBoard/src/drivers/Buzzer.cpp` siguen verificándolas en
  compilación
- *(Cubierto por compilación: `pio run -e main` en ambas placas.)*

### Requirement: La señal cesa sola al restablecerse el enlace

La señal SHALL cesar en cuanto vuelva a recibirse una línea válida de la
motherBoard, sin requerir acción del operador y sin dejar el zumbador encendido.

Esto no contradice 6.10 —que prohíbe que el audio cese por haber sonado
suficiente tiempo—: aquí cesa porque **la condición ha desaparecido**, que es la
otra vía legítima de terminación.

#### Scenario: Vuelve el enlace
- **WHEN** llega una línea válida de la motherBoard estando la señal activa
- **THEN** el display deja de emitir el patrón dentro del siguiente ciclo del
  bucle de UI
- **AND** el zumbador queda apagado, incluso si la condición desapareció en
  mitad de un pulso encendido
- *(Verificación manual en banco: reconectar el cable UART durante un pulso.)*

#### Scenario: La señal no se agota
- **WHEN** la condición se mantiene durante más de 30 minutos sin acción del
  operador
- **THEN** el patrón se sigue regenerando indefinidamente
- *(Cubierto por test en `[env:native]`: la función de patrón es sin estado
  respecto al número de ráfagas emitidas.)*

### Requirement: Pausa de audio local de 10 minutos

El display SHALL ofrecer al operador una pausa de audio de
`ALARM_AUDIO_PAUSE_MS` (10 minutos) sobre su propia señal, tras la cual el audio
se reanuda solo si la condición persiste.

Es imprescindible y no un añadido: con el enlace caído, el botón de SILENCIAR no
llega a la motherBoard, así que sin una pausa local la señal del display sería
incallable salvo apagando el equipo. El estado de esta pausa SHALL ser local al
display y no depender de ninguna trama de la placa —por la misma razón que la
señal—, lo que además la mantiene al margen del defecto abierto #6 de
`docs/known_issues.md`.

#### Scenario: El operador pausa el audio
- **WHEN** la señal está sonando y el operador pulsa el control de silenciar
- **THEN** el zumbador del display calla
- **AND** se muestra el indicador AUDIO PAUSED con su cuenta atrás
- *(Verificación manual en banco.)*

#### Scenario: La pausa expira con la condición aún presente
- **WHEN** han transcurrido 10 minutos desde la pausa
- **AND** el enlace sigue perdido
- **THEN** el display reanuda el patrón por sí solo
- *(Verificación manual en banco, o test en `[env:native]` si la lógica de la
  pausa queda como función pura.)*

#### Scenario: La pausa no sobrevive a la condición
- **WHEN** el enlace se restablece durante la pausa y vuelve a perderse después
- **THEN** la nueva pérdida suena desde el primer instante, sin arrastrar la
  pausa anterior
- *(Verificación manual en banco.)*

### Requirement: El chasquido de pulsación cede ante la señal de alarma

El chasquido de confirmación de 12 ms SHALL ceder ante la señal de alarma
mientras se emite una ráfaga: no suena y no apaga el zumbador. Ambos comparten
un único transductor en el display.

Un `buzzerOff()` del servicio de chasquido cayendo dentro de un pulso de alarma
lo trunca, y el patrón deja de ser el de la Tabla 3.

#### Scenario: Pulsación durante una ráfaga
- **WHEN** el operador toca la pantalla mientras se emite un pulso de la ráfaga
- **THEN** el pulso se completa con su duración íntegra
- **AND** el chasquido se omite
- *(Verificación manual en banco: pulsar repetidamente durante la ráfaga y
  comprobar que el ritmo de 3 pulsos no se altera.)*

#### Scenario: Pulsación fuera de la ráfaga
- **WHEN** el operador toca la pantalla durante el silencio entre ráfagas
- **THEN** el chasquido suena con normalidad
- *(Verificación manual en banco.)*

