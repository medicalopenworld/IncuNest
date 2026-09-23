## Context

El anuncio de la pérdida de enlace está hoy repartido entre las dos placas: el
display pone la mitad visual (banner, `UITask.cpp:2331`) y la motherBoard la
mitad audible (`ALARM_HMI_LINK_LOST`, prioridad MEDIA, su zumbador). El reparto
falla exactamente en el caso en que la que calla es la placa, que es también el
único caso que el detector del display puede ver y la placa no.

Restricciones de partida, todas verificadas en el árbol:

- **El transductor del display no tiene control de frecuencia ni de amplitud.**
  Es un STC8H1K28 por I2C en 0x30 y la API es `buzzerOn()` / `buzzerOff()`
  (`src/drivers/buzzer.cpp`). Todo lo que se puede modelar es el **ritmo**.
- **No se puede bloquear.** `hmi_audio_module_beep()` existe pero hace `delay()`;
  llamarlo desde el bucle de UI congela el despacho de eventos de LVGL durante
  el pitido. El patrón se sirve desde el bucle, como ya hace
  `click_beep_service()` (`UITask.cpp:2105`, `:4195`).
- **Display_HMI no tiene entorno de test.** El único entorno real es
  `[env:native]` de motherBoard, hoy limitado a `modules/control` vía
  `build_src_filter`.
- **El zumbador del display se oye más que el de la placa** (`UITask.cpp:2095`),
  con la medida acústica todavía pendiente (`docs/alarms.md` §8).

## Goals / Non-Goals

**Goals**
- Que la pérdida de enlace tenga señal audible aunque la motherBoard esté muerta.
- Que esa señal sea la de la Tabla 3 para MEDIA, idéntica a la que la placa
  emite para la misma condición.
- Que sea silenciable localmente, porque el mute no puede viajar por un enlace
  caído.
- Que la lógica de temporización quede cubierta por tests reales, pese a que el
  display no tenga entorno.

**Non-Goals**
- **No** se toca el motor de audio de la motherBoard (`Buzzer.cpp`,
  `buzzerAlarmUpdate()`). Funciona, tiene `static_assert` que lo blindan y
  reescribirlo para compartir código con el display es un riesgo sin beneficio
  en este cambio. Solo cambia de dónde vienen sus constantes.
- **No** se da sonido en el display al resto de alarmas.
- **No** se resuelve la medida acústica pendiente (§8): sigue abierta, y ahora
  para dos transductores en vez de uno.

## Decisions

### D1: El display suena aunque la placa también esté sonando (doble anuncio)

Con el cable roto y la placa viva, los dos zumbadores emiten el patrón MEDIA,
desfasados entre sí. Se acepta.

*Por qué:* el display **no puede saber** si la placa sigue viva — averiguarlo
requeriría precisamente el enlace que se ha perdido. Cualquier intento de
arbitrar es una conjetura sobre el estado del otro extremo.

*Alternativas descartadas:*
- *Que la motherBoard delegue el audio de `ALARM_HMI_LINK_LOST` en el display.*
  Invierte el agujero: si el muerto es el display, nadie suena. Y el display es
  la placa no crítica, la que más se reinicia.
- *Que el display espere más que la placa antes de sonar, confiando en que el
  operador ya habrá oído a la placa.* No arregla nada: si la placa está muerta,
  esperar solo retrasa el único aviso que va a existir.

*Por qué no es una inconsistencia normativa:* 6.3.3.1 se ocupa de señales de
alarma **inconsistentes**. Aquí las dos anuncian la misma condición con la misma
prioridad y el mismo patrón — es redundancia, no inconsistencia. Que ambas sean
MEDIA no es coincidencia: las dos derivan de
`alarm_priority(ALARM_HMI_LINK_LOST)` en `shared/src/alarm_policy.cpp`.

### D2: Usar el patrón MEDIA acota el enmascaramiento a ~3,4 % del tiempo

Este es el riesgo serio del cambio: si la placa está viva y anunciando una
condición de prioridad **ALTA**, el zumbador más ruidoso del equipo estará
emitiendo un patrón MEDIA. El display no puede cederle el paso porque, con el
enlace caído, no sabe qué está anunciando la placa.

La mitigación sale del propio patrón correcto, y por eso no se negocia: la
ráfaga MEDIA ocupa 850 ms de cada 25 000 ms, un **3,4 % del tiempo**. La ráfaga
ALTA de la placa se repite cada 10 000 ms, así que aunque una ráfaga concreta
quede tapada, la siguiente llega 10 s después con el display en silencio. El
solapamiento sostenido es imposible por construcción.

*Alternativa descartada:* bajar el nivel del zumbador del display. No hay
palanca — el hardware es on/off. La única forma real sería un cambio de
hardware, que queda fuera.

Esto refuerza por qué D3 (constantes compartidas) importa: si alguien "afina" el
patrón del display y lo hace más denso, se carga la mitigación sin saberlo.

### D3: Las constantes del patrón se mudan a `shared/`, no se duplican

Nuevo `shared/include/alarm_audio_pattern.h` con `ALARM_PULSE_MS`,
`ALARM_PULSE_SPACING_X_MS` / `_Y_MS`, `ALARM_GROUP_GAP_MS`,
`ALARM_BURST_PULSES_*` y `ALARM_BURST_PERIOD_MS_*`, hoy en
`motherBoard/include/main.h:261-283`. `main.h` pasa a incluirlo.

*Por qué:* dos copias de una tabla normativa divergen en el primer ajuste, y la
divergencia sería silenciosa — dos zumbadores emitiendo patrones distintos para
la misma condición, que sí es la inconsistencia que 6.3.3.1 prohíbe. Los
`static_assert` de `Buzzer.cpp` siguen comprobando las ventanas de las Tablas 3
y 4 contra estas constantes; al vivir en `shared/`, ahora blindan a las dos
placas.

### D4: La temporización es una función pura en `shared/`, y por tanto testeable

`shared/src/alarm_audio_pattern.cpp`:

```
bool alarm_audio_pulse_on(uint32_t elapsed_ms, int priority);
```

`elapsed_ms` es el tiempo desde el inicio de la ráfaga en curso; devuelve si el
transductor debe estar encendido. Sin estado, sin `millis()`, sin hardware.

*Por qué:* es la forma de que este cambio tenga tests de verdad. Display_HMI no
tiene entorno, pero `shared/` sí puede entrar en el `build_src_filter` del
`[env:native]` de motherBoard, donde la función se ejercita con Unity contra los
instantes exactos de la Tabla 3. Lo que queda sin cubrir por tests es solo la
parte que de verdad no se puede cubrir: el I2C y el bucle de LVGL.

El consumidor en el display es un servicio de tres líneas que calcula
`elapsed = (millis() - burstStartMs) % ALARM_BURST_PERIOD_MS_MEDIUM` y llama a
`buzzerOn()` / `buzzerOff()` cuando el valor devuelto cambia — nunca en cada
pasada, porque cada llamada es una transacción I2C.

*Alternativa descartada:* replicar la máquina de estados de `buzzerAlarmUpdate()`
en el display. Es un motor con estado, acoplado al PWM del ESP32 y a las rampas
de amplitud, que en este hardware no existen.

### D5: El arbitraje con el chasquido se resuelve en el lado del chasquido

`click_beep_start()` no emite si hay una ráfaga de alarma en curso, y
`click_beep_service()` no apaga el zumbador en ese caso.

*Por qué en ese lado:* el chasquido es cosmético y la señal de alarma es
normativa; ante un único transductor, quien cede está claro. Al revés —que la
alarma reclamase el transductor— habría que interceptar un `buzzerOff()` que
puede llegar en cualquier momento desde el servicio del chasquido, y truncar un
pulso rompe el patrón de la Tabla 3.

### D6: La pausa de audio es estado local del display

Un `uint32_t s_linkAudioPauseUntilMs` en `UITask.cpp`, no un campo del protocolo.

*Por qué:* la señal existe precisamente porque el enlace no está. Hacer que su
pausa dependa de una trama de la placa la haría inservible en el único escenario
en que se usa. Como efecto secundario queda al margen del defecto abierto #6 de
`known_issues.md` (el indicador AUDIO PAUSED que no se refresca), que vive en la
ruta de estado que viene de la placa.

El control es el botón de SILENCIAR que ya existe (`ui_MuteAlarm` /
`MuteAlarm_cb`). Hoy solo se muestra con `alarmActive`, que llega de la placa y
por tanto está congelado cuando hay enlace perdido: hay que añadir la condición
de enlace caído a su visibilidad.

## Risks / Trade-offs

- **Enmascaramiento de una alarma ALTA de la placa** → acotado a ~3,4 % del
  tiempo por el propio patrón MEDIA (D2). Queda como punto explícito para la
  medida acústica pendiente de §8, que ahora debe cubrir los dos transductores y
  su nivel relativo.
- **Ruido percibido en banco y en fábrica**: montar o cablear el equipo con el
  display encendido y la placa apagada ahora suena. Es el comportamiento
  pretendido, pero cambia la experiencia de trabajo. Se documenta en el caso 17
  de `alarms_bench_verification.md`.
- **Interacción con `feat/hmi-error-enlace-arranque`** (abierta, fuera de `dev`):
  al integrarse, arrancar el display sin la placa pasará a sonar tras el margen
  de arranque. No hay conflicto de código —este cambio consume
  `Display_IsBoardLinkLost()` sin redefinirla— pero sí de comportamiento, y hay
  que revisarlo al mergear la segunda de las dos.
- **Coste I2C en el bucle de UI**: dos transacciones por pulso, seis por ráfaga,
  una ráfaga cada 25 s. Despreciable frente al chasquido, que ya hace dos por
  pulsación.
- **`shared/` entra en el `[env:native]` de motherBoard**: hay que ampliar
  `build_src_filter` sin arrastrar dentro código que no compile en host.
  `alarm_audio_pattern.cpp` es aritmética entera pura, sin Arduino.

## Migration Plan

Sin migración de datos ni de protocolo: no cambia ninguna trama, ningún campo ni
ningún `AlarmId`. Un display con este firmware y una motherBoard sin él
interoperan igual que hoy.

Rollback: revertir el merge. El único punto con efecto en la otra placa es el
traslado de constantes a `shared/`, que es un cambio de ubicación sin cambio de
valor — verificable porque los `static_assert` existentes siguen pasando.

## Open Questions

- **Nivel relativo de los dos zumbadores**: pendiente de medida acústica, ya
  abierta en §8. Este cambio la hace más necesaria, no la resuelve.
- **¿Debe la señal del display distinguirse de la de la placa?** Hoy son
  idénticas a propósito (D1). Si en banco resulta que el operador necesita saber
  *cuál* de las dos placas está viva, la vía sería la señal visual, no otro
  patrón acústico: inventar un ritmo fuera de la Tabla 3 no es una opción.
