## ADDED Requirements

### Requirement: Una capa del shim sólo se retira si su contrato queda fijado antes por un test

El modo de fallo de esta capa no es que se rompa: es que **se comporta distinto que el original y no da error**. Cuatro veces esta semana. Un cambio que sustituya una capa de `components/incunest_platform` por llamadas directas a ESP-IDF SHALL ir precedido de una prueba que fije el contrato observable de esa capa, escrita **contra el comportamiento actual** y pasando **antes** de tocar el código.

La prueba SHALL cubrir, como mínimo, la propiedad concreta cuyo incumplimiento ya causó un fallo en producción, cuando la haya. Un cambio que sólo compile y pase la batería de banco existente SHALL considerarse insuficiente: esa batería estaba verde mientras los cuatro fallos existían.

#### Scenario: Retirar `plat_fs`

- **WHEN** se sustituye `FsFile` por llamadas POSIX directas
- **THEN** existe antes un test que fija que `name()` devuelve el nombre pelado y no la ruta
- **AND** ese test falla si se le devuelve `/littlefs/x.csv` en vez de `x.csv`
- **AND** cubre también el prefijo de montaje, porque de él depende que una unidad desplegada siga encontrando los perfiles de bebé tras el OTA

#### Scenario: Retirar `plat_i2c`

- **WHEN** se sustituye `I2cBus` por `i2c_master_*` de IDF
- **THEN** existe antes un test o una verificación documentada de que el bloqueo abarca la transacción completa con repeated-start
- **AND** de que dos tareas concurrentes sobre el mismo bus no se entrelazan

#### Scenario: Una capa sin fallo conocido

- **WHEN** se retira una capa del grupo C, sin defecto histórico
- **THEN** basta con fijar su contrato observable y verificar en banco lo que el test no alcance
- **AND** la verificación en banco queda escrita en el commit, no implícita

### Requirement: El orden de retirada lo fija el riesgo medido, no la comodidad

El shim SHALL retirarse por capas, cada una en su propio commit, y el orden SHALL derivarse de dos datos medibles: si la capa ya ha causado un fallo en producción, y qué custodia (datos de paciente, actuadores, diagnóstico).

Las capas sin implementación propia —`plat_gpio`, `plat_pwm`, `plat_time`, `plat_num`, `plat_ip`, `plat_types`, `plat_string_json`— SHALL quedar fuera del alcance: son `inline` sobre IDF, no tienen semántica que pueda divergir, y tocarlas mueve 157 inclusiones sin eliminar ningún riesgo.

#### Scenario: Orden derivado de los datos

- **WHEN** se planifica la retirada
- **THEN** van primero `plat_fs`, `plat_i2c` y `plat_net_client`, que ya han fallado
- **AND** después `plat_nvs`, que no ha fallado pero guarda los perfiles de bebé y el histórico de pesos
- **AND** el resto sólo si la Fase 0 decide ir más allá del grupo B

### Requirement: `String` no se migra por barrido

`String` tiene 388 usos y su sustitución SHALL hacerse por zonas, empezando por los caminos calientes (500 Hz, ISR, tareas de control), y SHALL no introducir asignación de heap donde hoy no la hay.

Una migración que sustituya `String` por `std::string` manteniendo el patrón de concatenación encadenada SHALL rechazarse: es exactamente lo que agotó el heap en el camino de PPG y abortó la placa (`25d745e`), porque `operator new` lanza `std::bad_alloc` y nadie lo captura.

#### Scenario: Camino a 500 Hz

- **WHEN** se migra una construcción de cadena en un camino que corre a 500 Hz
- **THEN** el resultado usa un buffer de pila con `snprintf`, sin asignación dinámica
- **AND** si la cadena es para un log de canal apagado, la construcción queda dentro de la guarda de compilación y desaparece del binario

#### Scenario: Zona fría

- **WHEN** se migra una construcción de cadena en un camino de arranque o de configuración
- **THEN** basta con que no asigne más que hoy y con que el resultado sea idéntico
