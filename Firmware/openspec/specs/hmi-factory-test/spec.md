# hmi-factory-test Specification

## Purpose
TBD - created by archiving change shared-factory-test. Update Purpose after archive.
## Requirements
### Requirement: Botón de test de fábrica en la pantalla de arranque

`ui_ScreenIntro` SHALL mostrar un botón "HW test" (mismo literal en los tres
idiomas, centrado abajo en la pantalla, sin área de toque ampliada). Al
pulsarlo, el display SHALL cancelar la transición automática a
`ui_ScreenMain`, abrir la pantalla de test y suspender el bloqueo por
inactividad mientras esté abierta. El botón SHALL existir solo en el splash:
ninguna otra pantalla ofrece la entrada. El evento de pulsación SHALL
ignorarse si han pasado menos de 1500 ms desde que se creó la pantalla, o si
el punto del toque no cae dentro del rectángulo del botón — barrera contra el
toque fantasma de un solo frame que el controlador táctil (GT911) puede emitir
en una esquina fija mientras se inicializa (feedback de banco: con el botón en
`LV_ALIGN_BOTTOM_LEFT` y área de toque ampliada, ese fantasma abría el test
sin que el operario tocara nada).

#### Scenario: Pulsar durante el splash
- **WHEN** el operario toca el botón pasados los primeros 1500 ms
- **THEN** el splash no avanza a la pantalla principal y aparece la pantalla de
  test con todos los tests en estado pendiente
- **AND** a los 20 s no aparece `ui_ScreenLock`
- *(Verificación manual en el CrowPanel: Display_HMI no tiene entorno de test.)*

#### Scenario: Toque fantasma al arrancar
- **WHEN** el controlador táctil reporta un toque de un solo frame en el
  rectángulo del botón durante los primeros 1500 ms tras crear la pantalla, o
  el evento no lo dispara `LV_EVENT_CLICKED` con el punto dentro del botón
- **THEN** el test de fábrica NO se abre y el splash continúa su temporizador
  normal
- *(Verificación manual en el CrowPanel: reproducir el arranque en frío varias
  veces y confirmar que el test no se abre solo.)*

#### Scenario: No pulsar
- **WHEN** el operario no toca nada
- **THEN** el splash avanza a `ui_ScreenMain` exactamente como hoy (mínimo 5 s,
  máximo 15 s esperando `CTRL,STATE`)
- *(Verificación manual.)*

### Requirement: Barrera de entrada de seguridad

Antes de ejecutar cualquier test, local o remoto, la pantalla SHALL mostrar en
la propia zona de acción del overlay un aviso: "el equipo debe estar VACÍO,
sin paciente; los actuadores se encenderán en lazo abierto" (traducido según
`g_lang`) con botones Sí / No. Sí SHALL iniciar la secuencia de tests locales.
No SHALL cerrar el overlay sin ejecutar ningún test y devolver el flujo al
comportamiento normal del splash (`g_factoryTestRequested` vuelve a `false` y
la transición automática a `ui_ScreenMain` puede continuar).

#### Scenario: Aceptar la barrera
- **WHEN** el operario pulsa "TEST FÁBRICA" y luego Sí en el aviso de seguridad
- **THEN** arranca `HMI_SYSINFO`, el primer test de la secuencia local
- *(Verificación manual en el CrowPanel: Display_HMI no tiene entorno de test.)*

#### Scenario: Rechazar la barrera
- **WHEN** el operario pulsa No en el aviso de seguridad
- **THEN** el overlay se cierra sin haber ejecutado ningún test y el splash
  retoma su temporizador normal
- *(Verificación manual.)*

### Requirement: Tests locales del display

La pantalla SHALL ejecutar en orden, en `UI_Task`, sin `delay()` y por
polling: `HMI_SYSINFO` (flash 16 MB, PSRAM 8 MB, heap libre ≥ 60 kB),
`HMI_I2C` (ACK en 0x14 y 0x30; 0x18 solo informativo en `detail`),
`HMI_BUZZER` y `HMI_SPEAKER` (300 ms con `buzzerOn()/Off()` y los comandos
248/249, después Sí / No), `HMI_WIFI` (MAC en `detail`; PASS si
`WIFIIsConnected()` con RSSI, si no `WiFi.scanNetworks(true)` y PASS si ve ≥ 1
red), `HMI_NVS` (escribir y releer `hmi_ftest/probe` fuera de `LVGL_Lock()`),
`HMI_LINK` (`Display_BoardEverSeen() && !Display_IsBoardLinkLost()`, `fwVer`
de la MB en `detail`). Las preguntas al operario SHALL expirar a los 60 s como
FAIL. `HMI_PANEL` y `HMI_TOUCH` SHALL retirarse de esta secuencia (feedback de
banco: alargaban la batería sin aportar en la línea de montaje actual); sus
IDs permanecen en la tabla compartida sin usarse desde este lado.

#### Scenario: Secuencia local sin panel ni toque
- **WHEN** el operario acepta la barrera de entrada
- **THEN** la secuencia local ejecuta `HMI_SYSINFO`, `HMI_I2C`, `HMI_BUZZER`,
  `HMI_SPEAKER`, `HMI_WIFI`, `HMI_NVS`, `HMI_LINK`, en ese orden, sin pasar
  nunca por un rectángulo de color ni pedir tocar objetivos en pantalla
- *(Verificación manual.)*

#### Scenario: Chasquido y patrón de alarma no interfieren
- **WHEN** `HMI_BUZZER` está sonando
- **THEN** `click_beep_start()` no emite y `link_audio_service()` no apaga el
  zumbador hasta que el test termina, por el mismo arbitraje que ya usan entre
  sí
- *(Verificación manual.)*

### Requirement: Orquestación del test remoto de la motherBoard

Tras los tests locales la pantalla SHALL enviar `HMI,FTEST,START` y añadir una
fila por cada `CTRL,FTEST` recibido, actualizándola con su estado. Todas las
filas visibles (locales y de motherBoard) SHALL pintarse en una **cuadrícula
de 3 columnas de botones**, dentro de un contenedor con scroll vertical: cada
botón muestra el título del test (hasta 2 líneas) y una palabra de estado.
El orden de la cuadrícula SHALL ser FALLA → AVISO (`FTEST_WARN`) → en curso
(pendiente, `RUNNING`, `WAIT`, `CONFIRM`) → PASA; las filas en `SKIP` SHALL
ocultarse (no se pintan, no cuentan en el resumen visible). La cuadrícula
SHALL reordenarse únicamente cuando cambia el estado de alguna fila, nunca en
cada pasada de `FactoryTest_Poll()`, y SHALL mantener visible la fila en curso
(`lv_obj_scroll_to_view`). Colores: FALLA `0xFFE0E4`/`0xD5283C`, AVISO
`0xFFF0D6`/`0xC98A00`, PASA `0xDFF3FF`/`0x2196C4`, en curso blanco/`0x0B2E4F`.

Al tocar un botón de la cuadrícula SHALL abrirse un panel de detalle modal
sobre la cuadrícula, con: título del test, una descripción de una línea (por
ID) de qué comprueba, el estado, el valor de `detail` recibido, y los botones
Reintentar (solo visible con la batería terminada y la fila en FALLA o AVISO;
para un ID de motherBoard envía `HMI,FTEST,RUN,id`, para uno local repite ese
test) y Cerrar.

En `WAIT` SHALL mostrarse la instrucción asociada al ID (`SB_DOOR`: "Abre y
cierra la puerta", `SB_LIGHT`: "Tapa el sensor de luz") en la zona de acción;
en `CONFIRM` SHALL mostrarse la pregunta del ID y los botones Sí / No, cuyo
resultado envía `HMI,FTEST,CONFIRM,id,ok`. Un `CTRL,FTEST_REJECT` SHALL
mostrarse con su motivo traducido. Si no llega ningún `CTRL,FTEST*` en 10 s
SHALL marcarse "MB sin soporte". Si tras el primer `CTRL,FTEST` la motherBoard
deja de emitir durante 120 s sin llegar `CTRL,FTEST_DONE` ni
`CTRL,FTEST_REJECT`, la pantalla SHALL marcar como FALLA con detalle "sin
respuesta" todas las filas de motherBoard que no hayan llegado a un estado
terminal (PASS/FAIL/SKIP/**WARN**) y pasar al resumen. Cualquier `CTRL,FTEST*`
recibido SHALL reiniciar ese plazo de 120 s. Las líneas SHALL parsearse en
`CommTask.cpp` con `ftest_parse_result()` a un anillo de 8 entradas protegido
con `portMUX_TYPE`, drenado por `FactoryTest_Poll()`; ninguna llamada a LVGL
desde `Comm_Task`.

#### Scenario: Cuadrícula ordenada por prioridad
- **WHEN** hay filas en FALLA, AVISO, en curso y PASA a la vez
- **THEN** la cuadrícula las pinta en ese orden (FALLA primero) y ninguna fila
  en SKIP aparece
- *(Verificación manual.)*

#### Scenario: Panel de detalle de un aviso
- **WHEN** el operario toca el botón de `WIFI` en AVISO
- **THEN** se abre el panel con el título, la descripción del test, "AVISO" y
  el `detail` recibido, con el botón Reintentar visible si la batería ya
  terminó
- *(Verificación manual.)*

#### Scenario: Estímulo de puerta
- **WHEN** llega `CTRL,FTEST,10,4,`
- **THEN** la fila `SB_DOOR` muestra "Abre y cierra la puerta" en la zona de
  acción
- **AND** al llegar `CTRL,FTEST,10,1,` pasa a PASA (azul) y la instrucción
  desaparece
- *(Verificación manual.)*

#### Scenario: Confirmación de zumbador
- **WHEN** llega `CTRL,FTEST,16,5,` y el operario pulsa Sí
- **THEN** el display envía `HMI,FTEST,CONFIRM,16,1` una sola vez y los
  botones se ocultan
- *(Verificación manual.)*

#### Scenario: Rechazo por control activo
- **WHEN** llega `CTRL,FTEST_REJECT,1`
- **THEN** la sección de motherBoard muestra "Control activo: apaga el control
  antes del test" y el resumen se muestra solo con los tests locales
- *(Verificación manual.)*

#### Scenario: Enlace perdido a mitad de batería
- **WHEN** llega `CTRL,FTEST,10,4,` y no llega ningún otro `CTRL,FTEST*`
  durante 120 s
- **THEN** las filas de motherBoard que seguían pendientes pasan a FALLA con
  detalle "sin respuesta" y el resumen lo refleja en el contador de errores
- *(Verificación manual.)*

### Requirement: Resumen, reintento y salida

Al recibir `CTRL,FTEST_DONE` (o agotar los plazos) la pantalla SHALL mostrar
una cabecera "N errores - M avisos - K OK" (traducida según `g_lang`),
combinando local y motherBoard, **sin mostrar los omitidos**. Cada fila en
FALLA o AVISO SHALL ofrecer Reintentar desde su panel de detalle: para un ID
de motherBoard envía `HMI,FTEST,RUN,id`; para uno local repite ese test. El
botón Salir SHALL cerrar el overlay, restaurar el bloqueo por inactividad y
cargar `ui_ScreenMain`. Si hay una batería de motherBoard en curso, Salir
SHALL enviar `HMI,FTEST,ABORT` antes. Si el resumen queda abierto 10 min sin
que el operario pulse Reintentar ni Salir, la pantalla SHALL cerrarlo
automáticamente por el mismo camino que Salir (mismo precedente que
`HELP_IDLE_TIMEOUT_MS`).

#### Scenario: Cabecera del resumen con avisos
- **WHEN** la batería termina con 1 fallo local, 2 fallos de motherBoard y 3
  avisos de motherBoard
- **THEN** la cabecera muestra "3 errores - 3 avisos - N OK" (N = resto de
  tests en PASA), sin mencionar los omitidos
- *(Verificación manual.)*

#### Scenario: Reintentar un test de la motherBoard desde el detalle
- **WHEN** el operario abre el detalle de `SB_CAMERA` en FALLA y pulsa
  Reintentar
- **THEN** el display envía `HMI,FTEST,RUN,12`, el panel se cierra y la fila
  vuelve a "en curso" hasta el nuevo resultado
- *(Verificación manual.)*

#### Scenario: Salir con la batería en curso
- **WHEN** el operario pulsa Salir mientras la motherBoard ejecuta tests
- **THEN** el display envía `HMI,FTEST,ABORT`, cierra el overlay y carga
  `ui_ScreenMain`
- *(Verificación manual.)*

#### Scenario: Inactividad en el resumen
- **WHEN** el resumen queda abierto 10 min sin que el operario pulse
  Reintentar ni Salir
- **THEN** el overlay se cierra automáticamente y carga `ui_ScreenMain`
- *(Verificación manual.)*

### Requirement: Persistencia del resultado en el display

Al mostrar el resumen la pantalla SHALL guardar en NVS `hmi_ftest`: `epoch`
(`HMI_GetEpochNow()`, 0 si no hay hora), máscaras PASA y FALLA de los tests
locales, contadores `pass`/`fail`/`skip`/**`warn`** (clave `mb_warn`)
recibidos de la motherBoard y `FWversion`. La escritura SHALL hacerse fuera de
`LVGL_Lock()`.

#### Scenario: Consulta tras reinicio
- **WHEN** se reinicia el display tras una batería con avisos
- **THEN** `hmi_ftest` conserva epoch, máscaras y contadores (incluido
  `mb_warn`) de la última ejecución
- *(Verificación manual leyendo NVS por log al arrancar.)*

