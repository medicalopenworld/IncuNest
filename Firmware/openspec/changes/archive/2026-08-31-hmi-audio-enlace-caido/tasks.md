## 1. shared/ — patrón de la Tabla 3 como código compartido y testeable

Commit: `refactor(shared): mover las constantes del patrón de Tabla 3 a shared/`
y `feat(shared): función pura de temporización del patrón de alarma`.

- [x] 1.1 Crear `shared/include/alarm_audio_pattern.h` con las constantes hoy en
      `motherBoard/include/main.h:261-283` (`ALARM_PULSE_MS`,
      `ALARM_PULSE_SPACING_X_MS`/`_Y_MS`, `ALARM_GROUP_GAP_MS`,
      `ALARM_BURST_PULSES_*`, `ALARM_BURST_PERIOD_MS_*`, `ALARM_BURST_LEN_MS_*`),
      con el comentario que explica que son la Tabla 3 y que no se tocan sin
      rehacer los `static_assert`.
- [x] 1.2 Sustituirlas en `motherBoard/include/main.h` por el `#include` del
      header compartido. Sin cambio de valores.
- [x] 1.3 Declarar y implementar `alarm_audio_pulse_on(uint32_t elapsed_ms,
      int priority)` en `shared/include/alarm_audio_pattern.h` +
      `shared/src/alarm_audio_pattern.cpp`. Aritmética entera pura: sin
      `millis()`, sin Arduino, sin estado.
- [x] 1.4 **RED** — Tests Unity en
      `motherBoard/test/test_alarm_audio_pattern/` que fallen: instantes exactos
      de la ráfaga MEDIA (encendido en [0,150), [350,500), [700,850); apagado en
      850..25000), el arranque inmediato de la primera ráfaga, y que a los 30 min
      el patrón sigue regenerándose. Añadir también ALTA (dos grupos de cinco con
      el hueco de `2x+y`) y BAJA, que la función cubre por firma.
- [x] 1.5 **GREEN** — Implementar y hacer pasar los tests: `pio test -e native`.
      No hizo falta tocar `build_src_filter`: `pre_native.py` ya compila todo
      `shared/src` para ese entorno.
- [x] 1.6 Verificar que `motherBoard/src/drivers/Buzzer.cpp` compila sin cambios
      de comportamiento y que sus `static_assert` de las Tablas 3 y 4 siguen
      pasando: `pio run -e IncuNest_V18` en verde (ver la nota de entorno en la
      tarea 4.2). `IncuNest_V18` no tiene `lib_deps` propio: hace
      `extends = common` y hereda la lista entera, como el resto de entornos.

## 2. Display_HMI — señal acústica de enlace perdido

Commit: `feat(hmi): señal acústica propia al perder el enlace con la placa`.

- [x] 2.1 Añadir el servicio del patrón en `UITask.cpp`, junto a
      `click_beep_service()`: estado de inicio de ráfaga, `elapsed` módulo
      `ALARM_BURST_PERIOD_MS_MEDIUM`, y `buzzerOn()`/`buzzerOff()` **solo en los
      cambios** de lo que devuelve `alarm_audio_pulse_on()` — nunca en cada
      pasada, que cada llamada es una transacción I2C.
- [x] 2.2 Enganchar el servicio al bucle de UI (`UITask.cpp:4195`, donde ya se
      llama a `click_beep_service()`), gobernado por `Display_IsBoardLinkLost()`.
- [x] 2.3 Arranque y cese: primera ráfaga inmediata al pasar la condición a
      verdadera; `buzzerOff()` incondicional al pasar a falsa, incluso en mitad
      de un pulso.
- [x] 2.4 Arbitraje con el chasquido: `click_beep_start()` no emite y
      `click_beep_service()` no apaga mientras haya ráfaga de alarma en curso.

## 3. Display_HMI — pausa de audio local

Commit: `feat(hmi): pausa de audio local para la señal de enlace perdido`.

- [x] 3.1 Añadir `s_linkAudioPauseUntilMs` en `UITask.cpp` y respetarlo en el
      servicio del patrón (`ALARM_AUDIO_PAUSE_MS`, 10 min).
- [x] 3.2 Extender `MuteAlarm_cb()` para que, con el enlace caído, arme la pausa
      local además de lo que ya hace (que con el enlace caído no llega a la
      placa, pero no estorba).
- [x] 3.3 Extender la visibilidad de `ui_MuteAlarm` (`UITask.cpp:2592`) para que
      se muestre también cuando `Display_IsBoardLinkLost()`, hoy condicionada a
      `alarmActive`, que viene congelado de la placa.
- [x] 3.4 Mostrar el indicador AUDIO PAUSED y su cuenta atrás desde el estado
      local, sin pasar por la ruta de estado de la placa (defecto abierto #6 de
      `known_issues.md`).
- [x] 3.5 Reiniciar la pausa al restablecerse el enlace, para que una pérdida
      posterior suene desde el primer instante.

## 4. Verificación

- [x] 4.1 `pio test -e native` en motherBoard: 195/195, incluidos los 7 nuevos.
- [x] 4.2 Ambas placas compilan: `pio run -e IncuNest_V18` (motherBoard) y
      `pio run -e main` (Display_HMI), las dos en verde.
      Nota de entorno, ajena a este cambio: en un worktree limpio la resolución
      de dependencias falla en `thingsboard/TBPubSubClient@2.9.4`, que ya no
      existe en el registro de PlatformIO (`UnknownPackageError`). El worktree
      principal solo compila porque lo tiene instalado de antes. Para verificar
      aquí se fijó temporalmente el equivalente de git
      (`https://github.com/thingsboard/pubsubclient.git#v2.9.4`) y se revirtió
      `platformio.ini` antes de commitear — el fichero no forma parte de este
      cambio. **Conviene arreglarlo aparte**: hoy el proyecto no se puede
      construir desde cero.
- [ ] 4.3 **Verificación manual en hardware** (no hay entorno de test en
      Display_HMI; documentar qué se probó y con qué resultado):
      desconectar el cable UART con ambas placas encendidas; cortar la
      alimentación solo de la motherBoard; reconectar durante un pulso; pulsar la
      pantalla durante la ráfaga; pausar el audio y esperar los 10 min.

## 5. Documentación

Commit: `docs: registrar la señal acústica del display para el enlace perdido`.

- [x] 5.1 `docs/alarms.md` §8: añadir el reparto de qué transductor anuncia qué,
      y el razonamiento del doble anuncio y del 3,4 % de solapamiento.
- [x] 5.2 `docs/alarms.md` §9: la carencia queda cerrada; dejar apuntado que la
      medida acústica pendiente ahora abarca dos transductores y su nivel
      relativo.
- [x] 5.3 `docs/alarms_bench_verification.md` caso 17: añadir los pasos de 4.3.
- [x] 5.4 Actualizar el comentario de `UITask.cpp:2327-2330`, que hoy afirma que
      la mitad audible la pone la placa.

## 6. Cierre

- [ ] 6.1 `openspec archive hmi-audio-enlace-caido`.
- [ ] 6.2 Retro y learnings (`meta-self-improvement`) — obligatorio en todas las
      modalidades.
- [ ] 6.3 Parar y pedir aprobación para el merge a `dev` (`guard-merge.sh`).
