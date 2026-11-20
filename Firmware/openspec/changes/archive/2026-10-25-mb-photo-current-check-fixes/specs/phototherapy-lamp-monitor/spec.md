## MODIFIED Requirements

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

## ADDED Requirements

### Requirement: El apagado del encendido de depuración se confirma, no se espera

Tras soltar el encendido de depuración de la fototerapia, la placa SHALL forzar el valor real previo a la prueba y SHALL NOT guardar nada en NVS hasta que llegue un keepalive del display cuyo valor sea ese valor real, y nunca antes de `PHOTO_OVERRIDE_CONFIRM_MIN_MS` (2 s) tras soltar. Si pasan `PHOTO_OVERRIDE_RELEASE_BACKSTOP_MS` (60 s) sin confirmación, la placa SHALL registrar un error una sola vez y SHALL seguir forzando el valor real sin guardar: SHALL NOT adoptar un ON del display sin confirmar. La contabilidad de exposición por bebé SHALL NOT contar la prueba de depuración ni una lámpara con `ALARM_PHOTO_LAMP_FAULT` declarada.

#### Scenario: El display sigue devolviendo ON pasados 5 s

- **GIVEN** un encendido de depuración soltado, con el display devolviendo el ON adoptado
- **WHEN** pasan 5, 8, 20 y 59 s
- **THEN** `photo_override_effective()` devuelve el valor real previo y `photo_override_may_persist()` es falso

Cubierto por `[env:native]` (`test_photo_override`).

#### Scenario: Confirmación a los 8 s

- **GIVEN** un encendido de depuración soltado
- **WHEN** a los 8 s el display devuelve el valor real
- **THEN** el control vuelve al display y se puede guardar

Cubierto por `[env:native]`.

#### Scenario: Plazo largo vencido sin confirmar

- **WHEN** pasan 60 s sin confirmación
- **THEN** `photo_override_unconfirmed_overdue()` devuelve verdadero una sola vez
- **AND** el valor aplicado sigue siendo el real previo y nada se guarda

Cubierto por `[env:native]`. El registro del error es verificación manual.

#### Scenario: Eco temprano

- **WHEN** a los 500 ms tras soltar llega una trama con el valor real
- **THEN** no cuenta como confirmación

Cubierto por `[env:native]`.

#### Scenario: Contabilidad de exposición

- **GIVEN** una prueba de depuración activa o por confirmar, o `ALARM_PHOTO_LAMP_FAULT` declarada
- **WHEN** el display devuelve fototerapia ON
- **THEN** la exposición por bebé no se acumula

Verificación manual: `CommTask.cpp` no entra en host.
