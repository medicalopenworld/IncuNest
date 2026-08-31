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
  mantiene, para cada una de las 17 condiciones, si está físicamente presente
  y en qué estado de señalización se encuentra. Es el único sitio con
  temporizadores.
- **Detección y actuación** (`motherBoard/src/system/security.cpp`,
  `PID.cpp`, `Actuators.cpp`): mide sensores, decide si cada condición física
  está presente, llama a la máquina de estados y aplica los cortes de
  actuador.

## 1. Las 17 condiciones

El enum `AlarmId` (`shared/include/alarm_ids.h`) define 17 condiciones más el
centinela `ALARM_NONE = 0`. El valor numérico es el índice de bit del
protocolo serie, así que el orden es significativo y las condiciones nuevas
se añaden al final.

| ID | Identificador | Qué la dispara en el código | Prioridad | Corta calefactor |
|---|---|---|---|---|
| 1 | `ALARM_AIR_THERMAL_CUTOUT` | `in3.temperature[ROOM_DIGITAL_TEMP_SENSOR] > in3.airTemperatureSetMax` (histéresis 0.2 °C, `AIR_THERMAL_CUTOUT_HYSTERESIS`, `security.cpp:191`), evaluada en `checkThermalCutOuts()` | ALTA | sí |
| 2 | `ALARM_SKIN_THERMAL_CUTOUT` | `in3.temperature[SKIN_SENSOR] > in3.skinTemperatureSetMax` (histéresis 0.2 °C, `SKIN_THERMAL_CUTOUT_HYSTERESIS`), misma función | ALTA | sí |
| 3 | `ALARM_AIR_SENSOR_FAULT` | lectura del sensor de aire sin refrescar durante `MINIMUM_SUCCESSFULL_AIR_SENSOR_UPDATE` = 5000 ms (`checkStatusOfSensor()`) | ALTA | sí |
| 4 | `ALARM_SKIN_SENSOR_FAULT_SKIN_MODE` | mismo mecanismo de staleness que el 3 pero sobre la sonda de piel, con su propia ventana `MINIMUM_SUCCESSFULL_SKIN_SENSOR_UPDATE` = 5000 ms (~25 muestras a 200 ms), y solo cuando `in3.controlMode == CONTROL_SKIN` | ALTA | sí |
| 5 | `ALARM_FAN_FAILURE` | en marcha, `in3.fan_rpm < FAN_MIN_RPM` (3000 rpm, `board.h:185`), con histéresis de 300 rpm para despejar (`checkFanSpeed()`); solo evaluable con `in3.fanHasSpeedFeedback`. **También la declara el autotest de arranque** (`initHardware.cpp:412,677,732,740,779`), antes de que exista lazo de control | ALTA | sí |
| 6 | `ALARM_AIR_OUTLET_BLOCKED` | en marcha, `fanControlPIDOutput > FAN_DUTY_BLOCKED_THRESHOLD` (190, `board.h:221`) sostenido `AIR_BLOCKED_SUSTAIN_MS` = 5000 ms (`checkAirBlockage()`). **También la declara el autotest de arranque** (`initHardware.cpp:803`) | ALTA | sí |
| 7 | `ALARM_MAINS_INTERRUPTION` | **sin detector** — ver §9 | ALTA | no (no hay condición que cortar). **No silenciable**: 201.12.3.103 exige 10 min de aviso y la pausa dura justo eso |
| 8 | `ALARM_AIR_TEMP_DEVIATION_HIGH` | en modo aire, `temperatura − consigna > 3.0 °C` (`AIR_TEMP_DEVIATION_LIMIT_C`, `checkAlarms()`) | MEDIA | sí |
| 9 | `ALARM_AIR_TEMP_DEVIATION_LOW` | en modo aire, `consigna − temperatura > 3.0 °C`, y solo tras cerrar la ventana de estabilización (§5) | MEDIA | no |
| 10 | `ALARM_SKIN_TEMP_DEVIATION_HIGH` | en modo piel, `temperatura − consigna > 1.0 °C` (`SKIN_TEMP_DEVIATION_LIMIT_C`) | MEDIA | sí |
| 11 | `ALARM_SKIN_TEMP_DEVIATION_LOW` | en modo piel, `consigna − temperatura > 1.0 °C`, tras cerrar la ventana | MEDIA | no |
| 12 | `ALARM_HEATER_FAULT` | corriente de calefactor fuera de `[HEATER_CONSUMPTION_MIN, HEATER_CONSUMPTION_MAX]` en el autotest de arranque (`initHardware.cpp`). Es avería de cableado o resistencia | MEDIA | sí |
| 13 | `ALARM_SUPPLY_UNDERVOLTAGE` | `MIN_SYSTEM_VOLTAGE_TRIGGER (0 V) < in3.system_voltage < MAX_SYSTEM_VOLTAGE_TRIGGER (8 V)`, y solo si `digitalCurrentSensorPresent[MAIN]`, muestreado cada `POWER_SUPPLY_CHECK_PERIOD` = 2000 ms (`powerSupplyCheck()`) | MEDIA | no (corta ventilador, no calefactor) |
| 14 | `ALARM_HMI_LINK_LOST` | sin recibir línea válida del display durante `HMI_LINK_TIMEOUT_MS` = 5000 ms, con latido de 1 s desde el HMI (`checkHmiLink()`). No se declara antes de la primera trama | MEDIA | no |
| 15 | `ALARM_SKIN_SENSOR_FAULT_AIR_MODE` | staleness de la sonda de piel en modo aire, solo si la sonda llegó a leer alguna vez en este ciclo de encendido (`skinProbeEverRead`) | BAJA | no |
| 16 | `ALARM_HUMIDITY_DEVIATION` | `|humedad − consigna| > 10 %RH` (`HUMIDITY_ERROR`), solo con `in3.humidityControl` activo y tras cerrar la ventana de estabilización | BAJA | no |
| 17 | `ALARM_HEATER_SENSOR_FAULT` | `HEATER_SENSOR_DROPOUT_ALARM_CYCLES` = 10 fallos consecutivos de sondeo I2C del sensor de corriente SECUNDARY en marcha (`sensors_module.cpp`). El calefactor puede estar bien; lo perdido es la medida de su consumo | MEDIA | sí |

Que el autotest de arranque declare las condiciones 5 y 6 importa: son
declaraciones vivas que sobreviven al arranque y que ningún detector de marcha
retira por sí solo — solo se limpian cuando un detector declara `false`
explícitamente. Es la razón por la que `checkAirBlockage()` tiene que poder
declarar `false` incluso cuando su medida es inviable (ver §4, latching).

Todas las condiciones con umbral usan banda muerta (`thresholdWithHysteresis()`,
`security.cpp:236`): se declaran al superar el umbral y no se retiran hasta
caer por debajo de `umbral − histéresis`. Los valores de histéresis de cada
condición están en la tabla o en los comentarios citados.

## 2. Prioridades: Tabla 1 de IEC 60601-1-8 y su reparto real

`alarm_priority()` (`shared/src/alarm_policy.cpp`) asigna la prioridad de
forma estática por `AlarmId`, sin lógica dinámica. El reparto que produce el
código es:

- **ALTA (7)**: `AIR_THERMAL_CUTOUT`, `SKIN_THERMAL_CUTOUT`, `AIR_SENSOR_FAULT`,
  `SKIN_SENSOR_FAULT_SKIN_MODE`, `FAN_FAILURE`, `AIR_OUTLET_BLOCKED`,
  `MAINS_INTERRUPTION`.
- **MEDIA (8)**: `AIR_TEMP_DEVIATION_HIGH`, `AIR_TEMP_DEVIATION_LOW`,
  `SKIN_TEMP_DEVIATION_HIGH`, `SKIN_TEMP_DEVIATION_LOW`, `HEATER_FAULT`,
  `HEATER_SENSOR_FAULT`, `SUPPLY_UNDERVOLTAGE`, `HMI_LINK_LOST`.
- **BAJA (2)**: `SKIN_SENSOR_FAULT_AIR_MODE`, `HUMIDITY_DEVIATION`.

Este 7/8/2 coincide con el reparto que propone
`alarms_normative_analysis.md` §5 a partir de la Tabla 1 (resultado de no
responder × tiempo de aparición), así que el código ya refleja esa derivación,
condición por condición.

Un `AlarmId` fuera de rango (el `default` del `switch`) devuelve ALTA
deliberadamente — sobreestimar la urgencia de algo no reconocido es la opción
seguridad-primero, según el comentario de `alarm_policy.h:9-10`.

Hay **dos** consultas de prioridad, y no son intercambiables:

- `alarm_machine_top_priority()` (`alarm_machine.cpp:241`) calcula la prioridad
  más alta entre las condiciones que se están anunciando (ACTIVE, SILENCED o
  ACKED). Es la prioridad de la señal **visual**: incluye SILENCED/ACKED a
  propósito, porque inactivar el audio no puede rebajar lo que se ve.
- `alarm_machine_audible_priority()` (`alarm_machine.cpp:256`) calcula la
  prioridad más alta entre las condiciones que **exigen audio ahora mismo**:
  el mismo criterio que `alarm_machine_audio_required()` (ACTIVE, o INACTIVE
  todavía dentro de su ventana de ráfaga mínima), sin SILENCED ni ACKED. Es la
  que gobierna el patrón del zumbador (§8). Sin esta distinción, silenciar una
  ALTA mientras una BAJA distinta sigue activa haría que el zumbador
  reprodujera el patrón de 10 pulsos de la ALTA para una condición que en
  realidad es BAJA.

En ambos casos, si no hay ninguna condición cualificada devuelven BAJA por
convención de valor de retorno, no porque exista una condición de prioridad
baja activa.

## 3. Puertas de actuador

Hay tres puertas, cada una con su propio conjunto de condiciones y su propia
recuperación. **Las tres no leen lo mismo, y la diferencia importa:**

- La puerta del **calefactor** lee la condición **física**:
  `alarm_machine_heater_must_cut()` recorre `Entry::present`, es decir si el
  detector está declarando la condición ahora mismo, sin mirar el estado de
  señalización.
- Las puertas del **ventilador** y del **lazo de control** leen el **estado de
  señalización**: `ongoingFanCriticalAlarm()` y `ongoingCriticalWiringAlarm()`
  están escritas sobre `alarmSignalling(id)` (`security.cpp:228-231`), que es
  `alarm_machine_state(id) != ALARM_STATE_INACTIVE`.

Hoy las dos lecturas coinciden para las condiciones que gobiernan esas dos
puertas (IDs 5, 12 y 13), porque ninguna de ellas es *latching*: su estado
vuelve a INACTIVE en el mismo ciclo en que el detector retira la condición
física, así que "presente" y "señalizando" entran y salen a la vez. La
equivalencia no es estructural, es una coincidencia del conjunto actual, y se
rompería en dos supuestos concretos:

1. Si alguna de esas tres condiciones pasara a ser *latching* (`alarm_policy.cpp:32-34`),
   su señal sobreviviría a la condición y la puerta del ventilador se quedaría
   cerrada hasta un reset manual, con el ventilador parado sin causa física.
2. Si apareciera un estado de señalización que no implique condición presente
   (hoy solo lo produce el latching), el efecto sería el mismo.

Al revés no ocurre: SILENCED y ACKED siguen contando como señalizando, así que
inactivar el audio nunca abre ninguna de las tres puertas.

| Puerta | Función | Efecto | Condiciones | Recuperación |
|---|---|---|---|---|
| Calefactor | `ongoingCriticalAlarm()` → `alarm_machine_heater_must_cut()` | `HeaterPIDOutput * !ongoingCriticalAlarm()` escrito en `ledcWrite(HEATER_PWM_CHANNEL, ...)`, tanto en la rama de control por aire como por piel (`PID.cpp:201,211`) | `alarm_cuts_heater()` (`alarm_policy.cpp:36`): IDs 1, 2, 3, 4, 5, 6, 8, 10, 12 (9 condiciones) | automática, en cuanto la condición física deja de estar presente |
| Ventilador | `ongoingFanCriticalAlarm()` | `fanControlPIDOutput * !ongoingFanCriticalAlarm()` (`PID.cpp:267`) y el PWM abierto de `turnFans()` (`Actuators.cpp:46,70`) | ID 5 (`FAN_FAILURE`), ID 13 (`SUPPLY_UNDERVOLTAGE`), e ID 12 (`HEATER_FAULT`) **solo si `!in3.fanHasSpeedFeedback`** (`security.cpp:455-460`) | automática, al volver el estado a INACTIVE |
| Lazo de control (`in3.temperatureControl`) | `ongoingCriticalWiringAlarm()` | bloquea únicamente el **intento de volver a encender** el control de temperatura desde un comando HMI entrante (`main.cpp:393,397,405`); si la condición aparece con el control ya encendido, esta puerta no lo apaga por sí sola — solo lo hacen las puertas de calefactor/ventilador arriba | IDs 12, 5, 13 (`HEATER_FAULT`, `FAN_FAILURE`, `SUPPLY_UNDERVOLTAGE`) (`security.cpp:445-450`) | manual: el operador debe reintentar activar el control una vez la condición ya no esté señalizando |

`ongoingAlarms()` (`security.cpp:422`, envoltorio de
`alarm_machine_any_signalling()`) es una cosa distinta y **no decide nada del
audio**. Guardaba la pulsación de un encoder físico que nunca llegó a montarse
en esta placa — es de una revisión de hardware anterior — y su único
llamante, `encSwitchHandler()` (`ISR.cpp`), tampoco estaba enganchado a
ninguna interrupción real (`initInterrupts()`, `initHardware.cpp`, solo
registra `fanEncoderISR`), así que era código inalcanzable. Ambas funciones se
retiraron; `ongoingAlarms()` sigue existiendo porque nada más obliga a
quitarla, pero hoy no tiene llamantes de producción.

Quien silencia de verdad es el botón de silencio del **display HMI**, la única
interacción de operador que existe hoy. Su estado llega en
`hmi_cmd_msg.muteAlarm`, y `CommTask.cpp` (motherBoard) lo cablea a
`silenceActiveAlarmsFromDisplayMute()` (`security.cpp`, la función que hasta
este cambio se llamaba `reestartOngoingAlarms()`) en el **flanco de subida**
del campo, no de forma continua: la trama HMI llega periódicamente con el bit
a nivel alto mientras el operador siga en estado muteado (el display lo
mantiene así, no lo pulsa una vez), y silenciar en cada ciclo reiniciaría la
ventana de `ALARM_AUDIO_PAUSE_MS` eternamente sin dejar que el audio se
reanudase nunca. Antes de este cambio, `hmi_cmd_msg.muteAlarm` llegaba desde
el display y no se usaba para nada — solo se registraba en log y se
devolvía en el eco de la trama de estado —, así que el botón de silencio del
display no silenciaba nada.

Quien decide si el zumbador suena es `alarm_machine_audio_required()`, y con
qué patrón, `alarm_machine_audible_priority()` (§8). Los dos criterios no
coinciden: PENDING señaliza pero no exige audio, y una condición dentro de su
ventana de ráfaga mínima exige audio sin señalizar.

**El humidificador no está gateado por ninguna de estas tres puertas.** Un
corte térmico, un fallo de ventilador o un fallo de tensión detienen el
calefactor y posiblemente el ventilador, pero el humidificador sigue
inyectando vapor. Tiene su propia protección, independiente del sistema de
alarmas: en hardware con `HW_NUM >= 16`, `checkUsbFault()`
(`security.cpp:833-846`) lee la línea activa-baja `USB_FAULT` y, ante un
cortocircuito o sobrecarga, apaga el humidificador, borra
`in3.humidityControl` y detiene su PID — sin levantar ningún `AlarmId`.

**Por qué el corte de calefactor sí se decide por condición física.** Los dos
cortes térmicos (IDs 1 y 2) son *latching*: la señal (visual/audio) sobrevive
a que la temperatura ya haya bajado, hasta que alguien la resetea
manualmente. Si el corte de calefactor leyera el estado de la máquina en vez
de la condición física, un corte térmico que ya se enfrió pero sigue
"latcheado" seguiría bloqueando el calefactor indefinidamente — y a la
inversa, silenciar o aceptar una alarma (que solo cambia el estado de
señalización, nunca la condición física, ver §4) nunca podría, por
construcción, restaurar el calefactor mientras la condición siga presente, ni
cortarlo mientras no lo esté. El comentario de `security.cpp:438-442` lo deja
explícito: *"la maquina la aplica sobre la condicion FISICA presente, no
sobre la senal"*.

Ese razonamiento es exactamente el que **no** se ha aplicado a las otras dos
puertas, porque ninguna de sus condiciones es *latching*: leer la señal les da
hoy el mismo resultado. Si mañana lo dejara de dar, la corrección sería
llevarlas también a la condición física.

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
`alarm_machine.cpp:108-110`: *"un corte termico nunca espera"*) — se declaran
ACTIVE de inmediato, con audio, en el mismo ciclo en que se detectan. Las
demás condiciones que fijan un retardo (hoy, solo el lado caliente de las
desviaciones de temperatura, §5) permanecen en PENDING hasta que expire; el
estado PENDING ya cuenta en `alarm_machine_bitmask()`, así que la condición es
visible en el bitmask del protocolo desde el primer ciclo — el retardo solo
aplaza el audio, no la señal visual.

**Silenciado (AUDIO PAUSED).** `alarm_machine_silence(id, duration_ms, now)`
mueve una condición ACTIVE a SILENCED durante `duration_ms`; al expirar,
`alarm_machine_tick()` la hace madurar de vuelta a ACTIVE
(`alarm_machine.cpp`). Hay dos vías de operador, ambas en el display HMI:

- **Por condición** — el botón SILENCIAR/REANUDAR de cada fila del centro de
  alarmas, que manda `HMI,ALM_SILENCE,<id>,<on>`. Es la única vía.
No hay botón global de silencio, y es deliberado: llegó a haberlo en la
cabecera del centro de alarmas y se retiró. Silenciaba todas las condiciones
activas de golpe sin decir cuáles, que es justo lo que 6.8.1 obliga a que el
operador pueda determinar, y creaba un segundo camino hacia el mismo estado
—uno por `hmi_cmd_msg.muteAlarm` y otro por `HMI,ALM_SILENCE`— sobre el único
control capaz de callar una alarma. `silenceActiveAlarmsFromDisplayMute()`
sigue existiendo para el flanco de `muteAlarm`, pero ya no lo dispara ninguna
interfaz.

En ambos casos la duración es `ALARM_AUDIO_PAUSE_MS` = **600000 ms (10 min)**,
definida en `motherBoard/include/main.h`.

> **Duración declarada (IEC 60601-1-8, 6.8.5).** La cláusula obliga a que la
> duración del AUDIO PAUSED se declare en las instrucciones de uso. **Son 10
> minutos.** El valor lo fija el fabricante: 60601-2-19 201.12.3.104 solo
> exige que las alarmas silenciadas deliberadamente "reanuden automáticamente
> su función normal dentro de un tiempo especificado por el FABRICANTE", sin
> imponer un límite. (La cita a 60601-1-8 6.8.3 que aparecía antes en el
> código era errónea: esa cláusula trata de los estados globales
> *indefinidos* ALARM OFF / AUDIO OFF y no fija duración alguna.)
>
> **Interacción conocida:** 201.12.3.103 exige que el aviso de interrupción de
> alimentación se mantenga un mínimo de 10 min — exactamente lo que dura esta
> pausa. Silenciar esa alarma concreta consume prácticamente toda su duración
> obligatoria. Se acepta porque la señal **visual** nunca se inactiva y el
> operador puede terminar el silencio cuando quiera (6.8.4). Si el análisis de
> riesgos lo revisa, el candidato es excluir `ALARM_MAINS_INTERRUPTION` del
> silencio, no acortar la pausa general.

Como el silencio actúa condición por condición y solo sobre las que ya están
activas en el momento de la pulsación, una condición nueva que aparezca
después de silenciar suena con normalidad — no hereda el silencio de las
demás (6.8.1).

**Cancelación del silencio (6.8.4).** `alarm_machine_unsilence(id, now)`
devuelve una condición de SILENCED a ACTIVE sin esperar a que caduque el
temporizador. Lo dispara `HMI,ALM_SILENCE,<id>,0` desde el botón REANUDAR de
la fila. La cláusula lo exige literalmente: *"Means shall be provided for the
OPERATOR to terminate any ALARM SIGNAL inactivation state"*.

**Indicación del estado (6.8.1, 6.8.5, 201.12.3.104).** Qué condiciones están
en AUDIO PAUSED viaja en `silencedBitmask` de `CTRL,STATE` — un bit por
`AlarmId`, producido por `alarm_machine_silenced_bitmask()`. El display lo
pinta en dos sitios:

- **Icono por fila** en el centro de alarmas, junto al título, más el texto
  "AUDIO EN PAUSA". Responde a 6.8.1, que exige poder determinar **cuáles**
  están inactivadas, no solo que alguna lo esté.
- **Icono permanente** en la esquina superior derecha de `lv_layer_top()`,
  visible en todas las pantallas mientras haya alguna condición callada.
  Responde a 201.12.3.104 ("las alarmas silenciadas deliberadamente mantendrán
  indicación visual") y a la legibilidad a 1 m de 6.8.5.

El icono es el símbolo **IEC 60417-5576** (campana con X) al que remite la
Tabla 5, con la X **discontinua** que la propia norma describe para los
estados temporizados. Se dibuja en LVGL (campana de la fuente + dos `lv_line`
con estilo discontinuo). **Pendiente de la evaluación formal:** contrastar las
proporciones del trazado contra la lámina original de la Tabla C.1.

**Aceptación (ACKNOWLEDGED).** `alarm_machine_ack(id, now)` existe en la
máquina de estados y tiene tests unitarios, pero **ningún punto del firmware
de producción la invoca, y es deliberado**. 60601-1-8 la trata siempre como
opcional (*"ACKNOWLEDGED, **if provided**"*) y 60601-2-19 no la pide. En una
incubadora con siete condiciones de prioridad ALTA, un estado de inactivación
indefinida permitiría callar para siempre un aviso con el paciente todavía en
riesgo, justo lo contrario de lo que persigue 201.12.3.104. Si algún día tiene
sentido, será para las condiciones *latching* que esperan reset manual — y ahí
lo que falta es el **reset**, no el ACK.

**Ráfaga mínima (6.10) y qué la cancela.** Una condición que se va antes de
haber sonado lo suficiente sigue exigiendo audio hasta completar su ventana:
`ALARM_MIN_BURST_MS_HIGH` = 1300 ms (media ráfaga de ALTA: 1150 ms reales) y
`ALARM_MIN_BURST_MS_MEDIUM` = 1000 ms (ráfaga entera de MEDIA: 850 ms reales),
`alarm_machine.h:36-37`. Ambas están atadas por comentario al patrón de pulsos
de `main.h:242-249`, del que se derivan. La ventana se sella al pasar a ACTIVE
(en `alarm_machine_condition()` o en `alarm_machine_tick()`) mediante un flag
explícito, `Entry::audio_hold_active`, y no por comparación con el instante 0:
la señal de "nunca se selló" tiene que ser un booleano, porque el 0 es un
instante válido del reloj y a los 24,86 días (millis() cruzando 2³¹) la
comparación con signo lo daba por futuro.

La ventana se cancela en las tres inactivaciones del OPERADOR —
`alarm_machine_silence()`, `alarm_machine_ack()` y `alarm_machine_reset()`;
6.10 exime de completarla *"unless inactivated by the OPERATOR"* — y **caduca
en `alarm_machine_tick()`, para todas las entradas y sin condicionar al
estado**. Esto último no es un detalle de estilo: el predicado del audio solo
se alcanza con la entrada en INACTIVE, así que una condición que aguante
ACTIVE dejaría el flag armado con su instante congelado, y si eso durase más
de 2³¹ ms, al retirarse la condición reaparecería el mismo zumbador fantasma.
Como el tick corre en cada ciclo de `securityCheck()`, la ventana se cierra a
los ~1,5 s de armarse y el flag no puede sobrevivir armado a un desbordamiento
del reloj.

Consecuencia de diseño a tener presente: `alarm_machine_audio_required()` y
`alarm_machine_audible_priority()` **no son consultas puras**. Comparten el
predicado y, al evaluarlo, cierran también las ventanas ya consumidas. No hay
carrera —el único llamante de producción es `driveAlarmBuzzer()`, en la misma
tarea que el tick, y la vía de ISR (`ongoingAlarms()`) es de solo lectura—,
pero está anotado en el header para que nadie las llame desde un contexto que
no pueda escribir la máquina.

**Latching y reset manual.** `alarm_is_latching()` (`alarm_policy.cpp:32-34`)
es verdadero únicamente para los dos cortes térmicos. Cuando la condición
física de un corte térmico desaparece, `alarm_machine_condition()`
(`alarm_machine.cpp:116-122`) no toca el estado: sigue en ACTIVE/SILENCED/ACKED
aunque ya no haya sobretemperatura. `alarm_machine_is_latched(id)` es
verdadero mientras eso ocurra, y `alarm_machine_reset(id, now)` es la única
función que puede devolverla a INACTIVE — y solo si la condición física ya no
está presente. **Esta función solo se llama desde los tests unitarios
(`test_alarm_machine.cpp`); no existe ningún camino de producción — botón de
silencio del display, USB o `/config` — que la invoque.** En la práctica, hoy
la única forma de
limpiar la señal de un corte térmico ya enfriado es reiniciar el equipo:
`initAlarms()` (`security.cpp:279-288`) reinicializa toda la máquina
(`alarm_machine_init()`) y se llama una única vez, desde el arranque
(`initHardware.cpp:905`). Es el **único** punto de reset de la máquina: hubo
un segundo, `control_module_init()`, sin llamantes, y se ha eliminado — un
reset de más borra las condiciones que ya declaró el autotest de arranque
(§1).

## 5. La asimetría de las desviaciones de temperatura

`checkAlarms()` (`security.cpp:587-670`) trata el lado caliente y el lado frío
de la desviación de temperatura de forma deliberadamente distinta, y el
propio código lo documenta como no siendo una inconsistencia
(`security.cpp:601-615`):

- **Lado caliente** (`AIR_TEMP_DEVIATION_HIGH`, `SKIN_TEMP_DEVIATION_HIGH`):
  se declara **siempre**, incluso durante la ventana de estabilización de 30
  min tras la activación. `declareHotDeviation()` (`security.cpp:269-277`)
  arma el retardo de anuncio con lo que le quede a la ventana en el flanco de
  subida, pero la condición física se declara ya, con el corte de calefactor
  ya vigente desde el primer ciclo (el retardo de anuncio no afecta a
  `alarm_machine_heater_must_cut()`, que mira la condición física). Retener
  este lado durante la ventana no silenciaría un aviso: inhibiría una
  protección térmica, justo durante la rampa desde frío, que es cuando el
  sobreimpulso del PID es más probable.
- **Lado frío** (`AIR_TEMP_DEVIATION_LOW`, `SKIN_TEMP_DEVIATION_LOW`): se
  evalúa solo tras `stabilizationElapsed()` (`security.cpp:255-258`) — antes
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
`in3.humidityControl` (`security.cpp:659-661`), porque tampoco gobierna
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
fábrica (`main.h:319-320`) son `SKIN_TEMPERATURE_SET_MAX` = 37.5 °C y
`AIR_TEMPERATURE_SET_MAX` = 38 °C — el de piel arranca 2.5 °C por debajo de su
propio techo de acotado (40 °C), no en el límite.

`in3.airTemperatureSetMax` / `in3.skinTemperatureSetMax` son escribibles en
caliente, y las tres vías de escritura existentes aplican el acotado en todas
ellas:

1. **Arranque desde NVS** (`EEPROM.cpp:322-333`): al leer
   `KEY_SKIN_T_MAX`/`KEY_AIR_T_MAX`, si el valor es NaN o ≤0 se sustituye por
   el valor por defecto, y en cualquier caso se pasa por
   `alarm_clamp_skin_cutout()`/`alarm_clamp_air_cutout()`.
2. **Página web `/config`** (`Wifi_OTA.cpp:452-462`): el valor que llega en
   `wifiServer.arg("air_tmax")`/`"skin_tmax"` se acota antes de guardarse en
   `in3` y en NVS.
3. **Comando USB** (`CommTask.cpp:550-557`): mismo patrón, acotando antes de
   escribir a `in3` y a NVS.

Con esto, un valor propuesto de 45 °C para el corte de aire queda recortado a
38 °C en cualquiera de las tres vías; no hay un cuarto punto de entrada que
modifique estos campos sin pasar por el acotado (búsqueda de
`airTemperatureSetMax`/`skinTemperatureSetMax` en `motherBoard/src`).

## 7. Protocolo de sincronización motherBoard → display

Todo lo que sigue es lo verificable desde `security.cpp`; el comportamiento
interno del display queda fuera de alcance de este documento.

La máquina de alarmas es un **estado**, no un flujo de eventos: cada ciclo de
`securityCheck()` (`security.cpp:894-912`) recalcula `alarm_machine_bitmask()`
y `publishAlarmChanges()` (`security.cpp:853-871`) lo compara contra el
bitmask del ciclo anterior. Solo los bits que cambian generan una línea
`CTRL,ALM,<id>,<titulo>,<descripcion>,<0|1>\n` vía `sendAlarmUSB()`
(`security.cpp:516-560`); el resto del bitmask completo, junto con `alarmCount`,
se envía además en la trama periódica de estado desde `CommTask.cpp`, que
también dispara `resendActiveAlarms()` (reenvía la línea `CTRL,ALM` de cada
condición actualmente señalizando) cuando `alarmCount > 0`.

Si el HMI no está conectado (`hmi_connected == false`), los eventos de cambio
se encolan en `pending_alarms[PENDING_ALARM_QUEUE_LEN]` (`security.cpp:35-60`),
con `PENDING_ALARM_QUEUE_LEN` = `ALARM_COUNT`. `resendActiveAlarms()` puede
empujar una línea por cada una de las 16 condiciones de una sola vez, y con la
capacidad fija de 10 que tenía antes las 6 últimas se perdían en silencio: una
instantánea incompleta, y arbitraria en qué condiciones se caían.

**El descarte silencioso sigue existiendo**, y el guardián que lo produce
también (`pending_alarm_count < PENDING_ALARM_QUEUE_LEN`, `security.cpp:542`).
La cola solo se vacía en `setHMIConnected(true)`, mientras que la trama
periódica de estado llama a `resendActiveAlarms()` siempre que
`alarmCount > 0`, esté el display conectado o no: con una alarma viva y el
display caído, bastan un par de periodos para llenarla. Lo que cambia con el
dimensionado por `ALARM_COUNT` es que las posiciones de la cola contienen ya
una instantánea **completa** de las condiciones señalizando, así que lo que se
descarta a partir de ahí son repeticiones de lo que la cola ya lleva. Al
reconectar, `setHMIConnected(true)` la vacía con `sendPendingAlarms()` y el
display se resincroniza además por el bitmask de la trama de estado.

**Los dos campos de texto tienen un límite y el protocolo no perdona
pasárselo.** Su capacidad se declara una sola vez, en
`shared/include/alarm_ids.h`: `ALARM_TITLE_MAX_CHARS` = 29 y
`ALARM_DESC_MAX_CHARS` = 99, sin contar el terminador. De ahí salen tanto los
buffers del display como los anchos de su `sscanf`, construidos por
stringificación en el preprocesador (`Display_HMI/src/tasks/CommTask.cpp`), de
modo que no pueden divergir. Que fueran números independientes fue un defecto
real: el display parseaba con `%31[^,]` mientras las descripciones llegaban a
43 caracteres, y como el ancho de `%[...]` **no trunca** —`sscanf` para al
llenarlo, no encuentra la coma que el formato exige y devuelve menos campos de
los pedidos— la línea entera se descartaba. La línea completa tiene además un
segundo límite en el emisor, `ALARM_LINE_BUF_SIZE` = 128, que `snprintf`
recortaría en silencio.

Los 48 textos (16 condiciones × 3 idiomas) viven en
`shared/src/alarm_text.cpp`, y no en `security.cpp`, precisamente para que el
test nativo `test/test_alarm_text/` pueda comprobar en cada ejecución que
todos caben en su campo, que la línea compuesta cabe en el buffer del emisor,
que ninguno contiene el separador de campos ni un salto de línea, y que todo
es ASCII imprimible — las fuentes del display no tienen glifos fuera de ese
rango. `alarmIDtoString()` (`security.cpp:468-471`) es hoy un envoltorio sobre
`alarm_title_text()`.

## 8. Señal acústica

El motor de audio es `buzzerAlarmUpdate()` (`Buzzer.cpp:90-166`), y lo alimenta
`driveAlarmBuzzer()` (`security.cpp:881-892`) una vez por ciclo de
`securityCheck()` con dos valores: `alarm_machine_audio_required()` (si debe
sonar) y `alarm_machine_audible_priority()` (con qué patrón).

**Se gobierna por estado, no por eventos.** No hay flanco de subida ni contador
de agotamiento. Mientras `audioRequired` siga en `true`, el patrón se regenera
indefinidamente. Es lo que exige 6.10: el audio de alarma solo cesa por acción
del operador (inactivación o reset), nunca por haber sonado "suficiente"
tiempo. La versión anterior del motor sí se agotaba —`buzzerTone()` disparado
una sola vez en el flanco, con un contador de 500 conmutaciones × 500 ms ≈ 250
s—, y esa era una no conformidad con 6.10: **está cerrada**, y las constantes
de aquel contador ya no existen en el árbol.

**El patrón de ráfaga y de pulso de las Tablas 3 y 4 está implementado**, con
las constantes de `motherBoard/include/main.h`:

| | ALTA | MEDIA | BAJA |
|---|---|---|---|
| Pulsos por ráfaga | 10 | 3 | 1 |
| Duración del pulso | 150 ms | 150 ms | 150 ms |
| Espaciado entre pulsos | `x` = 100 ms | `y` = 200 ms | — |
| Hueco entre el 5.º y el 6.º | `2x + y` = 400 ms | — | — |
| Duración de la ráfaga | 2700 ms | 850 ms | 150 ms |
| Silencio entre ráfagas | 7300 ms | 24150 ms | 29850 ms |
| Ventana de la Tabla 3 | 2,5–15 s ✔ | 2,5–30 s ✔ | > 15 s ✔ |

El **hueco de `2x + y` tras el quinto pulso** parte la ráfaga de diez en dos
grupos de cinco. No es estético: es lo que hace reconocible el patrón de
prioridad ALTA frente a otro equipo de la misma sala. Sin él suena como un
tren monótono de diez y deja de ser el patrón de la norma. (Lo estuvo: hasta
la corrección de este apartado el motor usaba un hueco uniforme de 150 ms, que
además se salía de la ventana `x` de 50–125 ms.)

**Pulso (Tabla 4).** 150 ms cumple a la vez la ventana de ALTA (75–200 ms) y la
de MEDIA/BAJA (125–250 ms), por eso hay un único valor. Las rampas de subida y
bajada son de 30 ms, el 20 % de la duración del pulso, dentro del 10–40 % que
pide la norma; se generan variando el ciclo de trabajo del PWM, única palanca
de amplitud que hay sobre un zumbador pasivo.

**Espectro (Tabla 4).** `BUZZER_PWM_FREQUENCY` = 400 Hz. La onda cuadrada da
armónicos impares en 400, 1200, 2000, 2800 y 3600 Hz: cinco picos dentro de la
banda de 150–4000 Hz (se exigen al menos cuatro) y el fundamental dentro de
150–1000 Hz siendo además la componente de mayor nivel. La constante está
**desacoplada** de `DEFAULT_PWM_FREQUENCY` a propósito: esa la comparten
calefactor y ventilador, y tocarla por motivos de potencia cambiaría el tono
del zumbador sin que nadie lo relacione.

El silencio entre ráfagas separa ráfagas **sucesivas** de un patrón que ya está
sonando, nunca la primera: un cambio de prioridad, o el primer ciclo con audio
exigido tras un silencio, arrancan la ráfaga en ese mismo ciclo (`freshStart`).
Sin esa excepción el zumbador quedaría mudo hasta 30 s tras la primera alarma.

Las duraciones de ráfaga mínima de 6.10 (§4) se derivan de estas mismas
constantes, y desde la corrección de este apartado ya no dependen de que
alguien recuerde rehacer la cuenta: `Buzzer.cpp` lleva `static_assert` que
comprueban en compilación todas las ventanas de las Tablas 3 y 4 y que
`ALARM_MIN_BURST_MS_HIGH`/`_MEDIUM` siguen cubriendo media ráfaga y ráfaga
entera. Si el patrón cambia y deja de cuadrar, la compilación falla.

> **Pendiente de medida acústica.** Lo anterior es cierto en la señal
> eléctrica. El tiempo de subida *acústico* real lo domina la respuesta
> mecánica del transductor, no la rampa de PWM, y la recomendación de la Tabla
> 4 de que las cuatro componentes mayores queden dentro de 15 dB entre sí
> depende de la respuesta del zumbador. Afirmar cumplimiento de esos dos
> puntos exige medirlos con micrófono; los niveles de presión sonora siguen
> igualmente pendientes (§9).

### 8.1 El display tiene su propio zumbador, y ahora también emite alarma

Hay **dos** transductores en el equipo: el zumbador pasivo de la motherBoard
(el descrito arriba, PWM) y el zumbador I2C del display (STC8H1K28 @ 0x30,
`Display_HMI/src/drivers/buzzer.cpp`). Este último es on/off puro, sin control
de frecuencia ni de amplitud — todo lo que se puede modelar en él es el
**ritmo**.

Reparto de qué transductor anuncia qué:

- **Todas las condiciones salvo la pérdida de enlace**: las anuncia solo la
  motherBoard, con el motor descrito arriba. El display recibe estas alarmas
  por el enlace y no las repite en su zumbador — solo pinta el banner y el
  centro de alarmas.
- **`ALARM_HMI_LINK_LOST` / pérdida de enlace vista desde el display**: la
  anuncian **los dos**, cada uno por su cuenta. La motherBoard la declara y la
  suena como cualquier otra condición MEDIA. El display, independientemente,
  detecta el mismo silencio (`Display_IsBoardLinkLost()`,
  `Display_HMI/src/tasks/CommTask.cpp`) y emite el **mismo patrón MEDIA** de
  esta sección por su zumbador (`link_audio_service()`,
  `Display_HMI/src/tasks/UITask.cpp`), leyendo las constantes de
  `shared/include/alarm_audio_pattern.h` — la misma tabla que usa la placa, no
  una copia.

Es la única condición del sistema cuyo detector vive en el display, y por eso
es la única cuya mitad audible no puede depender por entero de la
motherBoard: si su firmware se cuelga o entra en reset loop, `securityCheck()`
deja de correr y con él `driveAlarmBuzzer()` — no suena nada, y antes de este
cambio el display se limitaba a un banner mudo con las últimas cifras
recibidas en pantalla, leídas por el operador como actuales.

**Doble anuncio, aceptado.** Con el cable roto y la placa viva, los dos
zumbadores suenan el mismo patrón MEDIA, desfasados. Se acepta: el display no
puede saber si la placa sigue viva sin el enlace que precisamente ha perdido,
así que no lo intenta. No es la inconsistencia de señales que prohíbe 6.3.3.1
— las dos anuncian la misma condición, con la misma prioridad y el mismo
patrón, porque ambas derivan de `alarm_priority(ALARM_HMI_LINK_LOST)`.

**Enmascaramiento acotado, no eliminado.** Si la placa sigue viva y anuncia una
condición ALTA real mientras el display suena su MEDIA, el zumbador del
display — que en este equipo se oye más que el de la placa (medida pendiente,
más abajo) — podría tapar una ráfaga. El patrón MEDIA lo acota por
construcción: su ráfaga ocupa 850 ms de cada 25 000 ms, un 3,4 % del tiempo,
así que aunque una ráfaga ALTA quede tapada, la siguiente llega 10 s después
con el display en silencio. Un patrón más denso se comería esta mitigación,
por eso las constantes viven en un único sitio compartido y no se retocan sin
rehacer esta cuenta.

**Pausa de audio local.** Con el enlace caído, el botón de SILENCIAR del
display no llega a la placa — es justo lo que se ha perdido —, así que la
señal del display tiene su propia pausa de `ALARM_AUDIO_PAUSE_MS` (10 min,
`shared/include/alarm_policy.h`), independiente de la de la placa, con su
propio indicador AUDIO PAUSED. No sobrevive a una reconexión: una pérdida de
enlace posterior suena desde el primer instante.

## 9. Qué NO está implementado todavía

Esta sección es tan relevante como el resto: enumera lo que el sistema no
hace hoy, para no inducir a confiar en protecciones que no existen.

- **`ALARM_MAINS_INTERRUPTION` no tiene detector.** Ningún punto del código
  llama a `alarm_machine_condition(ALARM_MAINS_INTERRUPTION, ...)`. La
  condición existe en el enum, tiene prioridad ALTA asignada, pero nunca se
  activa. Un detector real exigiría además una reserva de energía dedicada
  que sobreviva 10 minutos de corte de red — es un requisito de hardware, no
  solo de firmware.
- ~~`ALARM_HMI_LINK_LOST` no tiene detector~~ — **implementado**. El display manda un latido cada 1 s (`HMI_KEEPALIVE_PERIOD_MS`) y la placa declara la condición tras 5 s sin recibir nada (`HMI_LINK_TIMEOUT_MS`, `checkHmiLink()`). Antes no era detectable: el display solo transmitía al cambiar algo, así que el silencio no significaba nada. No se declara antes de la primera trama — la placa arranca antes que el display y un enlace que nunca existió no es un enlace caído.
- ~~El display no tenía señal audible propia si la placa se colgaba~~ —
  **implementado**. El detector de esta misma condición vive en el display
  (`Display_IsBoardLinkLost()`), pero hasta ahora su mitad audible se delegaba
  entera en la motherBoard — precisamente el extremo que puede estar muerto.
  Ver §8.1.
- **No hay reset manual accesible al operador para los cortes térmicos
  latching.** `alarm_machine_reset()` solo se invoca desde
  `test_alarm_machine.cpp`. No existe botón del display, comando USB ni
  entrada de `/config` que la llame. Hoy, un corte térmico que ya se enfrió
  se queda en **ACTIVE**, no en silencio de audio: **el zumbador sigue sonando
  indefinidamente**, con su patrón de prioridad ALTA, hasta que el equipo se
  reinicia por completo. Pulsar el botón de silencio del display solo compra
  10 minutos — `ALARM_AUDIO_PAUSE_MS`—, tras los cuales `alarm_machine_tick()`
  devuelve la condición a ACTIVE y el audio se reanuda solo, y así
  indefinidamente. Es lo correcto según 201.15.4.2.1 aa)/bb) y 6.10 —la
  alarma debe operar de forma continua hasta un reset manual—, pero significa
  que la única forma de callar el equipo es apagarlo, con la incubadora aún
  en terapia. La consecuencia es peor que la de "quedarse señalizando en
  silencio": es un equipo que suena sin parar y sin vía de reset.
- **No hay aceptación (ACK) indefinida accesible al operador — y es una
  decisión, no una carencia.** `alarm_machine_ack()` existe y tiene tests,
  pero no se invoca desde producción a propósito: la norma la trata siempre
  como opcional y un estado de inactivación indefinida es indeseable en este
  equipo. Ver §4 para el razonamiento completo. La acción de operador
  disponible es el AUDIO PAUSED de 10 minutos, por condición o global, con su
  cancelación (§4).
- **Las prioridades se distinguen por ritmo, no por tono.** Y eso **cumple**:
  ni la Tabla 3 ni la Tabla 4 exigen tonos distintos por prioridad — lo que
  distingue ALTA de MEDIA es el número de pulsos, el espaciado y el intervalo
  entre ráfagas, y todo eso está implementado (§8). La frecuencia queda fijada
  en el arranque por `ledcSetup(BUZZER_PWM_CHANNEL, BUZZER_PWM_FREQUENCY, ...)`
  y el tercer argumento de `buzzerTone()` sigue siendo código muerto, pero eso
  ya no es una carencia normativa. *(Una versión anterior de este documento
  afirmaba además que "tampoco hay armónicos": era falso. La onda cuadrada de
  400 Hz tiene armónicos impares en 400/1200/2000/2800/3600 Hz, que es
  justamente lo que satisface los cuatro picos que pide la Tabla 4.)*
- **Lo que sí queda pendiente del audio es acústico, no de firmware**: el
  tiempo de subida real, la relación de 15 dB entre las cuatro componentes
  mayores y los niveles de presión sonora. Los tres exigen micrófono.
  **Desde que el display emite su propia señal (§8.1), esta medida abarca DOS
  transductores y no uno**, y añade una pregunta que antes no existía: el
  **nivel relativo** entre ellos. El zumbador del display es on/off puro, sin
  control de amplitud, así que si la medida confirma lo que hoy solo se observa
  a oído —que se oye más que el de la placa— no hay palanca de firmware para
  corregirlo; el margen quedaría en el patrón (§8.1 acota el solapamiento al
  3,4 % del tiempo) o en un cambio de hardware. Nada de esto bloquea la señal,
  que hoy cubre un caso que antes quedaba mudo del todo, pero es lo que hay que
  medir antes de afirmar conformidad de la Tabla 4 en el display.
- ~~Detección de cortocircuito en la sonda de piel~~ — **implementada**. `skinProbeFault()` clasifica por RESISTENCIA: por debajo de 800 Ω es cortocircuito, por encima de 6000 Ω circuito abierto, y se distinguen en el log porque la acción de servicio es distinta. Cierra además un agujero real: los límites anteriores estaban en milivoltios y venían de cuando la excitación eran 3,3 V, así que el superior quedaba por encima del raíl y no filtraba nada. Un cortocircuito PARCIAL se leía como piel plausiblemente caliente y, en modo piel, hacía que el control dejase de calentar sin ninguna alarma. La **urgencia**
  sigue dependiendo del modo, que es lo correcto: en modo piel la avería es
  ALTA y corta calefactor, en modo aire es BAJA. Lo que cambia con el corto no
  es la gravedad, es qué hay que revisar.
- **La detección de obstrucción de salida de aire está activa, con un umbral
  razonado pero pendiente de ajuste fino en banco.**
  `AIR_BLOCKED_DETECTION_ENABLED` es `true` (`board.h:234`) y ya corta el
  calefactor a través de `alarm_cuts_heater()`.
  `FAN_DUTY_BLOCKED_THRESHOLD` vale **190** (`board.h:221`), derivado del duty
  de fábrica de 137 que sostiene `FAN_TARGET_RPM` con la salida limpia: queda
  +39 % por encima de esa línea base, holgadamente sobre el duty al que el PID
  llega compensando la caída de tensión con el calefactor a máxima potencia
  (~158), y aún 65 cuentas por debajo de la saturación a la que lleva una
  obstrucción real. El margen está sesgado a propósito contra falsos
  positivos, porque un falso positivo corta el calefactor y enfría al bebé sin
  red de respaldo, mientras que una obstrucción parcial que se escape sigue
  apareciendo como desviación de temperatura o corte térmico.

  Sigue siendo un punto de partida calculado, **no validado en banco**: el
  propio `board.h` lo dice y el arranque registra el duty necesario para
  sostener `FAN_TARGET_RPM` precisamente para recoger esos datos. El ajuste
  contra la dispersión real entre unidades es del responsable del proyecto,
  antes de que el equipo salga a campo.
- **Canal de corte térmico independiente del termostato: no existe.** El
  corte por aire (ID 1) lee el mismo `ROOM_DIGITAL_TEMP_SENSOR` que usa el PID
  de control. Un único fallo de sensor se lleva por delante tanto el control
  como su propio corte de seguridad. Esto no se resuelve en firmware: exige
  un segundo canal de temperatura físicamente independiente.
- ~~Prueba de función de alarma para el operador~~ — **implementada**
  (201.12.3.105). Botón **PROBAR** en la cabecera del centro de alarmas: reproduce una ráfaga de
  cada prioridad, de BAJA a ALTA, y pinta el banner con el color y el parpadeo
  de cada una, de modo que se comprueban a la vez la señal audible y la visual.
  Corre por el **mismo camino de audio** que las alarmas reales
  (`buzzerAlarmUpdate`), que es lo que le da valor: no verifica una imitación.
  La placa la rechaza si hay una alarma en curso, y una alarma que aparezca a
  mitad la cancela en el acto. Lógica en
  `motherBoard/src/modules/control/alarm_test.cpp`, con tests nativos.
  Vive en el centro de alarmas y no en ajustes —donde estuvo primero— porque
  es donde el operador ya está cuando piensa en alarmas y se llega en un toque
  desde el icono de la barra, a cualquier hora. Se descartó un gesto oculto
  durante el arranque: obligaría a apagar la incubadora, con el bebé dentro,
  para comprobar las alarmas, y dejaría de ser un medio "for the OPERATOR".
  Si hay cualquier condición señalizando la placa la rechaza, y el display lo
  dice en vez de callarse.
  **Pendiente de expediente**: 201.12.3.105 exige además que el medio se
  describa en las instrucciones de uso.
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
