## ADDED Requirements

### Requirement: Botón de test de fábrica en la pantalla de arranque

`ui_ScreenIntro` SHALL mostrar un botón "TEST FÁBRICA" (traducido según
`g_lang`). Al pulsarlo, el display SHALL cancelar la transición automática a
`ui_ScreenMain`, abrir la pantalla de test y suspender el bloqueo por
inactividad mientras esté abierta. El botón SHALL existir solo en el splash:
ninguna otra pantalla ofrece la entrada.

#### Scenario: Pulsar durante el splash
- **WHEN** el operario toca el botón en los primeros 5 s
- **THEN** el splash no avanza a la pantalla principal y aparece la pantalla de
  test con todos los tests en estado pendiente
- **AND** a los 20 s no aparece `ui_ScreenLock`
- *(Verificación manual en el CrowPanel: Display_HMI no tiene entorno de test.)*

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
`HMI_PANEL` (cinco rectángulos a pantalla completa rojo, verde, azul, blanco y
negro, 800 ms cada uno, después Sí / No), `HMI_TOUCH` (cinco objetivos en las
esquinas y el centro, acierto a ≤ 40 px; 20 s por objetivo), `HMI_BUZZER` y
`HMI_SPEAKER` (300 ms con `buzzerOn()/Off()` y los comandos 248/249, después
Sí / No), `HMI_WIFI` (MAC en `detail`; PASS si `WIFIIsConnected()` con RSSI,
si no `WiFi.scanNetworks(true)` y PASS si ve ≥ 1 red), `HMI_NVS` (escribir y
releer `hmi_ftest/probe` fuera de `LVGL_Lock()`), `HMI_LINK`
(`Display_BoardEverSeen() && !Display_IsBoardLinkLost()`, `fwVer` de la MB en
`detail`). Las preguntas al operario SHALL expirar a los 60 s como FAIL.

#### Scenario: Panel con zona muerta
- **WHEN** el operario responde No a la pregunta del panel
- **THEN** `HMI_PANEL` queda en FALLA y la secuencia continúa con el toque
- *(Verificación manual.)*

#### Scenario: Toque en las esquinas
- **WHEN** el operario toca los cinco objetivos en orden a menos de 40 px
- **THEN** `HMI_TOUCH` da PASA y en `detail` figura el error máximo en px
- *(Verificación manual.)*

#### Scenario: Chasquido y patrón de alarma no interfieren
- **WHEN** `HMI_BUZZER` está sonando
- **THEN** `click_beep_start()` no emite y `link_audio_service()` no apaga el
  zumbador hasta que el test termina, por el mismo arbitraje que ya usan entre
  sí
- *(Verificación manual.)*

### Requirement: Orquestación del test remoto de la motherBoard

Tras los tests locales la pantalla SHALL enviar `HMI,FTEST,START` y añadir una
fila por cada `CTRL,FTEST` recibido, actualizándola con su estado. En `WAIT`
SHALL mostrar la instrucción asociada al ID (`SB_DOOR`: "Abre y cierra la
puerta", `SB_LIGHT`: "Tapa el sensor de luz"); en `CONFIRM` SHALL mostrar la
pregunta del ID y los botones Sí / No, cuyo resultado envía
`HMI,FTEST,CONFIRM,id,ok`. Un `CTRL,FTEST_REJECT` SHALL mostrarse con su
motivo traducido. Si no llega ningún `CTRL,FTEST*` en 10 s SHALL marcarse "MB
sin soporte". Si tras el primer `CTRL,FTEST` la motherBoard deja de emitir
durante 120 s sin llegar `CTRL,FTEST_DONE` ni `CTRL,FTEST_REJECT`, la pantalla
SHALL marcar como FALLA con detalle "sin respuesta" todas las filas de
motherBoard que no hayan llegado a un estado terminal, mostrar "Placa: enlace
perdido durante el test" en el resumen y pasar a él. Cualquier `CTRL,FTEST*`
recibido SHALL reiniciar ese plazo de 120 s. Las líneas SHALL parsearse en
`CommTask.cpp` con `ftest_parse_result()` a un anillo de 8 entradas protegido
con `portMUX_TYPE`, drenado por `FactoryTest_Poll()`; ninguna llamada a LVGL
desde `Comm_Task`.

#### Scenario: Estímulo de puerta
- **WHEN** llega `CTRL,FTEST,11,4,`
- **THEN** la fila `SB_DOOR` muestra "Abre y cierra la puerta" en amarillo
- **AND** al llegar `CTRL,FTEST,11,1,` pasa a PASA en azul y la instrucción
  desaparece
- *(Verificación manual.)*

#### Scenario: Confirmación de zumbador
- **WHEN** llega `CTRL,FTEST,17,5,` y el operario pulsa Sí
- **THEN** el display envía `HMI,FTEST,CONFIRM,17,1` una sola vez y los
  botones se ocultan
- *(Verificación manual.)*

#### Scenario: Rechazo por control activo
- **WHEN** llega `CTRL,FTEST_REJECT,1`
- **THEN** la sección de motherBoard muestra "Control activo: apaga el control
  antes del test" y el resumen se muestra solo con los tests locales
- *(Verificación manual.)*

#### Scenario: Enlace perdido a mitad de batería
- **WHEN** llega `CTRL,FTEST,11,4,` y no llega ningún otro `CTRL,FTEST*`
  durante 120 s
- **THEN** las filas de motherBoard que seguían pendientes pasan a FALLA con
  detalle "sin respuesta" y el resumen muestra "Placa: enlace perdido durante
  el test"
- *(Verificación manual.)*

### Requirement: Resumen, reintento y salida

Al recibir `CTRL,FTEST_DONE` (o agotar los plazos) la pantalla SHALL mostrar
el resumen PASA / FALLA / omitidos de ambas placas. Cada fila en FALLA SHALL
ofrecer Reintentar: para un ID de motherBoard envía `HMI,FTEST,RUN,id`; para
uno local repite ese test. El botón Salir SHALL cerrar el overlay, restaurar
el bloqueo por inactividad y cargar `ui_ScreenMain`. Si hay una batería de
motherBoard en curso, Salir SHALL enviar `HMI,FTEST,ABORT` antes. Si el
resumen queda abierto 10 min sin que el operario pulse Reintentar ni Salir, la
pantalla SHALL cerrarlo automáticamente por el mismo camino que Salir (mismo
precedente que `HELP_IDLE_TIMEOUT_MS`).

#### Scenario: Reintentar un test de la motherBoard
- **WHEN** el operario pulsa Reintentar en la fila `SB_CAMERA`
- **THEN** el display envía `HMI,FTEST,RUN,13`, la fila vuelve a "en curso" y
  se actualiza con el nuevo resultado
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
locales, contadores `pass`/`fail`/`skip` recibidos de la motherBoard y
`FWversion`. La escritura SHALL hacerse fuera de `LVGL_Lock()`.

#### Scenario: Consulta tras reinicio
- **WHEN** se reinicia el display tras una batería
- **THEN** `hmi_ftest` conserva epoch, máscaras y contadores de la última
  ejecución
- *(Verificación manual leyendo NVS por log al arrancar.)*
