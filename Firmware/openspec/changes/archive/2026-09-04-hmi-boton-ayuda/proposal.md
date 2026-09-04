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
  3. **Contacto con soporte**: un QR `mailto:` que el operador escanea con
     su móvil. Se abre un correo a `SUPPORT_EMAIL` con el asunto
     `IncuNest SN 0042 - Solicitud de soporte` y, en el cuerpo, un informe
     de depuración (identidad, versiones, arranque, red, enlace, control,
     telemetría, alarmas, memoria) bajo el que el operador escribe su
     consulta. El correo sale de la cuenta del operador: el equipo no envía
     nada por red, así que funciona igual con o sin WiFi. Un botón quita el
     informe del QR si el móvil no lo lee. (Una primera versión publicaba
     además la petición como telemetría ThingsBoard para que el servidor la
     reenviara por correo; se retiró por decisión de producto para
     simplificar: exigía una regla en el servidor y un formulario con
     teclado en pantalla.)
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

No se añade ninguna librería nueva (`lib_deps` intacto): los QR los dibuja
LVGL (`lv_qrcode`, ya habilitado).

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
  poll, callback, auto-bloqueo), `include/protocol/Credentials_public.h`
  (defaults). `Wifi_OTA.cpp` no cambia.
- **Servidor**: sin cambios. El contacto no usa ThingsBoard ni ningún otro
  canal del equipo.
- **Flash**: build de referencia al 78 % (2 454 400 B de 3 145 728 B). El
  cambio añade código y una tabla de textos en 3 idiomas; no añade assets de
  imagen (el `?` es texto).
- **Sin cambios de protocolo serie** (`PROTOCOL.md` intacto) ni de `shared/`.
- **Verificación**: manual, en el CrowPanel real. `Display_HMI` no tiene
  entorno de test; se documenta qué se probó a mano.
- **Riesgo conocido**: `known_issues.md` no documenta bugs en el heading ni
  en los diálogos; este cambio no toca alarmas, parseo serie ni arranque.
