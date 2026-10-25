## ADDED Requirements

### Requirement: La interrupción de red se declara tras confirmarla con dos lecturas consecutivas

La motherBoard SHALL declarar `ALARM_MAINS_INTERRUPTION` cuando **dos lecturas válidas consecutivas** del cargador indiquen que la red no está presente (con el periodo de lectura de 5 s, ≈ 10 s). Una sola lectura de ausencia SHALL NOT declararla. La condición SHALL ser la entrada de `alarm_machine_condition()`, que aplica la política existente (prioridad ALTA, no silenciable, no corta calefactor).

#### Scenario: Corte sostenido

- **WHEN** dos lecturas válidas consecutivas dicen que la red está ausente
- **THEN** el detector declara la condición presente

Cubierto por `[env:native]` (`test_mains_detector`).

#### Scenario: Una sola lectura de ausencia

- **WHEN** una lectura válida dice «ausente» y la siguiente válida dice «presente»
- **THEN** la condición no se declara en ningún momento

Cubierto por `[env:native]`.

#### Scenario: La racha se cuenta solo con lecturas válidas

- **WHEN** hay una lectura válida de ausencia, luego una no válida, luego otra válida de ausencia
- **THEN** la condición se declara en la segunda lectura válida de ausencia

Cubierto por `[env:native]`.

### Requirement: El aviso se mantiene un mínimo de 10 minutos desde su declaración

Por IEC 60601-2-19 201.12.3.103 («shall be provided for a minimum time of 10 min»), una vez declarada la condición SHALL mantenerse presente al menos `MAINS_ALARM_MIN_HOLD_MS` (10 min) desde el instante de la declaración **aunque la red vuelva antes**. Si la red sigue ausente pasado ese plazo, la condición SHALL seguir presente mientras dure el corte. Un nuevo corte confirmado cuando el aviso aún se mantiene por uno anterior SHALL reiniciar el plazo.

#### Scenario: La red vuelve a los 30 s

- **WHEN** la condición se declara y la red vuelve (confirmada con dos lecturas) 30 s después
- **THEN** la condición sigue presente hasta que se cumplen 10 min desde la declaración
- **AND** a partir de ahí se retira

Cubierto por `[env:native]`.

#### Scenario: Corte más largo que el mínimo

- **WHEN** la red sigue ausente 25 min
- **THEN** la condición está presente durante todo ese tiempo

Cubierto por `[env:native]`.

#### Scenario: Retorno de red confirmado tras el plazo

- **WHEN** han pasado más de 10 min desde la declaración y dos lecturas válidas consecutivas dicen que la red está presente
- **THEN** la condición se retira

Cubierto por `[env:native]`.

#### Scenario: Reinterrupción durante el plazo

- **WHEN** la red vuelve a los 3 min y se confirma un segundo corte a los 5 min de la primera declaración
- **THEN** la condición no se retira en ningún momento
- **AND** el plazo de 10 min se cuenta desde el segundo corte

Cubierto por `[env:native]`.

#### Scenario: Desbordamiento del reloj

- **WHEN** `millis()` desborda entre la declaración y el vencimiento del plazo
- **THEN** el plazo sigue contándose correctamente

Cubierto por `[env:native]`.

### Requirement: Una lectura no válida no cambia el estado

Cuando la lectura del cargador no es válida (fallo de I2C, o `vbat_mv == 0`), el detector SHALL NOT declarar ni retirar la condición ni reiniciar la racha de confirmación.

#### Scenario: Fallo de I2C con la condición declarada

- **WHEN** la condición está declarada y llegan lecturas no válidas
- **THEN** la condición permanece presente (sujeta al plazo mínimo)

Cubierto por `[env:native]`.

#### Scenario: Fallo de I2C sin condición

- **WHEN** no hay condición y llegan lecturas no válidas indefinidamente
- **THEN** la condición nunca se declara

Cubierto por `[env:native]`.

### Requirement: Sin cargador, la condición nunca se declara

Si `chargerPresent` es falso, la motherBoard SHALL NOT declarar `ALARM_MAINS_INTERRUPTION`, y SHALL anotarlo una sola vez en el log del arranque. Durante el test de fábrica (`g_factoryTestActive`) la condición SHALL NOT declararse.

#### Scenario: Placa sin BQ25730

- **WHEN** la placa arranca sin cargador
- **THEN** la alarma no se declara en ningún momento y el log lo anota una vez

Verificación manual: no hay entorno de test para el cableado. Compilación y prueba en una placa sin cargador.

### Requirement: Un único punto de entrada de la presencia de red

La presencia de red SHALL obtenerse por una única función de la motherBoard (`mains_present_now()`), de modo que una fuente nueva (el pin `MAINS_OK_PIN` de la rev. V19) se añada sin tocar el detector ni el cableado a la máquina de alarmas.

#### Scenario: Presencia de red con la fuente actual

- **WHEN** el cargador está presente y la última lectura es válida
- **THEN** `mains_present_now()` devuelve lectura válida y `present` igual al bit AC_STAT

Verificación manual en banco (no hay entorno de test para `main.cpp`).
