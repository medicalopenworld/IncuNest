# hmi-help-center Specification

## Purpose
TBD - created by archiving change hmi-boton-ayuda. Update Purpose after archive.
## Requirements
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
  principal, con tres opciones: tutorial guiado (que abre el selector de
  cursos de `hmi-training-courses`), vídeo tutorial y contacto
- *(Verificación manual en banco.)*

#### Scenario: El heading redistribuido no rompe las zonas táctiles
- **WHEN** el operador toca el borde izquierdo del reloj o cualquier punto
  de la zona táctil ampliada del candado
- **THEN** responde el reloj o el candado, no el botón de ayuda
- *(Verificación manual en banco.)*

#### Scenario: La pantalla de bloqueo no cambia de reparto
- **WHEN** la pantalla pasa a `ui_ScreenLock`
- **THEN** reloj y conectividad aparecen en la misma posición horizontal
  que tenían en `ui_ScreenMain`
- **AND** no hay botón de ayuda
- *(Verificación manual en banco.)*

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
  de UI
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
ANTERIOR, SIGUIENTE y SALIR. El resto de la pantalla SHALL quedar atenuado
mientras el interior del marco conserva su brillo normal (foco). SHALL navegar a la pantalla de Ajustes cuando
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

#### Scenario: Foco
- **WHEN** el tutorial está en un paso con control
- **THEN** el control se ve con su brillo normal dentro del marco ámbar y el
  resto de la pantalla atenuado
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

### Requirement: El contacto con soporte es un QR `mailto:` con el número de serie en el asunto

La vista de contacto SHALL mostrar un código QR generado en el dispositivo
con un URI `mailto:` cuyo destinatario es `SUPPORT_EMAIL` (valor por
defecto en `Credentials_public.h`, redefinible desde `Credentials.h`) y
cuyo asunto es `IncuNest SN <serie a 4 cifras> - Solicitud de soporte`. El
operador escanea el QR con su móvil y el correo sale de su propia cuenta:
el equipo no envía nada por red. La vista SHALL mostrar además, en texto,
el destinatario y el asunto.

El cuerpo del correo SHALL llevar, por defecto, un informe de depuración
ASCII de ≤ 400 bytes con, al menos: número de serie, versión de firmware de
HMI y de motherBoard, hardware, contador de arranques y motivo del último
reset, tiempo encendido, estado WiFi (RSSI, IP) y de conexión con
ThingsBoard, estado del enlace serie, idioma, modo y actuación de control,
consignas, telemetría actual, estado de la sonda, fototerapia, bitmask de
alarmas activas y silenciadas, títulos de las alarmas activas, y memoria
libre interna y PSRAM. El informe NO SHALL contener datos del paciente. El
operador SHALL poder quitar el informe del QR (botón SIN INFORME) si su
móvil no lo lee.

#### Scenario: El QR abre un correo a soporte con el asunto correcto
- **WHEN** el operador entra en CONTACTAR SOPORTE y escanea el QR con la
  cámara de un móvil
- **THEN** el móvil ofrece abrir su app de correo con destinatario
  `SUPPORT_EMAIL`, asunto `IncuNest SN 0042 - Solicitud de soporte` (con el
  número de serie del equipo) y el informe de depuración en el cuerpo
- *(Verificación manual en banco, tarea 6.5.)*

#### Scenario: QR reducido sin informe
- **WHEN** el operador pulsa SIN INFORME
- **THEN** el QR se regenera solo con destinatario y asunto, y la pantalla
  lo indica
- **AND** CON INFORME lo devuelve al contenido completo
- *(Verificación manual en banco, tarea 6.5.)*

#### Scenario: El QR degrada antes de fallar
- **WHEN** el contenido `mailto:` con informe no cabe en el QR del tamaño
  elegido
- **THEN** se regenera sin el informe y la pantalla indica que se ha omitido
- *(No alcanzable con los presupuestos actuales, ~650 B frente a 914 B de
  capacidad; queda como red de seguridad. Verificación manual: revisar en
  banco que el QR con informe se lee, tarea 6.5.)*

