## ADDED Requirements

### Requirement: El heading de la pantalla principal ofrece un botón de ayuda

`ui_ScreenMain` SHALL mostrar un botón de ayuda (`?`) en el heading, a la
izquierda del reloj, visible en todo momento mientras la pantalla principal
esté cargada. El resto de widgets del heading (reloj, conectividad, candado,
Bebés, alarmas, ajustes) SHALL conservar su función y su zona táctil.

La réplica del heading en `ui_ScreenLock` SHALL usar las mismas posiciones
de reloj y conectividad que `ui_ScreenMain` y NO SHALL mostrar el botón de
ayuda.

#### Scenario: El botón está en el heading y abre el menú
- **WHEN** el operador toca el `?` del heading en `ui_ScreenMain`
- **THEN** se abre el menú de ayuda como diálogo modal sobre la pantalla
  principal, con tres opciones: tutorial guiado, vídeo tutorial y contacto
- *(Verificación manual: Display_HMI no tiene entorno de test. Banco, tarea
  6.1 y 6.3.)*

#### Scenario: El heading redistribuido no rompe las zonas táctiles
- **WHEN** el operador toca el borde izquierdo del reloj o cualquier punto
  de la zona táctil ampliada del candado
- **THEN** responde el reloj o el candado, no el botón de ayuda
- *(Verificación manual en banco, tarea 6.1.)*

#### Scenario: La pantalla de bloqueo no cambia de reparto
- **WHEN** la pantalla pasa a `ui_ScreenLock`
- **THEN** reloj y conectividad aparecen en la misma posición horizontal
  que tenían en `ui_ScreenMain`
- **AND** no hay botón de ayuda
- *(Verificación manual en banco, tarea 6.2.)*

### Requirement: El menú de ayuda es modal y respeta las reglas de los diálogos

El menú de ayuda SHALL comportarse como los overlays de solo lectura del
display (`TelemetryHistory`): bloquea los toques al resto de la pantalla, se
cierra con su botón X desde cualquier vista, y cede la pantalla en cuanto
haya **cualquier** alarma activa (`UI_IsAnyAlarmActive()`, sin filtrar por
prioridad ni por lista de ids) o se pierda el enlace con la placa
(`Display_IsBoardLinkLost()`). Ni el menú ni el tutorial tienen información
de alarma propia que compense tapar la pantalla.

Mientras el menú o el tutorial guiado estén abiertos, la inactividad NO
SHALL llevar la pantalla a `ui_ScreenLock` durante `HELP_IDLE_TIMEOUT_MS`
(3 min). Pasado ese tiempo sin ningún toque, la ayuda SHALL cerrarse sola y
el auto-bloqueo normal (`INACTIVITY_TIMEOUT_MS`) vuelve a contar desde cero.
La exención nunca es indefinida: el banner de alarma solo se pinta en la
pantalla de bloqueo y una ayuda olvidada no debe impedir llegar a él.

#### Scenario: Cierre por alarma o enlace perdido
- **WHEN** el menú de ayuda (en cualquiera de sus vistas) o el tutorial
  guiado están abiertos
- **AND** la motherBoard anuncia una alarma de cualquier prioridad, o el
  enlace con la placa se pierde
- **THEN** el menú o el tutorial se cierran en la siguiente pasada del bucle
  de UI, y cualquier petición de soporte aún no publicada se descarta
- **AND** la pantalla activa vuelve a ser `ui_ScreenMain`
- *(Verificación manual en banco con la prueba de alarmas, tarea 6.9.)*

#### Scenario: Sin auto-bloqueo con la ayuda abierta, con tope
- **WHEN** el menú de ayuda o el tutorial llevan más de
  `INACTIVITY_TIMEOUT_MS` (20 s) pero menos de `HELP_IDLE_TIMEOUT_MS` (3 min)
  abiertos sin ningún toque
- **THEN** la pantalla no cambia a `ui_ScreenLock`
- **AND** al cerrarlos el temporizador de inactividad vuelve a contar desde
  cero
- *(Verificación manual en banco, tarea 6.8.)*

#### Scenario: Una ayuda olvidada se cierra sola
- **WHEN** el menú de ayuda o el tutorial llevan más de
  `HELP_IDLE_TIMEOUT_MS` abiertos sin ningún toque
- **THEN** se cierran solos y la pantalla vuelve a `ui_ScreenMain`
- **AND** 20 s después sin tocar, la pantalla pasa a `ui_ScreenLock` como
  siempre
- *(Verificación manual en banco, tarea 6.8.)*

#### Scenario: Textos en el idioma activo
- **WHEN** el operador cambia el idioma en Ajustes y vuelve a abrir la ayuda
- **THEN** el menú, cada vista y cada paso del tutorial se muestran en el
  idioma nuevo (ES, EN o FR)
- *(Verificación manual en banco, tarea 6.10.)*

### Requirement: El tutorial guiado recorre los controles reales de la UI

El tutorial SHALL resaltar, paso a paso y sobre la interfaz real, cada
control principal con un marco visible y un texto explicativo, con botones
ANTERIOR, SIGUIENTE y SALIR. SHALL navegar a la pantalla de Ajustes cuando
un paso lo requiera y SHALL devolver `ui_ScreenMain` al salir o terminar.
Durante el recorrido ningún toque SHALL accionar el control resaltado.

Los pasos cuyo control no esté visible en ese momento SHALL saltarse.

#### Scenario: Recorrido completo
- **WHEN** el operador pulsa SIGUIENTE en cada paso desde la bienvenida
- **THEN** se resaltan, en orden, reloj, conectividad, candado, Bebés,
  alarmas, Ajustes, temperatura, humedad, fototerapia, y las filas Info,
  WiFi, Idiomas y Modos de la pantalla de Ajustes, y se termina en la
  pantalla principal
- *(Verificación manual en banco, tarea 6.7.)*

#### Scenario: Control oculto
- **WHEN** el control de humedad está deshabilitado en Ajustes (`ui_HumCont`
  oculto)
- **THEN** el tutorial pasa de temperatura a fototerapia sin mostrar el paso
  de humedad
- *(Verificación manual en banco, tarea 6.7.)*

#### Scenario: Nada se acciona durante el tutorial
- **WHEN** el operador toca el control resaltado o cualquier otro punto de
  la pantalla fuera del bocadillo
- **THEN** no cambia ningún ajuste ni se abre ninguna pantalla
- *(Verificación manual en banco, tarea 6.7.)*

### Requirement: El vídeo tutorial se ofrece como código QR a la web

La vista de vídeo tutorial SHALL mostrar un código QR generado en el
dispositivo con `SUPPORT_TUTORIAL_URL`, la URL en texto legible y una
instrucción de escaneo.

#### Scenario: El QR abre la web
- **WHEN** el operador escanea el QR con la cámara de un móvil
- **THEN** el móvil ofrece abrir exactamente `SUPPORT_TUTORIAL_URL`
- *(Verificación manual en banco, tarea 6.4.)*

### Requirement: El contacto con soporte compone asunto e informe automáticamente

La vista de contacto SHALL permitir escribir un mensaje breve (≤ 160
caracteres ASCII) con un teclado en pantalla. El asunto SHALL ser
`IncuNest SN <serie a 4 cifras> - Solicitud de soporte` y el cuerpo SHALL
contener el mensaje seguido de un informe de depuración ASCII de ≤ 400
bytes con, al menos: número de serie, versión de firmware de HMI y de
motherBoard, hardware, contador de arranques y motivo del último reset,
tiempo encendido, estado WiFi (RSSI, IP), estado de conexión con
ThingsBoard, estado del enlace serie, idioma, modo y actuación de control,
consignas, telemetría actual, estado de la sonda, fototerapia, bitmask de
alarmas activas y silenciadas, títulos de las alarmas activas, y memoria
libre interna y PSRAM.

El destinatario SHALL ser `SUPPORT_EMAIL`, definido con valor por defecto
en `Credentials_public.h` y redefinible desde `Credentials.h`.

#### Scenario: El operador siempre puede enviar desde su móvil
- **WHEN** el operador pulsa QR MOVIL, o pulsa ENVIAR sin conexión con el
  servidor
- **THEN** se muestra un QR `mailto:` con destinatario `SUPPORT_EMAIL`,
  el asunto con el número de serie y el cuerpo con mensaje e informe
- **AND** si no había servidor se indica "Sin conexion con el servidor"
- *(Verificación manual en banco, tarea 6.5.)*

#### Scenario: El QR degrada antes de fallar
- **WHEN** el contenido `mailto:` no cabe en el QR del tamaño elegido
- **THEN** se regenera sin el mensaje libre, y si aún no cabe, sin el informe
- **AND** la pantalla indica qué se ha omitido
- *(Verificación manual: forzar un mensaje de 160 caracteres, tarea 6.5.)*

### Requirement: El envío desde el equipo usa el canal ThingsBoard y nunca bloquea la UI

Cuando `WIFIIsConnectedToServer()` sea verdadero, ENVIAR SHALL publicar la
petición como telemetría con las claves `support_request` (asunto),
`support_message`, `support_report` y `support_to`, desde la tarea WiFi/OTA
y nunca desde un callback de LVGL. La UI SHALL mostrar el resultado
(registrada / fallida) y SHALL darlo por fallido si no hay respuesta en 15 s.
En ambos casos SHALL ofrecer el QR `mailto:` como vía alternativa.

Sin conexión con el servidor NO SHALL encolarse ninguna petición.

#### Scenario: Envío con servidor
- **WHEN** hay conexión con ThingsBoard y el operador pulsa ENVIAR
- **THEN** la UI muestra "Enviando..." y, tras la publicación, "Peticion
  registrada"
- **AND** el dispositivo de ThingsBoard recibe la telemetría con las cuatro
  claves y `support_to` igual a `SUPPORT_EMAIL`
- *(Verificación manual en banco con la consola de ThingsBoard, tarea 6.6.)*

#### Scenario: Fallo de publicación
- **WHEN** la publicación falla o no se confirma en 15 s
- **THEN** la UI muestra el fallo y el QR `mailto:` con el mismo contenido
- *(Verificación manual: cortar la red tras pulsar ENVIAR, tarea 6.6.)*

#### Scenario: Sin servidor no se encola nada
- **WHEN** no hay conexión con el servidor y el operador pulsa ENVIAR
- **THEN** no se publica nada más tarde al recuperar la conexión
- **AND** la UI pasa directamente a la vista del QR
- *(Verificación manual en banco: reconectar tras el intento y comprobar
  que no aparece telemetría de soporte, tarea 6.5.)*
