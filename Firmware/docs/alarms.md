# Sistema de alarmas de IncuNest

Este documento describe el comportamiento que implementa el código hoy, no el
que debería tener. Cada cifra remite al fichero y a la constante que la
define para que pueda verificarse sin tener que releer todo el módulo. La
justificación normativa (qué cláusula de IEC 60601-2-19 / IEC 60601-1-8 exige
cada cosa) vive en `docs/alarms_normative_analysis.md`; aquí solo se cita la
cláusula que el propio código referencia en sus comentarios.

El sistema tiene tres capas, cada una en su propio fichero:

- **Política** (`shared/include/alarm_policy.h`, `shared/src/alarm_policy.cpp`):
  funciones puras que, dado un `AlarmId`, devuelven su prioridad, si es
  *latching* y si corta el calefactor. No tienen estado ni dependen de
  Arduino, así que se comparten entre motherBoard y el análisis normativo.
- **Máquina de estados** (`motherBoard/src/modules/control/alarm_machine.{h,cpp}`):
  mantiene, para cada una de las 16 condiciones, si está físicamente presente
  y en qué estado de señalización se encuentra. Es el único sitio con
  temporizadores.
- **Detección y actuación** (`motherBoard/src/system/security.cpp`,
  `PID.cpp`, `Actuators.cpp`): mide sensores, decide si cada condición física
  está presente, llama a la máquina de estados y aplica los cortes de
  actuador.

## 1. Las 16 condiciones

El enum `AlarmId` (`shared/include/alarm_ids.h`) define 16 condiciones más el
centinela `ALARM_NONE = 0`. El valor numérico es el índice de bit del
protocolo serie, así que el orden es significativo y las condiciones nuevas
se añaden al final.

| ID | Identificador | Qué la dispara en el código | Prioridad | Corta calefactor |
|---|---|---|---|---|
| 1 | `ALARM_AIR_THERMAL_CUTOUT` | `in3.temperature[ROOM_DIGITAL_TEMP_SENSOR] > in3.airTemperatureSetMax` (histéresis 0.2 °C, `AIR_THERMAL_CUTOUT_HYSTERESIS`, `security.cpp:173`), evaluada en `checkThermalCutOuts()` | ALTA | sí |
| 2 | `ALARM_SKIN_THERMAL_CUTOUT` | `in3.temperature[SKIN_SENSOR] > in3.skinTemperatureSetMax` (histéresis 0.2 °C, `SKIN_THERMAL_CUTOUT_HYSTERESIS`), misma función | ALTA | sí |
| 3 | `ALARM_AIR_SENSOR_FAULT` | lectura del sensor de aire sin refrescar durante `MINIMUM_SUCCESSFULL_SENSOR_UPDATE` = 20000 ms (`checkStatusOfSensor()`) | ALTA | sí |
| 4 | `ALARM_SKIN_SENSOR_FAULT_SKIN_MODE` | igual staleness que el 3, pero de la sonda de piel, y solo cuando `in3.controlMode == CONTROL_SKIN` | ALTA | sí |
| 5 | `ALARM_FAN_FAILURE` | `in3.fan_rpm < FAN_MIN_RPM` (3000 rpm, `board.h:185`), con histéresis de 300 rpm para despejar (`checkFanSpeed()`); solo evaluable con `in3.fanHasSpeedFeedback` | ALTA | sí |
| 6 | `ALARM_AIR_OUTLET_BLOCKED` | `fanControlPIDOutput > FAN_DUTY_BLOCKED_THRESHOLD` (160, `board.h:207`) sostenido `AIR_BLOCKED_SUSTAIN_MS` = 5000 ms (`checkAirBlockage()`) | ALTA | sí |
| 7 | `ALARM_MAINS_INTERRUPTION` | **sin detector** — ver §7 | ALTA | no (no hay condición que cortar) |
| 8 | `ALARM_AIR_TEMP_DEVIATION_HIGH` | en modo aire, `temperatura − consigna > 3.0 °C` (`AIR_TEMP_DEVIATION_LIMIT_C`, `checkAlarms()`) | MEDIA | sí |
| 9 | `ALARM_AIR_TEMP_DEVIATION_LOW` | en modo aire, `consigna − temperatura > 3.0 °C`, y solo tras cerrar la ventana de estabilización (§5) | MEDIA | no |
| 10 | `ALARM_SKIN_TEMP_DEVIATION_HIGH` | en modo piel, `temperatura − consigna > 1.0 °C` (`SKIN_TEMP_DEVIATION_LIMIT_C`) | MEDIA | sí |
| 11 | `ALARM_SKIN_TEMP_DEVIATION_LOW` | en modo piel, `consigna − temperatura > 1.0 °C`, tras cerrar la ventana | MEDIA | no |
| 12 | `ALARM_HEATER_FAULT` | corriente de calefactor fuera de `[HEATER_CONSUMPTION_MIN, HEATER_CONSUMPTION_MAX]` en el autotest de arranque, o `HEATER_SENSOR_DROPOUT_ALARM_CYCLES` = 10 fallos consecutivos de sondeo I2C del sensor de corriente SECUNDARY en marcha (`initHardware.cpp`, `sensors_module.cpp`) | MEDIA | sí |
| 13 | `ALARM_SUPPLY_UNDERVOLTAGE` | `MIN_SYSTEM_VOLTAGE_TRIGGER (0 V) < in3.system_voltage < MAX_SYSTEM_VOLTAGE_TRIGGER (8 V)`, y solo si `digitalCurrentSensorPresent[MAIN]`, muestreado cada `POWER_SUPPLY_CHECK_PERIOD` = 2000 ms (`powerSupplyCheck()`) | MEDIA | no (corta ventilador, no calefactor) |
| 14 | `ALARM_HMI_LINK_LOST` | **sin detector** — ver §7 | MEDIA | no |
| 15 | `ALARM_SKIN_SENSOR_FAULT_AIR_MODE` | staleness de la sonda de piel en modo aire, solo si la sonda llegó a leer alguna vez en este ciclo de encendido (`skinProbeEverRead`) | BAJA | no |
| 16 | `ALARM_HUMIDITY_DEVIATION` | `|humedad − consigna| > 10 %RH` (`HUMIDITY_ERROR`), solo con `in3.humidityControl` activo y tras cerrar la ventana de estabilización | BAJA | no |

Todas las condiciones con umbral usan banda muerta (`thresholdWithHysteresis()`,
`security.cpp:217`): se declaran al superar el umbral y no se retiran hasta
caer por debajo de `umbral − histéresis`. Los valores de histéresis de cada
condición están en la tabla o en los comentarios citados.

## 2. Prioridades: Tabla 1 de IEC 60601-1-8 y su reparto real

`alarm_priority()` (`shared/src/alarm_policy.cpp`) asigna la prioridad de
forma estática por `AlarmId`, sin lógica dinámica. El reparto que produce el
código es:

- **ALTA (7)**: `AIR_THERMAL_CUTOUT`, `SKIN_THERMAL_CUTOUT`, `AIR_SENSOR_FAULT`,
  `SKIN_SENSOR_FAULT_SKIN_MODE`, `FAN_FAILURE`, `AIR_OUTLET_BLOCKED`,
  `MAINS_INTERRUPTION`.
- **MEDIA (7)**: `AIR_TEMP_DEVIATION_HIGH`, `AIR_TEMP_DEVIATION_LOW`,
  `SKIN_TEMP_DEVIATION_HIGH`, `SKIN_TEMP_DEVIATION_LOW`, `HEATER_FAULT`,
  `SUPPLY_UNDERVOLTAGE`, `HMI_LINK_LOST`.
- **BAJA (2)**: `SKIN_SENSOR_FAULT_AIR_MODE`, `HUMIDITY_DEVIATION`.

Este 7/7/2 coincide exactamente con el reparto que propone
`alarms_normative_analysis.md` §5 a partir de la Tabla 1 (resultado de no
responder × tiempo de aparición), así que el código ya refleja esa derivación,
condición por condición.

Un `AlarmId` fuera de rango (el `default` del `switch`) devuelve ALTA
deliberadamente — sobreestimar la urgencia de algo no reconocido es la opción
seguridad-primero, según el comentario de `alarm_policy.h:9-10`.

`alarm_machine_top_priority()` (`alarm_machine.cpp:181`) calcula la prioridad
más alta entre las condiciones que se están anunciando (ACTIVE, SILENCED o
ACKED); si no hay ninguna, devuelve BAJA por convención de valor de retorno,
no porque exista una condición de prioridad baja activa.

## 3. Puertas de actuador

Hay tres puertas, cada una con su propio conjunto de condiciones y su propia
recuperación. Ninguna de las tres depende de si la condición se está
*señalizando*: todas leen `alarm_machine_condition()` — el estado físico — y
no `alarm_machine_state()` — el estado de la máquina. Esto se explica más
abajo, es intencional y está comentado explícitamente en el código.

| Puerta | Función | Efecto | Condiciones | Recuperación |
|---|---|---|---|---|
| Calefactor | `ongoingCriticalAlarm()` → `alarm_machine_heater_must_cut()` | `HeaterPIDOutput * !ongoingCriticalAlarm()` escrito en `ledcWrite(HEATER_PWM_CHANNEL, ...)`, tanto en la rama de control por aire como por piel (`PID.cpp:201,211`) | `alarm_cuts_heater()` (`alarm_policy.cpp:36`): IDs 1, 2, 3, 4, 5, 6, 8, 10, 12 (9 condiciones) | automática, en cuanto la condición física deja de estar presente |
| Ventilador | `ongoingFanCriticalAlarm()` | `fanControlPIDOutput * !ongoingFanCriticalAlarm()` (`PID.cpp:267`) y el PWM abierto de `turnFans()` (`Actuators.cpp:46,70`) | ID 5 (`FAN_FAILURE`), ID 13 (`SUPPLY_UNDERVOLTAGE`), e ID 12 (`HEATER_FAULT`) **solo si `!in3.fanHasSpeedFeedback`** (`security.cpp:437-442`) | automática |
| Lazo de control (`in3.temperatureControl`) | `ongoingCriticalWiringAlarm()` | bloquea únicamente el **intento de volver a encender** el control de temperatura desde un comando HMI entrante (`main.cpp:393,397,405`); si la condición aparece con el control ya encendido, esta puerta no lo apaga por sí sola — solo lo hacen las puertas de calefactor/ventilador arriba | IDs 12, 5, 13 (`HEATER_FAULT`, `FAN_FAILURE`, `SUPPLY_UNDERVOLTAGE`) | manual: el operador debe reintentar activar el control una vez la condición ya no esté presente |

`ongoingAlarms()` (`alarm_machine_any_signalling()`) es una cosa distinta:
cubre las 16 condiciones y solo se usa para decidir si el audio debe seguir
sonando (§5), no para gatear ningún actuador.

**El humidificador no está gateado por ninguna de estas tres puertas.** Un
corte térmico, un fallo de ventilador o un fallo de tensión detienen el
calefactor y posiblemente el ventilador, pero el humidificador sigue
inyectando vapor. Tiene su propia protección, independiente del sistema de
alarmas: en hardware con `HW_NUM >= 16`, `checkUsbFault()`
(`security.cpp:1040-1054`) lee la línea activa-baja `USB_FAULT` y, ante un
cortocircuito o sobrecarga, apaga el humidificador, borra
`in3.humidityControl` y detiene su PID — sin levantar ningún `AlarmId`.

**Por qué el corte se decide por condición física y no por señal.** Los dos
cortes térmicos (IDs 1 y 2) son *latching*: la señal (visual/audio) sobrevive
a que la temperatura ya haya bajado, hasta que alguien la resetea
manualmente. Si el corte de calefactor leyera el estado de la máquina en vez
de la condición física, un corte térmico que ya se enfrió pero sigue
"latcheado" seguiría bloqueando el calefactor indefinidamente — y a la
inversa, silenciar o aceptar una alarma (que solo cambia el estado de
señalización, nunca la condición física, ver §5) nunca podría, por
construcción, restaurar el calefactor mientras la condición siga presente, ni
cortarlo mientras no lo esté. El comentario de `security.cpp:420-424` lo deja
explícito: *"la maquina la aplica sobre la condicion FISICA presente, no
sobre la senal"*.

## 4. Ciclo de vida de una condición

`AlarmState` (`alarm_machine.h:12-18`) define cinco estados:

1. **INACTIVE** — condición no presente.
2. **PENDING** — condición presente, dentro del retardo de anuncio.
3. **ACTIVE** — anunciándose: señal visual y audio.
4. **SILENCED** — audio inactivo por acción del operador; la señal visual
   sigue.
5. **ACKED** — audio inactivo indefinidamente; la señal visual sigue.

**Retardo de anuncio.** `alarm_machine_set_announce_delay()` fija, por
condición, cuánto puede esperar el estado PENDING antes de madurar a ACTIVE.
Los cortes térmicos lo ignoran siempre (`alarm_machine_condition()`,
`alarm_machine.cpp:57-58`: *"un corte termico nunca espera"*) — se declaran
ACTIVE de inmediato, con audio, en el mismo ciclo en que se detectan. Las
demás condiciones que fijan un retardo (hoy, solo el lado caliente de las
desviaciones de temperatura, §5) permanecen en PENDING hasta que expire; el
estado PENDING ya cuenta en `alarm_machine_bitmask()`, así que la condición es
visible en el bitmask del protocolo desde el primer ciclo — el retardo solo
aplaza el audio, no la señal visual.

**Silenciado.** `alarm_machine_silence(id, duration_ms, now)` mueve una
condición ACTIVE a SILENCED durante `duration_ms`; al expirar,
`alarm_machine_tick()` la hace madurar de vuelta a ACTIVE
(`alarm_machine.cpp:89-92`). El único punto del firmware que invoca esta
función es `reestartOngoingAlarms()` (`security.cpp:785-792`), llamada desde
`encSwitchHandler()` (`ISR.cpp:148-165`) al pulsar el encoder: silencia, una
por una, **todas** las condiciones que en ese instante estén en ACTIVE,
durante `ALARM_AUDIO_PAUSE_MS` = 120000 ms (2 min, `security.cpp:180`). Como
el silencio actúa condición por condición y solo sobre las que ya están
activas en el momento de la pulsación, una condición nueva que aparezca
después de silenciar suena con normalidad — no hereda el silencio de las
demás.

**Aceptación.** `alarm_machine_ack(id, now)` mueve una condición ACTIVE o
SILENCED a ACKED de forma indefinida. Existe en la máquina de estados y tiene
tests unitarios, pero **no hay ningún punto de la firmware de producción que
la invoque** — ni el encoder, ni el protocolo USB, ni la página `/config`
llaman a `alarm_machine_ack()`. Hoy la única acción de operador disponible es
el silencio temporal de 2 minutos descrito arriba; el estado ACKED es
alcanzable solo desde tests.

**Latching y reset manual.** `alarm_is_latching()` (`alarm_policy.cpp:32-34`)
es verdadero únicamente para los dos cortes térmicos. Cuando la condición
física de un corte térmico desaparece, `alarm_machine_condition()`
(`alarm_machine.cpp:67-74`) no toca el estado: sigue en ACTIVE/SILENCED/ACKED
aunque ya no haya sobretemperatura. `alarm_machine_is_latched(id)` es
verdadero mientras eso ocurra, y `alarm_machine_reset(id, now)` es la única
función que puede devolverla a INACTIVE — y solo si la condición física ya no
está presente. **Esta función solo se llama desde los tests unitarios
(`test_alarm_machine.cpp`); no existe ningún camino de producción — encoder,
USB o `/config` — que la invoque.** En la práctica, hoy la única forma de
limpiar la señal de un corte térmico ya enfriado es reiniciar el equipo:
`initAlarms()` (`security.cpp:260-270`) reinicializa toda la máquina
(`alarm_machine_init()`) y se llama una única vez, desde el arranque
(`initHardware.cpp:905`).

## 5. La asimetría de las desviaciones de temperatura

`checkAlarms()` (`security.cpp:794-877`) trata el lado caliente y el lado frío
de la desviación de temperatura de forma deliberadamente distinta, y el
propio código lo documenta como no siendo una inconsistencia
(`security.cpp:808-822`):

- **Lado caliente** (`AIR_TEMP_DEVIATION_HIGH`, `SKIN_TEMP_DEVIATION_HIGH`):
  se declara **siempre**, incluso durante la ventana de estabilización de 30
  min tras la activación. `declareHotDeviation()` (`security.cpp:250-258`)
  arma el retardo de anuncio con lo que le quede a la ventana en el flanco de
  subida, pero la condición física se declara ya, con el corte de calefactor
  ya vigente desde el primer ciclo (el retardo de anuncio no afecta a
  `alarm_machine_heater_must_cut()`, que mira la condición física). Retener
  este lado durante la ventana no silenciaría un aviso: inhibiría una
  protección térmica, justo durante la rampa desde frío, que es cuando el
  sobreimpulso del PID es más probable.
- **Lado frío** (`AIR_TEMP_DEVIATION_LOW`, `SKIN_TEMP_DEVIATION_LOW`): se
  evalúa solo tras `stabilizationElapsed()` (`security.cpp:236-239`) — antes
  de eso, la condición ni siquiera se declara como presente. No gobierna
  ningún actuador, así que cerrarla durante el calentamiento no compromete
  ninguna protección, y evita que cada arranque normal (por ejemplo, de 22 °C
  de sala a una consigna de 36 °C) levante una alarma MEDIA visible en
  pantalla y publicada a nube durante media hora.

La razón normativa que cita el código: 201.15.4.2.1 dd) y ee) condicionan
literalmente la existencia de la desviación a que ya se hayan alcanzado
*"STEADY TEMPERATURE CONDITIONS"* — durante el calentamiento, la norma no
reconoce la condición. El lado caliente se declara de todos modos por
conservadurismo (es más estricto que lo exigido), y es lo único que mantiene
inmediato el corte de calefactor asociado.

La humedad (`ALARM_HUMIDITY_DEVIATION`) sigue el mismo patrón de puerta que el
lado frío: `evaluateHumidity` exige `stabilizationElapsed()` además de
`in3.humidityControl` (`security.cpp:866-868`), porque tampoco gobierna
actuador alguno.

La ventana de estabilización, `ACTUATORS_ALARM_STABILIZATION_MINS` = 30 min
(`main.h:124`), se reinicia en la transición OFF→ON de la actuación
(`main.cpp:385-386`, llamando a `alarmTimerStart()` sin argumento, que usa el
valor por defecto de 30 min completos por delante). Un arranque que reanuda
estado tras un crash/watchdog (`initHardware.cpp:964`) llama en cambio a
`alarmTimerStart(RESTART_ALARM_GRACE_MINS)` con `RESTART_ALARM_GRACE_MINS` = 0
(`main.h:125`): la ventana nace ya completamente consumida, es decir, sin
ninguna espera adicional, bajo la premisa de que la terapia ya estaba en
marcha y probablemente estabilizada antes del reinicio. La aritmética de la
ventana (`alarm_window_start()`/`alarm_window_remaining_ms()`,
`alarm_window.cpp`) usa resta modular sin signo sobre `millis()`
deliberadamente, para seguir dando el intervalo correcto al desbordar el
reloj a los ~49 días.

## 6. Límites acotados de los cortes térmicos

`alarm_policy.h` define tres constantes de acotado:
`ALARM_AIR_CUTOUT_MAX_C` = 38.0 °C, `ALARM_SKIN_CUTOUT_MAX_C` = 40.0 °C, y un
suelo común `ALARM_CUTOUT_MIN_C` = 34.0 °C. `alarm_clamp_air_cutout()` y
`alarm_clamp_skin_cutout()` (`alarm_policy.cpp:59-65`) recortan cualquier
valor propuesto a `[34.0, 38.0]` o `[34.0, 40.0]` respectivamente.

Importante no confundir el acotado con el valor por defecto: los valores de
fábrica (`main.h:304-305`) son `SKIN_TEMPERATURE_SET_MAX` = 37.5 °C y
`AIR_TEMPERATURE_SET_MAX` = 38 °C — el de piel arranca 2.5 °C por debajo de su
propio techo de acotado (40 °C), no en el límite.

`in3.airTemperatureSetMax` / `in3.skinTemperatureSetMax` son escribibles en
caliente, y las tres vías de escritura existentes aplican el acotado en todas
ellas:

1. **Arranque desde NVS** (`EEPROM.cpp:323-333`): al leer
   `KEY_SKIN_T_MAX`/`KEY_AIR_T_MAX`, si el valor es NaN o ≤0 se sustituye por
   el valor por defecto, y en cualquier caso se pasa por
   `alarm_clamp_skin_cutout()`/`alarm_clamp_air_cutout()`.
2. **Página web `/config`** (`Wifi_OTA.cpp:453-461`): el valor que llega en
   `wifiServer.arg("air_tmax")`/`"skin_tmax"` se acota antes de guardarse en
   `in3` y en NVS.
3. **Comando USB** (`CommTask.cpp:551-557`): mismo patrón, acotando antes de
   escribir a `in3` y a NVS.

Con esto, un valor propuesto de 45 °C para el corte de aire queda recortado a
38 °C en cualquiera de las tres vías; no hay un cuarto punto de entrada que
modifique estos campos sin pasar por el acotado (búsqueda de
`airTemperatureSetMax`/`skinTemperatureSetMax` en `motherBoard/src`).

## 7. Protocolo de sincronización motherBoard → display

Todo lo que sigue es lo verificable desde `security.cpp`; el comportamiento
interno del display queda fuera de alcance de este documento.

La máquina de alarmas es un **estado**, no un flujo de eventos: cada ciclo de
`securityCheck()` (`security.cpp:1096-1114`) recalcula `alarm_machine_bitmask()`
y `publishAlarmChanges()` (`security.cpp:1060-1078`) lo compara contra el
bitmask del ciclo anterior. Solo los bits que cambian generan una línea
`CTRL,ALM,<id>,<titulo>,<descripcion>,<0|1>\n` vía `sendAlarmUSB()`
(`security.cpp:596-767`); el resto del bitmask completo, junto con `alarmCount`,
se envía además en la trama periódica de estado desde `CommTask.cpp`, que
también dispara `resendActiveAlarms()` (reenvía la línea `CTRL,ALM` de cada
condición actualmente señalizando) cuando `alarmCount > 0`.

Si el HMI no está conectado (`hmi_connected == false`), los eventos de cambio
se encolan en `pending_alarms[10]` (`security.cpp:41`); la cola tiene
capacidad fija de 10 entradas y **descarta silenciosamente** cualquier evento
por encima de ese límite (`pending_alarm_count < 10`, `security.cpp:751`) —
no hay mecanismo de resincronización más allá de la próxima
`resendActiveAlarms()` con el bitmask ya consolidado. Al reconectar,
`setHMIConnected(true)` vacía la cola con `sendPendingAlarms()`.

El texto de cada alarma (`alarmIDtoString()`, `security.cpp:447-551`) y su
línea de acción (`sendAlarmUSB()`) están en ASCII puro, sin acentos, en los
tres idiomas soportados (`in3.language`): las fuentes del display no
representan caracteres fuera de ese rango.

## 8. Qué NO está implementado todavía

Esta sección es tan relevante como el resto: enumera lo que el sistema no
hace hoy, para no inducir a confiar en protecciones que no existen.

- **`ALARM_MAINS_INTERRUPTION` no tiene detector.** Ningún punto del código
  llama a `alarm_machine_condition(ALARM_MAINS_INTERRUPTION, ...)`. La
  condición existe en el enum, tiene prioridad ALTA asignada, pero nunca se
  activa. Un detector real exigiría además una reserva de energía dedicada
  que sobreviva 10 minutos de corte de red — es un requisito de hardware, no
  solo de firmware.
- **`ALARM_HMI_LINK_LOST` no tiene detector.** Igual que el anterior: existe
  en el enum y en la tabla de prioridades (MEDIA), pero ningún punto del
  firmware de motherBoard llama a `alarm_machine_condition(ALARM_HMI_LINK_LOST, ...)`.
- **No hay reset manual accesible al operador para los cortes térmicos
  latching.** `alarm_machine_reset()` solo se invoca desde
  `test_alarm_machine.cpp`. No existe gesto de encoder, comando USB ni
  entrada de `/config` que la llame. Hoy, un corte térmico que ya se enfrió
  permanece señalizando (visual y, tras su ráfaga mínima, en silencio de
  audio) hasta que el equipo se reinicia por completo.
- **No hay aceptación (ACK) indefinida accesible al operador.** `alarm_machine_ack()`
  existe y tiene tests, pero no se invoca desde ningún punto de producción.
  La única acción de operador disponible hoy es el silencio temporal de 2
  minutos por pulsación de encoder (§4).
- **El patrón acústico de la Tabla 3 de IEC 60601-1-8 no está implementado.**
  `Buzzer.cpp` solo expone un contador de conmutaciones (`buzzerAlarmBeepCount`
  = 500) a periodo fijo (`buzzerAlarmBeepTime` = 500 ms), sin estructura de
  ráfagas discretas ni silencio entre ráfagas. Las constantes
  `ALARM_MIN_BURST_MS_HIGH`/`_MEDIUM` (`alarm_machine.h:23-24`) garantizan una
  duración mínima de audio en la máquina de estados, pero no producen el
  patrón de pulsos por ráfaga que exige la norma. Además, el tercer argumento
  de `buzzerTone()` (el tono) es código muerto: `buzzerHandler()` nunca lo usa,
  y el tono real queda fijado una vez en el arranque por
  `ledcSetup(BUZZER_PWM_CHANNEL, BUZZER_PWM_FREQUENCY, ...)`
  (`initHardware.cpp:228`) — las alarmas no se distinguen hoy por tono.
- **El audio puede detenerse solo, sin acción del operador, si una condición
  permanece activa más de ~250 s continuos.** `driveAlarmBuzzer()`
  (`security.cpp:1082-1094`) llama a `buzzerTone()` una única vez, en el
  flanco de subida agregado de `alarm_machine_audio_required()`; si ninguna
  otra condición produce un nuevo flanco mientras la primera sigue activa,
  `buzzerAlarmBeepCount` (500 toggles × 500 ms ≈ 250 s) se agota y el piezo
  calla aunque la condición física no se haya retirado ni el operador haya
  actuado.
- **Detección de cortocircuito en la sonda de piel: no implementada.** El
  detector de fallo (`checkStatusOfSensor()`) es un timeout de 20 s de
  `lastSuccesfullSensorUpdate`, que cubre desconexión y circuito abierto pero
  no una sonda cortocircuitada que siga devolviendo una lectura verosímil.
- **La detección de obstrucción de salida de aire está activa pero sin
  calibrar en banco.** `AIR_BLOCKED_DETECTION_ENABLED` es `true`
  (`board.h:220`) y ya corta el calefactor a través de `alarm_cuts_heater()`,
  pero el propio código advierte, literalmente, que
  `FAN_DUTY_BLOCKED_THRESHOLD` (160) *"is still NOT bench-calibrated"* y que
  *"the unit must not go to the field until that threshold is validated"*
  (`board.h:214-219`). Un falso positivo con esta puerta activa corta el
  calefactor de verdad.
- **Canal de corte térmico independiente del termostato: no existe.** El
  corte por aire (ID 1) lee el mismo `ROOM_DIGITAL_TEMP_SENSOR` que usa el PID
  de control. Un único fallo de sensor se lleva por delante tanto el control
  como su propio corte de seguridad. Esto no se resuelve en firmware: exige
  un segundo canal de temperatura físicamente independiente.
- **Prueba de función de alarma para el operador: no implementada.** No hay
  ningún medio para que el operador compruebe el funcionamiento del audio y
  del indicador visual bajo demanda.
- **Nivel sonoro: sin medir.** No hay verificación en este repositorio de que
  el piezo alcance el nivel exigido a 3 m ni de que se mantenga por debajo
  del límite dentro del habitáculo; son medidas de banco/hardware, no de
  firmware.

## Notas sobre el sistema anterior

La versión previa de este documento describía un sistema de 10 identificadores
(`TEMPERATURE_ALARM`, `AIR_BLOCKED_ALARM`, etc.) con un `evaluateAlarm()` y un
`setAlarm()` que ya no existen en este árbol: la arquitectura actual —
`alarm_ids.h`, `alarm_policy.{h,cpp}`, `alarm_machine.{h,cpp}`,
`alarm_window.{h,cpp}` — la sustituyó por completo. Ninguna cifra ni mecanismo
de aquella versión debe darse por válido sin comprobarlo contra el código: ver
el informe de la tarea para el detalle de qué afirmaciones concretas resultaron
falsas.
