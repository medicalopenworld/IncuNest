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

`ui_ScreenSettings` SHALL ofrecer una segunda entrada: una fila "Test de
hardware" (traducida según `g_lang`) al final de la lista, debajo de Info,
con el mismo estilo (Cont/Panel/Label/Button/Arrow) que las filas existentes.
La fila SHALL estar habilitada únicamente cuando `UI_AnyControlActive()`
devuelve false (ningún control de temperatura/humedad ni fototerapia activos
— mismo criterio que usa el diálogo de salida de bebé). Mientras algo esté
activo, la fila SHALL mostrarse deshabilitada (texto atenuado, sin reaccionar
al toque) con un subtexto "Apaga el control para testear" (traducido). Este
estado SHALL refrescarse en cada pasada de `UI_Task` mientras
`ui_ScreenSettings` esté en pantalla — cubriendo tanto la entrada a Settings
como cualquier `CTRL,STATE` que active o desactive el control mientras el
operario sigue ahí — sin repintar si no cambió desde la última pasada. Al
pulsar la fila (con la condición cumplida) SHALL abrirse la misma pantalla de
test que el botón del splash, con el mismo aviso de la barrera de entrada.
Un origen desde Settings SHALL recordarse mientras el overlay esté abierto:
si el operario responde "No" al aviso (o pulsa "Salir" sin haberlo
respondido todavía, antes de que arranque ningún test), el overlay SHALL
cerrarse sin cargar `ui_ScreenMain` — permaneciendo en `ui_ScreenSettings`,
de donde vino. Cualquier cierre posterior a esa barrera (batería completa o
abortada a mitad mediante "Salir") SHALL conservar el comportamiento
existente y cargar `ui_ScreenMain`, igual que si se hubiera entrado desde el
splash.

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

#### Scenario: Fila deshabilitada con control activo
- **WHEN** el operario entra en `ui_ScreenSettings` con la temperatura, la
  humedad o la fototerapia activas (`UI_AnyControlActive()` true)
- **THEN** la fila "Test de hardware" aparece en gris, no clicable, con el
  subtexto "Apaga el control para testear" visible
- *(Verificación manual en el CrowPanel: Display_HMI no tiene entorno de
  test.)*

#### Scenario: La fila se habilita sin salir de Settings
- **WHEN** el operario apaga el control (o la fototerapia) mientras sigue en
  `ui_ScreenSettings`, sin volver a la pantalla principal
- **THEN** en la siguiente pasada de `UI_Task` la fila pasa a su estilo
  habitual (texto normal, clicable) y el subtexto desaparece, sin que el
  operario haya tocado nada más
- *(Verificación manual.)*

#### Scenario: Entrar al test desde Settings y rechazar el aviso
- **WHEN** el operario toca la fila "Test de hardware" habilitada y luego
  pulsa "No" en el aviso de "equipo vacío"
- **THEN** el overlay se cierra y la pantalla activa sigue siendo
  `ui_ScreenSettings` (no navega a `ui_ScreenMain`)
- *(Verificación manual.)*

#### Scenario: Entrar al test desde Settings, completar la batería y salir
- **WHEN** el operario toca la fila, acepta el aviso, la batería de tests
  corre hasta el resumen y pulsa "Salir"
- **THEN** el overlay se cierra y el display carga `ui_ScreenMain`, igual que
  si se hubiera entrado desde el botón del splash
- *(Verificación manual.)*

### Requirement: Barrera de entrada de seguridad

Antes de ejecutar cualquier test, local o remoto, la pantalla SHALL mostrar en
la propia zona de acción del overlay un aviso: "el equipo debe estar VACÍO,
sin paciente; los actuadores se encenderán en lazo abierto" (traducido según
`g_lang`) con botones Sí / No. Sí SHALL iniciar la secuencia de tests locales.
No SHALL cerrar el overlay sin ejecutar ningún test. Si el origen fue el
botón del splash, el flujo SHALL devolverse al comportamiento normal del
splash (`g_factoryTestRequested` vuelve a `false` y la transición automática
a `ui_ScreenMain` puede continuar); si el origen fue la fila de
`ui_ScreenSettings`, el overlay SHALL cerrarse permaneciendo en
`ui_ScreenSettings` en su lugar.

#### Scenario: Aceptar la barrera
- **WHEN** el operario pulsa "TEST FÁBRICA" y luego Sí en el aviso de seguridad
- **THEN** arranca `HMI_SYSINFO`, el primer test de la secuencia local
- *(Verificación manual en el CrowPanel: Display_HMI no tiene entorno de test.)*

#### Scenario: Rechazar la barrera desde el splash
- **WHEN** el operario pulsa No en el aviso de seguridad, habiendo entrado
  desde el botón del splash
- **THEN** el overlay se cierra sin haber ejecutado ningún test y el splash
  retoma su temporizador normal
- *(Verificación manual.)*

#### Scenario: Rechazar la barrera desde Settings
- **WHEN** el operario pulsa No en el aviso de seguridad, habiendo entrado
  desde la fila "Test de hardware" de `ui_ScreenSettings`
- **THEN** el overlay se cierra sin haber ejecutado ningún test y la pantalla
  activa sigue siendo `ui_ScreenSettings`
- *(Verificación manual.)*

### Requirement: Tests locales del display

La pantalla SHALL ejecutar en orden, en `UI_Task`, sin `delay()` y por
polling: `HMI_SYSINFO` (flash 16 MB, PSRAM 8 MB, heap libre ≥ 60 kB),
`HMI_I2C` (PASA si `UI_TouchInitOk()` y `UI_BacklightInitOk()` son true —
la evidencia de que el init del touch, GT911 con reintentos, y de la
secuencia de backlight, STC8H1K28 con 5 reintentos, del arranque tuvieron
éxito; un `endTransmission()` vacío no prueba nada frente a ninguno de los
dos chips, que pueden NACKear una escritura vacía con hardware sano, así que
el sondeo de 0x14/0x30/0x18 queda solo en el `detail`, informativo, y nunca
decide), `HMI_WIFI` (MAC en `detail`; PASS si `WIFIIsConnected()` con RSSI,
si no `WiFi.scanNetworks(true)` y PASS si ve ≥ 1 red), `HMI_NVS` (escribir y
releer `hmi_ftest/probe` fuera de `LVGL_Lock()`), `HMI_LINK`
(`Display_BoardEverSeen() && !Display_IsBoardLinkLost()`, `fwVer` de la MB en
`detail`). `HMI_PANEL`, `HMI_TOUCH`, `HMI_BUZZER` y `HMI_SPEAKER` SHALL
retirarse de esta secuencia (feedback de banco: los dos primeros alargaban la
batería sin aportar en la línea de montaje actual; el jig actual no puede
verificar el zumbador ni el altavoz del display); sus IDs permanecen en la
tabla compartida sin usarse desde este lado.

#### Scenario: Secuencia local sin panel, toque, zumbador ni altavoz
- **WHEN** el operario acepta la barrera de entrada
- **THEN** la secuencia local ejecuta `HMI_SYSINFO`, `HMI_I2C`, `HMI_WIFI`,
  `HMI_NVS`, `HMI_LINK`, en ese orden, sin pasar nunca por un rectángulo de
  color, pedir tocar objetivos en pantalla ni sonar el zumbador o el altavoz
  del display
- *(Verificación manual en el CrowPanel: Display_HMI no tiene entorno de
  test.)*

#### Scenario: HMI_I2C decide con la evidencia del arranque, no con el sondeo
- **WHEN** el GT911 y el STC8H1K28 inicializaron correctamente en `UI_Task`
  (con sus reintentos), aunque el sondeo posterior de 0x30 o de 0x18 devuelva
  NACK
- **THEN** `HMI_I2C` PASA, con un `detail` del tipo "0x14:si 0x30:no 0x18:no"
  puramente informativo (no decide el resultado)
- *(Verificación manual.)*

### Requirement: Orquestación del test remoto de la motherBoard

Tras los tests locales la pantalla SHALL enviar `HMI,FTEST,START` y añadir una
fila por cada `CTRL,FTEST` recibido, actualizándola con su estado. Todas las
filas visibles (locales y de motherBoard) SHALL pintarse en una **cuadrícula
de 3 columnas**, **paginada** en páginas de 3×3 botones que quepan en la
tarjeta sin scroll (feedback de banco: el scroll no se maneja bien con
guantes), con botones "<" y ">" y un indicador "Página i/n", deshabilitados
en el primer y el último extremo respectivamente. La página SHALL seguir a
la fila en curso mientras la batería avanza (salta a la página que contiene
esa fila); si el operario pagina a mano con "<"/">" la pantalla SHALL dejar
de seguirla hasta el siguiente cambio de test en curso. El orden de la
cuadrícula (y por tanto de las páginas) SHALL ser FALLA → AVISO
(`FTEST_WARN`) → en curso (pendiente, `RUNNING`, `WAIT`, `CONFIRM`) → PASA;
las filas en `SKIP` SHALL ocultarse (no se pintan, no cuentan en el resumen
visible). La cuadrícula SHALL reordenarse únicamente cuando cambia el estado
de alguna fila, nunca en cada pasada de `FactoryTest_Poll()`. El código de
color SHALL ser el mismo en toda la pantalla (cuadrícula, panel de detalle y
veredicto): FALLA `0xFFE0E4`/`0xD5283C` (rojo), AVISO `0xFFF0D6`/`0xC98A00`
(amarillo), PASA `0xDFF3E4`/`0x1B7F3B` (verde), en curso blanco/`0x0B2E4F`.

Si una fila de motherBoard permanece más de `FTEST_ROW_TIMEOUT_MS` (100 s) en
`RUNNING`, `WAIT` o `CONFIRM` sin cambiar de estado, la pantalla SHALL
marcarla FALLA con detalle "timeout". La motherBoard tiene su propia cota de
90 s por test y debería emitir FAIL "timeout" antes; esta vigilancia por fila
es solo la red de seguridad del display si la placa se cuelga en ese test
concreto, y no decide el resumen por sí sola: si además no llega ningún
`CTRL,FTEST*` en los 120 s del timeout de silencio, se aplica el
comportamiento ya existente de enlace perdido (ver más abajo).

Al tocar un botón de la cuadrícula SHALL abrirse un panel de detalle modal
sobre la cuadrícula, con: título del test, una descripción de una línea (por
ID) de qué comprueba, el estado con el mismo código de color de fondo/texto
que la cuadrícula, el valor de `detail` recibido, y los botones Reintentar
(solo visible con la batería terminada y la fila en FALLA o AVISO; para un ID
de motherBoard envía `HMI,FTEST,RUN,id`, para uno local repite ese test) y
Cerrar.

En `WAIT` SHALL mostrarse la instrucción asociada al ID (`SB_DOOR`: "Abre y
cierra la puerta", `SB_LIGHT`: "Tapa el sensor de luz") en la zona de acción;
en `CONFIRM` SHALL mostrarse la pregunta del ID y los botones Sí / No, cuyo
resultado envía `HMI,FTEST,CONFIRM,id,ok`. Un `CTRL,FTEST_REJECT` SHALL
mostrarse con su motivo traducido. Si no llega ningún `CTRL,FTEST*` en 10 s
SHALL marcarse "MB sin soporte". Si tras el primer `CTRL,FTEST` la motherBoard
deja de emitir durante 120 s sin llegar `CTRL,FTEST_DONE` ni
`CTRL,FTEST_REJECT`, la pantalla SHALL marcar como FALLA con detalle "sin
respuesta" todas las filas de motherBoard que no hayan llegado a un estado
terminal (PASS/FAIL/SKIP/WARN) y pasar al resumen. Cualquier `CTRL,FTEST*`
recibido SHALL reiniciar ese plazo de 120 s. Las líneas SHALL parsearse en
`CommTask.cpp` con `ftest_parse_result()` a un anillo de 8 entradas protegido
con `portMUX_TYPE`, drenado por `FactoryTest_Poll()`; ninguna llamada a LVGL
desde `Comm_Task`.

#### Scenario: La página sigue al test en curso
- **WHEN** la batería remota avanza de una fila de la página 1 a otra de la
  página 3 y el operario no ha tocado "<"/">"
- **THEN** la cuadrícula salta automáticamente a la página 3
- *(Verificación manual.)*

#### Scenario: La paginación manual no se pisa hasta el siguiente test
- **WHEN** el operario pulsa ">" para ver la página 2 mientras un test de la
  página 1 sigue en curso
- **THEN** la pantalla se queda en la página 2 hasta que el test en curso
  cambia a otro; en ese momento vuelve a seguirlo
- *(Verificación manual.)*

#### Scenario: Botones de paginación deshabilitados en los extremos
- **WHEN** la cuadrícula tiene 4 páginas y se muestra la página 1
- **THEN** "<" está deshabilitado y ">" habilitado; en la página 4 es al
  revés
- *(Verificación manual.)*

#### Scenario: Cuadrícula ordenada por prioridad con el código de color nuevo
- **WHEN** hay filas en FALLA, AVISO, en curso y PASA a la vez
- **THEN** la cuadrícula las pinta en ese orden (FALLA primero), con PASA en
  verde (`0xDFF3E4`/`0x1B7F3B`), y ninguna fila en SKIP aparece
- *(Verificación manual.)*

#### Scenario: Fila colgada marcada por timeout
- **WHEN** una fila de motherBoard queda en `RUNNING` más de 100 s sin que
  llegue ningún otro `CTRL,FTEST` para ese id, pero la motherBoard sigue
  emitiendo `CTRL,FTEST` de otros ids dentro del plazo de silencio de 120 s
- **THEN** la pantalla marca esa fila FALLA con detalle "timeout" y la
  batería sigue con el resto de tests
- *(Verificación manual.)*

#### Scenario: Panel de detalle de un aviso
- **WHEN** el operario toca el botón de `WIFI` en AVISO
- **THEN** se abre el panel con el título, la descripción del test, "AVISO"
  en amarillo (`0xFFF0D6`/`0xC98A00`) y el `detail` recibido, con el botón
  Reintentar visible si la batería ya terminó
- *(Verificación manual.)*

#### Scenario: Estímulo de puerta
- **WHEN** llega `CTRL,FTEST,9,4,`
- **THEN** la fila `SB_DOOR` muestra "Abre y cierra la puerta" en la zona de
  acción
- **AND** al llegar `CTRL,FTEST,9,1,` pasa a PASA (verde) y la instrucción
  desaparece
- *(Verificación manual.)*

#### Scenario: Confirmación de zumbador de la motherBoard
- **WHEN** llega `CTRL,FTEST,15,5,` y el operario pulsa Sí
- **THEN** el display envía `HMI,FTEST,CONFIRM,15,1` una sola vez y los
  botones se ocultan
- *(Verificación manual.)*

#### Scenario: Rechazo por control activo
- **WHEN** llega `CTRL,FTEST_REJECT,1`
- **THEN** la sección de motherBoard muestra "Control activo: apaga el control
  antes del test" y el resumen se muestra solo con los tests locales
- *(Verificación manual.)*

#### Scenario: Enlace perdido a mitad de batería
- **WHEN** llega `CTRL,FTEST,9,4,` y no llega ningún otro `CTRL,FTEST*`
  durante 120 s
- **THEN** las filas de motherBoard que seguían pendientes pasan a FALLA con
  detalle "sin respuesta" y el resumen lo refleja en el contador de errores
- *(Verificación manual.)*

### Requirement: Resumen, reintento y salida

La pantalla SHALL mostrar, abajo del todo de la tarjeta y mientras la batería
avanza (tests locales o de motherBoard, sin haber terminado aún), una
barra de progreso horizontal (`lv_bar`) con el cociente de tests terminales
(PASA, FALLA, AVISO o SKIP — SKIP cuenta como terminado) sobre los tests
esperados (tests locales activos + `FTEST_MB_COUNT`), y junto a ella el
veredicto en su propio recuadro, en tipografía grande (montserrat_20 o
mayor): "EN CURSO..." en blanco (`0xFFFFFF`/`0x0B2E4F`).

Al recibir `CTRL,FTEST_DONE` (o agotar los plazos) la pantalla SHALL mostrar
una cabecera "N errores - M avisos - K OK" (traducida según `g_lang`),
combinando local y motherBoard, **sin mostrar los omitidos**, y fijar el
veredicto final: **"HW OK"** en verde (`0xDFF3E4`/`0x1B7F3B`) si ningún test
(local o de motherBoard) terminó en FALLA; **"HW ERROR"** en rojo
(`0xFFE0E4`/`0xD5283C`) si al menos uno terminó en FALLA, o si la motherBoard
quedó "sin soporte", fue rechazada (`CTRL,FTEST_REJECT`) o perdió el enlace a
mitad de batería. Los AVISOS no cambian el veredicto, pero se cuentan en la
cabecera. Cada fila en FALLA o AVISO SHALL ofrecer Reintentar desde su panel
de detalle: para un ID de motherBoard envía `HMI,FTEST,RUN,id`; para uno
local repite ese test. El botón Salir SHALL cerrar el overlay, restaurar el
bloqueo por inactividad y cargar `ui_ScreenMain`. Si hay una batería de
motherBoard en curso, Salir SHALL enviar `HMI,FTEST,ABORT` antes. Si el
resumen queda abierto 10 min sin que el operario pulse Reintentar ni Salir,
la pantalla SHALL cerrarlo automáticamente por el mismo camino que Salir
(mismo precedente que `HELP_IDLE_TIMEOUT_MS`).

#### Scenario: La barra de progreso llega al 100 % aunque haya omitidos
- **WHEN** la batería termina y varios tests de motherBoard resultan SKIP
  (hardware no montado en este equipo)
- **THEN** la barra de progreso llega al 100 % igualmente, porque SKIP cuenta
  como terminado
- *(Verificación manual.)*

#### Scenario: Veredicto HW OK con avisos
- **WHEN** la batería termina sin ningún FALLA, con uno o más AVISOS
- **THEN** el veredicto muestra "HW OK" en verde
- *(Verificación manual.)*

#### Scenario: Veredicto HW ERROR por placa sin soporte
- **WHEN** no llega ningún `CTRL,FTEST*` en 10 s tras `HMI,FTEST,START`
- **THEN** el resumen muestra "Placa: sin soporte" y el veredicto es "HW
  ERROR" en rojo, aunque ningún test individual haya llegado a FALLA
- *(Verificación manual.)*

#### Scenario: Cabecera del resumen con avisos
- **WHEN** la batería termina con 1 fallo local, 2 fallos de motherBoard y 3
  avisos de motherBoard
- **THEN** la cabecera muestra "3 errores - 3 avisos - N OK" (N = resto de
  tests en PASA), sin mencionar los omitidos, y el veredicto es "HW ERROR"
- *(Verificación manual.)*

#### Scenario: Reintentar un test de la motherBoard desde el detalle
- **WHEN** el operario abre el detalle de `SB_CAMERA` en FALLA y pulsa
  Reintentar
- **THEN** el display envía `HMI,FTEST,RUN,11`, el panel se cierra y la fila
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
locales, contadores `pass`/`fail`/`skip`/`warn` (clave `mb_warn`) recibidos de
la motherBoard, `FWversion` y el **veredicto** (clave `verdict`: 0 = nunca
persistido con un valor válido, 1 = HW OK, 2 = HW ERROR). La escritura SHALL
hacerse fuera de `LVGL_Lock()`.

#### Scenario: Consulta tras reinicio
- **WHEN** se reinicia el display tras una batería con avisos
- **THEN** `hmi_ftest` conserva epoch, máscaras, contadores (incluido
  `mb_warn`) y el veredicto (`verdict`) de la última ejecución
- *(Verificación manual leyendo NVS por log al arrancar.)*

