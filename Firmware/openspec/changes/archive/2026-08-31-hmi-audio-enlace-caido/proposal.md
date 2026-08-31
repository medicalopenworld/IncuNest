## Why

Afecta a **Display_HMI** y a **shared/** (constantes del patrón de Tabla 3);
`motherBoard` solo cambia porque esas constantes se mudan de sitio, sin cambio
de comportamiento.

Hoy la pérdida del enlace motherBoard ↔ display se anuncia con las dos mitades
repartidas entre las dos placas: el display pinta el banner
`!! SIN ENLACE CON LA PLACA` (`UITask.cpp:2331`) y la motherBoard declara
`ALARM_HMI_LINK_LOST` y lo hace sonar en **su** zumbador
(`security.cpp:checkHmiLink()`). El display no emite sonido por ninguna alarma:
`AlarmSound_Update()` (`UITask.cpp:2798`) está vaciada a propósito y su zumbador
I2C solo da el chasquido de 12 ms al pulsar.

Ese reparto deja un modo de fallo sin cubrir: **cuando la que calla es la
placa**. Si su firmware se cuelga, entra en reset loop o el watchdog lo reinicia,
`securityCheck()` deja de correr y con él `driveAlarmBuzzer()`, así que no suena
nada — y el display, que sí lo detecta, se limita a un banner mudo mientras
mantiene en pantalla unas cifras congeladas que el operador lee como actuales.
El propio comentario que justifica el detector del display
(`CommTask.cpp:34-38`) razona que *"cuando el enlace cae, la alarma que declara
la motherBoard no puede llegar — es justamente lo que se ha perdido"*; ese
argumento vale igual para el audio, pero ahí la mitad audible se sigue delegando
en el extremo que puede estar muerto.

Es la única condición de alarma del sistema cuyo detector vive en el display, y
por tanto la única cuyo anuncio audible no puede depender de la otra placa.

## What Changes

- El display gana **señal acústica propia** para una sola condición: enlace con
  la placa perdido. Patrón de **prioridad MEDIA** de la Tabla 3 de
  IEC 60601-1-8 (3 pulsos × 150 ms, espaciado `y` = 200 ms, ráfaga cada 25 s),
  el mismo que anuncia `ALARM_HMI_LINK_LOST` en la motherBoard, para que las dos
  mitades cuenten lo mismo.
- El display gana un **AUDIO PAUSED local de 10 minutos** para esa señal, con su
  propio indicador. Es necesario porque con el enlace caído el botón de
  SILENCIAR no llega a la placa: sin esto, la señal sería incallable.
- Las constantes de temporización del patrón (`ALARM_PULSE_MS`,
  `ALARM_BURST_PULSES_*`, `ALARM_BURST_PERIOD_MS_*`, espaciados) se mueven de
  `motherBoard/include/main.h` a `shared/include/alarm_audio_pattern.h`. Hoy son
  privadas de una placa y la otra las necesita; duplicarlas garantizaba que
  divergieran en el primer ajuste.
- Se decide y se deja escrito el **doble anuncio**: con el cable roto y la placa
  viva sonarán los dos zumbadores. Se acepta explícitamente (ver design.md), no
  se intenta evitar — el display no puede saber si la placa sigue viva, que es
  todo el problema.
- `docs/alarms.md` §8 y §9 se actualizan: §8 gana el reparto de qué transductor
  anuncia qué, y §9 pierde la carencia al quedar cerrada.

**No** cambia: el resto de alarmas siguen sonando solo en la motherBoard.
`AlarmSound_Update()` sigue sin emitir sonido por alarmas recibidas por el
enlace — esas llegan de la placa, que ya las está sonando.

## Capabilities

### New Capabilities
- `hmi-link-loss-audio`: la señal acústica que el display emite por su cuenta
  cuando pierde el enlace con la motherBoard — patrón, prioridad, arranque y
  cese, pausa de audio local, y su relación con la señal visual ya existente.

### Modified Capabilities
<!-- Ninguna: openspec/specs/ está vacío, no hay capabilities previas cuyos
     requisitos cambien. -->

## Impact

- `Display_HMI/src/tasks/UITask.cpp`: nuevo servicio no bloqueante del patrón en
  el bucle de UI, junto a `click_beep_service()` (`:4195`), que ya resuelve el
  mismo problema de encender/apagar sin `delay()`. Arbitraje con el chasquido de
  pulsación, que comparte transductor.
- `Display_HMI/src/drivers/buzzer.cpp`: sin cambios de API — `buzzerOn()` /
  `buzzerOff()` bastan; el hardware (STC8H1K28 por I2C, 0x30) no ofrece control
  de frecuencia ni de amplitud.
- `shared/include/alarm_audio_pattern.h`: nuevo.
- `motherBoard/include/main.h` y `src/drivers/Buzzer.cpp`: pasan a incluir el
  header compartido. Los `static_assert` de las ventanas de Tablas 3 y 4 siguen
  compilando contra las mismas constantes, ahora en su nueva ubicación.
- `docs/alarms.md` (§8, §9) y `docs/alarms_bench_verification.md` (caso 17).
- **Riesgo de enmascaramiento acústico**, tratado en design.md: `UITask.cpp:2095`
  ya documenta que en este equipo *"el zumbador del display se oye MAS que el de
  la placa"*, con la medida acústica pendiente (`alarms.md` §8). Una señal MEDIA
  del display podría tapar un ALTA real de la placa si la placa sigue viva.
- **Interacción con `feat/hmi-error-enlace-arranque`** (rama abierta, aún no en
  `dev`): esa rama hace que `Display_IsBoardLinkLost()` sea verdadero también
  cuando la placa no ha hablado nunca, pasado el margen de arranque. Este cambio
  consume esa función sin redefinirla, así que no hay conflicto de código, pero
  al integrarse ambas el display sonará al arrancar sin la placa. Es deseable
  (arrancar sin cable deja de ser silencioso), pero cambia el comportamiento en
  banco y hay que decirlo.
- `docs/known_issues.md`: ninguno de los seis se reintroduce. El más cercano es
  el #1 (alarmas fantasma), pero aquí el display no reconstruye ni retiene una
  alarma que le llegó por el enlace: anuncia una condición que detecta **él**,
  con su propio temporizador, y que se apaga sola en cuanto vuelve una línea.
