## REMOVED Requirements

### Requirement: Botón de test de fábrica en la pantalla de arranque

**Reason**: el test de fábrica es una herramienta de montaje/servicio
técnico, no un control de uso clínico; no debe estar accesible con un solo
toque desde el splash, que ve cualquier persona que reinicie la incubadora —
fue además la causa del arranque espontáneo del test en banco (toque
fantasma del GT911 durante el init del táctil). La zona técnica (fila de
Settings) es el único sitio donde debe estar.

**Migration**: la entrada única pasa a ser la fila "Test de hardware" de
`ui_ScreenSettings` (ver "Entrada única desde Settings"). No hay dato ni
configuración que migrar: el botón del splash no persistía nada propio.

## ADDED Requirements

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
