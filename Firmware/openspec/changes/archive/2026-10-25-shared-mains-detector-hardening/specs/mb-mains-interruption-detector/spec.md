## MODIFIED Requirements

### Requirement: El plazo mínimo de aviso es configurable y vale 0 por defecto

El detector SHALL conservar un plazo mínimo de aviso (`MAINS_ALARM_MIN_HOLD_MS`), contado **desde la declaración**, durante el cual la condición se mantiene aunque la red vuelva. Su valor por defecto SHALL ser **0**: la condición SHALL retirarse en cuanto se confirme el retorno de la red (seis lecturas válidas seguidas con red). La decisión se apoya en que `docs/alarms_normative_analysis.md` §2.5 lee 201.12.3.103 («shall be provided for a minimum time of 10 min») como un requisito de autonomía de la alarma (hardware), no como la obligación de mantenerla con la red presente. Con un plazo mayor que 0, un corte nuevo confirmado antes de vencer SHALL reiniciarlo, y un plazo vencido SHALL darse por cerrado para que no reviva al desbordar `millis()`. Revertir la decisión SHALL requerir cambiar solo la constante, y la instancia de producción SHALL usarla (`MAINS_DETECTOR_INITIALIZER`).

#### Scenario: La red vuelve con el plazo por defecto

- **WHEN** la condición se declara y llegan seis lecturas válidas seguidas de red presente
- **THEN** la condición se retira en la sexta lectura

Cubierto por `[env:native]`.

#### Scenario: Cinco lecturas con red no bastan

- **WHEN** la condición está declarada y llegan cinco lecturas de red presente
- **THEN** la condición sigue presente

Cubierto por `[env:native]`.

#### Scenario: Corte más largo que cualquier plazo

- **WHEN** la red sigue ausente 25 min
- **THEN** la condición está presente durante todo ese tiempo

Cubierto por `[env:native]`.

#### Scenario: Mecanismo de plazo con un plazo de prueba

- **WHEN** el detector se inicializa con un plazo de 10 min, se declara y la red vuelve a los 30 s
- **THEN** la condición sigue presente hasta cumplirse el plazo desde la declaración y luego se retira
- **AND** un corte nuevo confirmado antes de vencer reinicia el plazo
- **AND** el plazo se cuenta bien al desbordar `millis()` y un plazo vencido no revive tras una vuelta de reloj

Cubierto por `[env:native]`.

#### Scenario: La instancia de producción usa la constante

- **WHEN** se construye el detector con `MAINS_DETECTOR_INITIALIZER`
- **THEN** su plazo es `MAINS_ALARM_MIN_HOLD_MS` y todos sus campos coinciden con los de `mains_detector_init()`

Cubierto por `[env:native]`.

### Requirement: Sin cargador, la condición nunca se declara

Si la placa **no lleva cargador** (`CHARGER_EXPECTED` = 0, HW anterior a la 17) y el chip nunca contestó, la motherBoard SHALL NOT declarar `ALARM_MAINS_INTERRUPTION`, y SHALL anotarlo una sola vez con `logE`. En una placa que lo lleva (`CHARGER_EXPECTED` = 1) un chip que no contesta al arrancar SHALL tratarse como ceguera, no como «sin cargador». Durante el test de fábrica (`g_factoryTestActive`) la condición SHALL NOT declararse.

#### Scenario: Placa sin BQ25730

- **WHEN** la placa es anterior a la HW 17 y el cargador nunca contesta
- **THEN** la alarma no se declara en ningún momento y el log lo anota una vez como error

Verificación manual: no hay entorno de test para el cableado.

### Requirement: La ceguera del cargador se hace visible

Con el cargador esperado (por hardware, o porque contestó alguna vez), si `millis() - última lectura válida` supera `MAINS_DETECTOR_BLIND_MS` (30 s), el detector SHALL considerarse ciego (`mains_detector_blind()`), y la motherBoard SHALL hacerlo visible por `logE` y por la clave de telemetría `charger_comm_fault` (grupo DIAG). Con la alarma ya declarada y el detector ciego, la alarma SHALL mantenerse. Sin cargador esperado el detector SHALL NOT considerarse ciego. No SHALL añadirse un `AlarmId` en este cambio.

#### Scenario: El cargador deja de responder

- **WHEN** pasan más de 30 s sin lecturas válidas con el cargador esperado
- **THEN** `mains_detector_blind()` es verdadero
- **AND** una lectura válida lo vuelve a falso

Cubierto por `[env:native]`.

#### Scenario: Chip mudo en una placa que lo lleva

- **WHEN** la placa es HW 17 o posterior y el chip no contesta desde el arranque
- **THEN** a los 30 s hay `logE` y `charger_comm_fault`, y no se trata como «sin cargador»

Verificación manual en banco.

#### Scenario: Sin cargador esperado

- **WHEN** el cargador no se espera
- **THEN** el detector nunca está ciego

Cubierto por `[env:native]`.

## ADDED Requirements

### Requirement: La retirada de la alarma exige seis lecturas con red seguidas

El detector SHALL declarar la condición con dos lecturas válidas consecutivas de ausencia y SHALL retirarla solo tras seis lecturas válidas consecutivas con red (≈ 30 s); cualquier ausencia SHALL reiniciar esa racha. Una fuente intermitente que no mantenga seis lecturas seguidas con red SHALL mantener la alarma continua en vez de oscilar.

#### Scenario: Fuente intermitente

- **WHEN** la alarma está declarada y la red alterna cinco lecturas presentes y dos ausentes durante 20 ciclos
- **THEN** la condición está presente en todo momento

Cubierto por `[env:native]`.

#### Scenario: Una ausencia reinicia la racha

- **WHEN** hay cinco lecturas con red, una ausente y otras cinco con red
- **THEN** la condición sigue presente hasta la sexta lectura seguida con red

Cubierto por `[env:native]`.

### Requirement: Una configuración fallida del cargador se reintenta con espera progresiva

Con el cargador esperado y `chargerPresent` falso, `sensors_Task` SHALL reintentar `init_BQ25730()` con una espera progresiva de 5 s hasta 5 min, reiniciada por un éxito, y SHALL registrar el fallo con `logE` una vez por episodio y después como máximo una vez cada 5 min. La misma limitación SHALL aplicarse al log de la reconfiguración fallida al volver la red.

#### Scenario: Espera progresiva

- **WHEN** los intentos fallan sucesivamente
- **THEN** las esperas son 5, 10, 20, 40, 80, 160 y 300 s, y 300 s a partir de ahí
- **AND** un éxito reinicia la espera

Cubierto por `[env:native]` (`test_retry_backoff`).

#### Scenario: Log limitado

- **WHEN** el fallo persiste
- **THEN** se escribe un log al empezar el episodio y después uno cada 5 min

Cubierto por `[env:native]`.

#### Scenario: Chip configurado tras el reintento

- **WHEN** el arranque no pudo configurar el cargador y un reintento lo consigue
- **THEN** `chargerPresent` pasa a verdadero y la batería carga

Verificación manual en banco.
