# phototherapy-lamp-monitor Specification

## Purpose
TBD - created by archiving change mb-photo-current-check. Update Purpose after archive.
## Requirements
### Requirement: CTRL,STATE informa del estado real de la lámpara, no del eco de la orden

El campo de fototerapia de `CTRL,STATE` (campo 8, `photo`) SHALL reflejar el estado que la placa tiene **aplicado** a la lámpara (`in3.phototherapy`), y SHALL NOT repetir la última orden recibida del display. Un encendido forzado desde el modo depuración SHALL verse, por tanto, como fototerapia ON en la pantalla mientras dure, y SHALL volver a OFF cuando la máquina de `photo_override` suelte la lámpara. Un encendido de depuración SHALL seguir sin guardarse en NVS y SHALL seguir dejando la lámpara apagada al soltarlo, aunque el display haya adoptado el ON emitido.

#### Scenario: La pantalla muestra la lámpara encendida por depuración

- **GIVEN** el modo depuración activo y la fototerapia apagada desde el display
- **WHEN** se manda `PHOTO,1` por la consola y la placa enciende la lámpara
- **THEN** el campo 8 del siguiente `CTRL,STATE` vale 1
- **AND** la pantalla muestra la fototerapia en ON

Verificación manual en banco: `CommTask.cpp` no entra en `[env:native]`.

#### Scenario: Al soltar el encendido de depuración la lámpara queda apagada

- **GIVEN** un encendido de depuración activo cuyo ON el display ya ha adoptado y devuelve en su keepalive
- **WHEN** se suelta el encendido de depuración
- **THEN** durante la ventana de gracia `photo_override_effective()` devuelve el valor real previo (apagado), ignorando el ON rancio del display
- **AND** `photo_override_may_persist()` es falso durante la gracia, de modo que nada se guarda en NVS

Cubierto por `[env:native]` (`test_photo_override`). El campo 8 pasa a 0 y el display converge: verificación manual.

#### Scenario: Orden del display aún no aplicada

- **GIVEN** la lámpara apagada
- **WHEN** el display pide fototerapia ON y un `CTRL,STATE` sale antes de que la placa aplique la orden
- **THEN** ese `CTRL,STATE` lleva el campo 8 a 0 (estado real)
- **AND** la pantalla mantiene el ON local hasta que la placa lo confirma, gracias a la guarda de confirmación del display

Verificación manual en banco.

### Requirement: Una lámpara que no consume corriente se anuncia como fallo de fototerapia

El equipo SHALL declarar la condición `ALARM_PHOTO_LAMP_FAULT` cuando, con la fototerapia ordenada ON, transcurrido `PHOTO_SETTLE_MS` desde el encendido y con el sensor de corriente principal presente, `in3.phototherapy_current` sea inferior a `PHOTO_FAULT_MIN_A` (0,05 A) durante `PHOTO_FAULT_CYCLES` (5) ciclos consecutivos del lazo de intensidad (1 s). La condición SHALL ser de prioridad BAJA, SHALL NOT ser latching y SHALL NOT cortar el calefactor. SHALL declararse con histéresis, retirándose solo cuando la corriente alcance `PHOTO_FAULT_CLEAR_A` (0,08 A), y SHALL no evaluarse mientras `g_factoryTestActive` esté activo.

#### Scenario: Lámpara sin corriente

- **GIVEN** la fototerapia ON, pasado `PHOTO_SETTLE_MS`, y el sensor presente
- **WHEN** la corriente medida es 0,00 A durante 5 ciclos consecutivos
- **THEN** `photo_fault_update()` devuelve «condición presente» en el quinto ciclo y no antes

Cubierto por `[env:native]` (`test_photo_fault`).

#### Scenario: Un ciclo bueno reinicia la cuenta

- **GIVEN** 4 ciclos consecutivos por debajo de 0,05 A
- **WHEN** llega un ciclo con 0,45 A y después otros 4 por debajo del umbral
- **THEN** la condición no se declara

Cubierto por `[env:native]`.

#### Scenario: Histéresis al retirar

- **GIVEN** la condición declarada
- **WHEN** la corriente sube a 0,06 A, entre `PHOTO_FAULT_MIN_A` y `PHOTO_FAULT_CLEAR_A`
- **THEN** la condición se mantiene
- **AND** al llegar a 0,08 A o más se retira

Cubierto por `[env:native]`.

#### Scenario: La condición se retira al apagar la fototerapia o dejar de ser evaluable

- **GIVEN** la condición declarada
- **WHEN** la fototerapia se apaga, o el sensor deja de contestar, o arranca el test de fábrica
- **THEN** la condición se retira y la cuenta vuelve a cero

Cubierto por `[env:native]`.

#### Scenario: No se evalúa durante el asentamiento

- **GIVEN** la fototerapia recién encendida, antes de `PHOTO_SETTLE_MS`
- **WHEN** la corriente medida es 0,00 A
- **THEN** no hay cuenta ni condición (el llamador no marca la evaluación como válida)

La compuerta de asentamiento vive en `sensors_module.cpp` (no entra en host): verificación manual en banco. El módulo puro se prueba con `evaluable=false`.

#### Scenario: La alarma llega a la pantalla con su prioridad y su texto

- **WHEN** la condición se declara
- **THEN** `alarm_priority(ALARM_PHOTO_LAMP_FAULT)` es BAJA, `alarm_cuts_heater()` y `alarm_is_latching()` son falsos
- **AND** título y acción existen en inglés, español, francés y portugués dentro de `ALARM_TITLE_MAX_CHARS` y `ALARM_DESC_MAX_CHARS`

Prioridad, no-corte y textos cubiertos por `[env:native]` (`test_alarm_policy`, `test_alarm_text`). El pop-up en la pantalla y la compilación del HMI contra el id nuevo son verificación manual.

### Requirement: La telemetría de fototerapia publica el estado real

La clave de ThingsBoard de fototerapia activa (`Phototherapy_active`) SHALL publicar `in3.phototherapy`, no la última orden del display. La alarma nueva SHALL publicarse con su propia clave siguiendo el patrón de las demás condiciones.

#### Scenario: Clave de fototerapia

- **WHEN** la placa publica telemetría por WiFi o GPRS con la lámpara encendida por depuración
- **THEN** `Phototherapy_active` vale 1

Verificación manual: `GPRS.cpp` y `Wifi_OTA.cpp` no entran en host. Por inspección, ambas rutas ya asignan `in3.phototherapy`.

