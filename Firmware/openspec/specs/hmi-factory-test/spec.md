# hmi-factory-test Specification

## Purpose
TBD - created by archiving change shared-factory-test. Update Purpose after archive.
## Requirements
### Requirement: Barrera de entrada de seguridad

Antes de ejecutar cualquier test, local o remoto, la pantalla SHALL mostrar
un **pop-up modal** (tarjeta ~560×260, centrada sobre el overlay, mismo
patrón de hand-off que el panel de detalle: los callbacks de LVGL solo
escriben la intención, `FactoryTest_Poll()` la resuelve) con: título
"ATENCIÓN" / "WARNING" / "ATTENTION" en tipografía grande, el texto "el
equipo debe estar VACÍO, sin paciente; los actuadores se encenderán en lazo
abierto. ¿Continuar?" (traducido según `g_lang`) y botones Sí / No grandes
(≥200×56 px). Mientras el pop-up esté abierto, la cuadrícula de resultados y
la barra de progreso/veredicto SHALL permanecer ocultas (no pintarse). Sí
SHALL iniciar la secuencia de tests locales. No SHALL cerrar el overlay sin
ejecutar ningún test, permaneciendo en `ui_ScreenSettings`, la única pantalla
desde la que se puede abrir el test.

#### Scenario: Aceptar la barrera
- **WHEN** el operario toca la fila "Test de hardware" y luego Sí en el pop-up
  de seguridad
- **THEN** arranca `HMI_SYSINFO`, el primer test de la secuencia local, y el
  pop-up se cierra
- *(Verificación manual en el CrowPanel: Display_HMI no tiene entorno de test.)*

#### Scenario: Rechazar la barrera
- **WHEN** el operario pulsa No en el pop-up de seguridad
- **THEN** el overlay se cierra sin haber ejecutado ningún test y la pantalla
  activa sigue siendo `ui_ScreenSettings`
- *(Verificación manual.)*

#### Scenario: El aviso es un pop-up, no un texto de fondo
- **WHEN** se abre la pantalla de test y todavía no se ha contestado el aviso
  de seguridad
- **THEN** se ve una tarjeta ~560×260 centrada, con título "ATENCIÓN" y los
  botones Sí/No, y ni la cuadrícula de resultados ni la barra de
  progreso/veredicto se pintan detrás mientras tanto
- *(Verificación manual en el CrowPanel.)*

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
recibido SHALL reiniciar ese plazo de 120 s.

Las líneas SHALL parsearse en `CommTask.cpp` con `ftest_parse_result()` a un
anillo de **32 entradas** (no 8: la motherBoard puede emitir una ráfaga de
`RUNNING`+`SKIP` casi instantánea al omitir varios tests seguidos, y 8 se
desbordaba — banco 2026-09-06) protegido con `portMUX_TYPE`. Dentro de la
sección crítica del anillo (y de cualquier otra sección crítica de
`CommTask.cpp`, p. ej. `g_telemetry_mux`) SHALL prohibirse cualquier llamada
de log o a LVGL: escribir por serie con las interrupciones deshabilitadas
dispara el Interrupt WDT del núcleo — causa confirmada del reinicio del HMI
observado en banco. El descarte por anillo lleno SHALL marcarse con un flag
dentro de la sección crítica y registrarse (log) **fuera** de ella, e
incrementar un contador `g_ftestRingDrops` visible (si > 0) en el detalle de
la fila "Enlace" del resumen. `FactoryTest_Poll()` SHALL drenar el anillo
entero cada pasada, en **todos** los estados en los que la motherBoard puede
estar emitiendo (`RemoteAwaitFirst`, `RemoteRunning`, y tras un Reintentar
lanzado desde el resumen, que vuelve a `RemoteRunning`): el drenado NO SHALL
depender de qué esté mostrando la pantalla en ese momento (panel de detalle o
pop-up de entrada abiertos no lo bloquean, ni retrasan más de una pasada de
`FactoryTest_Poll()`).

Los contadores agregados que consume el resto de la pantalla — la cabecera
del resumen ("N errores - M avisos - K OK", ver "Resumen, reintento y
salida") y la persistencia en NVS (ver "Persistencia del resultado en el
display") — SHALL calcularse **siempre** recorriendo las filas ya
construidas por este requisito (locales y de motherBoard, estado terminal
PASA/FALLA/AVISO/OMITIDO), incluidas las filas que el propio display marcó
FALLA por su cuenta (timeout de fila, enlace perdido), y nunca de los
contadores sueltos de `CTRL,FTEST_DONE` (`g_ftestDonePass`/`Fail`/`Skip`/
`Warn`). `CTRL,FTEST_DONE` SHALL seguir usándose únicamente para cerrar la
batería (pasar a `Summary`); si sus contadores no coinciden con lo que las
filas realmente muestran en ese momento, SHALL registrarse en el log a nivel
aviso, sin alterar la cabecera ni la persistencia que ve el operario.

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

#### Scenario: Ráfaga de descartes sin reiniciar el HMI

- **WHEN** la motherBoard emite más de 32 líneas `CTRL,FTEST` sin que
  `FactoryTest_Poll()` llegue a drenarlas (ráfaga de RUNNING+SKIP casi
  instantánea al omitir varios tests seguidos)
- **THEN** el anillo descarta las más antiguas, `g_ftestRingDrops` se
  incrementa y se registra un log por descarte, y el HMI **no** se reinicia
  (sin `Interrupt wdt timeout`)
- *(Verificación manual en el CrowPanel — banco 2026-09-06: reproducir con
  varios tests SKIP consecutivos y confirmar que no aparece "Guru Meditation
  Error" en el monitor serie.)*

#### Scenario: Descartes visibles en el detalle de "Enlace"

- **WHEN** `g_ftestRingDrops` > 0 al terminar la batería
- **THEN** el panel de detalle de la fila local "Enlace" muestra el número de
  descartes, además de su información habitual (`fwVer` de la MB)
- *(Verificación manual.)*

#### Scenario: Cabecera coherente con las filas aunque falte FTEST_DONE

- **WHEN** una o más filas de motherBoard pasan a FALLA por timeout de fila o
  por enlace perdido, sin que llegue nunca `CTRL,FTEST_DONE`
- **THEN** la cabecera del resumen ("N errores - M avisos - K OK") cuenta esas
  filas como error, coherente con los botones en rojo de la cuadrícula y con
  el veredicto "HW ERROR" — nunca "0 errores" con botones rojos a la vista
- *(Verificación manual en el CrowPanel — banco 2026-09-06, hallazgo del
  operario.)*

#### Scenario: El drenado del anillo no depende de la pantalla abierta

- **WHEN** la motherBoard sigue emitiendo `CTRL,FTEST` durante `RemoteRunning`
  mientras el operario tiene abierto el panel de detalle de otra fila, o
  mientras responde el pop-up de una `CONFIRM`
- **THEN** `FactoryTest_Poll()` sigue drenando el anillo esa misma pasada (o,
  como mucho, la siguiente si esa pasada resolvió un hand-off de UI) y
  ninguna fila se queda desactualizada por tener otra ventana abierta
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

### Requirement: Entrada única desde Settings

`ui_ScreenSettings` SHALL ofrecer la única entrada al test de fábrica: una
fila "Test de hardware" (traducida según `g_lang`) al final de la lista,
debajo de Info, con el mismo estilo (Cont/Panel/Label/Button/Arrow) que las
filas existentes. La fila SHALL estar habilitada únicamente cuando
`UI_AnyControlActive()` devuelve false (ningún control de
temperatura/humedad ni fototerapia activos — mismo criterio que usa el
diálogo de salida de bebé). Mientras algo esté activo, la fila SHALL
mostrarse deshabilitada (texto atenuado, sin reaccionar al toque) con un
subtexto "Apaga el control para testear" (traducido). Este estado SHALL
refrescarse en cada pasada de `UI_Task` mientras `ui_ScreenSettings` esté en
pantalla — cubriendo tanto la entrada a Settings como cualquier `CTRL,STATE`
que active o desactive el control mientras el operario sigue ahí — sin
repintar si no cambió desde la última pasada. Al pulsar la fila (con la
condición cumplida) SHALL abrirse la pantalla de test, con el aviso de la
barrera de entrada. Si el operario responde "No" al aviso (o pulsa "Salir"
sin haberlo respondido todavía, antes de que arranque ningún test), el
overlay SHALL cerrarse sin cargar `ui_ScreenMain` — permaneciendo en
`ui_ScreenSettings`, de donde vino. Cualquier cierre posterior a esa barrera
(batería completa o abortada a mitad mediante "Salir") SHALL conservar el
comportamiento existente y cargar `ui_ScreenMain`.

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
- **THEN** el overlay se cierra y el display carga `ui_ScreenMain`
- *(Verificación manual.)*

### Requirement: Varios tests en curso a la vez

La pantalla SHALL admitir varias filas de motherBoard en RUNNING
simultáneamente y resultados fuera de orden de id. La fila que sigue la
página SHALL ser la que recibió RUNNING más recientemente (el test activo del
momento). En la ordenación, dentro del bucket "en curso", WAIT SHALL ir antes
que RUNNING y el RUNNING más reciente primero. El watchdog por fila SHALL ser
`FTEST_ROW_TIMEOUT_MS` = 150 s: cubre los 90 s por test activo de la
motherBoard y los 45 s de plazo pasivo máximo con margen.

#### Scenario: Página que sigue al activo
- **WHEN** hay quince filas en RUNNING por los pasivos y llega
  `CTRL,FTEST,<id de actuators>,0`
- **THEN** la página salta a la fila de actuadores, no a la primera fila
  blanca de la lista
- *(Verificación manual en el CrowPanel.)*

#### Scenario: Pasivo legítimamente largo
- **WHEN** `gsm_at` lleva 100 s en RUNNING porque la parte activa tardó y su
  plazo de 45 s aún no se ha cumplido desde su arranque
- **THEN** el HMI no lo marca FALLA `timeout` antes de 150 s
- *(Verificación manual.)*

