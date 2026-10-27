# hw19-board-support Specification

## Purpose
TBD - created by archiving change mb-hw19. Update Purpose after archive.
## Requirements
### Requirement: La V19 compila como entorno propio y la V18 no cambia

El proyecto SHALL ofrecer un entorno `IncuNest_V19` con `HW_NUM=19`, PSRAM quad habilitada y la misma tabla de particiones de 8 MB que la V18. Ningún camino específico de la V19 SHALL compilarse en `IncuNest_V18` ni en `IncuNest_V17`, y los helpers de raíles SHALL ser no-op en HW<19, de modo que la V18 conserva su comportamiento.

#### Scenario: Compilación de las dos revisiones

- **WHEN** se ejecuta `pio run -e IncuNest_V19` y `pio run -e IncuNest_V18` en `Firmware/motherBoard`
- **THEN** ambos terminan sin error

Verificación de compilación; no hay entorno de test que ejecute el firmware completo.

#### Scenario: La V18 no ejecuta nada de la V19

- **WHEN** `HW_NUM` es 18
- **THEN** `pwr5vSet()` y `gsmPowerSet()` no tocan ningún pin y `mainsPresentPin()` devuelve `true`
- **AND** `GPRSForceReset()` no inicia ningún corte de alimentación

Verificación manual en banco V18 (unidad de banco) de que arranca y publica igual que antes.

### Requirement: El arranque de la V19 levanta los raíles en orden

En la V19 el firmware SHALL poner `PWR_5V_EN` a nivel alto lo primero tras el latch de `PWR_EN`, antes de abrir el enlace con el display, de iniciar el host USB de la SensorBoard y de `initSPO2()`. SHALL poner `GSM_PWR_EN` a nivel alto antes de que arranque la tarea GPRS. Mientras el firmware no los levante, ambos raíles SHALL estar apagados (pull-downs del PCB).

#### Scenario: Orden de encendido

- **WHEN** la V19 arranca
- **THEN** `PWR_5V_EN` pasa a HIGH antes que cualquier otra inicialización
- **AND** `GSM_PWR_EN` pasa a HIGH antes de crear la tarea GPRS

Verificación manual en banco V19 con osciloscopio o por el log de arranque.

### Requirement: El arranque informa de la PSRAM

El log de arranque SHALL imprimir `psramFound()` y `ESP.getPsramSize()`.

#### Scenario: Módulo N8R2

- **WHEN** la V19 arranca con un módulo ESP32-S3-WROOM-1-N8R2
- **THEN** el log muestra `psramFound()` verdadero y un tamaño de PSRAM de unos 2 MB

Verificación manual en banco V19.

### Requirement: Un reinicio del módem en la V19 corta la alimentación

En la V19, `GPRSForceReset()` SHALL, tras enviar `AT+CPOWD=1`, cortar `GSM_PWR_EN` y volver a levantarlo no antes de 2 s, sin bloquear la tarea GPRS durante la espera. El camino de `GPRSMonitorTask` que borra y recrea la tarea colgada SHALL pasar también por el corte. Cada power-cycle SHALL contarse y publicarse como `GSM_power_cycles` en el grupo DIAG de telemetría.

#### Scenario: Temporización del corte

- **WHEN** se inicia un power-cycle con un margen de apagado ordenado de 1500 ms
- **THEN** el corte se ordena una sola vez cuando han pasado los 1500 ms
- **AND** la restauración se ordena una sola vez cuando han pasado 2000 ms desde el corte, y no antes

Cubierto por `[env:native]` (`test_modem_power_cycle`).

#### Scenario: Corte inmediato del monitor

- **WHEN** el monitor inicia un power-cycle con margen 0
- **THEN** el corte se ordena en la primera consulta
- **AND** la restauración sigue sin ordenarse antes de 2000 ms

Cubierto por `[env:native]`.

#### Scenario: Contador y reentrada

- **WHEN** se pide un power-cycle mientras otro está en curso
- **THEN** no se reinicia el temporizador ni se cuenta de nuevo

Cubierto por `[env:native]`.

#### Scenario: Corte físico real del módem

- **WHEN** en la V19 el módem pierde la sesión y `GPRSForceReset()` se dispara
- **THEN** `GSM_PWR_EN` baja al menos 2 s y vuelve a subir, el módem responde a AT de nuevo y `GSM_power_cycles` aumenta en uno

Verificación manual en banco V19.

### Requirement: El diagnóstico del AFE4490 es visible

El `diag_code` de la librería del AFE4490 SHALL publicarse como `AFE_diag` (entero) en el grupo DIAG de telemetría y en `/debug/state`.

#### Scenario: Sonda ausente

- **WHEN** el AFE4490 funciona con la sonda ausente
- **THEN** `/debug/state` y la telemetría DIAG llevan el `diag_code` que calcula `runAfeDiagnostics()`

Verificación manual en banco (V18 y V19): no hay entorno de test que lea el AFE.

### Requirement: Un AFE4490 sin muestras se reinicia, con límite de frecuencia

En la V19 el firmware SHALL reiniciar el AFE4490 (parar la librería, pulsar PDN, volver a iniciarla) cuando no haya entregado ninguna muestra durante más de 10 s con la tarea de aplicación viva, como mucho una vez por minuto, y SHALL contar los reinicios en `AFE_resets` (grupo DIAG).

#### Scenario: Umbral de 10 s

- **WHEN** no llega ninguna muestra durante 10 s o menos
- **THEN** no se ordena ningún reinicio
- **AND** al superar los 10 s se ordena un reinicio

Cubierto por `[env:native]` (`test_afe_watchdog`).

#### Scenario: Límite de un reinicio por minuto

- **WHEN** se acaba de reiniciar el AFE y el fallo persiste
- **THEN** no se ordena otro reinicio antes de 60 s desde el anterior

Cubierto por `[env:native]`.

#### Scenario: Una muestra rearma el vigilante

- **WHEN** llega una muestra antes de cumplirse los 10 s
- **THEN** el plazo de 10 s vuelve a contar desde esa muestra

Cubierto por `[env:native]`.

#### Scenario: Recuperación real

- **WHEN** en la V19 se interrumpe la señal DRDY del AFE
- **THEN** a los 10 s el AFE se reinicia, vuelven las muestras y `AFE_resets` aumenta en uno

Verificación manual en banco V19.

### Requirement: El watchdog externo está desactivado hasta verificarlo en banco

`EXT_WDT_WDI` SHALL conmutarse desde el mismo punto que alimenta el Task WDT del `loopTask` únicamente cuando `EXT_WDT_ENABLED` sea 1, que SHALL ser 0 por defecto. Con 0, el pin SHALL no configurarse como salida.

#### Scenario: Desactivado por defecto

- **WHEN** se compila la V19 sin tocar `EXT_WDT_ENABLED`
- **THEN** el firmware no configura ni conmuta IO41

Verificación de compilación y revisión; la activación es verificación manual en banco V19 con la ventana del TPS3431 ya fijada.

### Requirement: CHRG_OK es legible sin implicar una alarma

En la V19 `mainsPresentPin()` SHALL devolver `true` cuando `MAINS_OK_PIN` esté a nivel alto, y SHALL publicarse en `/debug/state`. El firmware SHALL no generar ninguna alarma por esta señal en este cambio.

#### Scenario: Red presente y ausente

- **WHEN** en la V19 se conecta y se desconecta la fuente de entrada
- **THEN** `/debug/state` refleja el cambio en `mains_ok`

Verificación manual en banco V19.

### Requirement: El log de depuración sale por IO43/IO44 en la V19

El log SHALL salir por la UART0 en IO43 (TX) e IO44 (RX) a 115200 baudios, sin retirar ninguna otra salida de log existente.

#### Scenario: Conector J1

- **WHEN** se conecta un adaptador serie a J1 de la V19
- **THEN** se lee el log de arranque a 115200

Verificación manual en banco V19.

