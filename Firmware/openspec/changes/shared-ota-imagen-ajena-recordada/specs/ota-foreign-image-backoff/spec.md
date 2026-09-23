## ADDED Requirements

### Requirement: Una imagen rechazada por ser de otra placa no se vuelve a descargar
El equipo SHALL recordar el título y la versión de firmware que el servidor
anunció para una imagen rechazada por no llevar la marca de esta placa, y SHALL
abstenerse de descargarla de nuevo mientras el servidor siga anunciando ese
mismo par título/versión.

El recuerdo SHALL basarse en los metadatos que llegan **antes** de la descarga.
Si reconocer la imagen exigiera descargarla, no se ahorraría ni escritura de
flash ni datos, que es justo lo que este requisito existe para evitar.

#### Scenario: El servidor insiste con la misma imagen ajena
- **WHEN** el updater ha rechazado una imagen por marca de otra placa
- **AND** en la siguiente comprobación el servidor anuncia el mismo título y la misma versión
- **THEN** el equipo no inicia la descarga
- **AND** no se produce ni borrado ni escritura de la partición OTA

#### Scenario: El servidor ofrece una imagen distinta
- **WHEN** hay un rechazo recordado
- **AND** el servidor anuncia un título o una versión distintos de los recordados
- **THEN** el equipo descarga esa imagen con normalidad
- **AND** la guarda de placa vuelve a evaluarla de cero

### Requirement: El recuerdo sobrevive al reinicio
El rechazo SHALL persistirse en NVS. Un reinicio NO SHALL bastar para
reintentar la descarga de la misma imagen ajena.

Un equipo atrapado en el bucle suele reiniciarse por otras causas, así que un
recuerdo sólo en RAM no acota el desgaste: es la diferencia entre un problema
acotado y uno que reaparece cada arranque.

#### Scenario: Reinicio con un rechazo vigente
- **WHEN** el equipo tiene un rechazo recordado y se reinicia
- **AND** el servidor sigue anunciando el mismo título y versión
- **THEN** tras arrancar, el equipo sigue sin descargar esa imagen

### Requirement: El recuerdo nunca bloquea una actualización legítima
El recuerdo SHALL invalidarse en cuanto el servidor anuncie un par
título/versión distinto del recordado, y SHALL poder limpiarse sin reflashear.

Esta es la invariante que hace aceptable todo lo demás: un mecanismo pensado
para ahorrar escrituras no puede convertirse en la razón por la que un equipo
se queda sin recibir una corrección de seguridad.

#### Scenario: Actualización correcta tras un rechazo
- **WHEN** hay un rechazo recordado
- **AND** el servidor anuncia una imagen con la marca de esta placa
- **THEN** la instalación se completa y el equipo arranca desde la nueva imagen
- **AND** el rechazo recordado deja de existir

#### Scenario: Limpieza manual
- **WHEN** un operador o el servicio técnico solicita limpiar el rechazo recordado
- **THEN** el equipo vuelve a considerar cualquier imagen que el servidor ofrezca

### Requirement: El rechazo se comunica a quien puede arreglarlo
El equipo SHALL reportar al servidor que ha rechazado una imagen por pertenecer
a otra placa, indicando el título y la versión rechazados y la identidad de
placa esperada.

La causa de este bucle es siempre una mala configuración del servidor —un
binario en el slot equivocado—, y la única persona que puede corregirla no está
delante del equipo. Un rechazo que sólo queda en el log local es un rechazo que
nadie lee.

#### Scenario: Primer rechazo de una imagen
- **WHEN** el updater rechaza una imagen por marca de otra placa
- **THEN** el equipo publica el hecho con el título y la versión rechazados
- **AND** deja constancia local visible para el servicio técnico

#### Scenario: El aviso no se repite en bucle
- **WHEN** el mismo par título/versión sigue siendo anunciado tras el rechazo
- **THEN** el equipo no vuelve a publicar el aviso en cada comprobación
- (Un aviso repetido cada minuto es el mismo problema de desgaste trasladado a
  la telemetría.)

### Requirement: La guarda de placa no cambia
Este cambio NO SHALL modificar ninguna de las dos barreras de la guarda: ni la
cabecera de placa declarada por la herramienta, ni la comprobación de la marca
`IncuNestFW:<placa>` dentro del flujo. Un binario sin marca SHALL seguir
aceptándose, que es lo que permite volver a una versión anterior.

#### Scenario: Las pruebas de la guarda siguen en verde
- **WHEN** se ejecuta `tools/bench_tests/board_guard_test.py`
- **THEN** las cinco pruebas pasan, incluida la que comprueba que una OTA
  legítima sigue entrando
