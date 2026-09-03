## Why

Afecta solo a **Display_HMI**. No toca `shared/` ni `motherBoard`; el único
canal saliente que usa (telemetría ThingsBoard por MQTT) ya existe en el
display y no cambia de forma para la motherBoard.

Hoy el operador que no sabe qué hace un botón, o que tiene un problema con la
incubadora, no tiene ninguna salida desde la propia pantalla: no hay ayuda
contextual, no hay referencia a documentación externa y la única vía de
soporte es que alguien del hospital conozca de memoria el correo del
fabricante y describa a mano el estado del equipo. En campo (Ghana, Togo,
Senegal) eso se traduce en incidencias que llegan tarde, sin número de serie,
sin versión de firmware y sin el estado de alarmas del momento, y en
formaciones que dependen de que haya alguien presencial.

La pantalla ya tiene todo lo que hace falta para cerrar ese hueco sin
hardware nuevo: un heading permanente donde cabe un botón más
(`ElementsCreation.cpp:845-864`), un patrón de diálogo modal probado
(`TimeDialog.cpp`), `LV_USE_QRCODE` habilitado y sin usar
(`lv_conf.h:648`), y un enlace MQTT con ThingsBoard que publica telemetría
cada 5 s (`Wifi_OTA.cpp:461-469`).

## What Changes

- **Botón de ayuda en el heading** de `ui_ScreenMain`: un botón redondo con
  `?` a la izquierda del reloj, en un nuevo slot `HEADING_SLOT0_HELP`. El
  lado izquierdo del heading se redistribuye (reloj y conectividad se
  desplazan a la derecha) sin invadir la zona táctil ampliada del candado.
  La réplica del heading en `ui_ScreenLock` usa los mismos slots para que
  reloj y conectividad no salten al bloquear, pero **no** lleva botón de
  ayuda: la pantalla de bloqueo existe para no reaccionar a toques.
- **Menú de ayuda** (`HelpDialog`, modal sobre `ui_ScreenMain`) con tres
  opciones:
  1. **Tutorial guiado** (`HelpTour`): recorrido paso a paso sobre la UI
     real que resalta cada control (reloj, conectividad, candado, Bebés,
     alarmas, ajustes, aire/piel, humedad, fototerapia, filas de Ajustes) con
     un bocadillo explicativo y botones Anterior / Siguiente / Salir. Navega
     a la pantalla de Ajustes cuando el paso lo pide y vuelve a la principal
     al salir. Los pasos cuyo control esté oculto (p. ej. humedad
     deshabilitada) se saltan.
  2. **Vídeo tutorial**: código QR (`lv_qrcode`) con la URL de la web de
     Medical Open World, más la URL en texto por si el QR no se puede
     escanear.
  3. **Contacto con soporte**: formulario con teclado en pantalla para un
     mensaje breve. El asunto se compone automáticamente como
     `IncuNest SN 0042 - Solicitud de soporte` y el cuerpo lleva el mensaje
     más un informe de depuración (identidad, versiones, arranque, red,
     enlace, control, telemetría, alarmas, memoria). Dos vías de envío:
     - **Desde el equipo**: si hay conexión con ThingsBoard, se publica como
       telemetría (`support_request`, `support_message`, `support_report`,
       `support_to`) y una regla del servidor la reenvía por correo a
       `SUPPORT_EMAIL`. La publicación la hace la tarea WiFi/OTA, nunca el
       callback de LVGL (regla ARQ-LOCK-001: nada de red bajo el mutex de
       LVGL).
     - **Desde el móvil**: siempre disponible, con o sin red. Un QR
       `mailto:` con destinatario, asunto y cuerpo ya rellenos; el operador
       lo escanea y envía desde su propio correo.
- **Configuración**: `SUPPORT_EMAIL` y `SUPPORT_TUTORIAL_URL` con valores
  por defecto en `Credentials_public.h` (fichero versionado), redefinibles
  desde `Credentials.h` (no versionado). El correo por defecto es
  `support@medicalopenworld.org`.
- **Auto-bloqueo**: mientras el menú de ayuda o el tutorial estén abiertos,
  la inactividad no manda a `ui_ScreenLock` (hoy solo `ui_ScreenAlarms`
  está exento, `UITask.cpp:3354-3358`). Leer un QR o seguir un tutorial
  lleva más de 20 s sin tocar.
- **Alarma crítica**: cierra menú y tutorial, con el mismo contrato que
  `TimeDialog_Poll()`.

No se añade ninguna librería nueva (`lib_deps` intacto): el QR lo dibuja
LVGL, el teclado es un `lv_btnmatrix` como en `BabyWizard`, y el envío usa el
SDK de ThingsBoard ya presente.

## Capabilities

### New Capabilities

- `hmi-help-center`: botón de ayuda en el heading, menú con tutorial guiado,
  QR de vídeo tutorial y formulario de contacto con informe de depuración.

### Modified Capabilities

Ninguna capability existente cambia de contrato. El heading se redistribuye
pero mantiene todos sus widgets y comportamientos.

## Impact

- **Código**: `Display_HMI/src/ui/HelpDialog.cpp`, `HelpTour.cpp` (nuevos),
  `src/modules/support/support_report.cpp` (nuevo),
  `src/ui/ElementsCreation.cpp` (heading), `src/tasks/UITask.cpp` (init,
  poll, callback, auto-bloqueo), `src/tasks/Wifi_OTA.cpp` (publicación),
  `include/protocol/Credentials_public.h` (defaults).
- **Servidor**: hace falta una regla en ThingsBoard (`mon.medicalopenworld.org`)
  que, al recibir telemetría con `support_request`, envíe un correo a
  `support_to`. Se documenta en `docs/thingsboard_dashboards.md`. Hasta que
  exista, la telemetría queda registrada en el dispositivo de ThingsBoard
  (consultable) y la vía del móvil funciona igual.
- **Flash**: build de referencia al 78 % (2 454 400 B de 3 145 728 B). El
  cambio añade código y una tabla de textos en 3 idiomas; no añade assets de
  imagen (el `?` es texto).
- **Sin cambios de protocolo serie** (`PROTOCOL.md` intacto) ni de `shared/`.
- **Verificación**: manual, en el CrowPanel real. `Display_HMI` no tiene
  entorno de test; se documenta qué se probó a mano.
- **Riesgo conocido**: `known_issues.md` no documenta bugs en el heading ni
  en los diálogos; este cambio no toca alarmas, parseo serie ni arranque.
