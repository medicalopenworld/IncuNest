# mb-mains-interruption-detector Specification

## Purpose
TBD - created by archiving change shared-mains-detector. Update Purpose after archive.
## Requirements
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

### Requirement: Una lectura no válida no cambia el estado

Cuando la lectura del cargador no es válida (fallo de I2C de `charge_status()`), el detector SHALL NOT declarar ni retirar la condición ni reiniciar la racha de confirmación. No SHALL existir ningún otro criterio de invalidez basado en la tensión de batería.

#### Scenario: Fallo de I2C con la condición declarada

- **WHEN** la condición está declarada y llegan lecturas no válidas
- **THEN** la condición permanece presente

Cubierto por `[env:native]`.

#### Scenario: Fallo de I2C sin condición

- **WHEN** no hay condición y llegan lecturas no válidas indefinidamente
- **THEN** la condición nunca se declara

Cubierto por `[env:native]`.

### Requirement: Sin cargador, la condición nunca se declara

Si el cargador **no se detectó en el arranque** (`chargerDetected` falso), la motherBoard SHALL NOT declarar `ALARM_MAINS_INTERRUPTION`, y SHALL anotarlo una sola vez con `logE`. El log SHALL NOT salir si el cargador se detectó alguna vez. Durante el test de fábrica (`g_factoryTestActive`) la condición SHALL NOT declararse.

#### Scenario: Placa sin BQ25730

- **WHEN** la placa arranca sin que el cargador conteste por I2C
- **THEN** la alarma no se declara en ningún momento y el log lo anota una vez como error

Verificación manual: no hay entorno de test para el cableado.

### Requirement: Un único punto de entrada de la presencia de red

La presencia de red SHALL obtenerse por una única función de la motherBoard (`mains_present_now()`), de modo que una fuente nueva (el pin `MAINS_OK_PIN` de la rev. V19) se añada sin tocar el detector ni el cableado a la máquina de alarmas.

#### Scenario: Presencia de red con la fuente actual

- **WHEN** el cargador está presente y la última lectura es válida
- **THEN** `mains_present_now()` devuelve lectura válida y `present` igual al bit AC_STAT

Verificación manual en banco (no hay entorno de test para `main.cpp`).

### Requirement: El plazo mínimo de aviso es configurable y vale 0 por defecto

El detector SHALL conservar un plazo mínimo de aviso (`MAINS_ALARM_MIN_HOLD_MS`), contado **desde la declaración**, durante el cual la condición se mantiene aunque la red vuelva. Su valor por defecto SHALL ser **0**: la condición SHALL retirarse en cuanto se confirmen dos lecturas válidas de red presente. La decisión se apoya en que `docs/alarms_normative_analysis.md` §2.5 lee 201.12.3.103 («shall be provided for a minimum time of 10 min») como un requisito de autonomía de la alarma (hardware), no como la obligación de mantenerla con la red presente. Con un plazo mayor que 0, un corte nuevo confirmado antes de vencer SHALL reiniciarlo, y un plazo vencido SHALL darse por cerrado para que no reviva al desbordar `millis()`. Revertir la decisión SHALL requerir cambiar solo la constante.

#### Scenario: La red vuelve a los 30 s con el plazo por defecto

- **WHEN** la condición se declara y llegan dos lecturas válidas de red presente
- **THEN** la condición se retira en la segunda lectura

Cubierto por `[env:native]`.

#### Scenario: Corte más largo que cualquier plazo

- **WHEN** la red sigue ausente 25 min
- **THEN** la condición está presente durante todo ese tiempo

Cubierto por `[env:native]`.

#### Scenario: Una sola lectura de red no retira la condición

- **WHEN** la condición está declarada y llega una lectura de red presente y luego una de ausencia
- **THEN** la condición sigue presente

Cubierto por `[env:native]`.

#### Scenario: Mecanismo de plazo con un plazo de prueba

- **WHEN** el detector se inicializa con un plazo de 10 min, se declara y la red vuelve a los 30 s
- **THEN** la condición sigue presente hasta cumplirse el plazo desde la declaración y luego se retira
- **AND** un corte nuevo confirmado antes de vencer reinicia el plazo
- **AND** el plazo se cuenta bien al desbordar `millis()` y un plazo vencido no revive tras una vuelta de reloj

Cubierto por `[env:native]`.

### Requirement: La reconfiguración del cargador no apaga la vigilancia de la red

La reconfiguración del cargador al volver la red SHALL NOT bajar `chargerPresent` ni `_initialized`: la lectura del cargador y la vigilancia de la red SHALL continuar durante y después de ella. `chargerDetected` SHALL fijarse en el arranque cuando el chip contesta por I2C y SHALL NOT bajar nunca. Si la reconfiguración falla, SHALL registrarse con `logE`, SHALL NOT anotarse la red como presente (se reintenta en la lectura siguiente) y SHALL contar como fallo técnico del cargador. La alarma SHALL NOT retirarse y levantarse espuriamente en un retorno de red.

#### Scenario: Retorno de red con reconfiguración correcta

- **WHEN** vuelve la red y el cargador se reconfigura
- **THEN** `chargerPresent` no baja en ningún momento
- **AND** la alarma no tiene retirada ni levantada espuria en el historial

Verificación manual en banco (el driver y `main.cpp` no tienen entorno de test).

#### Scenario: Reconfiguración fallida

- **WHEN** una operación I2C de la reconfiguración falla
- **THEN** se registra con `logE`, el cargador se sigue leyendo y se reintenta en la lectura siguiente

Verificación manual en banco.

### Requirement: La ceguera del cargador se hace visible

Con el cargador detectado en el arranque, si `millis() - última lectura válida` supera `MAINS_DETECTOR_BLIND_MS` (30 s), el detector SHALL considerarse ciego (`mains_detector_blind()`), y la motherBoard SHALL hacerlo visible por `logE` y por la clave de telemetría `charger_comm_fault` (grupo DIAG). Con la alarma ya declarada y el detector ciego, la alarma SHALL mantenerse. Sin cargador detectado el detector SHALL NOT considerarse ciego. No SHALL añadirse un `AlarmId` en este cambio.

#### Scenario: El cargador deja de responder

- **WHEN** pasan más de 30 s sin lecturas válidas con el cargador detectado
- **THEN** `mains_detector_blind()` es verdadero
- **AND** una lectura válida lo vuelve a falso

Cubierto por `[env:native]`.

#### Scenario: Ceguera con la alarma declarada

- **WHEN** la alarma está declarada y el cargador deja de responder
- **THEN** la condición se mantiene y la ceguera se señala

Cubierto por `[env:native]`.

#### Scenario: Sin cargador detectado

- **WHEN** el cargador no se detectó
- **THEN** el detector nunca está ciego

Cubierto por `[env:native]`.

#### Scenario: Publicación del fallo

- **WHEN** el detector está ciego o la reconfiguración falla
- **THEN** `charger_comm_fault` se publica a `true` por GPRS

Verificación manual en banco.

