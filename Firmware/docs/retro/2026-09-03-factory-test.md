# Retro — test de fábrica desde el splash del HMI (2026-09-03)

Rama `feat/factory-test`, worktree `Firmware/.worktrees/factory-test`.
Modalidad `auto`. Change OpenSpec `shared-factory-test` (shared + motherBoard +
Display_HMI).

## Qué se hizo

Un botón "TEST FÁBRICA" en el splash del display abre una pantalla que ejecuta
nueve tests locales del HMI y ordena a la motherBoard una batería de treinta
(alimentación, sensores propios, SensorBoard por USB con estímulos de puerta y
luz, actuadores, zumbador medido con el micrófono de la SensorBoard, AFE4490,
GSM/WiFi/ThingsBoard pasivos, NVS, LittleFS). La tabla de tests y el codec de
las líneas `FTEST` viven en `shared/` con tests nativos. La motherBoard corre
la batería en una tarea propia dentro de un estado seguro acotado por task WDT,
6 min, dead-man del HMI y re-check de la precondición.

## Aprendizajes con coste observable

### 1. La base de la rama no era `dev`

`dev` no contenía todavía `sensorboard_comm` (vivía en
`feat/sb-usb-pin-autoswap`, en el checkout principal). El agente que lo
descubrió lo hizo a mitad de exploración; hubo que resetear el worktree a esa
rama y fusionar `dev` encima. **Regla**: antes de crear el worktree de una
feature, `git ls-tree` de los módulos de los que depende en `dev`; si faltan,
la base es la rama que los tiene y se fusiona `dev` en la feature.

### 2. Dos subagentes commiteando en el mismo worktree perdieron un commit

Ver `tooling.md` § "Subagentes en paralelo". Un `--amend` del agente de la MB
reescribió el commit del HMI; se recuperó del reflog. La segunda ronda
(fixes del review) se hizo con la regla nueva: los agentes editan y compilan,
el orquestador commitea por ámbito. Funcionó sin incidentes.

### 3. La lista de "escritores de actuadores" del diseño estaba incompleta

El diseño enumeró tres escritores de PWM (`PIDHandler`, `turnFans`,
`securityCheck`) a partir del informe de exploración, y el gate
`g_factoryTestActive` se puso solo en ellos. La revisión de seguridad encontró
tres más: el bloque `newCommand` de `main.cpp` (a 1 Hz con cada trama del HMI),
la regulación de fototerapia y `buzzerHandler()`. Sin el gate, el test de
actuadores habría fallado siempre y dejado `ALARM_HEATER_FAULT` latcheada.
**Regla**: una afirmación del tipo "los únicos escritores de X son…" se
verifica con `grep -n "ledcWrite\|digitalWrite(ACTUATORS_EN" src/` antes de
entrar en el diseño, no se toma del informe de un agente.

### 4. Off-by-one en prefijos `strncmp` calculados a mano

`"CTRL,FTEST_DONE,"` con `17` en vez de `16`: dos ramas muertas que ningún
build detecta y que el display no tiene entorno de test para cazar. Ya hay 20
prefijos así en `Display_HMI/src/tasks/CommTask.cpp`. Corregido con
`sizeof(literal) - 1`; queda como deuda añadir un test de host del despacho de
prefijos (o extraer el despacho a lógica pura compilable en `native`).

### 5. El validador de OpenSpec lee solo la primera línea del requisito

`openspec validate` exige SHALL/MUST en la **primera línea** del texto del
requisito; un requisito con el SHALL en la segunda línea falla. Se arregló
reordenando la frase.

### 6. Revisión de seguridad con mirada normativa

El `security-reviewer` fue el que encontró los cuatro bloqueantes (keepalive,
estado inhibido sin cota, precondición evaluada una vez, off-by-one). El
`code-reviewer` encontró los de UI (eventos obsoletos, tapar AUDIO PAUSED). Los
dos en paralelo y frescos siguen mereciendo la pena; lo que hay que revisar es
que el diseño llegue con las cotas del estado inhibido ya pensadas, no que las
aporte el revisor.

## Segunda vuelta tras el banco (2026-09-05, change `shared-factory-test-bench`)

### 7. El sondeo I2C2 da ACK falso con el SensorBoard conectado

`sensor_src` dio FAIL en una unidad sana: `roomSensorI2CDetected()` lee ACK
para cualquier dirección si una de las líneas USB está a nivel bajo. El test
se rehizo para exigir **datos** (temperatura real y fresca por USB o por
I2C2), no una clasificación. El sondeo en sí queda para su propia rama.
**Regla**: un test de fábrica comprueba que la función existe (llegan
lecturas), no cómo cree la placa que está cableada.

### 8. `GPRS.powerUp` no significa "el módem responde"

Es true solo durante la secuencia de arranque. Un test que lo espera falla si
el módem arrancó antes de pulsar el botón. Flags `modemResponded`/`simReady`
pegajosos en `GPRSstruct`. **Regla**: antes de usar un flag ajeno como
evidencia, leer dónde se pone Y dónde se quita.

### 9. Toque fantasma del GT911 al arrancar

El test se abrió solo. Botón en la esquina con área extendida + toque fantasma
del init del GT911 estirado por el debounce. Botón centrado, sin área
extendida, armado a 1,5 s y comprobación del punto del indev.

### 10. Los agentes sin commit funcionan

La ronda de fixes y toda la segunda vuelta se hicieron con agentes que editan
y compilan pero no commitean; el orquestador commiteó por ámbito. Cero
incidentes de índice compartido. Queda como regla en `tooling.md`.

## Deuda que queda

- Verificación en banco (sección 8 de `tasks.md`): sin hardware conectado en
  esta sesión.
- Umbrales de coherencia de las SHT40 (1.0 °C / 3.0 °C) son de partida.
- Un PIN de servicio en la barrera de entrada (hoy es una confirmación).
- Test de host del despacho de prefijos en el `CommTask.cpp` del display.
- `test_sensorboard_frame` ERRORED por DLL en esta máquina (independiente).
