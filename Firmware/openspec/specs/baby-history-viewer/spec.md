# baby-history-viewer Specification

## Purpose
TBD - created by archiving change hmi-bebes-alta-desde-lista. Update Purpose after archive.
## Requirements
### Requirement: Registrar un bebé nuevo desde la pantalla Bebes

La pantalla Bebes SHALL ofrecer un botón BEBE NUEVO bajo la sección de
activos. Al pulsarlo, y mientras haya menos de tres bebés activos, SHALL
abrirse un flujo de tres pantallas con el teclado en pantalla del asistente:
nombre (solo letras, sin coma, máx. 23 caracteres), semanas de gestación
(20-40) y peso al ingreso (400-5000 g) con la alternativa SIN PESO. El
registro SHALL confirmarse únicamente con el botón REGISTRAR de la última
pantalla: solo entonces la HMI SHALL enviar `HMI,PROFILE_NEW,<name>,<gest>`
y, si se indicó peso, `HMI,PROFILE_WEIGHT,<seq>,<grams>` tras recibir el
`CTRL,PROFILE_ACK` con `seq != 0`. Cerrar con la X o volver atrás antes de
REGISTRAR NO SHALL enviar ninguna trama. Tras el registro la HMI SHALL
mostrar "Bebe registrado", recargar la lista y mostrar el bebé nuevo entre
los activos. Un `CTRL,PROFILE_ACK,0` SHALL mostrarse como registro rechazado
y volver a la lista. La falta de respuesta durante 3 s SHALL avisar y
recargar la lista sin reenviar `PROFILE_NEW` (un reenvío podría crear dos
perfiles). Con tres bebés activos el botón SHALL avisar de que hay que dar de
alta a uno antes y NO SHALL abrir el flujo; esa guarda SHALL apoyarse solo
en una lista de activos recibida de la placa: si la carga de la lista venció
o el enlace con la placa está perdido, BEBE NUEVO SHALL avisar "Sin respuesta
de la placa" y NO SHALL abrir el flujo, y REGISTRAR con el enlace perdido NO
SHALL enviar nada y SHALL dejar al operador en la pantalla de peso. Al
abandonar una espera de respuesta (timeout, alarma crítica, cierre) y justo
antes de cada envío, la pantalla SHALL descartar cualquier
`CTRL,PROFILE_ACK` / `CTRL,PROFILE_RANGE` pendiente, para que una respuesta
tardía no se atribuya a otra petición (del asistente o de esta pantalla). El
registro NO SHALL fijar el bebé recordado por la HMI como paciente en
tratamiento: la identidad del bebé bajo terapia la sigue decidiendo el
asistente al encender el control. Si al registrar hay un bebé bajo terapia
(`BabyWizard_HasLiveSession()`), la HMI SHALL reenviar
`HMI,PROFILE_SELECT,<seq del bebé en terapia>` tras el registro y esperar su
ACK (3 s) antes de dar el registro por hecho, para que la placa vuelva a
tener al bebé de la incubadora como "bebé del asistente".

#### Scenario: Registro completo con peso
- **WHEN** el operador toca Bebes, BEBE NUEVO, escribe "ANA", CONTINUAR, 32,
  CONTINUAR, 1500 y REGISTRAR
- **THEN** la motherBoard recibe `HMI,PROFILE_NEW,ANA,32` y, tras su ACK,
  `HMI,PROFILE_WEIGHT,<seq>,1500` (monitor serie)
- **AND** la pantalla muestra "Bebe registrado" y la lista de activos
  incluye "ANA - 32 sem - 1500 g"
- *(Verificación manual en banco; Display_HMI no tiene entorno de test.)*

#### Scenario: Registro sin peso
- **WHEN** el operador completa nombre y semanas y pulsa SIN PESO
- **THEN** la motherBoard recibe solo `HMI,PROFILE_NEW,<name>,<gest>`
- **AND** el bebé aparece en activos con peso `--`
- *(Verificación manual en banco.)*

#### Scenario: Cancelar antes de registrar no deja rastro
- **WHEN** el operador escribe un nombre, avanza a las semanas y cierra con
  la X
- **THEN** no sale ninguna trama `HMI,PROFILE_*` y la lista de activos es la
  misma de antes
- *(Verificación manual en banco con el monitor serie.)*

#### Scenario: Tres activos bloquean el registro
- **WHEN** hay tres bebés activos y el operador toca BEBE NUEVO
- **THEN** aparece el aviso "Ya hay 3 bebes activos: da de alta a uno antes"
  y el flujo no se abre
- **AND** tras dar de alta a uno, BEBE NUEVO abre el flujo
- *(Verificación manual en banco.)*

#### Scenario: La placa no responde
- **WHEN** la motherBoard deja de responder justo después de pulsar
  REGISTRAR (enlace aún no declarado perdido)
- **THEN** a los 3 s aparece "Sin respuesta de la placa", no se reenvía
  `PROFILE_NEW` y la pantalla vuelve a la lista
- *(Verificación manual en banco.)*

#### Scenario: Sin lista real no se registra
- **WHEN** la carga de la lista de activos venció (la pantalla muestra "Sin
  bebes activos" por defecto) o el enlace con la placa está perdido, y el
  operador toca BEBE NUEVO
- **THEN** aparece "Sin respuesta de la placa" y el flujo no se abre
- **AND** con el enlace perdido en la pantalla de peso, REGISTRAR muestra el
  mismo aviso, no envía nada y el operador sigue en esa pantalla con lo
  tecleado
- *(Verificación manual en banco: desconectar la placa antes de abrir Bebes,
  y desconectarla con la pantalla de peso abierta.)*

#### Scenario: Registro con otro bebé bajo terapia
- **WHEN** el control de temperatura está activo con el bebé A y el operador
  registra a B desde Bebes
- **THEN** la motherBoard recibe `HMI,PROFILE_NEW,B,...` y, tras el ACK,
  `HMI,PROFILE_SELECT,<seqA>` (monitor serie)
- **AND** la terapia sigue acreditándose a A; si después se apaga todo y se
  vuelve a encender con SALTAR, la placa sella a A, no a B (log `baby`)
- *(Verificación manual en banco.)*

#### Scenario: El bebé registrado se selecciona después en el asistente
- **WHEN** tras registrar a "ANA" desde Bebes el operador enciende el control
  de temperatura
- **THEN** el asistente lista a ANA para seleccionarla, pide su peso y
  propone el rango como con cualquier bebé existente
- **AND** al aplicar, la placa sella su `seq` como activo (log `baby`)
- *(Verificación manual en banco.)*

