# Protocolo de Comunicación IncuNest (v2.3.2)

> Nota (v2.3.2): tercera vuelta del test de fábrica tras la segunda prueba en
> banco (motherBoard SOLO a batería, SensorBoard conectada, sin SHT4x
> exterior). **BREAKING dentro de la familia `FTEST`**: se renumeran los ids
> de la tabla de motherBoard (`ext_sht4x` id 6 y `sensorboard` id 7 se
> fusionan en un único `env_sensor` id 6 — un equipo lleva SensorBoard O
> sensor ambiental, no ambos; todo lo que iba de id 8 en adelante baja una
> posición, `FTEST_MB_COUNT` pasa de 29 a 28). Aceptable por el mismo motivo
> que en v2.3.1: ninguna placa en campo lleva todavía la versión anterior de
> `FTEST`, es la propia prueba en banco la que encontró los problemas. Además:
> no hay mutex de bus I2C en la motherBoard, así que **ningún test hace ya
> I2C directo** (leen, con sello de frescura, el estado que ya mantiene
> `sensors_Task`; únicas excepciones `actuators`/`standby`, que
> reusan `actuatorsTest()`/`testStandByCurrent()` sin tocar); `charger` pasó
> de RUNNING infinito con la placa a batería a esperar hasta 12 s el estado
> cacheado del BQ25730; `humid_usb` se omite por ahora (`SKIP`, detail
> `omitido`) hasta que el jig de fábrica pueda medir el humidificador;
> `gsm_signal` pasa a **WARN** `sin señal` al agotar su plazo (antes `FAIL`) —
> conectarse a la red celular es opcional, igual que `gsm_net`/`wifi`. Nueva
> cota **por test** (no de toda la batería): `FTEST_TEST_TIMEOUT_MS` = 90 s;
> superarla da `FAIL` con detail `timeout` para ESE test y la batería
> continúa con el siguiente (a diferencia de las cotas de batería completa,
> que sí la abortan entera).
>
> Cuarta vuelta (mismo banco, misma versión de protocolo — no cambia ningún
> campo ni formato de línea, solo el criterio interno de algunos tests):
> `buzzer` elimina el camino `CONFIRM` (preguntarle al operario "¿sonido del
> zumbador?"); sin micrófono de la SensorBoard el test sale directo `SKIP`
> `sin microfono`, sin hacer sonar el zumbador. `power_src` y `sb_door` se
> omiten por ahora (`SKIP` `omitido`): a batería el BQ25730 no está
> alimentado y el jig todavía no tiene la puerta montada. `charger` pasa a
> **WARN** `sin vbus` al agotar su plazo de 12 s (antes `FAIL`): sin VBUS es
> normal que el BQ25730 no responda, no es un fallo de la placa. La
> motherBoard además espera 60 ms entre el resultado final de un test y el
> `RUNNING` del siguiente — con varios tests omitidos seguidos la ráfaga de
> líneas casi instantáneas podía desbordar el anillo de recepción del
> display.

> Nota (v2.3.1): segunda vuelta del test de fábrica tras la primera prueba en
> banco (motherBoard V18 + SensorBoard + HMI reales). **BREAKING dentro de la
> familia `FTEST`**: se renumeran los ids de la tabla de motherBoard
> (`sensor_src` id 7 y `sb_link` id 8 se fusionan en un único `sensorboard`
> id 7; todo lo que iba de id 9 en adelante baja una posición). Aceptable
> porque ninguna placa en campo lleva todavía la versión anterior de `FTEST`
> — es la propia primera prueba en banco la que encontró los problemas que
> motivan este cambio. El resto de la familia `FTEST` (fuera de la tabla de
> ids) sigue sin ser breaking para una placa que no la conozca en absoluto,
> igual que en v2.3.0. Además: nuevo estado `WARN` (6, final, cuenta en un
> cuarto campo de `CTRL,FTEST_DONE`, opcional al parsear), criterios nuevos
> de `gsm_at`/`gsm_sim`, y `gsm_net`/`wifi`/`tb_provision`/`time` pasan a
> plazo de 30 s con `WARN` en vez de `SKIP`/`FAIL` al agotarlo.

> Nota (v2.3.0): se añade la familia `FTEST` (test de fábrica lanzado desde el
> splash del display). **No es breaking**: son mensajes nuevos con prefijo
> propio que una placa anterior descarta como desconocidos. Un display v2.3.0
> con una motherBoard anterior marca la sección de placa como "sin soporte" a
> los 10 s y sigue con sus tests locales.

> Nota (v2.2.0): `HMI,PROFILE_DISCHARGE` y `CTRL,PROFILE_HISTORY` añaden un
> campo `cause` (causa de fallecimiento, solo relevante cuando `outcome=2`).
> Ambas placas deben actualizarse juntas: un HMI v2.2.0 espera ese campo en
> el eco/historial, y una motherBoard v2.2.0 exige exactamente 5 campos en
> `HMI,PROFILE_DISCHARGE` (antes 4) — un HMI v2.1.0 verá sus líneas
> descartadas como malformadas hasta actualizar ambos.

> Nota (v2.1.0): `CTRL,PROFILE_LIST` y `CTRL,PROFILE_HISTORY` añaden un campo
> `humidityMin` al final de cada entrada. Ambas placas deben actualizarse
> juntas: un HMI v2.1.0 descarta una línea v2.0.0 porque le falta un campo, y
> un HMI v2.0.0 desalinea el parseo de la segunda entrada en adelante por
> sobrarle uno.

Este documento describe el protocolo de comunicación serie utilizado entre la Motherboard (MCU) y el Display (HMI).

## Especificaciones de Capa Física
- **Interfaz**: UART (vía chip puente USB-Serie CH340C).
- **Baudios**: 115200.
- **Formato**: 8N1.

## Tipos de Mensajes

### 1. Mensajes de la Motherboard (MCU → HMI)

#### CTRL,STATE
Enviado **cada 1 segundo** (intercalado con `CTRL,TEL`) y además bajo petición
(`HMI,REQ,STATE`).

> El envío periódico no existía: hasta ahora solo salía bajo petición, y el
> display deja de pedirlo en cuanto se sincroniza, así que en la práctica se
> emitía unas pocas veces al arrancar y nunca más. Todo lo que viaja aquí se
> quedaba congelado con el valor del arranque. **Los campos de este mensaje son
> estado vivo, no una instantánea de sincronización** — cualquier campo nuevo
> debe poder asumir la cadencia de 1 Hz.
**Formato**: `CTRL,STATE,act,mode,airSet,skinSet,humSet,photo,mute,sn,hwNum,hwRev,fwVer,numAlarms,skinE,commStatus,photoTimeRem,lang,probeState,alarmBitmask,silencedBitmask,almTest,silenceLeftS,linkBars`

- `commStatus`: transporte activo y si llega a servidor (`CommStatus` enum: `0`=ninguno,
  `1`=GPRS sin servidor, `2`=GPRS+servidor, `3`=WiFi sin servidor, `4`=WiFi+servidor).
  Alimenta el icono WIFI/2G/sin enlace del heading del display, además de la
  etiqueta "Placa:" de la pantalla de WiFi.
- `linkBars`: cobertura del transporte activo indicado por `commStatus`, en barras
  de `0` a `4` (como el icono de señal de un móvil), o `-1` si no hay transporte
  activo o el dato de señal (RSSI de WiFi / CSQ de GPRS) no es fiable todavía. Una
  placa antigua que no mande el campo se interpreta como `-1`: el display no
  pinta ningún nivel en vez de inventarse uno. Campo añadido después de
  `silenceLeftS`.
- `alarmBitmask`: (Hexadecimal, ej: `0x60`) Indica qué IDs de alarma están activos. Requerido para sincronización robusta.
- `mute`: estado **real** del audio en la placa, no el eco del comando del HMI.
  `1` = no queda ninguna condición que el operador pueda silenciar (todas las
  que señalizan están ya en AUDIO PAUSED); `0` = hay audio vivo. El HMI decide
  con esto si enseña el botón de silencio. Tiene que venir de la placa porque
  la pausa caduca sola (60601-2-19 201.12.3.104) y el display necesita saber
  que el zumbador ha vuelto para volver a ofrecer el botón.
- `silencedBitmask`: (Hexadecimal) un bit por `AlarmId` con las condiciones que
  están en AUDIO PAUSED. 60601-1-8 6.8.1 exige que el operador pueda determinar
  **cuáles** están inactivadas, no solo que alguna lo esté, y 60601-2-19
  201.12.3.104 que cada alarma silenciada mantenga indicación visual. Campo
  añadido después de `alarmBitmask`: una placa antigua que no lo mande hace que
  el HMI asuma `0` (ninguna silenciada), que es el lado seguro — como mucho
  ofrece silenciar algo que ya lo estaba, nunca oculta que una alarma sigue
  callada.
- `almTest`: prioridad (`0`=BAJA, `1`=MEDIA, `2`=ALTA) que está reproduciendo la
  prueba de funcionamiento de alarmas, o `255` si no hay prueba en curso. El
  display la usa para pintar el banner con el color y el parpadeo de esa
  prioridad: 60601-2-19 201.12.3.105 pide comprobar las alarmas "audible **and**
  visual", así que la prueba tiene que ejercitar también la señal visual. Una
  placa antigua que no mande el campo se interpreta como "sin prueba".
- `silenceLeftS`: segundos que faltan para que caduque la pausa de audio que
  expira **antes**, o `0` si no hay ninguna condición silenciada. Alimenta la
  cuenta atrás que el display pinta junto al icono de AUDIO PAUSED: el racional
  de 6.8.5 la recomienda expresamente *"adjoining the icon... so that they can
  more easily be distinguished from ALARM OFF or AUDIO OFF"*. Se manda la más
  próxima y no una por condición porque esa es la pregunta que contesta —
  cuándo vuelve el sonido—; el estado silenciada/no de cada condición ya viaja
  en `silencedBitmask`.

#### CTRL,TEL (Telemetría en tiempo real)
Enviado cada 1 segundo (intercalado con STATE).
**Formato**: `CTRL,TEL,airDet,skinDet,humDet,serverStatus`

- **Medida no disponible**: cuando la placa lleva más de 5 s sin conseguir una
  lectura válida de un sensor —o no la ha conseguido nunca, como la sonda de
  piel ausente en modo aire— el campo viaja con un centinela y **no** con `0`:
  - `airDet`/`skinDet`: `-999.0` (`PROTO_TEL_TEMP_UNAVAILABLE`)
  - `humDet`: `-1` (`PROTO_TEL_HUM_UNAVAILABLE`)
- El HMI pinta esos campos como `--`, el mismo tratamiento que ya daba a la
  caída del enlace. El motivo es de seguridad, no cosmético: `0` es un valor
  **plausible** y quien lee la pantalla lo interpreta como una medida real y
  alarmante, en lugar de como la ausencia de medida que es.
- La misma ventana de 5 s que usa `checkStatusOfSensor()` para levantar las
  alarmas de fallo de sensor, para que el `--` y la alarma aparezcan a la vez.
- Los centinelas afectan **solo a lo que se transmite**. `in3.temperature[]`
  conserva su convención interna porque alimenta el PID y las alarmas: la
  presentación no cambia el control.

#### CTRL,PROBE (Estado de contacto de la sonda SpO2)
Enviado cada 2 segundos **mientras el estado no sea `2` (APPLIED)**, y una vez
de forma inmediata en la transición a `2`. Mientras hay contacto válido no se
repite: en su lugar viaja `CTRL,VIT`.
**Formato**: `CTRL,PROBE,state`

- `state` es el valor crudo del enum `ProbeState` de la librería
  `incunest_afe4490`. **El rango no es 0–2**:

  | `state` | Significado |
  |---|---|
  | `0` | `DISCONNECTED` — sonda o conector sin enchufar |
  | `1` | `NOT_APPLIED` — sonda conectada, sin tejido en el camino óptico |
  | `2` | `APPLIED` — sonda sobre el paciente, medida normal |
  | `3` | `SATURATING` — canal saturado por exceso de luz; la presencia de tejido es **desconocida**, así que **no** implica `APPLIED` |

- `3` es el estado habitual justo al **retirar** la sonda: queda expuesta a la
  luz ambiente y el ADC se va al raíl positivo. Un receptor que solo acepte
  `0..2` descarta ese mensaje y se queda con el último `APPLIED` (fallo real
  corregido: la gráfica PPG no se ocultaba al quitar la sonda).
- **Regla fail-safe para el receptor**: solo `2` habilita mostrar traza y HR.
  Cualquier otro valor parseable —incluido uno que esta versión no conozca—
  debe tratarse como "sin contacto válido"; nunca descartarse. Si la librería
  añade un estado, hay que reflejarlo aquí y en `Display_HMI`'s `CommTask.h`.

#### CTRL,PPG (Onda pletismográfica)
Enviado a 25 Hz (cada 40 ms) **solo** cuando el estado de sonda es `2`.
**Formato**: `CTRL,PPG,value`
- `value`: muestra normalizada `0..255`, escala fija (128 = línea base). No va
  condicionada a la validez de SpO2: la librería reinicia un calentamiento de
  SpO2 de ~18 s en cada aplicación de sonda, y condicionar la traza a eso la
  dejaba plana 19 s.

#### CTRL,VIT (Constantes vitales fusionadas)
Enviado cada 1 segundo **solo** cuando el estado de sonda es `2`.
**Formato**: `CTRL,VIT,hr,spo2,pi`
- `hr`: `0` = sin valor válido (el HMI muestra `--`).
- `pi` es opcional; el HMI acepta la línea con 2 o 3 campos.

#### CTRL,ALM (Evento de Alarma)
Enviado cuando una alarma cambia de estado.
**Formato**: `CTRL,ALM,id,short_text,long_text,active,priority`
- `active`: `1` (Activa), `0` (Eliminada).
- `priority`: `0`=BAJA, `1`=MEDIA, `2`=ALTA. La calcula **la motherBoard**; el
  display no la deduce del texto ni la recalcula. Una segunda copia de la
  política de prioridades en el HMI sería una fuente de verdad paralela
  esperando a desincronizarse.
- `short_text` se trunca a `ALARM_TITLE_MAX_CHARS` y `long_text` a
  `ALARM_DESC_MAX_CHARS`; el HMI parsea con anchos derivados de esas mismas
  macros. El campo `priority` es opcional en el parseo (se acepta la línea con
  4 campos) para tolerar una placa antigua.

#### CTRL,TIME (Reloj de pared)
Enviado cada 10 segundos (y una vez al arrancar la tarea de comunicación).
**Formato**: `CTRL,TIME,epoch,tzq,tzsrc`
- `epoch`: hora Unix UTC de la motherboard, o `0` si aún no ha sincronizado.
- `tzq`: offset de zona horaria en **cuartos de hora** (`-48`..`+56`, o sea
  UTC-12:00..UTC+14:00). Cuartos y no horas porque existen husos no enteros
  (Nepal, UTC+5:45), y es la unidad que ya usa `civil_to_unix_utc()`.
- `tzsrc`: origen del offset. `0`=desconocido, `1`=NITZ (red móvil), `2`=IP,
  `3`=reloj puesto a mano desde `/config`. Con `3` el offset es siempre `0`
  **por diseño**: ese epoch ya es la hora local que tecleó el operador, así que
  sumarle el offset de la red la desplazaría. `3` gana a `1` y a `2`, y nada lo
  desplaza hasta el siguiente reinicio.
  **No es redundante con `tzq`**: sin él, «offset 0 porque estamos en Togo» y
  «offset 0 porque no lo sabemos» son indistinguibles, y el HMI no puede
  decidir si pintar la hora o el aviso «Sin hora».
- **`tzq`/`tzsrc` son opcionales**: una motherBoard anterior a esta versión
  envía solo `epoch`, y el HMI lo interpreta como `tzsrc=0` (hay hora, no hay
  zona). Al revés también funciona: un HMI antiguo ignora los campos de más.
- **La zona nunca altera un epoch.** Todo lo que se almacena o se transmite
  —historial de alarmas, perfiles, Drive, ThingsBoard— sigue en UTC; el offset
  se aplica solo al formatear para una persona. Un cambio de offset no
  reinterpreta ningún registro ya escrito.
- Prioridad entre fuentes: **NITZ gana a IP, siempre**. La antena está
  físicamente donde está el equipo; una IP puede ser de una VPN, un enlace
  satelital o la sede del operador en otro país.
- La motherboard es la **única** fuente de hora del sistema (NTP por WiFi);
  el HMI no tiene RTC ni sincroniza por su cuenta. El HMI interpola con su
  propio `millis()` entre difusiones (`HMI_GetEpochNow()`).
- Cadencia de 10 s a propósito, no 1 Hz: el HMI solo la necesita para
  formatear fechas, y `known_issues.md` #2 desaconseja añadir tráfico UART
  periódico evitable.

#### HMI,SET_TIME
Ajuste manual del reloj desde la pantalla táctil de la HMI: se toca la propia
hora de cabecera (pantalla principal) para abrir el panel. Llega por el UART
motherBoard↔HMI, así que funciona sin WiFi — a diferencia del formulario
`/config` (campo `set_time`), que necesita que el navegador llegue al
webserver del motherBoard por WiFi. Pensado para el caso GPRS puro sin NITZ
fiable, donde `/config` es inalcanzable.

**Formato**: `HMI,SET_TIME,YYYY,MM,DD,HH,MM` → `CTRL,TIME_ACK,0|1`
- Mismo contrato que `/config,set_time`: los campos son la hora LOCAL tal
  cual la ve el operador en el reloj de pared, sin zona — la motherBoard la
  aplica con offset `0` vía `systemClockSetManual()`, exactamente la misma
  función que usa el formulario web. Una vez fijada así, ninguna fuente
  automática (NITZ, NTP, IP) la desplaza hasta el siguiente reinicio.
- `CTRL,TIME_ACK,0` = aceptada. `CTRL,TIME_ACK,1` = rechazada (fecha/hora
  fuera de rango, año anterior a 2021).

#### CTRL,PROFILE_LIST (Respuesta a HMI,PROFILE_LIST_REQ)
Lista de los perfiles de bebé activos (0–3 slots).
**Formato**: `CTRL,PROFILE_LIST,n{,seq,name,gestWeeks,weightGrams,kangarooCount,phototherapyMin,thermoMin,humidityMin}×n`
- `weightGrams`: `0` = nunca informado (SKIP).
- `kangarooCount`: veces que el bebé ha salido con la madre.
- `phototherapyMin` / `thermoMin` / `humidityMin`: minutos acumulados **de ese
  bebé** bajo fototerapia, bajo control térmico y bajo control de humedad. No
  confundir con los contadores de vida del equipo
  (`Phototherapy_active_time`, `Control_active_time`,
  `Humidifier_active_time`).
- `humidityMin` cuenta cualquier actuación que incluya humedad (`act=2` o
  `act=3`), así que un tramo con temperatura + humedad suma en `thermoMin` y
  en `humidityMin` a la vez.

#### CTRL,PROFILE_ACK (Respuesta a PROFILE_NEW / PROFILE_SELECT / PROFILE_DISCHARGE)
Confirma la operación con el `seq` afectado.
**Formato**: `CTRL,PROFILE_ACK,seq`
- Tras `PROFILE_NEW`: `seq` es el identificador recién asignado (el HMI lo
  usa en el resto del flujo). `seq=0` = operación rechazada (p. ej. no hay
  slot elegible para desalojo porque el candidato está controlando ahora).
- Tras `PROFILE_SELECT`/`PROFILE_DISCHARGE`: eco del `seq`, o `0` si el
  `seq` no existe.

#### CTRL,PROFILE_RANGE (Respuesta a PROFILE_WEIGHT / PROFILE_AGE_MANUAL)
Rango NTE calculado por la motherboard (tabla clínica en `shared/nte_table`).
**Formato**: `CTRL,PROFILE_RANGE,seq,ageKnown,ageDays,lo,hi,mid,estimated`
- `ageKnown=0`: la placa no pudo derivar la edad (sin hora sincronizada o
  perfil sin `admissionEpoch`); el HMI debe preguntarla y responder con
  `HMI,PROFILE_AGE_MANUAL`. En ese caso `lo/hi/mid` llegan como `-1.0`.
- `estimated=1` con `lo=hi=mid=-1.0`: sin rango automático (peso SKIP o
  fuera de tabla). El HMI arranca AIR en manual y bloquea SKIN.

#### CTRL,PROFILE_HISTORY (Respuesta a HMI,PROFILE_HISTORY_REQ)
Página del historial de bebés archivados, más recientes primero.
**Formato**: `CTRL,PROFILE_HISTORY,page,totalCount,n{,seq,name,gestWeeks,lastWeightGrams,admissionEpoch,dischargeEpoch,outcome,cause,kangarooCount,phototherapyMin,thermoMin,humidityMin}×n`
- `outcome`: `0`=Desconocido, `1`=Sobrevivió, `2`=Fallecido, `3`=Trasladado.
- `cause` (BabyCause): solo relevante cuando `outcome=2`; para el resto de
  outcomes se guarda y se envía tal cual llegó (normalmente `0`), sin
  reinterpretarlo. `0`=Desconocida, `1`=Prematuridad, `2`=Asfixia perinatal,
  `3`=Sepsis/infección, `4`=Malformación congénita, `5`=Hipotermia,
  `6`=Otra. Orden por frecuencia de causas de mortalidad neonatal en
  entornos de bajos recursos (OMS/UNICEF), no alfabético.
- `dischargeEpoch=0`: nunca se dio de alta explícitamente (desalojo FIFO).
- Longitud máxima de línea (10 entradas, todos los campos al ancho máximo):
  ~1130 caracteres. Buffers dimensionados a 1280 en ambas placas — cualquier
  campo nuevo obliga a recalcular este margen.

#### CTRL,WEIGHT_HISTORY (Respuesta a HMI,WEIGHT_HISTORY_REQ)
Curva de evolución de peso, submuestreada a ≤50 puntos equiespaciados.
**Formato**: `CTRL,WEIGHT_HISTORY,seq,n{,dayOffset,weightGrams}×n`
- `dayOffset`: días de vida desde la admisión (o desde el primer punto si
  la admisión no tiene fecha).
- Longitud máxima de línea (peor caso, n=50): <700 caracteres — el buffer
  RX del HMI debe dimensionarse en consecuencia (≥1024).

#### CTRL,ALM_HISTORY (Respuesta a HMI,ALM_HISTORY_REQ)
Registro de alarmas exigido por IEC 60601-1-8 6.12.2: las últimas 10
condiciones, persistidas en NVS de la motherBoard.
**Formato**: `CTRL,ALM_HISTORY,n{,id,prio,resolved,raisedEpoch,clearedEpoch,limitCenti,valueCenti,titulo}×n`
- `n`: 0..10. El HMI descarta la línea entera si `n` está fuera de rango o si
  alguna entrada viene incompleta — nunca pinta un registro clínico a medias.
- `prio`: `0`=BAJA, `1`=MEDIA, `2`=ALTA.
- `resolved`: `1` si la condición ya se resolvió. Es un campo **propio**, no se
  deduce de `clearedEpoch != 0`: sin hora sincronizada la placa guarda
  `clearedEpoch = 0` también al resolverse, y una alarma resuelta quedaba
  indistinguible de una viva. El equipo arranca sin NTP, así que ese era el
  caso normal, no el raro.
- `raisedEpoch=0`: la placa no tenía hora sincronizada al registrarla.
- `clearedEpoch=0`: la placa no tenía hora al resolverla (mírese `resolved`
  para saber si se resolvió).
- `limitCenti`/`valueCenti`: límite en vigor y medida que la disparó, ×100;
  `0` cuando no aplica a esa condición.
- El **título viaja resuelto y traducido** por la motherBoard, no el id: la
  placa es la dueña de la información de alarmas y el display sólo la pinta.
- Longitud máxima de línea: buffer `ALARM_HISTORY_LINE_BUF_SIZE` (1024). La
  descripción **no** cabe aquí y por eso va aparte (`CTRL,ALM_DESC`).

#### CTRL,ALM_DESC (Respuesta a HMI,ALM_DESC_REQ)
Descripción de una alarma concreta, ya traducida.
**Formato**: `CTRL,ALM_DESC,id,titulo,descripcion`
- La descripción es el último campo y **puede contener comas**: se parsea
  hasta el fin de línea, no hasta la siguiente coma.
- Se pide bajo demanda, sólo al abrir el detalle de una entrada del registro
  (acción humana y esporádica). No entra en la telemetría periódica:
  `known_issues.md` #2 documenta una inundación real de UART por tráfico
  periódico evitable.

### 2. Mensajes del Display (HMI → MCU)

#### HMI,UI_READY (Handshake Crítico)
Enviado una sola vez cuando la interfaz gráfica del HMI está completamente cargada.
**Efecto**: La Motherboard responde reenviando inmediatamente el estado de todas las alarmas activas.

#### HMI (Comandos de Configuración)
Enviado cuando el usuario cambia un parámetro.
**Formato**: `HMI,act,skinE,mode,airSet,skinSet,humSet,photo,mute,lang,photoMin`

> Nota (v2.0.0): las versiones 1.5.x reales añadían 3 campos extra no
> documentados (`babyWeightGrams,babyGestWeeks,babyAgeDays`) a esta línea.
> Esos campos se han eliminado — los datos del bebé viajan ahora por los
> mensajes `PROFILE_*` dedicados. Ambas placas deben flashearse juntas
> (cambio breaking sin shim de compatibilidad, por diseño).

#### HMI,REQ,STATE
Solicitud manual de sincronización completa.

#### HMI,PROFILE_LIST_REQ
Pide la lista de perfiles activos. Respuesta: `CTRL,PROFILE_LIST`.

#### HMI,PROFILE_NEW
Crea un perfil de bebé nuevo (el wizard de activación).
**Formato**: `HMI,PROFILE_NEW,name,gestWeeks`
- `name`: sin comas (el teclado del HMI bloquea la tecla; la placa además
  las filtra defensivamente). Máx. 23 caracteres.
- Respuesta: `CTRL,PROFILE_ACK,seq`. Si los 3 slots están llenos, se
  desaloja por FIFO (menor `seq`) el que no esté controlando (`activeSeq`).

#### HMI,PROFILE_SELECT
Selecciona un bebé existente para el wizard.
**Formato**: `HMI,PROFILE_SELECT,seq` → `CTRL,PROFILE_ACK,seq|0`

#### HMI,PROFILE_WEIGHT
Peso actual del bebé del wizard (o SKIP si no se conoce).
**Formato**: `HMI,PROFILE_WEIGHT,seq,grams` o `HMI,PROFILE_WEIGHT,seq,SKIP`
- Con peso: actualiza el último peso conocido y añade un punto
  `(timestamp,grams)` a la curva de evolución (deduplicado si no cambió).
- Respuesta: `CTRL,PROFILE_RANGE`.
- Hasta este mensaje NADA se persiste del wizard en curso (un reinicio a
  mitad de wizard no toca los slots).

#### HMI,PROFILE_AGE_MANUAL
Edad en días introducida a mano (solo cuando `CTRL,PROFILE_RANGE` llegó con
`ageKnown=0`).
**Formato**: `HMI,PROFILE_AGE_MANUAL,seq,ageDays` → `CTRL,PROFILE_RANGE`
- No modifica `admissionEpoch`: si la hora vuelve a sincronizarse, la
  derivación automática se reanuda sola.

#### HMI,PROFILE_DISCHARGE
Alta explícita del bebé con su resultado clínico.
**Formato**: `HMI,PROFILE_DISCHARGE,seq,outcome,cause` → `CTRL,PROFILE_ACK,seq|0`
- `outcome`: `0`=Desconocido, `1`=Sobrevivió, `2`=Fallecido, `3`=Trasladado.
- `cause` (BabyCause, `0`-`6`): solo tiene sentido clínico cuando
  `outcome=2` (Fallecido); para el resto de outcomes el HMI envía `0` y la
  motherBoard lo almacena igual, sin exigir ni interpretar nada especial.
  `0`=Desconocida, `1`=Prematuridad, `2`=Asfixia perinatal,
  `3`=Sepsis/infección, `4`=Malformación congénita, `5`=Hipotermia,
  `6`=Otra.
- Línea con 4 campos (formato anterior a v2.2.0) o `cause` fuera de `0-6` se
  descarta entera como malformada (security.md) — no se archiva el alta a
  medias.
- Archiva inmediatamente (historial + curva de peso) y libera el slot. No
  apaga el control AIR/SKIN si estaba activo (acciones independientes).

#### HMI,PROFILE_KANGAROO
El bebé sale de la incubadora para estar con la madre (método canguro).
**Formato**: `HMI,PROFILE_KANGAROO,seq` → `CTRL,PROFILE_ACK,seq|0`
- **No es un alta**: el perfil se queda en su slot activo y conserva su
  protección `activeSeq`. Solo incrementa `kangarooCount` y sella
  `lastKangarooEpoch` (0 si no hay hora sincronizada — el contador sube
  igualmente, para no perder el evento por no tener fecha).
- Se publica a ThingsBoard como `baby_kangaroo_count` /
  `baby_kangaroo_last_epoch`. Los minutos por terapia van en
  `baby_phototherapy_min` y `baby_thermo_min`.

#### HMI,PROFILE_HISTORY_REQ
Página del historial de archivados (10 por página, página 0 = más reciente).
**Formato**: `HMI,PROFILE_HISTORY_REQ,page` → `CTRL,PROFILE_HISTORY`

#### HMI,WEIGHT_HISTORY_REQ
Curva de peso de cualquier bebé (activo o archivado).
**Formato**: `HMI,WEIGHT_HISTORY_REQ,seq` → `CTRL,WEIGHT_HISTORY`

#### HMI,ALM_HISTORY_REQ
Registro completo de alarmas (últimas 10).
**Formato**: `HMI,ALM_HISTORY_REQ` → `CTRL,ALM_HISTORY`

#### HMI,ALM_DESC_REQ
Descripción de una alarma concreta, para el pop-up de detalle.
**Formato**: `HMI,ALM_DESC_REQ,id` → `CTRL,ALM_DESC`
- `id` fuera de `[0, ALARM_COUNT)` se descarta con log en la motherBoard.

#### HMI,ALM_SILENCE
AUDIO PAUSED de **una** condición concreta.
**Formato**: `HMI,ALM_SILENCE,id,on` (sin respuesta; el efecto se observa en
`silencedBitmask` del siguiente `CTRL,STATE`)
- `on=1`: silencia esa condición durante `ALARM_AUDIO_PAUSE_MS` (10 min).
- `on=0`: **cancela** el silencio de inmediato. Lo exige 60601-1-8 6.8.4
  (*"Means shall be provided for the OPERATOR to terminate any ALARM SIGNAL
  inactivation state"*).
- Silenciar una condición no afecta a las demás (6.8.1).
- `id` fuera de rango o línea malformada: descarte con log.

#### HMI (trama de estado periódica)
El display manda su trama **al menos cada 1 s** (`HMI_KEEPALIVE_PERIOD_MS`),
aunque no haya cambiado nada. Es el latido que permite a la motherBoard
detectar `ALARM_HMI_LINK_LOST` tras 5 s de silencio (`HMI_LINK_TIMEOUT_MS`).
Cualquier trama con prefijo `HMI,` cuenta como latido, no solo la de estado:
lo que se vigila es que el display siga hablando, no lo que diga.

#### HMI,ALM_TEST
Lanza la prueba de funcionamiento de las señales de alarma
(60601-2-19 201.12.3.105).
**Formato**: `HMI,ALM_TEST` (sin respuesta directa; el avance se observa en
`almTest` de `CTRL,STATE`)
- Reproduce **una ráfaga de cada prioridad**, de BAJA a ALTA, por el mismo
  camino de audio que las alarmas reales. Comprobar una imitación no valdría:
  lo que se verifica es la cadena que sonará de verdad.
- **La placa la rechaza** si hay cualquier condición señalizando, y una alarma
  que aparezca a mitad la **cancela en el acto**. Una prueba no puede pisar el
  patrón de una alarma real: el operador dejaría de poder identificar qué suena
  (6.3.2.2.2).
- No toca actuadores ni declara condición alguna.

### 3. Test de fábrica (`FTEST`)

Batería de comprobaciones de hardware para el montaje en fábrica y el servicio
en campo. La lanza el operario desde la fila "Test de hardware" de la pantalla
de ajustes del display (zona técnica, tras el candado con pulsación larga;
solo con control y fototerapia apagados); el display ejecuta primero sus tests
locales y después ordena a la motherBoard los suyos.
Identificadores, estados y codec viven en `shared/include/factory_test.h`
(`ftest_format_*` / `ftest_parse_*`): **ninguna de las dos placas tiene una
copia propia de la tabla**.

#### HMI,FTEST,START / HMI,FTEST,RUN / HMI,FTEST,ABORT / HMI,FTEST,CONFIRM
- `HMI,FTEST,START` — batería completa (los `FTEST_MB_COUNT` = 28 tests).
- `HMI,FTEST,RUN,<id>` — un solo test (reintento). `id` debe ser de motherBoard
  (`0..27`); si no, `CTRL,FTEST_REJECT,2`.
- `HMI,FTEST,ABORT` — cancela la batería en curso. El test que estuviera
  corriendo termina como `SKIP` con `detail=abort` y se emite `FTEST_DONE`.
- `HMI,FTEST,CONFIRM,<id>,<0|1>` — respuesta del operario a un `CONFIRM`.
  Desde la cuarta vuelta ningún test de motherBoard usa ya este camino
  (`buzzer`, el único que preguntaba, SKIP directo sin micrófono): la placa
  sigue aceptando el comando para no romper a un display que lo mande, pero
  lo descarta con log "sin uso".

##### Orden de ejecución (quinta ronda, banco 2026-09-06: solape cooperativo)
Sin esto, `START` tardaba 4-5 min en fábrica sin cobertura celular ni AP: cada
test de conectividad (`gsm_at` 45 s, `gsm_sim` 15 s, `gsm_signal` 15 s,
`gsm_net`/`wifi`/`tb_provision`/`time` 30 s cada uno) agotaba su plazo
secuencialmente uno detrás de otro. La tabla se divide en **PASIVOS** (solo
observan un estado ya cacheado por otra tarea o una petición asíncrona ya en
vuelo: `charger`, `env_sensor`, `sb_status`, `sb_camera`, `gsm_at`, `gsm_sim`,
`gsm_signal`, `gsm_net`, `wifi`, `tb_provision`, `time` y los instantáneos
`sysinfo`, `ina3221`, `skin_adc`, `hmi_link`, `nvs`, `littlefs`, `power_src`,
`humid_usb`, `sb_door`, `afe_probe`) y **ACTIVOS** (`standby`, `actuators`,
`fan_rpm`, `buzzer`, `afe_spi`, `sb_env`, `sb_light`, en ese orden — no el
ascendente de id: `sb_env`/`sb_light` van al final para dar tiempo a que
`env_sensor` resuelva la cascada `sb_usb` en paralelo). Al arrancar `START`,
la motherBoard emite `RUNNING` de **todos** los pasivos a la vez y arranca un
único plazo común para ellos; los activos corren secuenciales, un turno cada
uno, como siempre, pero los pasivos siguen resolviéndose por debajo mientras
tanto. El HMI puede ver **varios `RUNNING` simultáneos** y los resultados
**no llegan en orden de id**: el test que "manda" en cada momento (el que
tiene el foco de la UI de fábrica) es el último que emitió `RUNNING`. Un
`ABORT`/dead-man del HMI/control reactivado/tope de batería corta de golpe a
los pasivos que sigan pendientes (`SKIP` con el motivo), igual que al activo
en curso. Peor caso: **~45 s** (el plazo más largo, `gsm_at`) en vez de
4-5 min. `HMI,FTEST,RUN,<id>` de un solo test no cambia: si `id` es pasivo se
sondea igual, sin el resto de la batería corriendo debajo.

**Precondición**: la motherBoard rechaza `START` y `RUN` con
`CTRL,FTEST_REJECT,1` si `in3.actuation != OFF` o hay fototerapia activa. Los
tests de actuadores encienden el calefactor y la fototerapia en lazo abierto y
no pueden pisar un control real; el test no tiene autoridad para apagarlo.
Con una batería ya en curso responde `CTRL,FTEST_REJECT,0`.

Mientras dura la batería la placa pone `in3.alarmsEnabled = false`, levanta
`g_factoryTestActive` (que inhibe todos los escritores de PWM: `PIDHandler()`,
`turnFans()`, el bloque `newCommand` de la trama HMI, la regulación de
fototerapia y `buzzerHandler()`) y deja todos los PWM a 0; lo restaura siempre
al terminar, abortar o fallar. Ese estado tiene cotas DE TODA LA BATERÍA: la
tarea está en el task WDT, la batería aborta a los 6 min (`detail=max time`),
si el HMI deja de enviar líneas durante 5 s (`detail=hmi lost`) o si alguien
enciende control o fototerapia a mitad (`detail=control on`); cualquiera de
estas cuatro hace que el resto de tests pendientes salga `SKIP`. Por eso el
display debe seguir emitiendo su trama periódica mientras la pantalla de test
está abierta.

Además hay una cota **por test** (`FTEST_TEST_TIMEOUT_MS` = 90 s), distinta de
las cuatro de arriba: si un cuerpo concreto tarda más de eso (ninguno de los
plazos propios de la tabla de abajo pasa de 60 s, así que superarlo solo
puede ser un cuerpo colgado), ESE test sale `FAIL` con `detail=timeout` y la
batería **continúa** con el siguiente — no se trata como las cotas de arriba,
que abortan el resto de la batería entera. Un cuerpo bloqueado dentro de una
llamada I2C/USB no cooperativa (sin bucle de sondeo que compruebe nada) no lo
detecta ni esta cota ni las de arriba: solo lo cubre el task WDT global
(75 s), que reinicia la placa — es el motivo por el que, desde esta versión,
ningún test de la tabla hace ya I2C directo salvo `actuators`/`standby`
(reusan `actuatorsTest()`/`testStandByCurrent()` tal cual, sin tocarlos).

#### CTRL,FTEST (resultado de un test)
**Formato**: `CTRL,FTEST,<id>,<status>,<detail>`
- `status`: `0` RUNNING (empieza), `1` PASS, `2` FAIL, `3` SKIP (opcional sin
  entorno, o en cascada tras un fallo previo), `4` WAIT (espera un estímulo del
  operario: abrir la puerta, tapar el sensor de luz), `5` CONFIRM (pregunta al
  operario; espera `HMI,FTEST,CONFIRM`), `6` WARN (no pudo completarse por
  falta de entorno — sin cobertura, sin AP, sin servidor, sin hora — dentro de
  su plazo; no es un fallo de la placa, pero es un estado FINAL: cuenta en
  `FTEST_DONE` y debe verse en ámbar, no ocultarse como SKIP).
- `detail`: último campo, hasta `FTEST_DETAIL_MAX` (40) caracteres, sin comas
  (el codec sustituye `,`/`\r`/`\n` por `;`). Lleva la medida o el motivo
  (`timeout`, `sin sim`, `sin sonda`, `sin usb`…). Puede ir vacío.
- Cada test emite exactamente un `RUNNING` y un resultado final; los estados
  `WAIT`/`CONFIRM` pueden repetirse y no cuentan en los totales.
- El receptor descarta la línea si faltan campos, si `id`/`status` no son
  numéricos, si `id` no es de motherBoard o `status` > 6.
- Las líneas salen por una **cola** drenada por la tarea de comunicación
  (`CommunicationHost_Enqueue()`), nunca desde la tarea de test: el `CTRL,PPG`
  a 25 Hz comparte el mismo UART y dos escritores entrelazarían caracteres.
  Cola llena = línea perdida con log; el display la da por FAIL por plazo.

| id | clave | qué verifica | opcional |
|---|---|---|---|
| 0 | sysinfo | flash, heap libre, reset reason, MAC | |
| 1 | ina3221 | ambos INA3221 presentes (0x40/0x41), solo flags cacheados | |
| 2 | standby | `testStandByCurrent()` sin bits nuevos en `HW_error` | |
| 3 | charger | el BQ25730 responde (estado cacheado por `sensors_Task`, ≤ 12 s); VBAT/VSYS y red/batería en detail, sin exigir VBUS ni carga activa; agotado el plazo → **WARN** `sin vbus` (sin VBUS es normal que no responda, no un fallo de placa) | |
| 4 | power_src | **omitido por ahora** (`SKIP`, detail `omitido`): a batería el BQ25730 no está alimentado (sin VBUS); reactivar cuando el jig alimente por VBUS | |
| 5 | skin_adc | NTC en rango con lectura de `sensors_Task` reciente (≤ 5 s) o `sin sonda` | |
| 6 | env_sensor | sensor ambiental por CUALQUIERA de los tres caminos (un equipo lleva SensorBoard O sensor ambiental, no ambos): SHT40 de la SensorBoard por USB (detail `usb`), STS35/SHTC3 por I2C2 (equipo antiguo, detail `i2c`) o SHT4x exterior por I2C1 (detail `sht4x`); FAIL `sin sensor ambiental` a los 10 s | |
| 7 | sb_status | `status`: sht0/sht1/sht2/als/door/cam disponibles; `fw` y `usb_swap` en detail; SKIP `sin usb` si 6 no fue por USB | |
| 8 | sb_env | 3×SHT40 válidas, dispersión ≤ 1.0 °C; vs SHT4x exterior ≤ 3.0 °C solo si hay uno con lectura fresca (si no, PASA solo con la dispersión); SKIP `sin usb` | |
| 9 | sb_door | **omitido por ahora** (`SKIP`, detail `omitido`, sin emitir WAIT): el jig de fábrica todavía no tiene la puerta montada; reactivar cuando la tenga | |
| 10 | sb_light | WAIT: lux cae < 50 % de la base (20 s); base < 20 lux → SKIP; SKIP `sin usb` | |
| 11 | sb_camera | `capture` devuelve JPEG ≥ 1000 B en 10 s; SKIP `sin usb` | |
| 12 | actuators | `actuatorsTest()` sin bits nuevos de calefactor/foto/ventilador | |
| 13 | fan_rpm | tacómetro con feedback y RPM ≥ `FAN_MIN_RPM` | |
| 14 | humid_usb | **omitido por ahora** (`SKIP`, detail `omitido`): el jig de fábrica todavía no puede medir el humidificador; no toca `USB_EN` | |
| 15 | buzzer | ΔdBA ≥ 6 dB con el micrófono de la SensorBoard; sin micrófono → **SKIP** `sin microfono` (ya no pregunta al operario, camino CONFIRM retirado) | |
| 16 | afe_spi | registros de timing del AFE4490 releídos por SPI; DIAG en detail | |
| 17 | afe_probe | sonda SpO2 conectada | ✓ |
| 18 | hmi_link | enlace con el display vivo | |
| 19 | gsm_at | módem ha respondido a algún AT o ya está más adelante en la secuencia (`GPRS.modemResponded`/`simReady`/`connect`/`post`, ≤ 45 s) | |
| 20 | gsm_sim | SIM lista (`GPRS.simReady`, `+CPIN: READY`, ≤ 15 s); CCID en detail si ya se leyó | |
| 21 | gsm_signal | CSQ 1..31 (≤ 15 s); agotado el plazo → **WARN** `sin señal` (conectarse a la red celular es opcional) | |
| 22 | gsm_net | adjunto a red (≤ 30 s); agotado el plazo → **WARN** `sin red` | ✓ |
| 23 | wifi | conectado al AP por defecto (≤ 30 s), RSSI en detail; agotado el plazo → **WARN** `sin AP` | ✓ |
| 24 | tb_provision | sesión ThingsBoard aceptada con el token provisionado (≤ 30 s); `sin serie` → **WARN** inmediato; agotado el plazo → **WARN** `sin servidor` | ✓ |
| 25 | time | hora sincronizada dentro de 30 s; fuente en detail; agotado el plazo → **WARN** `sin hora` | ✓ |
| 26 | nvs | escribir y releer `mb_ftest/probe` | |
| 27 | littlefs | partición montada | |

El test `env_sensor` (id 6) fusiona a los antiguos `ext_sht4x` (6) y
`sensorboard` (7): un equipo lleva SensorBoard O sensor ambiental, no ambos, y
exigir los dos por separado convertía la ausencia del que ese equipo no lleva
en un FAIL de fábrica con hardware sano. Con una SensorBoard conectada, el
sondeo I2C2 de clasificación de generación se hace además sobre las mismas
líneas que son D+/D− del USB y devuelve un ACK falso. Lo que importa en
fábrica es que la cabina tenga sensor ambiental de alguna fuente, no por qué
camino llega. Los tests `sb_status`/`sb_env`/`sb_door`/`sb_light`/`sb_camera`
solo tienen datos que leer si `env_sensor` pasó por el camino USB; si pasó por
I2C2/SHT4x, o si no llegó a correr (un `RUN` aislado de uno de ellos sin haber
corrido antes el 6), salen `SKIP` con detail `sin usb`.

**Ningún test hace ya I2C directo** (banco 2026-09-06): no hay mutex de bus
I2C en la motherBoard, así que una transacción I2C lanzada desde el cuerpo de
un test se entrelaza con las de `sensors_Task` (bucle de 1 ms, con el
refresco del BQ25730 cada 5 s), y puede ver un ACK falso o quedarse
esperando una respuesta que nunca llega — es lo que dejaba `charger` en
RUNNING para siempre con la placa a batería, y lo que hacía FALLAR
`skin_adc`/`ext_sht4x`/el antiguo sondeo de generación de `sensorboard` con
hardware sano. Los tests leen ahora el estado que ya mantienen esas tareas;
las únicas excepciones son `actuators`/`standby`, que reusan
`actuatorsTest()`/`testStandByCurrent()` sin tocarlos (ya funcionaban así en
banco).

Los tests GSM/WiFi/ThingsBoard son **pasivos**: leen el estado que ya
recogen `GPRS_Task` y la tarea WiFi, no envían comandos AT ni tocan
credenciales. El módem no tiene PWRKEY cableado y un reset por software lo
dejaría apagado hasta ciclo de alimentación. `gsm_signal` y `gsm_net` siguen
en cascada: `SKIP` con detail `sin sim` si `gsm_sim` falló, en vez de esperar
un plazo entero sin SIM.

#### CTRL,FTEST_DONE
**Formato**: `CTRL,FTEST_DONE,<pass>,<fail>,<skip>,<warn>` — cierre de la
batería o del test único. `warn` es el cuarto campo (avisos, estado `WARN`);
al **parsear**, una línea de 3 campos (placa anterior a esta versión) se
acepta con `warn = 0`, y una de 5 o más se rechaza. Los contadores coinciden
con la secuencia de resultados emitida. Tras una batería completa la placa
persiste en NVS `mb_ftest` el epoch, las máscaras PASA/FALLA/AVISO/EJECUTADO,
`FWversion` y el `fw` de la SensorBoard; un `RUN` actualiza solo los bits de
su `id`.

#### CTRL,FTEST_REJECT
**Formato**: `CTRL,FTEST_REJECT,<reason>` — `0` batería ya en curso, `1`
control o fototerapia activos, `2` `id` desconocido.

### Validación (ambos sentidos)
Toda línea `PROFILE_*`/`WEIGHT_HISTORY_*`/`ALM_*` malformada (número de campos
incorrecto, campo numérico no parseable, `outcome` fuera de 0–3, `cause`
fuera de 0–6, nombre vacío) se descarta en silencio con log de error — nunca se actúa sobre
datos parciales (regla general del protocolo, `.claude/rules/security.md`).

---

## Lógica de Sincronización Robusta

Para garantizar que el sistema siempre muestre el estado correcto, se siguen estas reglas:

1.  **Handshake**: Tras un reinicio o reconexión del HMI, este debe enviar `HMI,UI_READY`. La Board no disparará el reenvío de alarmas hasta recibir este comando o detectar tráfico válido.
2.  **Auto-Corrección vía Bitmask**: Si el HMI recibe un mensaje de `CTRL,STATE` con un `alarmBitmask` que no coincide con su lista interna de alarmas pintadas en pantalla, el HMI limpiará visualmente las alarmas que no figuren en el bitmask.
3.  **Filtrado de Etiquetas**: El HMI implementa un filtro de cambios en `update_labels()` para ignorar actualizaciones de texto idénticas, mejorando la respuesta táctil.

---

## Enlace con el SensorBoard (Motherboard ↔ SensorBoard, USB)

Enlace **distinto** al de la HMI: aquí la motherboard actúa de **USB host** y el
SensorBoard es un dispositivo **CDC-ACM nativo** (TinyUSB, VID `0x303A` / PID
`0x4001`), con framing binario propio en vez de las líneas `CTRL,...`/`HMI,...`.

**Los pines 19/20 tienen dos funciones según la generación del equipo.** En los
antiguos son el bus **I2C2** hacia una PCBA con SHTC3 + STS35 principal y
redundante; en los nuevos son el **USB** hacia el SensorBoard, que lleva esos
sensores a bordo (3× SHT40). No es un conflicto de pines: es un multiplexado
por generación, y la fuente se decide **una vez por arranque** sondeando el bus
I2C2 (`initRoomSensor()` ya lo sondea en 10 ms por dirección). Si responde
alguien → equipo antiguo, y el host USB **no se levanta**. Si el bus está mudo
→ equipo nuevo: se sueltan los pads y se abre el enlace USB. No se persiste en
NVS: un solo cambio de modo por arranque. Ver
`modules/sensors/sensor_source.h`.

**En modo SensorBoard, sus lecturas NO son telemetría auxiliar: son el sensor
de aire y de humedad de la incubadora.** `sensorboard_apply_room_sensor()`
—llamada desde la tarea de sensores, la misma que escribe esas variables en el
camino I2C— funde las posiciones válidas, aplica el mismo gate de
plausibilidad y alimenta `in3.temperature[ROOM_DIGITAL_TEMP_SENSOR]`,
`in3.airTemperatureRedundantSensor`, `in3.humidity[ROOM_DIGITAL_HUM_SENSOR]` y
`lastSuccesfullSensorUpdate[]`. Cadena de seguridad:

```
SensorBoard publica cada 1 s -> rancio a los 3 s -> deja de refrescarse el
sello -> ALARM_AIR_SENSOR_FAULT (ALTA, corta calefactor) a los 5 s
```

Esa es la protección real: `ALARM_SENSORBOARD_LINK_LOST` es el aviso que
explica **por qué**, no la única señal. Con la cadencia de 5 s original, una
sola publicación perdida cortaba el calefactor.

**El wire format lo define el SensorBoard** — fuente de verdad:
`SensorBoard_v2/README.md` y su `sensorBoard_comm_protocol.h`. Aquí solo se
documenta lo que la motherboard hace con él.

- **Trama**: `Magic(0xAB 0xCD) + Type(1B) + Length(4B LE) + Payload + CRC16(2B BE)`,
  CRC16-CCITT FALSE sobre `Type+Length+Payload`. `Type 0x00` = JSON (≤256 B),
  `Type 0x01` = JPEG. Los comandos salientes viajan enmarcados igual.
- **Consumido** (`modules/sensorboard_comm/`): `heartbeat`, `sensor_data`
  (3 temperaturas + 3 humedades + lux, con `null` por sensor caído),
  `door_open`/`door_closed`, `sound_level`, `log`, y las respuestas de
  `status`/`capture`. **Emitido**: `status` y `capture` bajo demanda.
- **DTR es obligatorio**: el SensorBoard guarda *toda* su transmisión con la
  señal de line state (`s_cdc_ready`), así que el host debe asertar DTR/RTS
  tras abrir el dispositivo. Sin eso el enlace enumera, abre y no llega ni un
  byte, y el único síntoma sería la alarma de enlace perdido.
- **Fail-safe del enlace**: sin `heartbeat` durante 90 s (3 periodos de 30 s) se
  declara `ALARM_SENSORBOARD_LINK_LOST` (prioridad MEDIA, mismo criterio que
  `ALARM_HMI_LINK_LOST`). Antes del primer heartbeat el enlace cuenta como
  caído, aunque el USB haya enumerado, pero la alarma tiene **60 s de margen de
  arranque** mientras no se haya visto ningún heartbeat: el primero llega a los
  30 s y sin margen sonaba una alarma audible en cada encendido. Una
  desconexión física del USB tumba el enlace **de inmediato**, sin esperar el
  timeout. Un `uptime` que retrocede delata un reinicio del SensorBoard con el
  USB enumerado e invalida las lecturas anteriores.
- **Fail-safe de la puerta**: 4 transiciones `open`/`closed` dentro de 60 s se
  tratan como hall sospechoso (`ALARM_SENSORBOARD_DOOR_FAULT`, prioridad BAJA).
  Las re-aserciones periódicas del mismo estado no cuentan como transición.
- **Ninguna de las dos alarmas corta el calefactor por sí misma**: quien corta
  es `ALARM_AIR_SENSOR_FAULT`, por caducidad del dato, igual que con un STS35
  averiado. La puerta y el nivel sonoro **no entran en el lazo de control ni en
  la UI**.
- **Telemetría**: la temperatura y la humedad salen por las claves clínicas de
  siempre (`Air_temp`, `Amb_humidity`…), porque son las de la incubadora. Solo
  lo genuinamente nuevo viaja como `sb_*` (luz, sonido, puerta, estado del
  enlace), cada clave omitida si su magnitud está rancia. Grupo `SENSORBOARD`
  de `config/transport_policy.h`: activo por WiFi, apagado por GPRS mientras el
  presupuesto de esa publicación siga al límite.
- **Captura JPEG**: `sensorboard_capture_request()` la encola y la transmite la
  tarea del módulo (único dueño del handle). El JPEG se recoge con
  `sensorboard_capture_take()`, que **transfiere la propiedad** del buffer; hay
  que devolverlo con `sensorboard_capture_free()`. Se reserva del montón solo
  mientras la captura está en vuelo (esta placa no tiene PSRAM), con tope de
  48 KB y caducidad de 10 s si el binario no llega. Hoy **no se sube a ningún
  sitio**.
