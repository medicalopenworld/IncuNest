## MODIFIED Requirements

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
