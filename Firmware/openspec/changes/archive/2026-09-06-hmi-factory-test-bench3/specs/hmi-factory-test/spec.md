## MODIFIED Requirements

### Requirement: Barrera de entrada de seguridad

Antes de ejecutar cualquier test, local o remoto, la pantalla SHALL mostrar
un **pop-up modal** (tarjeta ~560×260, centrada sobre el overlay, mismo
patrón de hand-off que el panel de detalle: los callbacks de LVGL solo
escriben la intención, `FactoryTest_Poll()` la resuelve) con: título
"ATENCIÓN" / "WARNING" / "ATTENTION" en tipografía grande, el texto "el
equipo debe estar VACÍO, sin paciente; los actuadores se encenderán en lazo
abierto. ¿Continuar?" (traducido según `g_lang`) y botones Sí / No grandes
(≥200×56 px). Mientras el pop-up esté abierto, la cuadrícula de resultados y
la barra de progreso/veredicto SHALL permanecer ocultas (no pintarse) — nada
que testear todavía, el operario aún no ha contestado. Sí SHALL iniciar la
secuencia de tests locales. No SHALL cerrar el overlay sin ejecutar ningún
test. Si el origen fue el botón del splash, el flujo SHALL devolverse al
comportamiento normal del splash (`g_factoryTestRequested` vuelve a `false`
y la transición automática a `ui_ScreenMain` puede continuar); si el origen
fue la fila de `ui_ScreenSettings`, el overlay SHALL cerrarse permaneciendo
en `ui_ScreenSettings` en su lugar.

#### Scenario: Aceptar la barrera

- **WHEN** el operario pulsa "TEST FÁBRICA" y luego Sí en el pop-up de
  seguridad
- **THEN** arranca `HMI_SYSINFO`, el primer test de la secuencia local, y el
  pop-up se cierra
- *(Verificación manual en el CrowPanel: Display_HMI no tiene entorno de test.)*

#### Scenario: Rechazar la barrera desde el splash

- **WHEN** el operario pulsa No en el pop-up de seguridad, habiendo entrado
  desde el botón del splash
- **THEN** el overlay se cierra sin haber ejecutado ningún test y el splash
  retoma su temporizador normal
- *(Verificación manual.)*

#### Scenario: Rechazar la barrera desde Settings

- **WHEN** el operario pulsa No en el pop-up de seguridad, habiendo entrado
  desde la fila "Test de hardware" de `ui_ScreenSettings`
- **THEN** el overlay se cierra sin haber ejecutado ningún test y la pantalla
  activa sigue siendo `ui_ScreenSettings`
- *(Verificación manual.)*

#### Scenario: El aviso es un pop-up, no un texto de fondo

- **WHEN** se abre la pantalla de test (splash o Settings) y todavía no se ha
  contestado el aviso de seguridad
- **THEN** se ve una tarjeta ~560×260 centrada, con título "ATENCIÓN" y los
  botones Sí/No, y NI la cuadrícula de resultados NI la barra de
  progreso/veredicto se pintan detrás mientras tanto
- *(Verificación manual en el CrowPanel: banco 2026-09-06, hallazgo del
  operario — el aviso anterior se pintaba como texto en la zona de acción y
  se confundía con el resto de la pantalla.)*

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
