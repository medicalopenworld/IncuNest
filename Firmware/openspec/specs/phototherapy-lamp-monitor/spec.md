# phototherapy-lamp-monitor Specification

## Purpose
TBD - created by archiving change mb-photo-current-check. Update Purpose after archive.
## Requirements
### Requirement: CTRL,STATE informa del estado real de la lámpara, no del eco de la orden

El campo de fototerapia de `CTRL,STATE` (campo 8, `photo`) SHALL reflejar el estado que la placa tiene **aplicado** a la lámpara (`in3.phototherapy`), y SHALL NOT repetir la última orden recibida del display. **Única excepción**: mientras una prueba de fototerapia de depuración (`PHOTO,1`) esté activa, el campo SHALL emitir la **última orden del display** y SHALL NOT emitir el ON forzado, para que el display (que adopta este campo y lo devuelve en su keepalive) nunca adopte el ON de depuración. El ON de la prueba SHALL ser visible en `/debug/state` (`ctl.photo_dbg`) y en el log.

#### Scenario: Fuera de una prueba el campo es el estado aplicado

- **GIVEN** ninguna prueba de depuración activa
- **WHEN** la lámpara está aplicada ON u OFF
- **THEN** el campo 8 vale 1 u 0 respectivamente

Cubierto por `[env:native]` (`test_photo_override`, `photo_override_state_field`). El camino de `CommTask.cpp` es verificación manual.

#### Scenario: Durante la prueba el campo emite la orden del display

- **GIVEN** una prueba de depuración activa con la lámpara forzada a ON y la última orden del display en OFF
- **WHEN** la placa compone `CTRL,STATE`
- **THEN** el campo 8 vale 0
- **AND** si el operador ordena ON desde la pantalla, vale 1

Cubierto por `[env:native]` (`photo_override_state_field`). La pantalla en banco es verificación manual.

#### Scenario: Orden del display aún no aplicada

- **GIVEN** la lámpara apagada
- **WHEN** el display pide fototerapia ON y un `CTRL,STATE` sale antes de que la placa aplique la orden
- **THEN** ese `CTRL,STATE` lleva el campo 8 a 0 (estado real)
- **AND** la pantalla mantiene el ON local hasta que la placa lo confirma, gracias a la guarda de confirmación del display

Verificación manual en banco.

### Requirement: Una lámpara que no consume corriente se anuncia como fallo de fototerapia

El equipo SHALL declarar la condición `ALARM_PHOTO_LAMP_FAULT` en dos casos con la fototerapia ordenada ON, pasado `PHOTO_SETTLE_MS` y sin test de fábrica (`g_factoryTestActive`): (a) `in3.phototherapy_current` inferior a `PHOTO_FAULT_MIN_A` (0,05 A) durante `PHOTO_FAULT_CYCLES` (5) ciclos consecutivos del lazo de 1 s con muestra nueva; (b) **sin medida utilizable** —el sensor de corriente principal no contesta, está ausente desde el arranque, o no hay muestra nueva— durante `PHOTO_FAULT_BLIND_CYCLES` (5) ciclos consecutivos. La condición SHALL ser de prioridad **MEDIA** (provisional hasta el expediente de riesgos; Tabla 1 de IEC 60601-1-8: no dar una fototerapia indicada puede causar una lesión irreversible de aparición diferida), SHALL NOT ser latching y SHALL NOT cortar el calefactor. SHALL retirarse solo al alcanzar `PHOTO_FAULT_CLEAR_A` (0,08 A) con muestra nueva, o cuando la fototerapia se apague o arranque el test de fábrica. Una lectura fallida aislada SHALL NOT retirarla ni reiniciar la cuenta de corriente baja. El lazo de intensidad SHALL NOT regular con una muestra que no sea nueva.

#### Scenario: Lámpara sin corriente

- **GIVEN** la fototerapia ON, pasado `PHOTO_SETTLE_MS`, y el sensor contestando
- **WHEN** la corriente medida es 0,00 A durante 5 ciclos consecutivos con muestra nueva
- **THEN** `photo_fault_update()` devuelve «presente» en el quinto ciclo y no antes

Cubierto por `[env:native]` (`test_photo_fault`).

#### Scenario: Un ciclo bueno reinicia la cuenta

- **GIVEN** 4 ciclos consecutivos por debajo de 0,05 A
- **WHEN** llega un ciclo con 0,45 A y después otros 4 por debajo del umbral
- **THEN** la condición no se declara

Cubierto por `[env:native]`.

#### Scenario: Histéresis al retirar

- **GIVEN** la condición declarada
- **WHEN** la corriente sube a 0,06 A
- **THEN** la condición se mantiene
- **AND** al llegar a 0,08 A o más se retira

Cubierto por `[env:native]`.

#### Scenario: Sensor mudo con la fototerapia ON

- **GIVEN** la fototerapia ON y asentada
- **WHEN** no hay medida utilizable durante 5 ciclos consecutivos, incluido un INA ausente desde el arranque
- **THEN** se declara la condición
- **AND** una muestra nueva rompe la racha de ceguera

Cubierto por `[env:native]`.

#### Scenario: Una lectura fallida no retira la condición

- **GIVEN** la condición declarada
- **WHEN** llegan ciclos sin medida utilizable
- **THEN** la condición se mantiene y la cuenta de corriente baja no se reinicia

Cubierto por `[env:native]`.

#### Scenario: Se retira al apagar o no aplicar

- **GIVEN** la condición declarada, por corriente o por ceguera
- **WHEN** la fototerapia se apaga, arranca el test de fábrica o se reinicia el asentamiento
- **THEN** la condición se retira y las dos cuentas vuelven a cero

Cubierto por `[env:native]`. La clasificación del ciclo vive en `sensors_module.cpp`: verificación manual en banco.

#### Scenario: El lazo de intensidad no regula con un dato congelado

- **GIVEN** la fototerapia ON y el INA principal sin contestar, con la última corriente por debajo del objetivo
- **WHEN** pasan los ciclos del lazo
- **THEN** el PWM de la lámpara no cambia

Verificación manual en banco (`sensors_module.cpp` no entra en host).

#### Scenario: Prioridad y textos

- **WHEN** la condición se declara
- **THEN** `alarm_priority(ALARM_PHOTO_LAMP_FAULT)` es MEDIA y `alarm_cuts_heater()` y `alarm_is_latching()` son falsos
- **AND** título y acción existen en EN/ES/FR/PT dentro de los límites del protocolo

Cubierto por `[env:native]` (`test_alarm_policy`, `test_alarm_text`).

### Requirement: La telemetría de fototerapia publica el estado real

La clave de ThingsBoard de fototerapia activa (`Phototherapy_active`) SHALL publicar `in3.phototherapy`, no la última orden del display. La alarma nueva SHALL publicarse con su propia clave siguiendo el patrón de las demás condiciones.

#### Scenario: Clave de fototerapia

- **WHEN** la placa publica telemetría por WiFi o GPRS con la lámpara encendida por depuración
- **THEN** `Phototherapy_active` vale 1

Verificación manual: `GPRS.cpp` y `Wifi_OTA.cpp` no entran en host. Por inspección, ambas rutas ya asignan `in3.phototherapy`.

### Requirement: Soltar la prueba de depuración devuelve el control al display en el acto

Mientras la prueba de fototerapia de depuración esté activa, la placa SHALL forzar la lámpara a ON, SHALL registrar las órdenes del display (y la caducidad del temporizador de fototerapia como orden de apagado) sin aplicarlas, y SHALL NOT guardar nada en NVS. Al soltarla (`PHOTO,0` o apagar el modo depuración), la placa SHALL aplicar en el siguiente tick periódico, por el camino normal de una orden, la **última orden del display** registrada (la del operador manda; si no hubo ninguna, la que había al empezar), SHALL NOT esperar ninguna trama del display, y SHALL NOT persistir ese valor. No SHALL haber ventana de gracia, confirmación, mínimo ni plazo de respaldo. La escritura del PWM de la lámpara SHALL serializarse entre el lazo de intensidad y el camino de aplicación para que un apagado no pueda ser pisado por una escritura tardía del lazo. La contabilidad de exposición por bebé SHALL NOT contar la prueba ni una lámpara con `ALARM_PHOTO_LAMP_FAULT` declarada.

#### Scenario: Real OFF, prueba y soltar sin tramas

- **GIVEN** la fototerapia real apagada y una prueba activa
- **WHEN** se suelta y no llega ninguna trama del display
- **THEN** `photo_override_take_release()` entrega OFF una sola vez
- **AND** la prueba deja de forzar y se puede guardar

Cubierto por `[env:native]` (`test_photo_override`).

#### Scenario: Real ON, vence el temporizador durante la prueba

- **GIVEN** la fototerapia real encendida y una prueba activa
- **WHEN** vence el temporizador de fototerapia y después se suelta
- **THEN** al soltar se entrega OFF y no se persiste ningún ON

Cubierto por `[env:native]`.

#### Scenario: El operador manda OFF durante la prueba

- **GIVEN** la fototerapia real encendida y una prueba activa
- **WHEN** el display ordena OFF y después se suelta
- **THEN** al soltar se entrega OFF

Cubierto por `[env:native]`.

#### Scenario: Soltar con el display colgado

- **GIVEN** una prueba activa y la tarea Comm del display atascada
- **WHEN** se suelta
- **THEN** la lámpara se apaga en el siguiente tick sin ninguna trama

La lógica pura está cubierta por `[env:native]`; el tick y el apagado físico son verificación manual en banco.

#### Scenario: Escritura del PWM serializada

- **GIVEN** el lazo de intensidad entre la comprobación de `in3.phototherapy` y la escritura del PWM
- **WHEN** la tarea Comm apaga la lámpara en ese intervalo
- **THEN** la escritura tardía del lazo no reenciende la lámpara

Verificación manual en banco (`photoPwmRefresh()` bajo spinlock; no entra en host).

