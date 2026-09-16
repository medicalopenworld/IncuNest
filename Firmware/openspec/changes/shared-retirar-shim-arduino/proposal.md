## Why

Placas afectadas: **motherBoard**, **Display_HMI** y **shared/**. El porte a ESP-IDF 6 dejó una capa de compatibilidad, `components/incunest_platform`, que reproduce la forma de las APIs de Arduino (`String`, `millis()`, `WiFi.`, `Wire`, `LittleFS.`, `Preferences`…) sobre las de IDF. Esta propuesta evalúa retirarla.

**El argumento NO es que quede más idiomático.** Es que cuatro fallos de esta semana fueron el shim comportándose distinto que el original **sin dar ningún error**:

| Fallo | Qué pasaba | Commit |
| --- | --- | --- |
| `FsFile::name()` devolvía la ruta y no el nombre | Dejó mudos **tres** subsistemas: la limpieza de ventanas de PPG, el tope de retención de logs de crash y el histórico de pesos. La partición se llenó hasta que la placa abortó | `d324f9e` |
| `available()` devolvía 0 para siempre sin `LWIP_SO_RCVBUF` | Reloj, geolocalización, Drive y SIM rotos en silencio | `91c578a` |
| `TwoWire` perdió su mutex | Accesos I2C concurrentes desde cuatro tareas | `91c578a` |
| `WiFiClient` no replicaba una espera | `LINK LOST` fantasma | `7f6475d` |

El patrón es siempre el mismo y es el que hace cara esta deuda: **cuando algo del porte "no hace nada" en vez de fallar, el sospechoso es el shim**. Un fallo que da error se arregla en una tarde; uno que calla cuesta días y llega al banco.

## What Changes

**No se propone un rediseño.** Se propone retirar el shim **por capas y por riesgo medido**, empezando por las que ya han fallado. Cada capa es un commit con su verificación.

### El inventario real

El shim son **4 496 líneas** frente a **47 743** de firmware propio: menos del 10 %. Y no es deuda uniforme — se parte en tres grupos:

**Grupo A — envoltorios finos, sin implementación propia (0 líneas de `.cpp`).** `plat_gpio`, `plat_pwm`, `plat_time`, `plat_num`, `plat_ip`, `plat_types`, `plat_string_json`. Son `inline` sobre IDF. `digitalWrite`/`pinMode` ya están a **0 usos**. Migrarlos no elimina ningún riesgo: es renombrar.

**Grupo B — semántica propia Y con fallo demostrado.** Aquí está el valor:

| Capa | Cabecera + impl. | Ficheros que la incluyen | ¿Ha fallado? |
| --- | --- | --- | --- |
| `plat_fs` | 441 líneas | 7 | **sí** (`d324f9e`) |
| `plat_i2c` | 494 líneas | 12 | **sí** (`91c578a`) |
| `plat_net_client` | 387 líneas | 9 | **sí** (`91c578a`, `7f6475d`) |
| `plat_nvs` | 417 líneas | 17 | no, pero guarda **datos de paciente** |

**Grupo C — superficie grande, sin fallo conocido.** `plat_webserver` (642), `plat_wifi` (479), `plat_uart` (268), `plat_string` (301), `plat_update`, `plat_spi`, `plat_print`, `plat_mdns`, `plat_esp`.

### El elefante: `String`

**388 usos** de `String` y **375** de `millis()`. `std::string`/`snprintf` no es sustitución mecánica: el patrón `String + String` en el camino de PPG a 500 Hz **tumbó la placa** hace dos días (`25d745e`) porque `operator new` lanzó `std::bad_alloc` y nadie lo capturaba. Migrar `String` a lo bruto reintroduce exactamente la clase de fallo que se acaba de cerrar.

## Capabilities

### New Capabilities

- `platform-layer-retirement`: el criterio para retirar cada capa del shim, el orden derivado del riesgo y la verificación que exige cada una antes de darse por hecha.

## Impact

- **Código**: `components/incunest_platform` (las 20 capas), y los puntos de llamada de `motherBoard/src`, `Display_HMI/src` y sus `include/`. 157 inclusiones de cabeceras `platform/*` repartidas por las dos placas.
- **Testing**: las capas del grupo B son las únicas que admiten prueba automática real — su contrato es observable (¿qué devuelve `name()`? ¿hay mutex?). Cada migración del grupo B **debe** llevar un test que fije el contrato **antes** de tocar el código, porque el modo de fallo es «se comporta distinto y calla». Para el resto, verificación en banco documentada.
- **Docs**: `docs/porte-esp-idf-nativo.md` (cerrar la sección 7), `docs/architecture.md`.

- **Out of scope (Non-goals)**:
  - **Migrar `String` en un barrido.** Iría al final y por zonas calientes (caminos a 500 Hz, ISR, tareas de control), no por búsqueda y reemplazo.
  - **El grupo A.** Renombrar envoltorios que ya son `inline` sobre IDF no elimina riesgo y sí mueve 157 inclusiones. Se deja explícitamente fuera.
  - Tocar los componentes de terceros (`incunest_afe4490`, `arduino_pid`, `incunest_sensors`): viven en otros repositorios y sus parches están documentados aparte.
  - La librería del AFE4490, que tiene su propio mantenedor y su propio pin de versión.

## Lo que hay que decidir antes de escribir código (Fase 0)

1. **¿Se retira el shim o se blinda?** La alternativa barata a migrar el grupo B es **dejarlo y ponerle tests de contrato**. Eso ataca el mismo riesgo —que se comporte distinto y calle— por una fracción del coste, y es reversible. Migrar sólo gana si además se quiere quitar la dependencia conceptual de Arduino.
2. **Si se retira: ¿hasta dónde?** Retirar el grupo B y dejar A y C es un estado final coherente y defendible. «Retirarlo todo» incluye `String`, que es el 80 % del esfuerzo y el 100 % del riesgo nuevo.
3. **Coste de oportunidad.** Hoy siguen sin probarse tras el porte GPRS/SIM, táctil, audio, humidificador, fototerapia y SensorBoard; el temblor del panel está acotado pero no eliminado; y el umbral del ventilador está calibrado sobre **una sola unidad**. Todo eso es riesgo *medido y abierto*; el shim es riesgo *ya mitigado cuatro veces*.
